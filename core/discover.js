//  be.js — repo discovery (JS-029).  Pure JS over the JABC runtime
//  (io.cwd/getenv/stat/mmap + the URI binding); no C, no dog.  Mirrors
//  dog/HOME.c::home_walk_up + home_anchor_resolve.
//
//  URI-016: this file no longer CLIMBS.  There is ONE `.be` climber, in
//  core/resolve_hash.js (climb/anchors), serving both [/wiki/URI] step 1
//  (projectRoot: the TOPMOST anchor) and step 4 (treeAt: the NEAREST).
//  projectRoot()/workRoot()/metaRoot()/todoRoot()/treeAt() here are
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
//  CODE-030: THE climber — and, since the anchor READER and the confining
//  resolve() are the climb's own halves, this module is now their FACADE.
const rh = require("./resolve_hash.js");
const join = pathlib.join, dirname = pathlib.dirname;

//  CODE-030: forwarded onto core/resolve_hash.js — the names verbs/views call.
const beRoot = rh.beRoot, resolveAnchor = rh.resolveAnchor, resolve = rh.resolve;
const repoFromBe = rh.repoFromBe, projectFromQuery = rh.projectFromQuery;
const projectFromPath = rh.projectFromPath;

function statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }

//  URI-016: the CLIMB LIMIT — `$BE_ROOT`, defaulting to `$HOME`.  THE climb
//  (core/resolve_hash.js climb(), the only one left) never reaches it, so a repo
//  tree outside $HOME (or a test's scratch base) sets BE_ROOT and the walk stops
//  there by construction.  [/wiki/URI] step 1 says an anchor is "still lower than
//  $HOME"; BE_ROOT is that limit, made explicit — and "lower than" is STRICT, so
//  $BE_ROOT/.be (the STORE) anchors neither a project root nor a worktree.

//  URI-016: `find()` DELETED — it was a LEGACY NAME for [/wiki/URI] step 4, kept
//  only because verb/view call sites spelled it `be.find`.  They now say what they
//  mean: `be.treeAt`, the step-4 routine itself (core/resolve_hash.js), which
//  shares climb()/anchors() with step 1's projectRoot() — one chain, one anchor
//  test, one limit.  The delegation below is the AMBIENT handle (core/loop.js
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

//  URI-016: topWt(wt) DELETED — it climbed past submodules by re-probing every
//  ancestor with find(), and drew the work/ boundary with a SECOND spelling of the
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
//  `.be` climber.
//  URI-016: todoRoot() DELETED here too — it read `$TODO_ROOT`, ran its OWN
//  find()/topWt() climb, and returned a LIST of candidate roots to probe.  All
//  three are gone: the project root CAN NOT be an env var (an env var that
//  disagreed with the climb is just a second, lying answer), there is exactly
//  ONE `.be` climber (resolve_hash.projectRoot), and the ticket tree is not a
//  search path — `todoRoot()` IS `projectRoot() + "/todo"`, one STRING.
//  URI-016: step 4 (the NEAREST anchor), the ambient handle verbs/views call as
//  `be.treeAt`.  `from`/`topDir` ride through for resolve_hash.frame's re-anchor.
function treeAt(path, from, topDir) {
  return rh.treeAt(path || io.cwd(), from, topDir);
}

function projectRoot() { return rh.projectRoot(); }
function workRoot()    { return rh.workRoot(); }
function metaRoot()    { return rh.metaRoot(); }
function todoRoot()    { return rh.todoRoot(); }

//  URI-011: wtdir(uriStr) → the ABSOLUTE dir a nav URI addresses, or null.
//    //name[/sub]  → <srcRoot>/name/sub  (a worktree, confined below)
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
    if (!rh.projectRoot()) return null;
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
  let repo; try { repo = rh.treeAt(dir); } catch (e) { return null; }
  if (!repo || (repo.wt !== top && repo.wt.indexOf(top + "/") !== 0)) return null;
  return dir;
}

