//  submount.js — GET-side submodule MOUNT + checkout (DIS-058 D2-D5,D13).
//  Pure JS over wire.js (child fetch), ingest.js (sibling-shard clone),
//  store.js (read the pinned tree), checkout.js (materialise the sub wt) and
//  ulog.js (the sub wtlog anchor).  Implements [Submodules] Recursion §1:
//
//    GET pre-order — after the parent's own files are written, each gitlink
//    leaf is MOUNTED: fetch the child shard from the SAME source (a beagle
//    store swaps the `?/<proj>` selector to the child [Title]; a git repo
//    WITH a worktree — path not ending `.git` — serves the sub's own checkout
//    at `<path>/<subpath>`; SUBS-047), CLONE it as a sibling shard at
//    `<beDir>/<title>/` (flat, same level as the parent — [Store] layout),
//    WRITE the sub wtlog anchor `<wt>/<path>/.be`, and CHECK OUT the commit
//    named by the parent's gitlink pin.  The same-source fetch falls back to
//    the `.gitmodules` URL (nearest enclosing file) when it fails, per
//    [Title] retrieval preference.  The child wt tracks the PARENT'S PIN by
//    its own local URI `//WT/path/to/sub#<pin>` (DIS-072, [DIS-071] law #4) —
//    recorded in the sub's tip row; no synthetic branch, no refs entry.
//
//  mount(opts) → { storePath, project, tip, oldPin, rows } | throws a friendly
//  str.  GET-047: oldPin = prior anchor pin (""=fresh), rows = checkout delta.
//    opts.wt        parent worktree root (absolute)
//    opts.beDir     parent's `.be` dir (where the sibling shard lands)
//    opts.subpath   gitlink path (wt-relative, e.g. "vendor/sub")
//    opts.pin       40-hex parent-gitlink commit sha (the checkout target)
//    opts.source    the parent's parsed remote (parseRemote result) OR null
//                   (a local/in-repo parent — then only the .gitmodules URL
//                   fallback applies)

"use strict";

const wire     = require("./wire.js");
const ingest   = require("./ingest.js");
const store    = require("./store.js");
const checkout = require("./checkout.js");
const ulog     = require("./ulog.js");
const ambient  = require("./ambient.js");   // GET-040: the global force flag
const pathlib  = require("./util/path.js");
const sha      = require("./util/sha.js");
const join = pathlib.join, basename = pathlib.basename, safeRel = pathlib.safeRel;
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
//  URI-016: also the ONE `.be` pivot (repoFromBe/projectFromPath) for titles.
const discover = require("../core/discover.js");
const wtpath = discover.wtpath;
const isFullSha = sha.isFullSha;

function exists(p) { try { io.stat(p); return true; } catch (e) { return false; } }

//  GET-040: the sub's currently-checked-out pin — the last `#<40hex>` in its
//  existing anchor's tip row (the checkout baseline).  "" when no prior anchor.
function currentSubPin(anchorPath) {
  let pin = "";
  try {
    ulog.each(anchorPath, function (log) {
      const u = (log && log.uri) || "";
      let f = ""; try { f = uri._parse(u).fragment || ""; } catch (e) {}
      if (/^[0-9a-f]{40}$/.test(f)) pin = f;
    });
  } catch (e) {}
  return pin;
}

//  Return the `.gitmodules` `url` for the [submodule] block whose `path` ==
//  subpath (or "" when absent).
//  PUT-004: delegates to the shared reader (was a copy-pasted git-config parser).
function gitmodulesUrl(wt, subpath) {
  return require("./gitmodules.js").urlOf(wt, subpath);
}

//  SUBS-047: the official url for `subpath` off the NEAREST enclosing
//  `.gitmodules` — a nested sub (`dog/abc`) is declared in `dog/.gitmodules`
//  as `abc`, never in the root file, so a root-only lookup returns "".
function declaredUrl(wt, subpath) {
  let base = wt, rest = subpath;
  for (;;) {
    const u = gitmodulesUrl(base, rest);
    if (u) return u;
    const cut = rest.indexOf("/");
    if (cut < 0) return "";
    base = join(base, rest.slice(0, cut));
    rest = rest.slice(cut + 1);
  }
}

