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
const join = pathlib.join, dirname = pathlib.dirname, safeRel = pathlib.safeRel;
//  BE-030: every worktree fs path is computed THROUGH resolve() (discover.wtpath),
//  so a checkout write/read/unlink honours the nav CONTEXT + stays confined.
const wtpath = require("../core/discover.js").wtpath;

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
const { readFileBytes } = require("./wtread.js");
function readFile(full, size) { return readFileBytes(full, size); }  // CODE-020: shared wt read

//  GET-056b: the one binary predicate (git's NUL-in-first-8000 heuristic),
//  the shared 3-blob weave merge, and the conflict-marker probe — the sub
//  checkout merges dirty text through the SAME code as the flat D5 path.
const isBinary = require("../views/diff/diff.js").isBinary;
const weavelib = require("./weave.js");
const conflict = require("./conflict.js");
const ulog = require("./ulog.js");

function bytesEq(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

//  GET-039: clear whatever occupies `full` so a leaf can be written in its
//  place across a TYPE-CHANGE.  A stale DIR (dir->file / dir->link) is rm -rf'd
//  via io.rmdir (io.unlink throws EISDIR on any directory); a stale file/symlink
//  is unlinked so io.open won't FOLLOW a pre-existing link to a dangling target.
function clearPath(full) {
  let ls; try { ls = io.lstat(full); } catch (e) { return; }   // nothing there
  if (ls.kind === "dir") { try { io.rmdir(full, true); } catch (e) {} }
  else { try { io.unlink(full); } catch (e) {} }
}

//  Write `bytes` to `full`, creating parent dirs.  Overwrites in place.
function writeFile(wtRoot, rel, bytes) {
  //  BE-011: leaf-local worktree confinement (defense-in-depth) — a `..`/reserved
  //  rel from a direct caller must refuse here, not just in materialise.
  if (!safeRel(rel)) throw "checkout: unsafe path " + rel;
  const full = wtpath(wtRoot, rel);
  const d = dirname(rel);            // "." for a top-level rel → no parent
  if (d && d !== ".") { try { io.mkdir(wtpath(wtRoot, d)); } catch (e) {} }
  clearPath(full);                                  // drop a stale dir/file/link
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
  //  BE-011: leaf-local worktree confinement (defense-in-depth), as writeFile.
  if (!safeRel(rel)) throw "checkout: unsafe path " + rel;
  const full = wtpath(wtRoot, rel);
  const d = dirname(rel);
  if (d && d !== ".") { try { io.mkdir(wtpath(wtRoot, d)); } catch (e) {} }
  clearPath(full);                                  // drop a stale dir/file/link
  io.symlink(target, full);
}

//  Materialise one tree leaf into the wt: a symlink → io.symlink(target),
//  a regular/exec file → write bytes, then chmod 0755 for an exec ("x")
//  blob.  Mirrors sniff/GET.c get_write_one (FILESymLink / FILEChmod 0755).
function materialise(wtRoot, rel, leaf, bytes) {
  if (!safeRel(rel)) throw "checkout: unsafe path " + rel;   // JS-065 guard
  if (leaf.kind === "l") {
    writeSymlink(wtRoot, rel, utf8.Decode(bytes));   // blob bytes = link target
    return;
  }
  writeFile(wtRoot, rel, bytes);
  if (leaf.kind === "x") {
    try { io.chmod(wtpath(wtRoot, rel), 0o755); } catch (e) {}
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

//  GET-040: blob bytes for a tree sha (null when absent) — the old-baseline
//  compare input for the non-force clean-vs-dirty decision.
function blobOf(keeper, sha) {
  if (!sha) return null;
  const o = keeper.getObject(sha);
  return o ? o.bytes : null;
}

//  GET-040: read a wt regular file's bytes (null if absent/not-regular).
function readOnDisk(full) {
  let ls; try { ls = io.lstat(full); } catch (e) { return null; }
  if (ls.kind !== "reg") return null;
  return readFile(full, ls.size);
}

//  apply(keeper, tipSha, wtRoot, opts?): materialise `tipSha`'s tree into wtRoot.
//  GET-040: FORCE (`get!`) clean-resets — every not-in-tree path (untracked
//  included) is swept.  NON-force merges/leaves: it (re)writes only MISSING or
//  CLEAN tracked files, PRESERVES a dirty local edit, and unlinks ONLY a TRACKED
//  deletion (a path in the OLD baseline tree, gone from the new) — an UNTRACKED
//  path (in neither tree) is never touched.  `opts.oldTip` is the sub's prior
//  pin (the baseline); absent oldTip = a fresh checkout (materialise all).
//  PUT-012: best-effort restamp of a just-materialised file (get.js trySetMtime
//  twin) — the sub-mount checkout stamps files to the track-row ts (GET-049).
function trySetMtime(full, ts) { try { io.setMtime(full, BigInt(ts)); } catch (e) {} }

function apply(keeper, tipSha, wtRoot, opts) {
  const force = !!(opts && opts.force);
  const oldTip = (opts && opts.oldTip) || "";
  //  GET-056b: the wt's OWN wtlog path (the sub anchor) — durable `con` rows +
  //  the POST-032 unresolved-conflict probe; absent → conflicts report-only.
  const bePath = (opts && opts.bePath) || "";
  let _cons = null;
  function conSet(rel) {
    if (!bePath) return false;
    if (!_cons) {
      try { _cons = require("./wtlog.js").open({ bePath: bePath }).conPaths(); }
      catch (e) { _cons = new Set(); }
    }
    return _cons.has(rel);
  }
  function conRow(rel) {
    if (!bePath) return;
    try { ulog.append(bePath, [{ verb: "con",
      uri: URI.make(undefined, undefined, rel) }]); } catch (e) {}
  }
  //  PUT-012: sub-mount hands the track-row ASSIGNED ts; restamp every file we
  //  materialise to it so status/put/diff agree (symlinks: setMtime follows link).
  const stampTs = (opts && opts.stampTs != null) ? opts.stampTs : null;
  const treeSha = keeper.commitTree(tipSha);
  if (!treeSha) throw "checkout: tip " + tipSha + " has no tree";

  const before = scanWt(wtRoot);
  const rows = [];
  const inTree = {};

  //  GET-040: the OLD baseline tree (path -> sha) so a non-force pass can tell a
  //  CLEAN file (== old blob → safe to update) from a DIRTY edit (preserve) and
  //  unlink only a TRACKED deletion.  Force clean-resets, so it needs no baseline.
  const oldMap = {};
  if (!force && oldTip) {
    const ot = keeper.commitTree(oldTip);
    if (ot) keeper.readTreeRecursive(ot, function (l) {
      if (l.kind !== "s") oldMap[l.path] = l.sha; });
  }

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
    const full = wtpath(wtRoot, rel);
    const existed = !!before[rel];
    if (existed && leafUnchanged(full, leaf, bytes)) return;  // no row
    //  GET-040: NON-force never clobbers a DIRTY tracked file; only a CLEAN
    //  file (== old blob) or a MISSING one is (re)materialised.  Fresh checkout
    //  (no oldTip) materialises all.  GET-056b RULING (the flat D5 matrix, per
    //  mode-blind bytes): clean → theirs; dirty TEXT → weave3; dirty BINARY → ours.
    if (!force && oldTip && existed && (leaf.kind === "f" || leaf.kind === "x")) {
      const onDisk = readOnDisk(full);
      const oldBytes = blobOf(keeper, oldMap[rel]);
      const dirty = !(oldBytes != null && bytesEq(onDisk, oldBytes));
      if (dirty) {
        if (onDisk == null || oldBytes == null ||
            isBinary(oldBytes) || isBinary(onDisk) || isBinary(bytes)) {
          rows.push({ verb: "mrg", path: rel }); return;   // dirty binary/unreadable → ours
        }
        //  POST-032 twin: a still-unresolved prior conflict stays as-is.
        if (conflict.hasConflictMarker(onDisk) && conSet(rel)) {
          rows.push({ verb: "con", path: rel }); return;
        }
        //  dirty TEXT → 3-way weave, the same weave3 as get.js's flat D5 leaf.
        let merged;
        try { merged = weavelib.weave3(oldBytes, onDisk, bytes, weavelib.extOf(rel)); }
        catch (e) { if (!("" + e).includes("full")) throw e; merged = null; }
        if (merged == null) {                    // unweavable → keep ours, conflict
          rows.push({ verb: "con", path: rel }); conRow(rel); return;
        }
        materialise(wtRoot, rel, leaf, merged);
        if (conflict.hasConflictMarker(merged)) {
          rows.push({ verb: "con", path: rel }); conRow(rel); return;
        }
        //  the edit reproduced the target exactly → clean, stamp to the row ts.
        if (bytesEq(merged, bytes) && stampTs != null) trySetMtime(full, stampTs);
        rows.push({ verb: "mrg", path: rel }); return;
      }
      //  clean → fall through, (re)materialise theirs.
    }
    materialise(wtRoot, rel, leaf, bytes);
    //  PUT-012: stamp the materialised file to the track-row ts (get.js:1085
    //  parity); a symlink stays unstamped (setMtime would follow the link).
    if (stampTs != null && leaf.kind !== "l") trySetMtime(full, stampTs);
    rows.push({ verb: existed ? "upd" : "new", path: rel });
  });

  //  Delete wt files absent from the new tree (del rows).  Prune now-empty
  //  dirs is best-effort (io has no rmdir leaf; leave empty dirs).
  for (const rel in before) {
    if (inTree[rel]) continue;
    //  GET-040: a NON-force checkout unlinks ONLY a TRACKED deletion (present in
    //  the OLD tree, gone from the new); an UNTRACKED path (in neither) is LEFT.
    //  `get!` (force) is the sole clean-reset that may remove untracked/dirty.
    if (!force && !oldMap[rel]) continue;
    try { io.unlink(wtpath(wtRoot, rel)); rows.push({ verb: "del", path: rel }); }
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
