//  stage.js — the shared staging engine behind `bin/put.js` (JS-049) and
//  `bin/delete.js` (JS-050).  Pure JS over store.js (baseline-tree read),
//  classify.js (wtScan / wtEqBase), wtlog.js (stamp-set + put/del scan),
//  ignore.js (.gitignore + `.be`/`.git` meta), sha.js (content hash) and the
//  io leaves (lstat / readlink / rename).  No C, no dog, no keeper binding.
//  Mirrors sniff/PUT.c (put_visit_tracked / put_classify_step / dir_collect
//  / put_detect_moves / put_move) — it computes WHAT to stage; the caller
//  (put.js) appends the `put` rows via ulog.append and restamps via
//  io.setMtime, so the row-write + restamp policy lives in one place.
//
//  prep(be, wtlogReader, storeReader) → engine.  The engine exposes:
//    bareWalk()            → [op]   bare `be put`: auto-pair moves first
//                                   (PUTAMBIG throws), then tracked-dirty
//                                   files in baseline-tree lex order
//    classifyNamed(raw)    → { stage, reason }   file-form decision
//    expandDir(prefix)     → { ops, sawTracked }  dir-form per-file expansion
//    explicitMove(src,dst) → op     renames on disk + the `<src>#<dst>` row
//  where an `op` is { path, dst?, kind, restamp }:
//    path     the `put` row's path slot (the URI's path; for a move it is
//             the SOURCE path).
//    dst      (move only) the `put` row's fragment slot — the DEST path.
//    restamp  the on-disk path whose mtime the caller stamps to the row ts
//             (the staged file, or the move DEST), or null for a row that
//             needs no restamp.
//  The caller assigns each op a strictly-increasing ts, writes one `put`
//  row (path + optional fragment) and restamps `op.restamp` to that ts.

"use strict";

const pathlib = require("./path.js");
const shalib = require("./sha.js");
const classify = require("./classify.js");
const ignorelib = require("./ignore.js");
const join = pathlib.join;
const basename = pathlib.basename;
const isFullSha = shalib.isFullSha;
const frameSha = shalib.frameSha;
const wtScan = classify.wtScan;
const wtEqBase = classify.wtEqBase;

//  PUT auto-pair errors (sniff/PUT.h).  Thrown (not returned) so a non-1:1
//  move pairing aborts the whole bare put, matching native PUTAMBIG.
const PUTAMBIG = "PUTAMBIG";
const PUTNOSRC = "PUTNOSRC";
const PUTDSTBAD = "PUTDSTBAD";
const PUTNODIR = "PUTNODIR";
const PUTMVMETA = "PUTMVMETA";

//  `.git` / `.be` / `..be.idx` path segments are always meta (mirror
//  sniff/SNIFF.c::SNIFFSkipMeta → ignore.js::isMeta).  Never staged.
function isMeta(rel) {
  if (!rel) return true;
  for (const seg of rel.split("/"))
    if (seg === ".git" || seg === ".be" || seg === "..be.idx") return true;
  return false;
}

//  Git-blob sha of the on-disk path at `rel` (symlink → hash of its target,
//  CLASS.c::CLASSWtEqBase).  undefined when unreadable / not a leaf kind.
function diskSha(wtRoot, rel) {
  const full = join(wtRoot, rel);
  let st;
  try { st = io.lstat(full); } catch (e) { return undefined; }
  let content;
  if (st.kind === "lnk") {
    let tgt;
    try { tgt = io.readlink(full); } catch (e) { return undefined; }
    content = utf8.Encode(tgt);
  } else if (st.kind === "reg") {
    if (st.size === 0) content = new Uint8Array(0);
    else {
      let fd;
      try { fd = io.open(full, "r"); } catch (e) { return undefined; }
      try {
        const b = io.buf(st.size + 16);
        io.readAll(fd, b, st.size);
        content = b.data();
      } catch (e) { try { io.close(fd); } catch (e2) {} return undefined; }
      try { io.close(fd); } catch (e) {}
    }
  } else return undefined;
  return frameSha("blob", content);
}

