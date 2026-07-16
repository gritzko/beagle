//  be.js — repo discovery (JS-029).  Pure JS over the JABC runtime
//  (io.cwd/getenv/stat/mmap + the URI binding); no C, no dog.  Mirrors
//  dog/HOME.c::home_walk_up + home_anchor_resolve.
//
//  URI-016: this file no longer CLIMBS.  There is ONE `.be` climber, in
//  core/resolve_hash.js (climb/anchors), serving both [/wiki/URI] step 1
//  (projectRoot: the TOPMOST anchor) and step 4 (treeAt: the NEAREST).
//  projectRoot()/workRoot()/metaRoot()/todoRoot()/treeAt() here are lazy-require
//  DELEGATIONS onto it — the names verbs/views call.  topWt()/launchTop()/
//  rootTop() are GONE: the outermost tree and the bare `//` are fixed by the
//  project layout, so they are arithmetic on projectRoot()/workRoot(), not walks.
//
//  treeAt(path?) → the nearest worktree anchor at/above `path` (default io.cwd()),
//  as [/wiki/URI] step 4's record; besides the spec's fields it carries the
//  ANCHOR itself — { root, wt, bePath, storePath, project } — which is what
//  store.open/wtlog.open and the verb/view call sites read.  Here
//    wt        = the anchor dir (where `.be` lives), the worktree root
//    bePath    = the on-disk wtlog path: <wt>/.be (secondary) or
//                <wt>/.be/wtlog (primary)
//    storePath = the store root, from row-0's anchor URI path
//                (DOGRepoFromBe: split on /.be/); == wt for a colocated
//                primary store with no redirect
//    project   = the store's Title, from row-0's `?/<title>/<branch>`
//                query (preferred) or the path-after-`.be` segment
//    root      = alias of storePath (the home `h->root`)

"use strict";

const pathlib = require("../shared/util/path.js");   // JSQUE-016: be.js -> core/discover.js
const ulog = require("../shared/ulog.js");
const join = pathlib.join, dirname = pathlib.dirname;

const BE = ".be";
const WTLOG = "wtlog";

//  URI-016: the CLIMB LIMIT — `$BE_ROOT`, defaulting to `$HOME`.  THE climb
//  (core/resolve_hash.js climb(), the only one left) never reaches it, so a repo
//  tree outside $HOME (or a test's scratch base) sets BE_ROOT and the walk stops
//  there by construction.  [/wiki/URI] step 1 says an anchor is "still lower than
//  $HOME"; BE_ROOT is that limit, made explicit — and "lower than" is STRICT, so
//  $BE_ROOT/.be (the STORE) anchors neither a project root nor a worktree.
function beRoot() { return io.getenv("BE_ROOT") || io.getenv("HOME"); }

function statKind(p) {
  try { return io.stat(p).kind; } catch (e) { return undefined; }
}
function isFile(p) { return statKind(p) === "reg"; }

//  DOGRepoFromBe: the store root is everything before the first `/.be/`
//  separator in a row-0 anchor URI path; falls back to stripping a
//  trailing `.be` (and trailing slashes) when no `/.be/` is present.
function repoFromBe(path) {
  let p = path;
  while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1);
  const i = p.indexOf("/" + BE + "/");
  if (i >= 0) return p.slice(0, i);
  if (p.endsWith("/" + BE)) p = p.slice(0, -(BE.length + 1));
  else if (p.endsWith(BE)) p = p.slice(0, -BE.length);
  while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1);
  return p;
}

//  DOGQueryProject: absolute `/<title>` or `/<title>/<branch>` → title;
//  a non-absolute or empty query → "".
function projectFromQuery(query) {
  if (!query || query[0] !== "/") return "";
  const rest = query.slice(1);
  const j = rest.indexOf("/");
  return j < 0 ? rest : rest.slice(0, j);
}

//  DOGProjectFromBe: the first path segment after `/.be/`, unless it is
//  itself `.be` (a doubled store dir) — then treat as elided.
function projectFromPath(path) {
  let p = path;
  while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1);
  const i = p.indexOf("/" + BE + "/");
  if (i < 0) return "";
  const seg = p.slice(i + BE.length + 2).split("/")[0] || "";
  return seg === BE ? "" : seg;
}

