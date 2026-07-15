//  project.js — `//WORK/path` URIs counted from the PROJECT root
//  ([/wiki/Project]): the top-level repo holding all the metainformation.
//  This file holds NO root-finding logic of its own — root() is a pure alias
//  of THE one `.be` climber (core/resolve_hash.js projectRoot()).  All
//  `//WORK/path` URIs count from that root: `//name/…` → `<root>/work/<name>/…`
//  (a worktree under `work/`, Project.mkd item 5), an empty/absent authority → the
//  main tree `<root>/…`.  Publication targets mirror into `<root>/html/`.
"use strict";

const pathlib = require("./util/path.js");

//  root() → the project root abs path, or null when no `.be` anchors one.
//  A pure alias of THE one `.be` climber ([/wiki/URI] step 1): the TOPMOST dir
//  carrying a store-resolving `.be`, still lower than $BE_ROOT (default $HOME),
//  climbed from cwd.  No start dir — the root is a property of the run, not of
//  a caller's dir.  projectRoot() caches its own climb on `be.projectRootDir`;
//  there is no second cache here.  The require stays LAZY: resolve_hash pulls
//  in discover, and a top-level require risks a cycle.
function root() {
  return require("../core/resolve_hash.js").projectRoot();
}

//  resolve(arg) → { root, tree, rel, abs } for a `//WORK/path/file` URI or a
//  bare path, counted from the project root.  `tree` is the TREE the path
//  lives in (`<root>/work/<name>` for an authority, else the root itself) —
//  link/existence probing anchors there; `rel` is the tree-relative path,
//  confined via resolveInTree (NAVESCAPE on any `..` climb-out).
function resolve(arg) {
  const r = root();
  if (!r) throw "PROJECT: no .be below $BE_ROOT above " + io.cwd();
  const s = String(arg);
  let u; try { u = uri._parse(s); } catch (e) { u = {}; }
  if (u.scheme !== undefined)
    throw "PROJECT: a scheme URI is not a project path: " + s;
  const host = u.host || "";
  if (host && !pathlib.safeRel(host))
    throw "NAVESCAPE: bad project authority //" + host;
  //  URI-016: workRoot(), never join(r, "work") — the `work` segment is spelled
  //  in ONE place (core/resolve_hash.js), and this was a third reading of it.
  const tree = host ? pathlib.join(require("../core/resolve_hash.js").workRoot(), host) : r;
  const rel = pathlib.resolveInTree("", u.path !== undefined ? u.path : s);
  return { root: r, tree: tree, rel: rel,
           abs: rel ? pathlib.join(tree, rel) : tree };
}

module.exports = { root: root, resolve: resolve };