//  URI-011: navCwd(dir?) → the `//name/path` context URI for a directory
//  (default cwd) — the INVERSE of wtdir, and the context a session STARTS with
//  ("where I am").  name = the worktree `wt` under srcRoot() (may nest, `src/dogs`);
//  path = `dir` under `wt`.  "" when the dir is in no known tree (repo-less cwd).
function navCwd(dir) {
  const d = dir || io.cwd();
  let repo; try { repo = rh.treeAt(d); } catch (e) { return ""; }
  if (!repo || !repo.wt) return "";
  //  Name off the TOP wt (past submodules); the sub-path crosses into the
  //  submodule (`//name/sub/inner`) — see [SUBS-045] joinPrefix.
  const top = rh.topOf(repo.wt) || repo.wt;
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
  let repo; try { repo = rh.treeAt(d); } catch (e) { return ""; }
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
  try { return rh.treeAt(io.cwd()).wt; } catch (e) { return io.cwd(); }
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
  let repo; try { repo = rh.treeAt(d); } catch (e) { return d; }
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
//  same wt.  null marks a wt that is repo-less / OUTSIDE work/ (the fallback).
//  BE-065: the entry now holds the PARSED context and the resolved base dir —
//  `{ c, base }`, base = resolve(c, "") — so a HIT costs no uri._parse and no
//  resolve_hash walk at all.  Lifetime is per-process, as before.
const _wtCtx = {};

//  BE-065: is `rel` a path the base+rel join provably resolves the same way as
//  resolve()?  YES iff it is relative and every "/"-segment is a plain name: no
//  ""/"."/".." (resolveInTree would normalise or refuse those) and no ":" (which
//  is the only way _relPath could read a scheme).  Anything else falls back to
//  the full resolve() — which stays THE resolver ([BE-030]/[URI-016]).
function _plainRel(rel) {
  const n = rel.length;
  if (n === 0 || rel.charCodeAt(0) === 47) return false;   // "" / rooted "/x"
  let s = 0;
  for (let i = 0; i <= n; i++) {
    const c = i === n ? 47 : rel.charCodeAt(i);            // 47 = "/", 46 = ".", 58 = ":"
    if (c === 58) return false;
    if (c !== 47) continue;
    const len = i - s;
    if (len === 0) return false;                           // "" segment ("a//b", "a/")
    if (rel.charCodeAt(s) === 46 &&
        (len === 1 || (len === 2 && rel.charCodeAt(s + 1) === 46))) return false;
    s = i + 1;
  }
  return true;
}

//  BE-030: wtpath(wt, rel) → the ABSOLUTE fs path of the wt-relative `rel` in the
//  tree rooted at `wt`, computed THROUGH resolve() so every worktree file access
//  takes the nav CONTEXT into account: navCwd(wt) yields the `//name` authority +
//  in-repo sub-path (a submodule wt crosses in as `//name/sub`), and resolve()
//  maps that context back to the abs path.  This is the ONE way a verb/view turns
//  a worktree path into bytes on disk — it REPLACES raw wtJoin(wt,rel)/join(wt,rel)
//  at every fs site.  Confinement is preserved EXACTLY: resolve() throws NAVESCAPE
//  on a `..` climb above the tree root, and the trailing guard refuses any path
//  that climbs OUT of `wt` (matching wtJoin's wt-level boundary).  A wt outside
//  outside work/ (a store edge / scratch dir) has no `//name` address → the
//  plain wtJoin confine is used (byte-identical to the pre-BE-030 behavior).
function wtpath(wt, rel) {
  let m = _wtCtx[wt];
  if (m === undefined) {
    const ctx = navCwd(wt) || "";
    m = null;
    if (ctx) {                                        // the context must reproduce wt
      const c = uri._parse(ctx);
      //  BE-065: resolve(c,"") is the memoised BASE DIR — computed ONCE, through
      //  resolve_hash, and kept only when it reproduces `wt` (so base === wt).
      try { const b = resolve(c, ""); if (b === wt) m = { c: c, base: b }; } catch (e) {}
    }
    _wtCtx[wt] = m;
  }
  if (!m) return pathlib.wtJoin(wt, rel);             // outside work/ → plain confine
  //  BE-065 fast path: base + "/" + rel.  base === wt and every segment is a plain
  //  name, so this IS resolve()'s answer and the wt boundary below holds by
  //  construction.  Empty rel is the base itself (resolve(c,"") memoised).
  if (rel === undefined || rel === null || rel === "") return m.base;
  if (typeof rel === "string" && _plainRel(rel)) return m.base + "/" + rel;
  const abs = resolve(m.c, rel || "");                // resolve-backed, context-honoured
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
