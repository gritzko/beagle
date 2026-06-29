//  submount.js — GET-side submodule MOUNT + checkout (DIS-058 D2-D5,D13).
//  Pure JS over wire.js (child fetch), ingest.js (sibling-shard clone),
//  store.js (read the pinned tree), checkout.js (materialise the sub wt) and
//  ulog.js (the sub wtlog anchor).  Implements [Submodules] Recursion §1:
//
//    GET pre-order — after the parent's own files are written, each gitlink
//    leaf is MOUNTED: fetch the child shard from the SAME source (the parent's
//    remote with the project swapped to the child [Title]), CLONE it as a
//    sibling shard at `<beDir>/<title>/` (flat, same level as the parent —
//    [Store] layout), WRITE the sub wtlog anchor `<wt>/<path>/.be`, and CHECK
//    OUT the commit named by the parent's gitlink pin.  The same-source fetch
//    falls back to the `.gitmodules` URL when it fails.  The child wt tracks a
//    SYNTHETIC branch `/<title>/.<parent>[/<parent_branch>]` ([Submodules]
//    bullet 1) — recorded in the sub's tip row.
//
//  mount(opts) → { storePath, project, tip, branch } | throws a friendly str.
//    opts.wt        parent worktree root (absolute)
//    opts.beDir     parent's `.be` dir (where the sibling shard lands)
//    opts.subpath   gitlink path (wt-relative, e.g. "vendor/sub")
//    opts.pin       40-hex parent-gitlink commit sha (the checkout target)
//    opts.source    the parent's parsed remote (parseRemote result) OR null
//                   (a local/in-repo parent — then only the .gitmodules URL
//                   fallback applies)
//    opts.parentTitle  the parent shard title (for the synthetic branch name)
//    opts.parentBranch the parent's current branch ("" = trunk)

"use strict";

const wire     = require("./wire.js");
const ingest   = require("./ingest.js");
const store    = require("./store.js");
const checkout = require("./checkout.js");
const ulog     = require("./ulog.js");
const pathlib  = require("./util/path.js");
const sha      = require("./util/sha.js");
const join = pathlib.join, basename = pathlib.basename;
const isFullSha = sha.isFullSha;

function exists(p) { try { io.stat(p); return true; } catch (e) { return false; } }

