//  checkout.js — materialise a baseline tree into the worktree (JS-041).
//  Pure JS over keeper.js (readTreeRecursive → object bytes) + io fs leaves
//  (mkdir/open/write/unlink).  Diffs the new tree against the current wt and
//  applies new/mod/del for files AND dirs, returning the change rows the
//  `be get` summary renders.  Mirrors the keeper checkout / SNIFF apply path.
//
//  apply(keeper, tipSha, wtRoot, opts?) -> { rows:[{verb,path}], tip }
//    verb: "new" (created) | "mod" (overwritten, bytes changed) | "del"
//          (removed: in the wt, gone from the new tree).  Unchanged files
//          earn no row (native get only reports what moved).
//
//  Symlink ("l") + exec-bit ("x") leaves ARE materialised (JS-044): a
//  symlink via io.symlink(target, path) (the blob bytes are the link
//  target), an exec blob via io.chmod(path, 0o755) — mirroring
//  sniff/GET.c::get_write_one (FILESymLink / FILEChmod 0755).  Gitlink
//  (submodule) leaves are still recorded-only (sub mount is status.js's
//  recursion concern).

"use strict";

const pathlib = require("./util/path.js");   // JSQUE-016: path.js -> shared/util/
const join = pathlib.join, dirname = pathlib.dirname;

function statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }

//  Recursively list the current wt's tracked-ish files (rel paths), skipping
//  the `.be`/`.git` meta + the `..be.idx` sidecar.  Used to compute del rows
//  + new-vs-mod.  (A re-get over a dirty wt is native's concern; v1 lists all
//  present files and lets the tree decide.)
function scanWt(wtRoot) {
  const out = {};
  let names;
  try { names = io.readdir(wtRoot, { recursive: true, hidden: true }); }
  catch (e) { return out; }
  for (const nm of names) {
    if (nm[nm.length - 1] === "/") continue;       // dirs
    if (nm === ".be" || nm === "..be.idx") continue;
    if (nm.indexOf(".be/") === 0 || nm.indexOf(".git/") === 0) continue;
    out[nm] = 1;
  }
  return out;
}

//  Read a wt file's bytes (for the mod-vs-unchanged content compare); null
//  on any error.  open/readAll/close — NOT io.mmap (mmap leaks, classify.js
//  JS-031 finding).
function readFile(full, size) {
  let fd;
  try { fd = io.open(full, "r"); } catch (e) { return null; }
  try {
    const b = io.buf((size || 0) + 16);
    io.readAll(fd, b, size);
    const d = b.data().slice();
    io.close(fd);
    return d;
  } catch (e) { try { io.close(fd); } catch (e2) {} return null; }
}

