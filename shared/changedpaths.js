//  changedpaths.js — GIT-016: the shared changed-PATHS tree diff HEAD reports
//  ("+ changed paths", deferred in T4).  cur's tree vs a target tip's tree →
//  the sorted list of leaf paths that were added / removed / content-changed.
//  REUSES store.readTreeRecursive (the SAME leaf walk views/diff/diff.js uses)
//  over a parameterised "tree reader", so it serves BOTH the local `?branch`
//  peek (both sides in the keeper) AND the remote fetch peek (the tip side in a
//  transient in-memory pack, unpersisted).  Report-only: it lists paths, never
//  renders content (that is views/diff/diff.js's job) — no blob inflate here.

"use strict";

const shalib = require("./util/sha.js");
const ingest = require("./ingest.js");
const isFullSha = shalib.isFullSha;
const frameSha  = shalib.frameSha;

//  GIT-016: leaf path -> sha map of a tree (files + gitlinks), via the reader's
//  own readTreeRecursive (store.js / packReader below).  The SAME per-leaf walk
//  diff.js's treeMap uses; here we keep ONE flat path->sha map (paths compare).
function treeLeaves(reader, treeSha) {
  const m = {};
  if (!reader || !treeSha || !isFullSha(treeSha)) return m;
  try { reader.readTreeRecursive(treeSha, function (l) { m[l.path] = l.sha; }); }
  catch (e) { /* a missing/short tree yields the leaves it could reach */ }
  return m;
}

//  GIT-016: changed leaf paths between two trees — added, removed, or a
//  different leaf sha at the same path.  Sorted lex (the diff.js iteration key).
//  `fromReader`/`toReader` each expose readTreeRecursive (may be the SAME
//  keeper, or keeper + a transient packReader for the remote tip).
function changedTrees(fromReader, fromTreeSha, toReader, toTreeSha) {
  const F = treeLeaves(fromReader, fromTreeSha);
  const T = treeLeaves(toReader, toTreeSha);
  const seen = {}, out = [];
  for (const p in T) if (F[p] !== T[p]) { seen[p] = 1; out.push(p); }   // add/mod
  for (const p in F) if (!(p in T) && !seen[p]) out.push(p);            // removed
  out.sort();
  return out;
}

//  GIT-016: the convenience twin over COMMIT shas — resolve each side's tree
//  (commitTree) on its own reader, then diff.  Used by head for cur (keeper)
//  vs a tip (keeper for a local `?branch`, a packReader for a fetch).
function changedCommits(fromReader, fromCommit, toReader, toCommit) {
  const ft = (fromReader && isFullSha(fromCommit)) ? fromReader.commitTree(fromCommit) : "";
  const tt = (toReader   && isFullSha(toCommit))   ? toReader.commitTree(toCommit)     : "";
  return changedTrees(fromReader, ft || "", toReader, tt || "");
}

//  BRO-044: the OBJECT-level twin of changedTrees.  changedTrees answers "which
//  PATHS differ" and must therefore read BOTH trees WHOLE; the mtime lane needs
//  "which OBJECTS does `to` carry that `from` does not", which is answered by a
//  PRUNING descent: an entry whose sha is unchanged is skipped, so an unchanged
//  subtree is never opened at all.  Emits blobs AND the tree objects of the dirs
//  that changed (the dir rows are what let a `list:` answer a whole directory
//  entry from one row).  `type` is the git pack type number — 2 tree, 3 blob,
//  1 a gitlink (a commit sha pinned in the tree).  Root-relative `path`; the
//  ROOT tree itself is emitted with path "" when the two roots differ.
function changedObjects(fromReader, fromTree, toReader, toTree, cb) {
  if (!toTree || !isFullSha(toTree)) return;
  if (fromTree === toTree) return;
  cb({ path: "", sha: toTree, type: 2 });
  (function walk(fSha, tSha, prefix) {
    const F = {};
    if (fSha && isFullSha(fSha)) {
      let fents;
      try { fents = fromReader.readTree(fSha); } catch (e) { fents = undefined; }
      for (const e of (fents || [])) F[e.name] = e;
    }
    let tents;
    try { tents = toReader.readTree(tSha); } catch (e) { tents = undefined; }
    for (const e of (tents || [])) {
      const prev = F[e.name];
      if (prev && prev.sha === e.sha) continue;          // unchanged: PRUNE
      const path = prefix ? (prefix + "/" + e.name) : e.name;
      if (e.mode === 0o40000) {
        cb({ path: path, sha: e.sha, type: 2 });
        walk(prev && prev.mode === 0o40000 ? prev.sha : "", e.sha, path);
        continue;
      }
      cb({ path: path, sha: e.sha, type: e.mode === 0o160000 ? 1 : 3 });
    }
  })(fromTree || "", toTree, "");
}