//  Read row 0 of a wtlog and return { verb, uri }, or undefined.  A store
//  anchor row 0 is a `get`/`repo` row pointing at the shared store.
function row0Row(bePath) {
  let row;
  ulog.each(bePath, function (log) {
    if (row === undefined) row = { verb: log.verb, uri: log.uri };
  });
  return row;
}

//  POST-027: wtlog.anchor()'s test — ONLY a get/repo row 0 anchors a store;
//  a fresh wt's put row 0 stages a FILE, whose path is never a store.
function isAnchorRow(r) { return !!r && (r.verb === "get" || r.verb === "repo"); }

//  Resolve the anchor at `wt` (a dir holding `.be`) into store/project.
//  Primary (`.be` is a dir): store == wt; project from the dir's row-0
//  wtlog anchor when present, else single-shard scan is left to keeper.
//  Secondary (`.be` is a file): store + project from row 0's redirect.
function resolveAnchor(wt) {
  const be = join(wt, BE);
  const kind = statKind(be);
  let bePath, storePath, project = "";

  if (kind === "reg") {
    //  Secondary worktree: the `.be` file is the wtlog.
    bePath = be;
    const r = row0Row(be);
    if (isAnchorRow(r)) {
      const p = new URI(r.uri);
      storePath = repoFromBe(p.path);
      project = projectFromQuery(p.query) || projectFromPath(p.path);
    }
    if (!storePath) storePath = wt;
  } else {
    //  Primary worktree: <wt>/.be/wtlog.
    bePath = join(be, WTLOG);
    storePath = wt;                          // colocated store == wt
    const r = isFile(bePath) ? row0Row(bePath) : undefined;
    if (r) {
      const p = new URI(r.uri);
      //  POST-027: only a get/repo row 0 is a store anchor; a fresh wt's put
      //  row 0 (or a jab-posted primary's post row 0) stays store==wt.
      if (isAnchorRow(r) && p.path) { const sp = repoFromBe(p.path); if (sp) storePath = sp; }
      project = projectFromQuery(p.query) || projectFromPath(p.path || "");
    }
  }
  return { root: storePath, wt: wt, bePath: bePath, storePath: storePath,
           project: project };
}

//  URI-016: `find()` DELETED — it was a LEGACY NAME for [/wiki/URI] step 4, kept
//  only because verb/view call sites spelled it `be.find`.  They now say what they
//  mean: `be.treeAt`, the step-4 routine itself (core/resolve_hash.js), which
//  shares climb()/anchors() with step 1's projectRoot() — one chain, one anchor
//  test, one limit.  The lazy delegation below is the AMBIENT handle (core/loop.js
//  Object.assign's this module onto `be`); the record it returns carries the spec's
//  fields PLUS the anchor itself ({root, wt, bePath, storePath, project}).
//  Two behaviours CHANGED vs the old find(), both toward the spec:
//    * an EMPTY `.be` FILE anchors NOTHING — the spec's anchor "references the
//      repo in the first line", and an empty file references nothing.  find() used
//      to accept it (and `shieldLike`, its ≤1-shard `.be`-dir heuristic, is gone
//      with it: a `.be/` dir IS "its own store in `.be/`", shard count be damned).
//    * the limit is now break-THEN-probe: an anchor must be "still lower than
//      $BE_ROOT", so $BE_ROOT/.be — the STORE — is no longer a worktree.  That is
//      what shieldLike was really refusing; the limit says it directly.

//  URI-016: the lazy handle on THE climber.  resolve_hash requires THIS module, so
//  the require must stay in-body — a top-level one closes a load cycle.
function _rh() { return require("./resolve_hash.js"); }

//  URI-016: topWt(wt) DELETED — it climbed past submodules by re-probing every
//  ancestor with find(), and drew the hive boundary with a SECOND spelling of the
//  `work` segment (`basename(dirname(wt)) === "work"`).  The outermost worktree is
//  not something to search for: it is FIXED by the project layout, so it is now
//  resolve_hash.topOf() — pure arithmetic on workRoot()/projectRoot(), no `.be`
//  walk, one spelling of `work`.  Its two callers say topOf() directly.
//  URI-016: launchTop() + rootTop() DELETED with it.  rootTop() was "the tree the
//  bare `//` NAMES", which [/wiki/URI] step 2 settles outright — `///mtrel` IS
//  `$SRC_ROOT/mtrel`, so `//` is the PROJECT ROOT, full stop.  That left launchTop()
//  (its repo-less fallback) with no callers: a cwd under no project root has no
//  repo at all, and the honest answer is PROJNONE, not a guess at a launch tree.

