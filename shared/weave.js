//  weave.js — WEAVE/HUNK fold/merge bindings + the ONE source-size policy.
//  (DIFF-010).  The C WEAVE builders (fold/merge/emit*) tokenise a source into a
//  marked-up buffer.  We cap the SOURCE we tokenise at MAX_SOURCE_SIZE; anything
//  bigger is a BLOB (callers skip tokenising/diffing it).  Because the source is
//  capped, its markup is too — so every WEAVE/HUNK/render buffer is allocated
//  ONCE at the fixed MAX_SOURCE_MARKED_UP (a lazy anonymous mmap, abc.ram/io.ram
//  — only touched pages fault in), never grown dynamically.

"use strict";

//  A source larger than this is a BLOB: not tokenised, not diffed (callers gate
//  on it like the binary check).  One place sets it; everyone imports it.
const MAX_SOURCE_SIZE = 4 << 20;                  // 4 MB
//  A tokenised source runs larger than its raw bytes; 4x covers the worst real
//  case (a fully-changed 2-layer diff measures ~3.3x).  Buffers are this size.
const MAX_SOURCE_MARKED_UP = MAX_SOURCE_SIZE * 4; // 16 MB

//  fold(base, blob, ext, hash): a WEAVENext fold into a fresh fixed WEAVE buffer.
//  `blob` is a source ≤ MAX_SOURCE_SIZE (the caller gates blobs out first).
function fold(base, blob, ext, hash) {
  const w = abc.ram("WEAVE", MAX_SOURCE_MARKED_UP);
  w.fold(base, blob, ext, hash);
  return w;
}

//  merge(a, b, hash): a WEAVEMerge into a fresh fixed WEAVE buffer.
function merge(a, b, hash) {
  const w = abc.ram("WEAVE", MAX_SOURCE_MARKED_UP);
  w.merge(a, b, hash);
  return w;
}

module.exports = { fold: fold, merge: merge,
  MAX_SOURCE_SIZE: MAX_SOURCE_SIZE, MAX_SOURCE_MARKED_UP: MAX_SOURCE_MARKED_UP };
