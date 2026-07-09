//  be.js — repo discovery (JS-029).  Pure JS over the JABC runtime
//  (io.cwd/getenv/stat/mmap + the URI binding); no C, no dog.  Mirrors
//  dog/HOME.c::home_walk_up + home_anchor_resolve.
//
//  find(cwd?) walks UP from `cwd` (default io.cwd()) to the first
//  ancestor anchoring a worktree — a `.be` that is either a FILE
//  (secondary wt: the file IS the wtlog) or a DIRECTORY containing a
//  `wtlog` (primary wt) — never escaping above $HOME.  Returns
//      { root, wt, bePath, storePath, project }
//  where
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

function statKind(p) {
  try { return io.stat(p).kind; } catch (e) { return undefined; }
}
function isFile(p) { return statKind(p) === "reg"; }
function isDir(p) { return statKind(p) === "dir"; }

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

//  Read row 0 of a secondary wt's `.be` (which IS the wtlog) and return
//  its anchor URI string, or undefined.  A secondary anchor row 0 is a
//  `get`/`repo` row pointing at the shared store.
function row0Uri(bePath) {
  let uri;
  ulog.each(bePath, function (log) { if (uri === undefined) uri = log.uri; });
  return uri;
}

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
    const u = row0Uri(be);
    if (u) {
      const p = new URI(u);
      storePath = repoFromBe(p.path);
      project = projectFromQuery(p.query) || projectFromPath(p.path);
    }
    if (!storePath) storePath = wt;
  } else {
    //  Primary worktree: <wt>/.be/wtlog.
    bePath = join(be, WTLOG);
    storePath = wt;                          // colocated store == wt
    const u = isFile(bePath) ? row0Uri(bePath) : undefined;
    if (u) {
      const p = new URI(u);
      //  A store-anchor row-0 (repo/get) carries a /.be/ path; a fresh jab-posted
      //  colocated primary opens with a post row-0 (no path) — stays store==wt.
      if (p.path) { const sp = repoFromBe(p.path); if (sp) storePath = sp; }
      project = projectFromQuery(p.query) || projectFromPath(p.path || "");
    }
  }
  return { root: storePath, wt: wt, bePath: bePath, storePath: storePath,
           project: project };
}

//  Walk up from `start` to the first ancestor that anchors a worktree:
//    * `.be` is a FILE                              → secondary wt
//    * `.be` is a DIR holding `wtlog`               → primary wt
//    * `.be` is a DIR that is shield-like (≤1 shard)→ fresh/single store
//  Stop at $HOME (after probing $HOME/.be); never ascend above it.
//  Throws when the walk reaches the top without an anchor.
function find(cwd) {
  let here = cwd || io.cwd();
  const home = io.getenv("HOME");

  for (;;) {
    const be = join(here, BE);
    const kind = statKind(be);
    if (kind !== undefined) {
      let isWt = (kind === "reg");
      if (kind === "dir") {
        if (isFile(join(be, WTLOG))) isWt = true;
        else if (shieldLike(be)) isWt = true;   // fresh / single-shard store
      }
      if (isWt) return resolveAnchor(here);
    }
    //  Stop at $HOME AFTER the probe (so $HOME/.be still counts).
    if (home && here === home) break;
    const up = dirname(here);
    if (up === here || up === "." ) break;       // reached /
    here = up;
  }
  throw "be.find: no .be worktree anchor from '" + (cwd || io.cwd()) + "'";
}

//  A `.be` dir is shield-like (a valid anchor) iff it has ≤1 immediate
//  non-dotted subdirectory — a fresh worktree shield or a single-project
//  store.  A multi-project store (>1 shard) is NOT a wt; keep walking.
//  Mirrors dog/HOME.c::home_dir_shieldlike / home_be_subdirs.
function shieldLike(beDir) {
  let subdirs = 0;
  try {
    io.readdir(beDir, function (name) {
      //  readdir marks dirs with a trailing '/'; dotted entries
      //  (".be" etc.) are never shards.
      if (name[name.length - 1] !== "/") return "more";
      const base = name.slice(0, -1);
      if (base === "" || base[0] === ".") return "more";
      if (++subdirs > 1) return "enough";
      return "more";
    });
  } catch (e) { return false; }              // dir vanished / unreadable
  return subdirs <= 1;
}

//  URI-011: topWt(wt) → the OUTERMOST worktree root, climbing PAST submodules (a
//  sub is a wt nested inside a parent wt; find(parent) hits the super's `.be`).
//  Keep climbing until nothing anchors above → that IS the top.  The `$HOME/.be`
//  STORE is not a worktree (find refuses it), so the climb stops at the real top.
function topWt(wt) {
  const home = io.getenv("HOME");
  for (;;) {
    //  BE-031: hive boundary — a cell (<meta>/work/<name>) is its OWN top; never
    //  climb across `work/` into the meta (whose `.be` FILE would claim the cell).
    if (pathlib.basename(dirname(wt)) === "work") break;
    const up = dirname(wt);
    if (!up || up === wt || up === "/") break;
    if (home && up === home) break;              // the $HOME store level —
    let outer; try { outer = find(up); } catch (e) { break; }   // a STORE, not a super
    if (!outer || !outer.wt || outer.wt === wt || outer.wt === home) break;
    wt = outer.wt;                               // wt was nested (submodule) → climb
  }
  return wt;
}