//  URI-016: srcRoot() DELETED — it was named for `$SRC_ROOT` but returned
//  `$SRC_ROOT/work`, so every caller either assumed the `/work` or dirname'd it
//  back off.  Callers now say which they mean: projectRoot() ($SRC_ROOT) or
//  workRoot() ($SRC_ROOT/work).  Both live in core/resolve_hash.js — THE one
//  `.be` climber.  Lazy require: resolve_hash requires this module.
//  URI-016: todoRoot() DELETED here too — it read `$TODO_ROOT`, ran its OWN
//  find()/topWt() climb, and returned a LIST of candidate roots to probe.  All
//  three are gone: the project root CAN NOT be an env var (an env var that
//  disagreed with the climb is just a second, lying answer), there is exactly
//  ONE `.be` climber (resolve_hash.projectRoot), and the ticket tree is not a
//  search path — `todoRoot()` IS `projectRoot() + "/todo"`, one STRING.
//  URI-016: step 4 (the NEAREST anchor), the ambient handle verbs/views call as
//  `be.treeAt`.  `from`/`topDir` ride through for resolve_hash.frame's re-anchor.
function treeAt(path, from, topDir) {
  return require("./resolve_hash.js").treeAt(path || io.cwd(), from, topDir);
}

function projectRoot() { return require("./resolve_hash.js").projectRoot(); }
function workRoot()    { return require("./resolve_hash.js").workRoot(); }
function metaRoot()    { return require("./resolve_hash.js").metaRoot(); }
function todoRoot()    { return require("./resolve_hash.js").todoRoot(); }

//  BE-030: the tree NAME comes from `context` ALONE — its `.host` (a URI object,
//  the nav scope a session STARTS with, navCwd(cwd) carrying BOTH the `//name`
//  authority AND the in-repo path).  A `context.host` of ""/"." → the LAUNCH tree.
function _ctxHost(context) {
  if (context && typeof context === "object") return context.host || "";   // a URI
  if (typeof context === "string") {                  // tolerate a raw string
    try { return (uri._parse(context) || {}).host || ""; } catch (e) { return ""; }
  }
  return "";
}

//  BE-030: the context's PATH — the TRUSTED in-repo dir the untrusted `rel`
//  resolves against (the old `base` arg, now folded INTO the context).  A path-less
//  context (`//name`) → "" = the tree root.  Same URI-object-or-string tolerance
//  as _ctxHost; an untrusted path must ride `rel`, never be planted here.
function _ctxPath(context) {
  if (context && typeof context === "object") return context.path || "";   // a URI
  if (typeof context === "string") {
    try { return (uri._parse(context) || {}).path || ""; } catch (e) { return ""; }
  }
  return "";
}

//  BE-030: `rel` is a PATH, not a URI — reject an authority (`//other`) or a
//  scheme (`git://…`) via the URI binding (no hand-parsing).  A tree SWAP is the
//  nav layer's job, NEVER the fs resolver's; this closes the `//OTHER` escape.
//  URI-016/JS-075: the parse is a SCHEME/AUTHORITY GUARD ONLY — the PATH handed
//  back is the arg VERBATIM.  `rel` names a file on disk, so a `#`/`?` in it is a
//  literal byte of the NAME, not a fragment/query delimiter: taking `u.path` back
//  truncated `a#b` -> `a` and silently checked out the wrong file.  Callers that
//  do mean a URI (wtdir/frame) shed the query/fragment themselves and pass the
//  bare `u.path` in, for which this is a no-op.
function _relPath(rel) {
  if (rel === undefined || rel === null || rel === "") return "";
  const s = String(rel);
  let u; try { u = uri._parse(s); } catch (e) { u = {}; }
  if (u.scheme !== undefined)
    throw "NAVESCAPE: rel path carries a scheme, not a path: " + s;
  if (u.authority !== undefined)
    throw "NAVESCAPE: rel path carries a // authority, not a path: " + s;
  return s;
}