//  BRO-044: the changedObjects twin over COMMIT shas (changedCommits mirror).
function changedObjectsCommits(fromReader, fromCommit, toReader, toCommit, cb) {
  const ft = (fromReader && isFullSha(fromCommit)) ? fromReader.commitTree(fromCommit) : "";
  const tt = (toReader   && isFullSha(toCommit))   ? toReader.commitTree(toCommit)     : "";
  changedObjects(fromReader, ft || "", toReader, tt || "", cb);
}

//  GIT-016: a TRANSIENT read-only object reader over an in-memory git pack — the
//  remote tip's trees/blobs the head fetch pulled but does NOT persist.  Reuses
//  git.pack.over + git.tree + git.parseCommit + frameSha (the SAME primitives
//  store.js reads on-disk packs with); a one-shot sha->offset scan indexes every
//  record so commitTree/readTree(Recursive) resolve without touching the keeper.
function packReader(packBytes) {
  const log = ingest.packLogBytes(packBytes);                   // strip trailer
  const pk = git.pack.over(log);
  pk.buffer.watermark = log.byteLength;
  //  One scan: full-sha -> offset for every record (all types, not just commit).
  const byOff = [];
  pk.rewind();
  while (pk.next()) byOff.push(pk.offset);
  const idx = {};
  for (const off of byOff) {
    let rec = resolveAt(pk, off);
    if (rec) idx[frameSha(rec.type, rec.bytes)] = off;
  }
  function resolveAt(p, off) {
    try {
      p.seek(off);
      const out = io.buf((p.size || 0) * 4 + 256);
      p.seek(off); p.resolve(out);
      return { type: p.type, bytes: out.data().slice() };
    } catch (e) { return undefined; }
  }
  const reader = {
    getObject: function (sha) {
      const off = idx[sha];
      if (off === undefined) return undefined;
      const rec = resolveAt(pk, off);
      return rec ? { type: rec.type, bytes: rec.bytes } : undefined;
    },
    commitTree: function (sha) {
      const o = reader.getObject(sha);
      if (!o || o.type !== "commit") return undefined;
      return git.parseCommit(o.bytes).tree;
    },
    readTree: function (sha) {
      const o = reader.getObject(sha);
      if (!o || o.type !== "tree") return undefined;
      const out = [];
      git.tree(o.bytes, function (e) { out.push({ mode: e.mode, name: e.str, sha: e.sha }); });
      return out;
    },
    //  Same leaf shape store.readTreeRecursive yields (path/mode/sha/kind), so
    //  treeLeaves treats a pack tree and a keeper tree identically.
    readTreeRecursive: function (treeSha, cb) {
      (function walk(sha, prefix) {
        const ents = reader.readTree(sha);
        if (!ents) return;
        for (const e of ents) {
          const path = prefix ? (prefix + "/" + e.name) : e.name;
          if (e.mode === 0o40000) { walk(e.sha, path); continue; }        // dir
          const kind = e.mode === 0o160000 ? "s" : e.mode === 0o120000 ? "l"
                     : e.mode === 0o100755 ? "x" : "f";
          cb({ path: path, mode: e.mode, sha: e.sha, kind: kind });
        }
      })(treeSha, "");
    }
  };
  return reader;
}

module.exports = { changedTrees: changedTrees, changedCommits: changedCommits,
                   changedObjects: changedObjects,
                   changedObjectsCommits: changedObjectsCommits,
                   packReader: packReader };
