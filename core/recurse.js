//  core/recurse.js — the generic in-process mounted-sub walk (JAB-014,
//  factored out of status.js::emitRepo per JAB-004).  ONE depth-first sub
//  recursion shared by EVERY recursing read view (status, diff, …): enumerate
//  a repo's live gitlink mounts in `.gitmodules` DECLARATION order, open each
//  sub's repo, and invoke a per-sub callback with the URI-aware emit prefix
//  joined under the parent's.  NO fork, NO `/tmp`, NO `/proc` — a synchronous
//  in-process walk (the JAB-004 mechanism the other 17 views inherit).
//
//  walk(repo, prefix, visit, opts):
//    repo    : the parent repo handle (be.find result).
//    prefix  : this repo's display path relative to the TOP wt ("" at top).
//    visit   : function(subRepo, subPrefix, sub) — called per MOUNTED sub in
//              `.gitmodules` order; `subPrefix` = joinPrefix(prefix, sub.path);
//              `sub` carries { path, pin } when the caller passed a gitlink map.
//    opts.gitlinks : optional { path -> {pin, mounted?} } from the parent's
//              tree walk — drives the mount gate + carries each sub's pin sha.
//              Absent → every `.gitmodules`-declared path with a `<sub>/.be`
//              mount file is visited (status's clean-recursion case).
//
//  Mirrors status.js's old emitRepo recursion tail: `.gitmodules` order is
//  authoritative (KEEPSubsAt parses the blob top-to-bottom), the tree/mount
//  state gates which declared path is a live gitlink, and the joined prefix
//  makes a grandchild read `<sub>/<grandchild>`.

"use strict";

const path = require("../shared/util/path.js");   // BE-026: wtJoin confinement
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const wtpath = require("./discover.js").wtpath;

//  YES iff `<wt>/<subpath>/.be` is a regular file (a live mount).
//  Mirrors SNIFFSubIsMount: only a mounted sub is recursed.
//  GET-036: a SYMLINK at `<wt>/<subpath>` is never a mount — the `be/`
//  self-locator `be -> .` follows to the wt's OWN `.be` anchor and would
//  otherwise read as a phantom mount.  A real mount point is a real dir.
function isMount(wtRoot, subpath) {
  //  BE-026: confine the (untrusted `.gitmodules`) subpath — a `..`/abs escape
  //  throws NAVESCAPE, refused as no-mount (never lstat/stat OUTSIDE the wt).
  let base;
  try { base = wtpath(wtRoot, subpath); } catch (e) { return false; }
  try { if (io.lstat(base).kind === "lnk") return false; } catch (e) {}
  const p = base + "/.be";
  //  SUBS-049: mount = the `.be` FILE form OR a PRIMARY nested wt (a `.be` DIR
  //  holding a wtlog reg file, what a green-field remote-get plants) — mirror
  //  be.find's primary-anchor rule so a DIR-anchored declared sub delegates.
  let k; try { k = io.stat(p).kind; } catch (e) { return false; }
  if (k === "reg") return true;
  if (k !== "dir") return false;
  try { return io.stat(p + "/wtlog").kind === "reg"; } catch (e) { return false; }
}

//  Parse `<wt>/.gitmodules` → declared submodule `path` values in FILE order
//  (the order native recurses; KEEPSubsAt drives SUBSu8sParse top-to-bottom).
//  PUT-004: delegates to the shared reader (was a copy-pasted git-config parser).
function gitmodulesOrder(wtRoot) {
  return require("../shared/gitmodules.js").paths(wtRoot);
}

//  URI-aware join of a path under a sub prefix (JAB-004) — empty prefix is a
//  no-op (top level); else `<prefix>/<path>`.  A non-path token (scheme/auth)
//  is returned untouched (defensive — sub paths are always bare paths).
function joinPrefix(prefix, col) {
  if (!prefix) return col;
  const u = uri._parse(col);
  if (u.scheme || u.authority) return col;
  return prefix + "/" + col;
}

//  walk(repo, prefix, visit, opts) — depth-first over the parent's MOUNTED
//  subs in `.gitmodules` order.  See the header.  The mount gate prefers an
//  explicit gitlink map (the caller's tree walk) when given, else probes the
//  `<sub>/.be` file directly.
function walk(repo, prefix, visit, opts) {
  opts = opts || {};
  const gitlinks = opts.gitlinks || null;
  for (const subPath of gitmodulesOrder(repo.wt)) {
    //  Gate: a declared path is a live sub only when it is BOTH a tree gitlink
    //  (when a map is supplied) AND a mounted `<sub>/.be`.  Absent a map, the
    //  mount file alone gates (status's clean-recursion path).
    if (gitlinks && !gitlinks[subPath]) continue;
    if (!isMount(repo.wt, subPath)) continue;
    //  BE-026: confine before be.find (belt-and-suspenders past the isMount gate).
    let subWt;
    try { subWt = wtpath(repo.wt, subPath); } catch (e) { continue; }
    let subRepo;
    try { subRepo = be.find(subWt); } catch (e) { continue; }
    const sub = gitlinks ? gitlinks[subPath] : { path: subPath };
    visit(subRepo, joinPrefix(prefix, subPath), sub || { path: subPath });
  }
}

//  SUBS-045: shared read-side path->repo splitter (read twin of SUBS-039).
//  Descends the deepest mounted-sub prefix → { repo, rest, prefix }: the descent
//  DELTA `prefix` (LOG-002's discarded joinPrefix) that log re-prefixes onto nav
//  URIs and commit descends before resolving; "" when nothing mounted.
function resolveRepoForPath(repo, relPath) {
  const segs = relPath ? relPath.split("/") : [];
  let i = 0, prefix = "";
  for (;;) {
    let hit = -1;
    //  Longest mounted prefix wins (so `a/b` descends a, then b next loop).
    for (let n = i + 1; n <= segs.length; n++) {
      if (isMount(repo.wt, segs.slice(i, n).join("/"))) hit = n;
    }
    if (hit < 0) break;
    const sub = segs.slice(i, hit).join("/");
    //  BE-026: confine the descended segment before be.find (NAVESCAPE → stop).
    let subWt;
    try { subWt = wtpath(repo.wt, sub); } catch (e) { break; }
    let subRepo;
    try { subRepo = be.find(subWt); } catch (e) { break; }
    repo = subRepo; prefix = joinPrefix(prefix, sub); i = hit;
  }
  return { repo: repo, rest: segs.slice(i).join("/"), prefix: prefix };
}

module.exports = {
  walk: walk,
  isMount: isMount,
  gitmodulesOrder: gitmodulesOrder,
  joinPrefix: joinPrefix,
  resolveRepoForPath: resolveRepoForPath,
};