//  BE-030: resolve(context, rel) → the ABSOLUTE fs path, CONFINED to the CONTEXT's
//  tree.  `context` (a URI object) carries BOTH the tree NAME (its authority) and
//  the TRUSTED in-repo dir the relative arg resolves against (its PATH); `rel` is
//  the UNTRUSTED relative arg.  Folds the old `base` INTO the context — the current
//  dir was always the context's own path, so it is no longer passed twice; a caller
//  whose context path is UNTRUSTED (wtdir) strips it into `rel`.  `rel` is
//  AUTHORITY-BLIND (no `//other`, no `scheme:` — a tree swap can no longer ride the
//  arg slot), and resolveInTree THROWS "NAVESCAPE" on any `..` that climbs above the
//  tree root, so the result can NEVER leave the `<srcRoot>/name` cell.  A rooted `/x` rel
//  addresses the tree root (drops the context path); ""/"." context host → the
//  PROJECT ROOT ([/wiki/URI] step 2).  Throws on escape / bad name.
function resolve(context, rel) {
  const host = _ctxHost(context);
  const relPath = _relPath(rel);                      // throws on a //other / scheme
  //  A rooted `/x` addresses the tree root (context path dropped); else `rel`
  //  resolves against the context's trusted in-repo path.  resolveInTree NORMALISES.
  const basePath = relPath[0] === "/" ? "" : _ctxPath(context);
  const sub = pathlib.resolveInTree(basePath, relPath);   // throws on climb-out
  if (host === "" || host === ".") {
    //  URI-016: `//` = the PROJECT ROOT — [/wiki/URI] step 2 says `///mtrel` IS
    //  `$SRC_ROOT/mtrel`, so there is nothing to search for and no launch-tree
    //  guess: no root means no repo, which is PROJNONE.
    const wt = _rh().projectRoot();
    if (!wt) throw "PROJNONE: no project root above " + io.cwd() + " — no repo";
    return sub ? join(wt, sub) : wt;
  }
  if (!pathlib.safeRel(host)) throw "NAVESCAPE: bad nav authority //" + host;
  //  URI-016: refuse LOUDLY when there is no project root — an unguarded join()
  //  fabricated the silent garbage path "null/NAME/..." at the confinement
  //  chokepoint, which is the last place that should invent a path.
  const work = workRoot();                   // step 2: $SRC_ROOT/work/WT
  if (!work) throw "PROJNONE: no project root above " + io.cwd() + " — no repo";
  const dir = join(work, host);
  return sub ? join(dir, sub) : dir;
}

//  URI-011: wtdir(uriStr) → the ABSOLUTE dir a nav URI addresses, or null.
//    //name[/sub]  → <srcRoot>/name/sub  (a hive cell, confined below)
//    // , //.       → the PROJECT ROOT    ([/wiki/URI] step 2)
//    //host…, file:/ssh:/be:, no `//`     → null (a cached remote / transport → wire)
//  A `//name` miss (treeAt has no anchor at/below <srcRoot>/name) is left to the
//  caller as a cached-remote-or-typo decision.  BE-011: confinement is now a
//  PROPERTY of resolve() (NAVESCAPE on any `..` climb), not a lexical prefix check.
function wtdir(uriStr) {
  let u; try { u = uri._parse(uriStr || ""); } catch (e) { return null; }
  if (u.scheme) return null;                          // a transport, not nav
  if (u.authority === undefined) return null;         // no `//` slot
  const host = u.host || "";
  if (host === "" || host === ".") {
    //  BE-037: `//[/path]` rides resolve like `//name` — the project root, path
    //  honoured; a repo-less cwd is the miss (null), NAVESCAPE still propagates.
    if (!_rh().projectRoot()) return null;
    return resolve("//" + host, u.path || "");
  }
  //  BE-030: compose + CONFINE via resolve(context, rel).  The nav URI's path is
  //  UNTRUSTED, so the context is host-ONLY (`//host`, empty trusted path) and the
  //  path rides `rel` — a `..` climb / bad authority throws NAVESCAPE and PROPAGATES
  //  (the CLI REFUSES loudly, never adopting an outside tree).  resolve throws ONLY
  //  on escape, never on a plain not-found → safe to let fly.
  const dir = resolve("//" + host, u.path || "");
  //  Confirm `//name` is a REAL anchored worktree AT/BELOW <srcRoot>/host (not an
  //  ancestor store treeAt() walked up to).  `dir` is `..`-free now, so this prefix
  //  compare is a sound EXISTENCE check, no longer a (broken) security boundary.
  const top = join(workRoot(), host);        // step 2: $SRC_ROOT/work/WT
  let repo; try { repo = _rh().treeAt(dir); } catch (e) { return null; }
  if (!repo || (repo.wt !== top && repo.wt.indexOf(top + "/") !== 0)) return null;
  return dir;
}