//  BE-031: srcRoot() → the HIVE dir `<meta>/work/` where the worktree cells live.
//  `SRC_ROOT` env wins (the flat-legacy escape hatch); else inferred from the TOP
//  wt: launched inside a cell → the cell's `work/` parent, launched in the meta
//  (or any plain wt) → `<top>/work`.  Falls back to $HOME when repo-less.
function srcRoot() {
  //  BE-011: memoize the resolved root on the `be` global — SRC_ROOT is fixed for
  //  a process run, so every resolve()/wtdir()/navCwd() reads ONE stable value
  //  (mintBe's Object.assign never carries a srcRootDir key, so it is never wiped).
  if (typeof be !== "undefined" && be.srcRootDir) return be.srcRootDir;
  let dir = io.getenv("SRC_ROOT");
  if (!dir) {
    //  BE-031: a cell's parent IS the hive; any other top wt (the meta, a plain
    //  standalone wt) hosts its hive at <top>/work.
    try {
      const t = topWt(find(io.cwd()).wt);
      dir = pathlib.basename(dirname(t)) === "work" ? dirname(t) : join(t, "work");
    }
    catch (e) { dir = io.getenv("HOME") || "."; }
  }
  if (typeof be !== "undefined") be.srcRootDir = dir;
  return dir;
}

//  BRO-012: todoRoot() → the ordered ticket-tree roots, a MIRROR of srcRoot():
//  (1) explicit $TODO_ROOT env; (2) the CURRENT wt root (the nav'd view's
//  authority, be.repo climbed past submodules); (3) the OPEN/launch wt root
//  (topWt(find(cwd)) — where jab started).  Returns the roots in order,
//  skipping unset/duplicate ones — the resolver probes each `<root>/todo/…`.
function todoRoot() {
  const out = [];
  const push = (d) => { if (d && out.indexOf(d) < 0) out.push(d); };
  push(io.getenv("TODO_ROOT"));
  //  current wt: the nav-scoped repo (be.repo), else cwd's repo, topWt'd.
  try {
    const cur = (typeof be !== "undefined" && be.repo && be.repo.wt) || find(io.cwd()).wt;
    push(topWt(cur));
  } catch (e) { /* repo-less current → skip */ }
  //  open/launch wt: cwd's own repo (jab's launch tree), topWt'd.
  try { push(topWt(find(io.cwd()).wt)); } catch (e) { /* repo-less launch → skip */ }
  //  BE-031: the meta root (the hive's parent) holds the shared todo/ tree —
  //  probe it so tickets resolve from inside a cell.
  push(dirname(srcRoot()));
  return out;
}

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
//  Returns the clean relative path (query/fragment shed) for resolveInTree.
function _relPath(rel) {
  if (rel === undefined || rel === null || rel === "") return "";
  const s = String(rel);
  let u; try { u = uri._parse(s); } catch (e) { u = {}; }
  if (u.scheme !== undefined)
    throw "NAVESCAPE: rel path carries a scheme, not a path: " + s;
  if (u.authority !== undefined)
    throw "NAVESCAPE: rel path carries a // authority, not a path: " + s;
  return u.path || "";
}