//  prep(): bind the engine to one repo.  Loads the baseline tree leaves, the
//  wt scan, and the get/post stamp gate ONCE; every method below reads them.
function prep(be, wtlogReader, storeReader) {
  const wtRoot = be.wt;
  const ignore = ignorelib.load(wtRoot);

  //  baseline tree leaves: rel → { sha, kind } (kind f/x/l/s).
  const base = {};
  let haveBase = false, baseTreeSha;
  const baseTip = wtlogReader.baselineTip();
  if (baseTip && baseTip.sha && isFullSha(baseTip.sha)) {
    const treeSha = storeReader.commitTree(baseTip.sha);
    if (treeSha) {
      baseTreeSha = treeSha;
      storeReader.readTreeRecursive(treeSha, function (leaf) {
        base[leaf.path] = { sha: leaf.sha, kind: leaf.kind };
      });
      haveBase = true;
    }
  }

  //  baseline-tree leaf order (readTreeRecursive yields native WALK order:
  //  depth-first, dir entries inlined at their git-tree position).  Kept as
  //  an array so the bare walk emits rows in the SAME order native does.
  const baseOrder = [];
  if (haveBase)
    storeReader.readTreeRecursive(baseTreeSha, function (leaf) {
      baseOrder.push(leaf.path);
    });

  //  baseline get/post ts — the re-stamp target for a content-equal file
  //  (put_visit_tracked: a baseline-equal file restamps to baseline_ts so
  //  the next bare put fast-paths it).  null on a fresh repo.
  const baselineTs = (baseTip && baseTip.ts != null) ? baseTip.ts : null;

  //  get/post stamp gate: ron(ts) → true iff a get/post row owns that ts.
  //  Native's "is unchanged" / clean fast-path keys on get/post stamps only
  //  (put/patch stamps fall through to the content hash).  Built from the
  //  wtlog rows (wtlogReader.rows).
  const gpStamp = {};
  for (const r of wtlogReader.rows)
    if (r.verb === "get" || r.verb === "post") gpStamp[r.ron] = true;
  function isGpStamp(tsRon) { return !!gpStamp[tsRon]; }

  const wt = wtScan(wtRoot, ignore);   // rel → { ts, kind, full }

  const engine = {
    be: be, wtRoot: wtRoot, base: base, wt: wt,
    haveBase: haveBase, baseTreeSha: baseTreeSha, baselineTs: baselineTs,

    //  --- bare `be put`: auto-pair moves, then tracked-dirty walk --------
    //  Returns the ordered op list (moves first, then tracked-dirty files
    //  in baseline lex order).  Throws PUTAMBIG when a move pairing is not
    //  strictly 1:1.  A content-equal tracked file produces a RESTAMP-only
    //  op (path null) so the caller restamps it to baselineTs without a row.
    bareWalk: function () {
      const ops = [];
      const moves = this.detectMoves();        // may throw PUTAMBIG
      for (const m of moves) ops.push(m);

      //  Tracked-dirty walk over the baseline tree, lex/WALK order.
      for (const rel of baseOrder) {
        const b = base[rel];
        if (!b || b.kind === "s") continue;     // gitlinks: nothing to put
        if (isMeta(rel)) continue;
        const w = wt[rel];
        if (!w) continue;                        // vanished on disk → skip
        //  Fast path: mtime ∈ get/post stamp → clean (user untouched).
        if (isGpStamp(w.ts != null ? ron.encode(w.ts) : "")) continue;
        //  Content compare to the baseline blob.
        if (wtEqBase(wtRoot, rel, b.sha)) {
          //  Equal → restamp to baselineTs, no row (suppresses next walk).
          if (baselineTs != null)
            ops.push({ path: null, kind: "restamp", restamp: rel,
                       stampTs: baselineTs });
          continue;
        }
        ops.push({ path: rel, kind: "put", restamp: rel });
      }
      return ops;
    },

    //  --- auto-pair system-`mv` renames (put_detect_moves) ---------------
    //  A tracked path missing on disk + an untracked path of identical
    //  content sha → one `put <old>#<new>` op per 1:1 pair.  Non-1:1
    //  (a sha matching >1 candidate either way) throws PUTAMBIG.
    detectMoves: function () {
      //  base-only (tracked, gone from disk) candidates, by baseline sha.
      const baseCand = [];     // { path, sha }
      for (const rel of baseOrder) {
        const b = base[rel];
        if (!b || b.kind === "s") continue;
        if (isMeta(rel)) continue;
        if (wt[rel]) continue;                   // still on disk → not moved
        if (!isFullSha(b.sha)) continue;
        baseCand.push({ path: rel, sha: b.sha });
      }
      //  wt-only (on disk, not tracked) candidates, by content sha.
      const wtCand = [];       // { path, sha }
      for (const rel in wt) {
        if (base[rel]) continue;                 // tracked → not a move dest
        if (isMeta(rel)) continue;
        const s = diskSha(wtRoot, rel);
        if (!s) continue;
        wtCand.push({ path: rel, sha: s });
      }
      if (!baseCand.length || !wtCand.length) return [];

      const pairs = [];
      for (const bc of baseCand) {
        let match = -1, nmatch = 0;
        for (let j = 0; j < wtCand.length; j++)
          if (wtCand[j].sha === bc.sha) { match = j; nmatch++; }
        if (nmatch === 0) continue;
        if (nmatch > 1) throw PUTAMBIG;
        //  Symmetry: the wt dest must match only this base source.
        let baseMatches = 0;
        for (const bc2 of baseCand) if (bc2.sha === wtCand[match].sha) baseMatches++;
        if (baseMatches > 1) throw PUTAMBIG;
        pairs.push({ path: bc.path, dst: wtCand[match].path });
      }
      //  `silent`: auto-paired moves get a wtlog row + restamp but are NOT
      //  rendered in the `put:` banner (put_detect_moves emits no HUNK row,
      //  unlike put_move / the dir + file forms).
      return pairs.map(function (p) {
        return { path: p.path, dst: p.dst, kind: "mov", restamp: p.dst,
                 silent: true };
      });
    },

    //  --- named file-form (`be put <file>`) decision (put_classify_step) -
    //    BASE_ONLY (tracked, gone)            → skip "does not exist"
    //    WT_ONLY   (on disk, untracked)       → stage
    //    BOTH      + mtime ∈ get/post stamp   → skip "is unchanged"
    //    BOTH      + otherwise                → stage
    //  Returns { stage, reason }.  `reason` set only when stage === false.
    classifyNamed: function (raw) {
      const b = base[raw], w = wt[raw];
      if (!w && b) return { stage: false, reason: "does not exist" };
      if (!w && !b) {
        //  Unseen-but-present (a special file the wt scan skipped) reads
        //  "exists but is not stageable"; truly absent reads "does not
        //  exist" (DIS-034).
        let onDisk = false;
        try { io.lstat(join(wtRoot, raw)); onDisk = true; } catch (e) {}
        return { stage: false,
                 reason: onDisk ? "exists but is not stageable" : "does not exist" };
      }
      //  on disk (WT_ONLY or BOTH).
      if (b && w && isGpStamp(w.ts != null ? ron.encode(w.ts) : ""))
        return { stage: false, reason: "is unchanged" };
      return { stage: true };
    },

    //  --- dir-form (`be put <dir>/`) expansion (dir_collect_step) --------
    //  `prefix` ends in `/` (or "" for the reporoot).  Per-file rule:
    //    BOTH    + mtime ∈ ANY stamp-set      → settled, skip
    //    BOTH    + otherwise                  → stage (tracked-and-dirty)
    //    WT_ONLY                              → stage (untracked sibling)
    //    BASE_ONLY                            → skip (gone; delete's job)
    //  Idempotence keys on the FULL stamp-set (get/post/put), so a second
    //  `be put dir/` emits nothing.  `sawTracked` = any baseline entry under
    //  the prefix (distinguishes "unchanged" from "no files to stage").
    //  Ops are emitted in lex order (the merge order).
    expandDir: function (prefix) {
      const ops = [];
      let sawTracked = false;
      //  Union of base + wt paths under the prefix, lex sorted.
      const keys = {};
      for (const k in base) if (under(k, prefix)) keys[k] = 1;
      for (const k in wt) if (under(k, prefix)) keys[k] = 1;
      const paths = Object.keys(keys).sort();
      for (const rel of paths) {
        if (isMeta(rel)) continue;
        const b = base[rel], w = wt[rel];
        if (b && (b.kind === "s")) continue;     // gitlink subtree
        if (b) sawTracked = true;                // tracked dir
        let doStage = false;
        if (b && w) {
          //  any stamp-set membership counts as settled (idempotence).
          const settled = w.ts != null && wtlogReader.has(w.ts);
          if (!settled) doStage = true;
        } else if (w && !b) {
          doStage = true;
        }
        if (doStage) ops.push({ path: rel, kind: "put", restamp: rel });
      }
      return { ops: ops, sawTracked: sawTracked };
    },

    //  --- explicit move (`be put <old>#<new>`) (put_move) ----------------
    //  Two accepted shapes: rename-in-flight (src on disk, dst free → we
    //  rename(2)) and claim (src gone, dst already carries the bytes → just
    //  the row).  Trailing-slash dst → directory target (append basename).
    //  Performs the on-disk io.rename when needed; returns the move op.
    explicitMove: function (srcRaw, dstInRaw) {
      if (!srcRaw || !dstInRaw) throw PUTNOSRC;
      if (isMeta(srcRaw) || isMeta(dstInRaw)) throw PUTMVMETA;

      //  Resolve the final dst (dir target appends basename(src)).
      let dst = dstInRaw;
      if (dst[dst.length - 1] === "/") {
        const sb = basename(srcRaw);
        if (!sb) throw PUTDSTBAD;
        dst = dstInRaw + sb;
      }
      const srcFull = join(wtRoot, srcRaw);
      const dstFull = join(wtRoot, dst);
      const srcHere = statExists(srcFull);
      const dstHere = statExists(dstFull);

      if (srcHere && dstHere) throw PUTDSTBAD;
      if (!srcHere && !dstHere) throw PUTNOSRC;
      let srcSt;
      if (srcHere) {
        try { srcSt = io.lstat(srcFull); } catch (e) { throw PUTNOSRC; }
        if (srcSt.kind === "dir") throw PUTDSTBAD;
      }
      //  Dest parent dir must exist (no mkdir -p).
      const dstDir = pathlib.dirname(dst);
      if (dstDir && dstDir !== "." && dstDir !== "/") {
        const ddFull = join(wtRoot, dstDir);
        let dst_st;
        try { dst_st = io.lstat(ddFull); } catch (e) { throw PUTNODIR; }
        if (dst_st.kind !== "dir") throw PUTNODIR;
      }
      //  Rename only when src is still on disk; claim flow records the row.
      if (srcHere) io.rename(srcFull, dstFull);
      return { path: srcRaw, dst: dst, kind: "mov", restamp: dst };
    }
  };

  return engine;
}

function statExists(p) { try { io.lstat(p); return true; } catch (e) { return false; } }

//  YES iff `rel` is under `prefix` (prefix "" = reporoot → everything).
function under(rel, prefix) {
  if (prefix === "" || prefix == null) return true;
  return rel.indexOf(prefix) === 0;
}

module.exports = {
  prep: prep,
  isMeta: isMeta,
  diskSha: diskSha,
  wtScan: wtScan,
  wtEqBase: wtEqBase,
  PUTAMBIG: PUTAMBIG, PUTNOSRC: PUTNOSRC, PUTDSTBAD: PUTDSTBAD,
  PUTNODIR: PUTNODIR, PUTMVMETA: PUTMVMETA
};