//  URI-011: navCwd(dir?) → the `//name/path` context URI for a directory
//  (default cwd) — the INVERSE of wtdir, and the context a session STARTS with
//  ("where I am").  name = the worktree `wt` under srcRoot() (may nest, `src/dogs`);
//  path = `dir` under `wt`.  "" when the dir is in no known tree (repo-less cwd).
function navCwd(dir) {
  const d = dir || io.cwd();
  let repo; try { repo = _rh().treeAt(d); } catch (e) { return ""; }
  if (!repo || !repo.wt) return "";
  //  Name off the TOP wt (past submodules); the sub-path crosses into the
  //  submodule (`//name/sub/inner`) — see [SUBS-045] joinPrefix.
  const top = _rh().topOf(repo.wt) || repo.wt;
  const work = workRoot();
  //  The project root itself has no `//name` address — its context is the bare
  //  `//`; a worktree slices its name off $SRC_ROOT/work.
  const name = top === work || top === projectRoot() ? ""
             : top.indexOf(work + "/") === 0 ? top.slice(work.length + 1)
             : top.slice(top.lastIndexOf("/") + 1);      // fallback: basename
  const sub = d.length > top.length ? d.slice(top.length + 1) : "";
  //  Compose the `//name[/sub]` context URI via the URI class (authority = name,
  //  path = the sub crossing into the submodule); byte-identical to the old concat.
  return uri._make(undefined, "//" + name, sub ? "/" + sub : undefined) ||
         ("//" + name);
}

//  URI-016: navTree(navStr) → the nav URI of the TREE that context is ANCHORED
//  on — wtdir, the ONE .be climb (treeAt, nearest), then navCwd back.  A plain
//  sub-dir reduces to its wt root (`//cli/plain` → `//cli`); a context INSIDE a
//  mount keeps the mount path (`//cli/vendor/sub`), since contextRepo anchors
//  the run there and its row paths are relative to THAT root, not the host top.
function navTree(navStr) {
  let d; try { d = wtdir(navStr); } catch (e) { return ""; }
  if (!d) return "";
  let repo; try { repo = _rh().treeAt(d); } catch (e) { return ""; }
  if (!repo || !repo.wt) return "";
  return navCwd(repo.wt);
}

//  URI-011: cwd() → the CONTEXT worktree ROOT a verb runs from — the ONE place a
//  verb asks "where am I operating," replacing raw io.cwd() so a nav'd verb acts
//  in the scoped tree, NOT the launch tree.  = the resolved repo's wt (be.repo.wt,
//  which authorityRepo may anchor on a SUBMODULE wt for a `//name/sub/…` path),
//  else the launch cwd's wt; repo-less falls back to io.cwd().  Verbs need only
//  this dir — never srcRoot().  The context URI (be.context / navCwd) is the "dir
//  to cd to"; no io.chdir binding needed.
function contextCwd() {
  if (typeof be !== "undefined" && be.repo && be.repo.wt) return be.repo.wt;
  try { return _rh().treeAt(io.cwd()).wt; } catch (e) { return io.cwd(); }
}

//  URI-016: ctxDir() — the run's context DIR, DERIVED from the ONE stored fact
//  (`be.context`, the context URI): wtdir() maps it back to the abs path, and a
//  FILE context (a cat view) climbs to its dir — the arg-resolution base, never
//  above the anchored wt root.  NO context (a plain CLI run — the launch tree is
//  "here") → the launch cwd.  Replaces the stored `be.ctxDir` field, which could
//  disagree with the context; there is nothing left to disagree with.
function ctxDir() {
  const c = (typeof be !== "undefined" && be.context) || "";
  if (!c) return io.cwd();
  let d; try { d = wtdir(c); } catch (e) { return io.cwd(); }   // NAVESCAPE → cwd
  if (!d) return io.cwd();
  let repo; try { repo = _rh().treeAt(d); } catch (e) { return d; }
  const root = (repo && repo.wt) || d;
  while (d.length > root.length && statKind(d) !== "dir") d = dirname(d);
  return d;
}