//  BE-037: the LAUNCH tree's TOP wt — the tree the empty `//` host names.  Memoized
//  on `be` (cwd is fixed per run): wtpath resolves through it on EVERY fs access.
function launchTop() {
  if (typeof be !== "undefined" && be.launchTopWt) return be.launchTopWt;
  const t = topWt(find(io.cwd()).wt);                 // throws when repo-less
  if (typeof be !== "undefined") be.launchTopWt = t;
  return t;
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
//  LAUNCH tree (find(cwd)).  Throws on escape / bad name.
function resolve(context, rel) {
  const host = _ctxHost(context);
  const relPath = _relPath(rel);                      // throws on a //other / scheme
  //  A rooted `/x` addresses the tree root (context path dropped); else `rel`
  //  resolves against the context's trusted in-repo path.  resolveInTree NORMALISES.
  const basePath = relPath[0] === "/" ? "" : _ctxPath(context);
  const sub = pathlib.resolveInTree(basePath, relPath);   // throws on climb-out
  if (host === "" || host === ".") {
    //  BE-037: `//` = the TOP tree (what navCwd names `//`), not the nearest
    //  anchor — a submodule launch must agree with the pager-composed `//`.
    const wt = launchTop();                           // throws when repo-less
    return sub ? join(wt, sub) : wt;
  }
  if (!pathlib.safeRel(host)) throw "NAVESCAPE: bad nav authority //" + host;
  const dir = join(srcRoot(), host);
  return sub ? join(dir, sub) : dir;
}

//  URI-011: wtdir(uriStr) → the ABSOLUTE dir a nav URI addresses, or null.
//    //name[/sub]  → <srcRoot>/name/sub  (a hive cell, confined below)
//    // , //.       → the LAUNCH tree     (find(cwd).wt — "where jab started")
//    //host…, file:/ssh:/be:, no `//`     → null (a cached remote / transport → wire)
//  A `//name` miss (find has no anchor at/below <srcRoot>/name) is left to the
//  caller as a cached-remote-or-typo decision.  BE-011: confinement is now a
//  PROPERTY of resolve() (NAVESCAPE on any `..` climb), not a lexical prefix check.
function wtdir(uriStr) {
  let u; try { u = uri._parse(uriStr || ""); } catch (e) { return null; }
  if (u.scheme) return null;                          // a transport, not nav
  if (u.authority === undefined) return null;         // no `//` slot
  const host = u.host || "";
  if (host === "" || host === ".") {
    //  BE-037: `//[/path]` rides resolve like `//name` — the TOP tree, path
    //  honoured; a repo-less cwd is the miss (null), NAVESCAPE still propagates.
    try { launchTop(); } catch (e) { return null; }
    return resolve("//" + host, u.path || "");
  }
  //  BE-030: compose + CONFINE via resolve(context, rel).  The nav URI's path is
  //  UNTRUSTED, so the context is host-ONLY (`//host`, empty trusted path) and the
  //  path rides `rel` — a `..` climb / bad authority throws NAVESCAPE and PROPAGATES
  //  (the CLI REFUSES loudly, never adopting an outside tree).  resolve throws ONLY
  //  on escape, never on a plain not-found → safe to let fly.
  const dir = resolve("//" + host, u.path || "");
  //  Confirm `//name` is a REAL anchored worktree AT/BELOW <srcRoot>/host (not an
  //  ancestor store find() walked up to).  `dir` is `..`-free now, so this prefix
  //  compare is a sound EXISTENCE check, no longer a (broken) security boundary.
  const top = join(srcRoot(), host);
  let repo; try { repo = find(dir); } catch (e) { return null; }
  if (!repo || (repo.wt !== top && repo.wt.indexOf(top + "/") !== 0)) return null;
  return dir;
}

//  URI-011: navCwd(dir?) → the `//name/path` context URI for a directory
//  (default cwd) — the INVERSE of wtdir, and the context a session STARTS with
//  ("where I am").  name = the worktree `wt` under srcRoot() (may nest, `src/dogs`);
//  path = `dir` under `wt`.  "" when the dir is in no known tree (repo-less cwd).
function navCwd(dir) {
  const d = dir || io.cwd();
  let repo; try { repo = find(d); } catch (e) { return ""; }
  if (!repo || !repo.wt) return "";
  //  Name off the TOP wt (climb past submodules); the sub-path crosses into the
  //  submodule (`//name/sub/inner`) — see [SUBS-045] joinPrefix.
  const top = topWt(repo.wt);
  const root = srcRoot();
  //  BE-031: the meta (the hive's parent wt) has no `//name` address — its own
  //  context is the bare `//`; a hive cell slices its name off srcRoot().
  const name = top === root || top === dirname(root) ? ""
             : top.indexOf(root + "/") === 0 ? top.slice(root.length + 1)
             : top.slice(top.lastIndexOf("/") + 1);      // fallback: basename
  const sub = d.length > top.length ? d.slice(top.length + 1) : "";
  //  Compose the `//name[/sub]` context URI via the URI class (authority = name,
  //  path = the sub crossing into the submodule); byte-identical to the old concat.
  return uri._make(undefined, "//" + name, sub ? "/" + sub : undefined) ||
         ("//" + name);
}

//  URI-011: cwd() → the CONTEXT worktree ROOT a verb runs from — the ONE place a
//  verb asks "where am I operating," replacing raw io.cwd() so a nav'd verb acts
//  in the scoped tree, NOT the launch tree.  = the resolved repo's wt (be.repo.wt,
//  which authorityRepo may anchor on a SUBMODULE wt for a `//name/sub/…` path),
//  else the launch cwd's wt; repo-less falls back to io.cwd().  Verbs need only
//  this dir — never srcRoot().  The context URI (be.authority / navCwd) is the "dir
//  to cd to"; no io.chdir binding needed.
function contextCwd() {
  if (typeof be !== "undefined" && be.repo && be.repo.wt) return be.repo.wt;
  try { return find(io.cwd()).wt; } catch (e) { return io.cwd(); }
}

//  BE-030: per-process cache of a wt root → its validated nav context URI, so the
//  per-fs-access wtpath() below never re-walks the tree (navCwd/find) twice for the
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

module.exports = { find: find, wtdir: wtdir, resolve: resolve, wtpath: wtpath,
                   navCwd: navCwd, cwd: contextCwd,
                   srcRoot: srcRoot, todoRoot: todoRoot, topWt: topWt,
                   //  exported for wtlog.js / tests
                   repoFromBe: repoFromBe,
                   projectFromQuery: projectFromQuery,
                   projectFromPath: projectFromPath };
