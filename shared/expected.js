//  expected.js — DIFF-016 (DIS-080): the EXPECTED reading of ONE path.
//  EXPECTED = base ⊕ every in-scope patch-in's THEIRS layer, with NO WT layer —
//  literally patch.js's mergeApply minus `weave.foldWt`: `weave.build` each side
//  from its commit DAG INTO ONE weave (DIS-082 — one shared ctx), `weave.merge`
//  the union, read it MARKERLESS through
//  `weave.mergedLive` (PATCH-025).  So EXPECTED is the same object `post` folds
//  as the merge commit, and `diff` can split wt dirt into "patched in" (wt ==
//  EXPECTED) vs "local edit on top" (wt != EXPECTED) — DIS-080 §6.
//
//  The patch rows come from `wtlog.patchTheirs()` (the classifier's 4th input);
//  an empty list makes the whole axis a no-op, so a repo with no patch in scope
//  pays nothing.

"use strict";

const weave = require("./weave.js");
const pathdag = require("./pathdag.js");
const shalib = require("./util/sha.js");
const isFullSha = shalib.isFullSha;

//  DIFF-016: the in-scope patch-ins' theirs commits, oldest-first (empty when no
//  patch row is in scope — every caller then skips the EXPECTED axis entirely).
function theirsShas(log) {
  return (log && typeof log.patchTheirs === "function") ? log.patchTheirs() : [];
}

//  A Set of hashlet strings → an Array (mergedLive's group arg) — patch.js twin.
function setArr(s) { const a = []; for (const x of s) a.push(x); return a; }

//  DIS-082: the JOIN commit — the contentless merge id the stacked views are
//  read under (the old sentinel hashlet), patch.js twin.
const JOIN_ID = weave.JOIN_ID;

//  The bytes of a weave AT `rev`, copied off the shared scratch buffer.  ONE
//  weave now carries every side, so alive() (the LAST folded commit) is the
//  wrong reading — always produce(rev).
function produceOf(w, rev) {
  const b = io.ram(weave.MAX_SOURCE_MARKED_UP);
  w.produce(rev, b);
  return b.data().slice();
}

//  CFOLD-001: the TRIVIAL resolution (weave3's "only one side moved" rule at
//  DAG level).  A side with no retained node carries the path EXACTLY as the
//  floor does, so when at most one side changed it above the floor the union
//  reads as that side's tip blob — no fold, no merge.  Returns
//  { bytes, seedOk, moved } (bytes undefined = the path exists on no side), or
//  null when it does not apply.
function trivial(reader, path, pd, tips, c) {
  if (!pd.floor) return null;                      // no floor: no shared seed
  let moved = -1;
  for (let i = 0; i < tips.length; i++) {
    const reps = pd.tips[tips[i]];
    if (reps === null) return null;                // unreadable tip: fold it out
    if (!reps.length) continue;
    if (moved >= 0) return null;                   // two sides moved: a real weave
    moved = i;
  }
  const seed = pd.seed ? blobOf(reader, pd.seed.sha) : undefined;
  const seedOk = seed !== undefined;
  if (moved < 0) return { bytes: seed, seedOk: seedOk, moved: -1 };
  const leaf = pathdag.at(reader, tips[moved], path, c);
  //  deleted at that tip: the fold-empty weave reads as no bytes at all.
  if (!leaf) return { bytes: new Uint8Array(0), seedOk: seedOk, moved: moved };
  const b = blobOf(reader, leaf.sha);
  return (b === undefined) ? null : { bytes: b, seedOk: seedOk, moved: moved };
}
function blobOf(reader, sha) {
  const b = weave.blobBytes(reader, sha);
  return (b !== undefined && b.length <= weave.MAX_SOURCE_SIZE) ? b : undefined;
}

//  expectedOf(reader, path, baseSha, shas, cache) → { bytes, patched }.
//  `bytes` is the RGA live reading of base ⊕ theirs¹ ⊕ theirs² … (undefined when
//  the path exists on no side, or the source is over the weave cap); `patched`
//  is true iff at least one theirs layer contributed.  Several patch runs STACK:
//  every in-scope row joins the SAME union weave, one group each, so a second
//  absorb composes with the first exactly as `patch` composed them on disk.
//  CFOLD-001: the sides come off ONE condensed pathdag (LCA floor + tree-sha
//  prune), so the cost is the path's changes above the floor — not the whole
//  ancestry per tip per path.  `cache` is the run's ONE `pathdag.cache()`.
function expectedOf(reader, path, baseSha, shas, cache) {
  const out = { bytes: undefined, patched: false };
  if (!shas || !shas.length) return out;
  const c = cache || pathdag.cache();
  const hasBase = !!(baseSha && isFullSha(baseSha));
  const tips = hasBase ? [baseSha] : [];
  for (const s of shas) if (isFullSha(s) && tips.indexOf(s) < 0) tips.push(s);
  if (!tips.length) return out;
  const sides = [];                       // one { rev, ids } per contributing tip
  let w = null;
  try {
    const pd = pathdag.of(reader, path, tips, c, { floor: "lca" });
    if (pd.truncated) {
      //  Over dag.WALK_CAP: fall back to the full-history reconstruction —
      //  slower, never a different answer.  DIS-082: ONE ctx for every side.
      const ctx = weave.makeCtx(reader, path, c.treeCache);
      for (let i = 0; i < tips.length; i++) {
        const s = weave.build(reader, path, tips[i], ctx);
        if (!s.weave) continue;
        if (!(hasBase && i === 0)) out.patched = true;
        sides.push(s);
      }
      w = ctx.w;
    } else {
      const tr = trivial(reader, path, pd, tips, c);
      if (tr) {
        //  a theirs side contributes iff it HAS a weave: it is the side that
        //  moved, or the floor itself carries the path (the shared seed).
        for (let i = hasBase ? 1 : 0; i < tips.length; i++)
          if (i === tr.moved || tr.seedOk) { out.patched = true; break; }
        if (tr.bytes === undefined) { out.patched = false; return out; }
        out.bytes = tr.bytes;
        return out;
      }
      const fb = weave.buildDag(reader, path, pd);
      for (let i = 0; i < tips.length; i++) {
        const s = fb.at(tips[i]);
        if (!s.weave) continue;                   // path absent on that side
        if (!(hasBase && i === 0)) out.patched = true;
        sides.push(s);
      }
      w = fb.ctx.w;
    }
    if (!sides.length) { out.patched = false; return out; }
    if (sides.length < 2) out.bytes = produceOf(w, sides[0].rev);
    else {
      const union = new Set();
      for (const s of sides) for (const id of s.ids) union.add(id);
      const wm = weave.merge(w, JOIN_ID, setArr(union));
      out.bytes = weave.mergedLive(wm, JOIN_ID,
                                   sides.map(function (s) { return setArr(s.ids); })).bytes;
    }
  } catch (e) {
    //  Over the fixed markup cap (or an unweavable source) → no EXPECTED; the
    //  caller falls back to the plain base-vs-wt axis, exactly as before.
    if (!("" + e).includes("full")) throw e;
    return { bytes: undefined, patched: false };
  }
  return out;
}

module.exports = { theirsShas: theirsShas, expectedOf: expectedOf,
                   //  CFOLD-001: the per-RUN memo every caller threads down.
                   cache: pathdag.cache };
