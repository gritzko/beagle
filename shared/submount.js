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
//  GET-037: a scp-style git url (`git@host:owner/repo.git`) is NOT a parseable
//  URI (`new URI` throws `uri.parse: malformed`) — fall back to the raw-string
//  basename so a github-style `.gitmodules` url yields a title instead of an
//  uncaught crash.
function titleFromUrl(url) {
  if (!url) return "";
  let path = url;
  try {
    const u = new URI(url);
    const q = u.query || "";
    if (q && q[0] === "/") {
      const segs = q.slice(1).split("/");
      if (segs[0]) return segs[0];
    }
    path = u.path || url;
  } catch (e) { path = url; }       // unparseable (scp git url) → raw basename
  let p = path.replace(/[?#].*$/, "").replace(/\/+$/, "");
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

  //  GET-039: a symlink (incl. the `be -> .` self-locator) is a `120000` BLOB,
  //  never a `160000` gitlink — it never reaches this mount path (it checks out
  //  via the generic symlink leaf), so the old `be`-name refusal is gone.

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

  //  GET-037: ATOMICITY — a sub-mount that fails part-way (an unreachable child,
  //  a raw io error like ENOTDIR from a worktree-source readdir) must not leave a
  //  LIVE-but-broken sub wt.  The pre-fetch/clone steps touch nothing under the
  //  sub wt; only the anchor + checked-out files (written last, in order) do.  On
  //  any failure, drop the anchor (so a stale `<wt>/<path>/.be` never makes
  //  be.find resolve a half-mounted sub) and surface a FRIENDLY string — our own
  //  throws pass through, a raw io error is wrapped — never a raw uncaught
  //  exception to the user (cf. [GET-018] atomicity).  (io has no rmdir, so an
  //  emptied sub dir may remain; it is inert without the anchor.)
  try {
    //  Already-local pin?  An idempotent re-mount (a re-get over an existing
    //  mount, or an in-repo FF `be get` with no remote) must NOT re-fetch: if the
    //  sibling shard already RESOLVES the pin commit, reuse it (anchor + checkout
    //  only).  This is also what makes a bumped sub re-checkout-able after a local
    //  post (the new commit already lives in the local sub shard).
    //  GET-037: open the sibling shard against the STORE dir (`beDir`), NOT the
    //  parent wt — for a local-store get `<wt>/.be` is a redirect FILE and the
    //  shard lives under `beDir` (the source store), so `store.open(wt, …)` would
    //  miss it (and a fresh re-fetch + `ingest.add` would mutate the user's
    //  canonical store).  `store.open(beDir, title)` resolves `<beDir>/<title>`.
    let havePin = false;
    if (exists(join(shard, "0000000001.keeper"))) {
      try { havePin = !!store.open(beDir, title).getObject(pin); } catch (e) {}
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
    //  Open against `beDir` (the store dir), per the havePin note above.
    const k = store.open(beDir, title);
    checkout.apply(k, pin, subWt);

    return { storePath: beDir, project: title, shard: shard, tip: pin,
             branch: branch, k: k };
  } catch (e) {
    //  Roll back this mount's anchor (best-effort; never mask the failure) so a
    //  stale `<wt>/<path>/.be` redirect can't outlive a failed mount.
    try { io.unlink(anchorPath); } catch (e2) {}
    //  Friendly surface: our own throws are already strings; a raw io error
    //  (e.g. ENOTDIR — a worktree-source `.be` wtlog FILE read as a store dir)
    //  becomes a friendly SUBMOUNT string so `get` never leaks an uncaught io
    //  exception.  (Worktree-source row-0 redirect is the deeper fix — GET-037
    //  stretch; here we at minimum refuse cleanly.)
    if (typeof e === "string") throw e;
    throw "be get: SUBMOUNT cannot mount sub " + subpath +
          " — " + ((e && e.message) || e);
  }
}

module.exports = { mount: mount, gitmodulesUrl: gitmodulesUrl,
                   titleFromUrl: titleFromUrl, syntheticBranch: syntheticBranch,
                   sameSourceUri: sameSourceUri };