function bytesEq(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

//  Write `bytes` to `full`, creating parent dirs.  Overwrites in place.
function writeFile(wtRoot, rel, bytes) {
  const full = join(wtRoot, rel);
  const d = dirname(rel);            // "." for a top-level rel → no parent
  if (d && d !== ".") { try { io.mkdir(join(wtRoot, d)); } catch (e) {} }
  let fd;
  fd = io.open(full, "c");                          // create/truncate
  try {
    try { io.resize(fd, 0); } catch (e) {}          // truncate if pre-existing
    const b = io.buf(bytes.length + 8);
    b.feed(bytes);
    io.writeAll(fd, b);
  } finally { io.close(fd); }
}

//  Create a symlink at `rel` pointing at `target` (the blob bytes).  Unlink
//  first so a stale file/symlink at the path is REPLACED outright — io.symlink
//  throws if linkpath exists (FILESymLink) and a pre-existing target would
//  otherwise be clobbered.  Mirrors sniff/GET.c get_write_one's LNK branch.
function writeSymlink(wtRoot, rel, target) {
  const full = join(wtRoot, rel);
  const d = dirname(rel);
  if (d && d !== ".") { try { io.mkdir(join(wtRoot, d)); } catch (e) {} }
  try { io.unlink(full); } catch (e) {}
  io.symlink(target, full);
}

//  Materialise one tree leaf into the wt: a symlink → io.symlink(target),
//  a regular/exec file → write bytes, then chmod 0755 for an exec ("x")
//  blob.  Mirrors sniff/GET.c get_write_one (FILESymLink / FILEChmod 0755).
function materialise(wtRoot, rel, leaf, bytes) {
  if (leaf.kind === "l") {
    writeSymlink(wtRoot, rel, utf8.Decode(bytes));   // blob bytes = link target
    return;
  }
  writeFile(wtRoot, rel, bytes);
  if (leaf.kind === "x") {
    try { io.chmod(join(wtRoot, rel), 0o755); } catch (e) {}
  }
}

//  YES iff the on-disk path already matches the leaf (kind + content/target
//  + exec bit), so the checkout can skip the write and emit no row.  Symlink:
//  lstat kind + readlink target equality (no follow).  Regular/exec: byte
//  equality AND the exec bit matching the leaf's kind (a 100644→100755 flip
//  must still re-chmod and earn a row).
function leafUnchanged(full, leaf, bytes) {
  let ls; try { ls = io.lstat(full); } catch (e) { return false; }
  if (leaf.kind === "l") {
    if (ls.kind !== "lnk") return false;
    let tgt; try { tgt = io.readlink(full); } catch (e) { return false; }
    return tgt === utf8.Decode(bytes);
  }
  if (ls.kind !== "reg") return false;               // a symlink→file flip, etc.
  const cur = readFile(full, ls.size);
  if (!bytesEq(cur, bytes)) return false;
  const isExec = !!(ls.mode && (ls.mode & 0o111));
  return isExec === (leaf.kind === "x");
}

function apply(keeper, tipSha, wtRoot, opts) {
  const treeSha = keeper.commitTree(tipSha);
  if (!treeSha) throw "checkout: tip " + tipSha + " has no tree";

  const before = scanWt(wtRoot);
  const rows = [];
  const inTree = {};

  //  Walk the new tree's leaves; create/overwrite, classify new vs mod.
  keeper.readTreeRecursive(treeSha, function (leaf) {
    const rel = leaf.path;
    inTree[rel] = 1;
    //  gitlink (submodule) — recorded, not materialised here (recursion is
    //  a follow-up like status.js's sub handling).
    if (leaf.kind === "s") { return; }
    //  The leaf's object bytes are the file content OR — for a symlink —
    //  the link target verbatim (git stores a symlink as a blob of the path).
    const obj = keeper.getObject(leaf.sha);
    const bytes = obj ? obj.bytes : new Uint8Array(0);
    const full = join(wtRoot, rel);
    const existed = !!before[rel];
    if (existed && leafUnchanged(full, leaf, bytes)) return;  // no row
    materialise(wtRoot, rel, leaf, bytes);
    rows.push({ verb: existed ? "upd" : "new", path: rel });
  });

  //  Delete wt files absent from the new tree (del rows).  Prune now-empty
  //  dirs is best-effort (io has no rmdir leaf; leave empty dirs).
  for (const rel in before) {
    if (inTree[rel]) continue;
    try { io.unlink(join(wtRoot, rel)); rows.push({ verb: "del", path: rel }); }
    catch (e) {}
  }

  //  Lex order by path within the stable new/mod/del grouping the summary
  //  renders (caller sorts as native does).
  return { rows, tip: tipSha, tree: treeSha };
}

//  materialise/writeFile/writeSymlink are exported for bin/patch.js (JS-052)
//  so the patch verb writes merged blobs through the SAME write_blob path.
//  leafUnchanged is exported for get.js (JSQUE-009): the loop-handler leaf
//  reuses the SAME skip-if-already-matches predicate as the one-shot checkout.
module.exports = { apply, scanWt, materialise, writeFile, writeSymlink,
                   leafUnchanged };
