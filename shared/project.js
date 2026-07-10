//  project.js — the PROJECT root and its fixed layout ([/wiki/Project]): the
//  top-level repo holding all the metainformation, anchored by a `.be` or
//  `.git` PLUS the `meta/` manuals dir.  root() climbs from a start dir to
//  the first such ancestor (probing $HOME, never above).  All `//WORK/path`
//  URIs count from that root: `//name/…` → `<root>/work/<name>/…` (the
//  worktree hive, Project.mkd item 5), an empty/absent authority → the main
//  tree `<root>/…`.  Publication targets mirror into `<root>/html/`.
"use strict";

const pathlib = require("./util/path.js");

function statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }

//  The project anchor is LAYOUT, not wtlog semantics (contrast core/discover
//  find()): a `.be` (file or dir) OR a `.git` (dir, or a submodule's gitlink
//  file) — plus `meta/`, which only the project root carries.
function anchors(dir) {
  if (statKind(pathlib.join(dir, "meta")) !== "dir") return false;
  return statKind(pathlib.join(dir, ".be")) !== undefined ||
         statKind(pathlib.join(dir, ".git")) !== undefined;
}

//  root(startDir?) → the project root abs path, or null when nothing anchors
//  one.  The default-cwd result is memoized on the `be` global (the BE-011
//  pattern: fixed for a process run); an explicit startDir is never cached.
function root(startDir) {
  if (!startDir && typeof be !== "undefined" && be.projectRootDir !== undefined)
    return be.projectRootDir;
  let dir = startDir || io.cwd();
  const home = io.getenv("HOME");
  let found = null;
  for (;;) {
    if (anchors(dir)) { found = dir; break; }
    if (home && dir === home) break;           // probe $HOME, never above
    const up = pathlib.dirname(dir);
    if (up === dir || up === ".") break;       // reached /
    dir = up;
  }
  if (!startDir && typeof be !== "undefined") be.projectRootDir = found;
  return found;
}

//  resolve(arg) → { root, tree, rel, abs } for a `//WORK/path/file` URI or a
//  bare path, counted from the project root.  `tree` is the TREE the path
//  lives in (`<root>/work/<name>` for an authority, else the root itself) —
//  link/existence probing anchors there; `rel` is the tree-relative path,
//  confined via resolveInTree (NAVESCAPE on any `..` climb-out).
function resolve(arg) {
  const r = root();
  if (!r) throw "PROJECT: no project root (.be/.git + meta/) above " + io.cwd();
  const s = String(arg);
  let u; try { u = uri._parse(s); } catch (e) { u = {}; }
  if (u.scheme !== undefined)
    throw "PROJECT: a scheme URI is not a project path: " + s;
  const host = u.host || "";
  if (host && !pathlib.safeRel(host))
    throw "NAVESCAPE: bad project authority //" + host;
  const tree = host ? pathlib.join(pathlib.join(r, "work"), host) : r;
  const rel = pathlib.resolveInTree("", u.path !== undefined ? u.path : s);
  return { root: r, tree: tree, rel: rel,
           abs: rel ? pathlib.join(tree, rel) : tree };
}

module.exports = { root: root, resolve: resolve };
