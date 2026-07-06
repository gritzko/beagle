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
    const up = dirname(wt);
    if (!up || up === wt || up === "/") break;
    if (home && up === home) break;              // the SRC_ROOT/$HOME store level —
    let outer; try { outer = find(up); } catch (e) { break; }   // a STORE, not a super
    if (!outer || !outer.wt || outer.wt === wt || outer.wt === home) break;
    wt = outer.wt;                               // wt was nested (submodule) → climb
  }
  return wt;
}

//  URI-011: srcRoot() → where the worktrees live.  `SRC_ROOT` env wins; else it is
//  IMPLIED as ONE LEVEL UP from the discovered TOP wt root (probe `.be` from cwd,
//  climb past submodules) — so `//name` addresses a SIBLING of the tree you
//  launched in, with no $HOME assumption.  Falls back to $HOME when repo-less.
function srcRoot() {
  const env = io.getenv("SRC_ROOT");
  if (env) return env;
  let wt; try { wt = topWt(find(io.cwd()).wt); } catch (e) { return io.getenv("HOME") || "."; }
  return dirname(wt) || io.getenv("HOME") || ".";
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
  return out;
}

//  URI-011: wtdir(uriStr) → the ABSOLUTE dir a nav URI addresses, or null.
//    //name[/sub]  → $SRC_ROOT/name/sub  (a tree under SRC_ROOT, confined below)
//    // , //.       → the LAUNCH tree     (find(cwd).wt — "where jab started")
//    //host…, file:/ssh:/be:, no `//`     → null (a cached remote / transport → wire)
//  SRC_ROOT env, default $HOME.  Confinement: the resolved tree must sit AT/BELOW
//  $SRC_ROOT/name — else find() walked UP to an ancestor store (the $HOME/.be
//  hazard) and this is NOT the named tree → null.  The abs path; the caller
//  find()s it for the repo and derives the in-tree scope.
function wtdir(uriStr) {
  let u; try { u = uri._parse(uriStr || ""); } catch (e) { return null; }
  if (u.scheme) return null;                          // a transport, not nav
  if (u.authority === undefined) return null;         // no `//` slot
  const host = u.host || "";
  if (host === "" || host === ".") {                  // `//` / `//.` → launch tree
    try { return find(io.cwd()).wt; } catch (e) { return null; }
  }
  const root = srcRoot();
  const top = root + "/" + host;                      // the named top-level tree
  const dir = top + (u.path || "");                   // + the nested path
  let repo; try { repo = find(dir); } catch (e) { return null; }
  if (!repo || (repo.wt !== top && repo.wt.indexOf(top + "/") !== 0)) return null;
  return dir;
}

//  URI-011: navCwd(dir?) → the `//name/path` context URI for a directory
//  (default cwd) — the INVERSE of wtdir, and the context a session STARTS with
//  ("where I am").  name = the worktree `wt` under SRC_ROOT (may nest, `src/dogs`);
//  path = `dir` under `wt`.  "" when the dir is in no known tree (repo-less cwd).
function navCwd(dir) {
  const d = dir || io.cwd();
  let repo; try { repo = find(d); } catch (e) { return ""; }
  if (!repo || !repo.wt) return "";
  //  Name off the TOP wt (climb past submodules); the sub-path crosses into the
  //  submodule (`//name/sub/inner`) — see [SUBS-045] joinPrefix.
  const top = topWt(repo.wt);
  const root = srcRoot();
  const name = top === root ? ""
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
//  this dir — never SRC_ROOT.  The context URI (be.authority / navCwd) is the "dir
//  to cd to"; no io.chdir binding needed.
function contextCwd() {
  if (typeof be !== "undefined" && be.repo && be.repo.wt) return be.repo.wt;
  try { return find(io.cwd()).wt; } catch (e) { return io.cwd(); }
}

module.exports = { find: find, wtdir: wtdir, navCwd: navCwd, cwd: contextCwd,
                   srcRoot: srcRoot, todoRoot: todoRoot, topWt: topWt,
                   //  exported for wtlog.js / tests
                   repoFromBe: repoFromBe,
                   projectFromQuery: projectFromQuery,
                   projectFromPath: projectFromPath };