//  [Title] from a `.gitmodules` URL — a `?/<proj>` selector wins, else the
//  `.be` pivot names the shard, else the path basename (`.git` stripped).
//  URI-016: `.be` is the STORE, never a [Title] — the shard is the segment
//  AFTER it (`be:/h/s/.be/libabc` → `libabc`; `be:/h/s/.be` ends AT the store
//  and carries NO title → "", the caller's other sources must supply one).
//  The `.be` pivot is NOT re-parsed here: discover's repoFromBe (everything
//  before `.be`) and projectFromPath (the segment after `/.be/`) already
//  answer it — repoFromBe(p) !== p IS "this path names a store".
//  GET-037: a scp-style git url (`git@host:owner/repo.git`) is NOT a parseable
//  URI (`new URI` throws `uri.parse: malformed`) — fall back to the raw-string
//  basename so a github-style `.gitmodules` url yields a title instead of an
//  uncaught crash.
function titleFromUrl(url) {
  if (!url) return "";
  let path = url;
  try {
    const u = new URI(url);
    const q = discover.projectFromQuery(u.query || "");
    if (q) return q;
    path = u.path || url;
  } catch (e) { path = url; }       // unparseable (scp git url) → raw basename
  let p = path.replace(/[?#].*$/, "").replace(/\/+$/, "");
  if (discover.repoFromBe(p) !== p) return discover.projectFromPath(p);
  let b = basename(p);
  if (b.slice(-4) === ".git") b = b.slice(0, -4);
  return b;
}

//  DIS-072: the pin TRACK URI `//WT/path/to/sub#<pin>` ([DIS-071] law #4) — the
//  child's own local URI, FLAT under the owning tree top (never a `.parent` chain).
//  Resolution via resolve_hash.treeAt (RULE ZERO): wtree/spath/rpath name the
//  address; an unaddressable tree (no anchor at `wt`) records a bare `#<pin>`.
function trackUri(wt, subpath, pin) {
  let t = null;
  try { t = discover.treeAt(wt); } catch (e) {}
  if (!t || t.wt !== wt)
    return URI.make(undefined, undefined, undefined, undefined, pin);
  const segs = pathlib.split(t.spath).concat(pathlib.split(t.rpath),
                                             pathlib.split(subpath));
  //  the authority slot carries its own `//` prefix (the URI binding's shape).
  return URI.make(undefined, "//" + t.wtree, "/" + pathlib.merge(segs),
                  undefined, pin);
}

//  SUBS-047: the SAME-SOURCE child URIs, tried IN ORDER before the official
//  `.gitmodules` url ([Submodules] §1) — the parent's remote re-addressed at
//  the child, composed via URI.make ONLY (URI-013: the old hand-compose
//  doubled the slotted authority into `ssh:////host/...`, so the same-source
//  fetch never reached the peer).  Two source shapes:
//    beagle store (source.local / be: / keeper: / a `.be` path) — a sibling
//      project in the same store: swap the `?/<proj>` selector to the [Title];
//    git repo WITH a worktree (path NOT ending `.git`) — the sub's own
//      checkout nested at `<path>/<subpath>`;
//    a bare `.git` source serves no subs — no same-source candidate.
function sameSourceUris(source, title, subpath) {
  if (!source || !source.raw) return [];
  let u; try { u = new URI(source.raw); } catch (e) { return []; }
  const scheme = u.scheme || "";
  const path = (u.path || "").replace(/\/+$/, "");
  const beagleish = source.local || scheme === "be" || scheme === "keeper" ||
                    path.slice(-3) === ".be";
  if (beagleish)
    return [URI.make(u.scheme, u.authority, u.path, "/" + title)];
  if (path.slice(-4) !== ".git" && subpath)
    return [URI.make(u.scheme, u.authority, path + "/" + subpath)];
  return [];
}

//  Fetch the child pack from `uri` (keeper/git wire).  Returns { pack, tip,
//  branch } or null on any failure (so the caller can fall back).
function tryFetch(uri, wantSha, packDir) {
  if (!uri) return null;
  try {
    //  A pinned want: fetch by the exact sha so the checkout target is in the
    //  pack regardless of the child's branch tip.  wire.fetch accepts a 40-hex
    //  want directly (pickWant short-circuits on isFullSha).
    //  GET-044: packDir streams the child pack to a tmp file (bounded RSS).
    const f = wire.fetch(uri, wantSha || "", null,
                         packDir ? { packDir: packDir } : undefined);
    if (!f || !(f.packFile || (f.pack && f.pack.length))) return null;
    return { pack: f.pack, packFile: f.packFile, packLen: f.packLen,
             tip: f.want || wantSha || "", branch: f.branch || "" };
  } catch (e) { return null; }
}

//  GET-044: normalize a tryFetch result to the ingest pack source — a streamed
//  { packFile, packLen } descriptor, or the in-memory Uint8Array.
function packSrc(f) {
  return f.packFile ? { packFile: f.packFile, packLen: f.packLen } : f.pack;
}

//  SUBS-046: `uri` a LOCAL (`file:`/scheme-less) store source (get.js `localish`)?
//  Such a child reuses the on-disk store, never the keeper wire.  Returns the URI or null.
function localSourceUri(uri) {
  if (!uri) return null;
  let u; try { u = new URI(uri); } catch (e) { return null; }   // unparseable → not local
  const scheme = u.scheme || "";
  if (scheme !== "" && scheme !== "file") return null;          // be:/git/ssh → wire
  if (scheme === "" && u.authority != null && u.authority !== "" && !u.path)
    return null;                                                 // BE-033: bare `//host` is no store
  return u;
}

//  SUBS-046: resolve a LOCAL source URI to the store that RESOLVES `pin`, following
//  GET-038's store/worktree redirect; jab is project-less so try declared proj THEN "".
function resolveLocalStore(u, pin) {
  let path = u.path || "";
  path = path.replace(/\/+$/, "");
  const storeRoot0 = be.repoFromBe(path);                        // strip trailing `.be`
  let declProj = be.projectFromQuery(u.query || "") ||
                 be.projectFromPath(path) || "";
  //  GET-038: a WORKTREE source (`.be` is a FILE) redirects to its real store.
  const beFile = (path.slice(-3) === ".be") ? path : join(path, ".be");
  let storeRoot = storeRoot0;
  let kind; try { kind = io.stat(beFile).kind; } catch (e) { kind = undefined; }
  if (kind === "reg") {
    let u0; ulog.each(beFile, function (log) { if (u0 === undefined) u0 = log.uri; });
    if (!u0) return null;
    const p = new URI(u0);
    storeRoot = be.repoFromBe(p.path || "");
    declProj = declProj || be.projectFromQuery(p.query || "") ||
               be.projectFromPath(p.path || "");
    if (!storeRoot) return null;
  }
  //  Keep the project whose shard actually resolves the pin (declared, then "").
  for (const proj of (declProj ? [declProj, ""] : [""])) {
    try {
      if (store.open(storeRoot, proj).getObject(pin))
        return { storeBe: join(storeRoot, ".be"), storeRoot: storeRoot, proj: proj };
    } catch (e) {}
  }
  return null;
}

//  mount(opts): fetch + clone + anchor + checkout one gitlink leaf.  Returns
//  the mounted sub's coords so the caller can recurse into IT (a sub of a sub).
function mount(opts) {
  const wt = opts.wt, beDir = opts.beDir, subpath = opts.subpath, pin = opts.pin;

  //  BE-011: leaf-local worktree confinement — refuse a `..`/reserved subpath
  //  BEFORE composing subWt (defense-in-depth; recurse.js already store-guards it).
  if (!safeRel(subpath)) throw "NAVESCAPE: sub " + subpath + " escapes the worktree";

  //  GET-039: a symlink (incl. the `be -> .` self-locator) is a `120000` BLOB,
  //  never a `160000` gitlink — it never reaches this mount path (it checks out
  //  via the generic symlink leaf), so the old `be`-name refusal is gone.

  if (!isFullSha(pin))
    throw "be get: sub " + subpath + " has no resolvable gitlink pin";

  //  SUBS-047: the official url resolves through the NEAREST enclosing
  //  `.gitmodules` (a nested `dog/abc` is declared in `dog/.gitmodules`), so
  //  the fallback — and the [Title] — work for subs at any depth.
  const url = declaredUrl(wt, subpath);
  const title = titleFromUrl(url) || basename(subpath);
  if (!title)
    throw "be get: cannot derive a title for sub " + subpath +
          " (no `.gitmodules` url)";

  const shard = join(beDir, title);
  const subWt = wtpath(wt, subpath);
  const anchorPath = wtpath(subWt, ".be");
  //  DIS-072: the child tracks the parent's pin by its own local URI.
  const track = trackUri(wt, subpath, pin);

  //  GET-037: ATOMICITY — a sub-mount that fails part-way (an unreachable child,
  //  a raw io error like ENOTDIR from a worktree-source readdir) must not leave a
  //  LIVE-but-broken sub wt.  The pre-fetch/clone steps touch nothing under the
  //  sub wt; only the anchor + checked-out files (written last, in order) do.  On
  //  any failure, drop the anchor (so a stale `<wt>/<path>/.be` never makes
  //  be.treeAt resolve a half-mounted sub) and surface a FRIENDLY string — our own
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
      //  SUBS-046: `store.open(beDir, title)` names the sibling shard; but a
      //  project-less/unnamed source (jab refs at `.be/refs`) resolves under the
      //  empty project too — try both so an already-local pin reuses w/o fetch.
      try { havePin = !!store.open(beDir, title).getObject(pin); } catch (e) {}
      if (!havePin) try { havePin = !!store.open(shard, "").getObject(pin); } catch (e) {}
    }

    //  SUBS-047: the same-source candidates, tried before the official url in
    //  BOTH the local-reuse and the wire branches below.
    const sames = sameSourceUris(opts.source, title, subpath);

    //  SUBS-046: LOCAL-SOURCE reuse (get.js localish path, NO wire): a `file:`/
    //  scheme-less child whose pin already lives on-disk is mounted by REDIRECTING
    //  the sub `.be` at that store + checkout; else fall through to the wire fetch.
    if (!havePin) {
      let ls = null;
      for (const s of sames) {
        const su = localSourceUri(s); if (su) ls = resolveLocalStore(su, pin);
        if (ls) break;
      }
      if (!ls) { const uu = localSourceUri(url); if (uu) ls = resolveLocalStore(uu, pin); }
      if (ls) {
        const oldPin = currentSubPin(anchorPath);
        try { io.mkdir(subWt); } catch (e) {}
        const redirect = URI.make("file", undefined, ls.storeBe + "/", "/" + ls.proj);
        ulog.write(anchorPath, [{ verb: "get", uri: redirect },
                                { verb: "get", uri: track }]);
        const k = store.open(ls.storeRoot, ls.proj);
        //  GET-047: surface the prior pin + checkout delta for the get report.
        const co = checkout.apply(k, pin, subWt, { force: ambient.force(), oldTip: oldPin });
        return { storePath: ls.storeRoot, project: ls.proj, shard: k.shard,
                 tip: pin, k: k, oldPin: oldPin, rows: co.rows };
      }
    }

    if (!havePin) {
      //  D4: same-source fetch first (the parent's remote re-addressed at the
      //  child — project swap or worktree-nested path), then the `.gitmodules`
      //  URL fallback.  Fetch by the EXACT pin so the checkout target rides
      //  the pack.
      //  GET-044: stream the child pack into `beDir` (same FS as the sibling
      //  shard → atomic land), bounded RSS.
      let f = null, usedUri = "";
      for (const s of sames) { f = tryFetch(s, pin, beDir); if (f) { usedUri = s; break; } }
      if (!f && url) { f = tryFetch(url, pin, beDir); usedUri = url; }
      if (!f)
        throw "be get: SUBFETCH cannot fetch sub " + subpath + " (" + title +
              ") from " + (sames.length ? sames.join(", ") : "(no same-source)") +
              (url ? " or " + url : "") + " — child unreachable";

      //  Clone/land the child shard as a sibling at `<beDir>/<title>/` ([Store]
      //  flat layout).  A FRESH shard clones (pack + refs + idx); an EXISTING
      //  shard that lacks the pin lands the new pack via ingest.add (a re-get
      //  pulling an advanced child).
      const psrc = packSrc(f);
      if (!exists(join(shard, "0000000001.keeper")))
        ingest.clone(psrc, beDir, title, pin, usedUri || URI.make("be", undefined, shard));
      else
        ingest.add(psrc, shard, usedUri || URI.make("be", undefined, shard), pin);
    }

    //  GET-040: the sub's PRIOR pin (its current anchor tip) is the checkout
    //  baseline — read it BEFORE the anchor is rewritten so a non-force re-get
    //  can tell a clean file from a dirty edit and preserve untracked content.
    const oldPin = currentSubPin(anchorPath);

    //  D13: write the sub wtlog anchor `<wt>/<path>/.be` — row-0 redirect names
    //  the sibling shard + project (so be.treeAt resolves the mount), then the
    //  `//WT/path/to/sub#<pin>` track row (DIS-072 pin-URI model).
    try { io.mkdir(subWt); } catch (e) {}
    const redirect = URI.make("file", undefined, beDir + "/", "/" + title);
    ulog.write(anchorPath, [{ verb: "get", uri: redirect },
                            { verb: "get", uri: track }]);

    //  D3: check out the commit named by the parent gitlink into `<wt>/<path>/`.
    //  GET-040: the global force flag (`get!`) — uniform across the root and
    //  EVERY submodule at every depth — decides clean-reset vs merge/leave.
    //  Open against `beDir` (the store dir), per the havePin note above.
    const k = store.open(beDir, title);
    //  GET-047: surface the prior pin + checkout delta for the get report.
    const co = checkout.apply(k, pin, subWt, { force: ambient.force(), oldTip: oldPin });

    return { storePath: beDir, project: title, shard: shard, tip: pin, k: k,
             oldPin: oldPin, rows: co.rows };
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
                   declaredUrl: declaredUrl, titleFromUrl: titleFromUrl,
                   trackUri: trackUri,
                   sameSourceUris: sameSourceUris };
