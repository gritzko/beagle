//  expected.js — DIFF-016 (DIS-080): the EXPECTED reading of ONE path.
//  EXPECTED = base ⊕ every in-scope patch-in's THEIRS layer, with NO WT layer —
//  literally patch.js's mergeApply minus `weave.foldWt`: `weave.build` each side
//  from its commit DAG, `weave.merge` the union, read it MARKERLESS through
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

//  The alive (tip) bytes of a weave, copied off the shared scratch buffer.
function aliveOf(w) {
  const b = io.ram(weave.MAX_SOURCE_MARKED_UP);
  w.alive(b);
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
  const ctx = weave.makeCtx(reader, path, treeCache);
  let w = null;
  const groups = [];
  try {
    if (baseSha) {
      const b = weave.build(reader, path, baseSha, ctx);
      if (b.weave) { w = b.weave; groups.push(setArr(b.ids)); }
    }
    for (const sha of shas) {
      const t = weave.build(reader, path, sha, ctx);
      if (!t.weave) continue;                     // path absent in that theirs
      out.patched = true;
      if (!w) { w = t.weave; groups.push(setArr(t.ids)); continue; }
      w = weave.merge(w, t.weave, "0000000000000000");
      groups.push(setArr(t.ids));
    }
    if (!w) { out.patched = false; return out; }
    out.bytes = (groups.length < 2) ? aliveOf(w) : weave.mergedLive(w, groups).bytes;
  } catch (e) {
    //  Over the fixed markup cap (or an unweavable source) → no EXPECTED; the
    //  caller falls back to the plain base-vs-wt axis, exactly as before.
    if (!("" + e).includes("full")) throw e;
    return { bytes: undefined, patched: false };
  }
  return out;
}

module.exports = { theirsShas: theirsShas, expectedOf: expectedOf };