//  BE-032: the run's context dir as a wt-relative prefix; "" at/outside the root.
function _ctxSub(repo) {
  const d = ctxDir();
  if (!repo || !repo.wt || !d || d === repo.wt) return "";
  return d.indexOf(repo.wt + "/") === 0 ? d.slice(repo.wt.length + 1) : "";
}

//  BE-032: argRel(repo, raw) — ONE relative verb arg → its wt-root-relative path,
//  resolved against the run's context dir (`cd wiki && jab put Sniff.mkd` →
//  `wiki/Sniff.mkd`).  A rooted `/x` addresses the wt root (context bypassed);
//  `..` climbing above the root throws NAVESCAPE; ""/dir-form `/` are preserved.
function argRel(repo, raw) {
  const s = String(raw == null ? "" : raw);
  if (s === "") return s;
  if (s[0] === "/") return s.replace(/^\/+/, "");
  //  A trailing `/`, `.` or `..` segment is inherently a DIR reference — keep the
  //  dir-form (`sub/`; the wt root round-trips as `./`, the verbs' reporoot form).
  const last = s.slice(s.lastIndexOf("/") + 1);
  const dir = s[s.length - 1] === "/" || last === "." || last === "..";
  const sub = pathlib.resolveInTree(_ctxSub(repo), s);   // throws on climb-out
  return dir ? (sub ? sub + "/" : "./") : sub;
}

//  BE-030: per-process cache of a wt root → its validated nav context URI, so the
//  per-fs-access wtpath() below never re-walks the tree (navCwd/treeAt) twice for the
//  same wt.  "" marks a wt that is repo-less / OUTSIDE the hive (the fallback).
const _wtCtx = {};

//  BE-030: wtpath(wt, rel) → the ABSOLUTE fs path of the wt-relative `rel` in the
//  tree rooted at `wt`, computed THROUGH resolve() so every worktree file access
//  takes the nav CONTEXT into account: navCwd(wt) yields the `//name` authority +
//  in-repo sub-path (a submodule wt crosses in as `//name/sub`), and resolve()
//  maps that context back to the abs path.  This is the ONE way a verb/view turns
//  a worktree path into bytes on disk — it REPLACES raw wtJoin(wt,rel)/join(wt,rel)
//  at every fs site.  Confinement is preserved EXACTLY: resolve() throws NAVESCAPE
//  on a `..` climb above the tree root, and the trailing guard refuses any path
//  that climbs OUT of `wt` (matching wtJoin's wt-level boundary).  A wt outside
//  the hive (a store edge / scratch dir) has no `//name` address → the
//  plain wtJoin confine is used (byte-identical to the pre-BE-030 behavior).
function wtpath(wt, rel) {
  let ctx = _wtCtx[wt];
  if (ctx === undefined) {
    ctx = navCwd(wt) || "";
    if (ctx) {                                        // the context must reproduce wt
      const c = uri._parse(ctx);
      try { if (resolve(c, "") !== wt) ctx = ""; } catch (e) { ctx = ""; }
    }
    _wtCtx[wt] = ctx;
  }
  if (!ctx) return pathlib.wtJoin(wt, rel);           // outside the hive → plain confine
  const c = uri._parse(ctx);
  const abs = resolve(c, rel || "");                  // resolve-backed, context-honoured
  //  keep wtJoin's WT-level boundary: resolve() confines to the TREE (for a
  //  submodule that is the parent tree), so refuse a path that climbs OUT of `wt`.
  if (abs !== wt && abs.indexOf(wt + "/") !== 0)
    throw "NAVESCAPE: path escapes the worktree";
  return abs;
}

module.exports = { treeAt: treeAt, wtdir: wtdir, resolve: resolve, wtpath: wtpath,
                   beRoot: beRoot,
                   argRel: argRel, ctxSub: _ctxSub, ctxDir: ctxDir,
                   navCwd: navCwd, navTree: navTree, cwd: contextCwd,
                   projectRoot: projectRoot, workRoot: workRoot, metaRoot: metaRoot,
                   todoRoot: todoRoot,
                   //  URI-016: the anchor READER (not a climb) — resolve_hash.treeAt
                   //  calls it once it has climbed to the anchor dir.
                   resolveAnchor: resolveAnchor,
                   //  exported for wtlog.js / tests
                   repoFromBe: repoFromBe,
                   projectFromQuery: projectFromQuery,
                   projectFromPath: projectFromPath };