//  Parse `<wt>/.gitmodules` for the [submodule] block whose `path` == subpath;
//  return its `url` (or "" when absent).  A minimal git-config reader (the
//  same shape core/recurse.js::gitmodulesOrder uses), keyed on path→url.
function gitmodulesUrl(wt, subpath) {
  const p = join(wt, ".gitmodules");
  let text;
  try { text = utf8.Decode(io.mmap(p, "r").data()); } catch (e) { return ""; }
  let curPath = "", curUrl = "", inSub = false, hit = "";
  function flush() { if (inSub && curPath === subpath && curUrl) hit = curUrl; }
  for (let line of text.split("\n")) {
    line = line.replace(/[#;].*$/, "").trim();
    if (!line) continue;
    if (line[0] === "[") { flush(); inSub = /^\[\s*submodule\b/i.test(line);
                           curPath = ""; curUrl = ""; continue; }
    if (!inSub) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    const key = line.slice(0, eq).trim().toLowerCase();
    const val = line.slice(eq + 1).trim();
    if (key === "path") curPath = val;
    else if (key === "url") curUrl = val;
  }
  flush();
  return hit;
}

//  [Title] from a `.gitmodules` URL basename — `.git` + trailing `/` stripped
//  (`…/libabc.git` → `libabc`, `be:/s/.be?/sub` → `sub`).  A `?/<proj>`
//  selector wins (its last segment IS the title); else the path basename.
function titleFromUrl(url) {
  if (!url) return "";
  const u = new URI(url);
  const q = u.query || "";
  if (q && q[0] === "/") {
    const segs = q.slice(1).split("/");
    if (segs[0]) return segs[0];
  }
  let p = (u.path || url).replace(/\/+$/, "");
  let b = basename(p);
  if (b.slice(-4) === ".git") b = b.slice(0, -4);
  return b;
}

//  The synthetic branch the child wt tracks ([Submodules] bullet 1):
//  `/<title>/.<parent>[/<parent_branch>]`.  A sub of a sub climbs the same
//  rule; here we record one level (the immediate parent), which round-trips
//  through the wtlog as the sub's `?<branch>` token.
function syntheticBranch(title, parentTitle, parentBranch) {
  let b = "/" + title + "/." + (parentTitle || "parent");
  if (parentBranch) b += "/" + parentBranch;
  return b;
}

//  Build the SAME-SOURCE child remote URI from the parent's parsed remote: the
//  parent fetched `<scheme>:<path>?/<parentProj>[/branch]`; the child swaps the
//  `?/<proj>` selector to the child title (same store, sibling project).
//  Returns "" when the parent has no usable same-source remote.
function sameSourceUri(source, title) {
  if (!source) return "";
  //  source.raw is the parent's remote URI; rebuild it with `?/<title>`.
  const u = new URI(source.raw);
  const scheme = u.scheme ? u.scheme + ":" : "";
  const auth = (u.authority != null && u.authority !== "") ? "//" + u.authority : "";
  const path = u.path || "";
  return scheme + auth + path + "?/" + title;
}

//  Fetch the child pack from `uri` (keeper/git wire).  Returns { pack, tip,
//  branch } or null on any failure (so the caller can fall back).
function tryFetch(uri, wantSha) {
  if (!uri) return null;
  try {
    //  A pinned want: fetch by the exact sha so the checkout target is in the
    //  pack regardless of the child's branch tip.  wire.fetch accepts a 40-hex
    //  want directly (pickWant short-circuits on isFullSha).
    const f = wire.fetch(uri, wantSha || "");
    if (!f || !f.pack || !f.pack.length) return null;
    return { pack: f.pack, tip: f.want || wantSha || "", branch: f.branch || "" };
  } catch (e) { return null; }
}

//  mount(opts): fetch + clone + anchor + checkout one gitlink leaf.  Returns
//  the mounted sub's coords so the caller can recurse into IT (a sub of a sub).
function mount(opts) {
  const wt = opts.wt, beDir = opts.beDir, subpath = opts.subpath, pin = opts.pin;
  if (!isFullSha(pin))
    throw "be get: sub " + subpath + " has no resolvable gitlink pin";

  const url = gitmodulesUrl(wt, subpath);
  const title = titleFromUrl(url) || basename(subpath);
  if (!title)
    throw "be get: cannot derive a title for sub " + subpath +
          " (no `.gitmodules` url)";

  const shard = join(beDir, title);
  const subWt = join(wt, subpath);
  const anchorPath = join(subWt, ".be");
  const branch = syntheticBranch(title, opts.parentTitle, opts.parentBranch);

  //  Already-local pin?  An idempotent re-mount (a re-get over an existing
  //  mount, or an in-repo FF `be get` with no remote) must NOT re-fetch: if the
  //  sibling shard already RESOLVES the pin commit, reuse it (anchor + checkout
  //  only).  This is also what makes a bumped sub re-checkout-able after a local
  //  post (the new commit already lives in the local sub shard).
  let havePin = false;
  if (exists(join(shard, "0000000001.keeper"))) {
    try { havePin = !!store.open(wt, title).getObject(pin); } catch (e) {}
  }

  if (!havePin) {
    //  D4: same-source fetch first (the parent's remote, project swapped), then
    //  the `.gitmodules` URL fallback.  Fetch by the EXACT pin so the checkout
    //  target rides the pack.
    const sameUri = sameSourceUri(opts.source, title);
    let f = tryFetch(sameUri, pin);
    let usedUri = sameUri;
    if (!f && url) { f = tryFetch(url, pin); usedUri = url; }
    if (!f)
      throw "be get: SUBFETCH cannot fetch sub " + subpath + " (" + title +
            ") from " + (sameUri || "(no same-source)") +
            (url ? " or " + url : "") + " — child unreachable";

    //  Clone/land the child shard as a sibling at `<beDir>/<title>/` ([Store]
    //  flat layout).  A FRESH shard clones (pack + refs + idx); an EXISTING
    //  shard that lacks the pin lands the new pack via ingest.add (a re-get
    //  pulling an advanced child).
    if (!exists(join(shard, "0000000001.keeper")))
      ingest.clone(f.pack, beDir, title, pin, usedUri || ("be:" + shard));
    else
      ingest.add(f.pack, shard, usedUri || ("be:" + shard), pin);
  }

  //  D13: write the sub wtlog anchor `<wt>/<path>/.be` — row-0 redirect names
  //  the sibling shard + project (so be.find resolves the mount), then the
  //  `?<synthetic-branch>#<pin>` tip the child wt tracks ([Submodules] §1).
  try { io.mkdir(subWt); } catch (e) {}
  const redirect = "file:" + beDir + "/?/" + title;
  ulog.write(anchorPath, [{ verb: "get", uri: redirect },
                          { verb: "get", uri: "?" + branch + "#" + pin }]);

  //  D3: check out the commit named by the parent gitlink into `<wt>/<path>/`.
  const k = store.open(wt, title);
  checkout.apply(k, pin, subWt);

  return { storePath: wt, project: title, shard: shard, tip: pin,
           branch: branch, k: k };
}

module.exports = { mount: mount, gitmodulesUrl: gitmodulesUrl,
                   titleFromUrl: titleFromUrl, syntheticBranch: syntheticBranch,
                   sameSourceUri: sameSourceUri };
