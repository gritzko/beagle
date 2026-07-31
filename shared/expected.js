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

//  expectedOf(reader, path, baseSha, shas, treeCache) → { bytes, patched }.
//  `bytes` is the RGA live reading of base ⊕ theirs¹ ⊕ theirs² … (undefined when
//  the path exists on no side, or the source is over the weave cap); `patched`
//  is true iff at least one theirs layer contributed.  Several patch runs STACK:
//  every in-scope row joins the SAME union weave, one group each, so a second
//  absorb composes with the first exactly as `patch` composed them on disk.
function expectedOf(reader, path, baseSha, shas, treeCache) {
  const out = { bytes: undefined, patched: false };
  if (!shas || !shas.length) return out;
  //  DIS-082: ONE ctx for every side — each build folds into the SAME weave, so
  //  the base and the theirs tips share their common history by construction and
  //  the old pairwise merge cascade is a single contentless JOIN over the union.
  const ctx = weave.makeCtx(reader, path, treeCache);
  const sides = [];                       // one { rev, ids } per contributing tip
  try {
    if (baseSha) {
      const b = weave.build(reader, path, baseSha, ctx);
      if (b.weave) sides.push(b);
    }
    for (const sha of shas) {
      const t = weave.build(reader, path, sha, ctx);
      if (!t.weave) continue;                     // path absent in that theirs
      out.patched = true;
      sides.push(t);
    }
    if (!sides.length) { out.patched = false; return out; }
    //  ctx.w is the LATEST container (fold/merge rewrite into a fresh buffer);
    //  the per-side `.weave` snapshots predate the later builds.
    const w = ctx.w;
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

module.exports = { theirsShas: theirsShas, expectedOf: expectedOf };
