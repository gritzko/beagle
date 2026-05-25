//
//  BRAM — Bram Cohen's patience diff over u64 token-hash arrays.
//
//  Sits on top of `DIFFu64s` from abc/DIFF.h.  Drop-in replacement
//  for callers that want line-coherent alignment when token-level
//  Myers would mis-align across repeated identifiers (e.g. multiple
//  `}` lines, multiple `u8bUnMap(obuf);` lines).
//
//  Algorithm (Cohen 2003; same shape as git's xpatience.c):
//
//    1.  Treat each contiguous token run delimited by NL_HASH (the
//        hash of "\n", as produced by `weave_blob_cb`'s whitespace-
//        split) as a "line".  Compute a per-line hash by FNV-1a-
//        folding the constituent token hashes in order.
//
//    2.  Count line-hashes on each side.  A line is an *anchor* iff
//        its hash appears exactly once on the OLD side AND exactly
//        once on the NEW side — common braces / blank lines / short
//        repeated boilerplate fall out by this rule.
//
//    3.  Run `DIFFu64s` on the anchor-hash arrays — small, distinctive
//        input — to find the longest monotone matched-anchor sequence.
//
//    4.  For each between-anchor region (a span of OLD tokens vs a
//        span of NEW tokens with no matched anchors inside), recurse:
//        run `DIFFu64s` on those token slices.  Append the resulting
//        EDL entries to the global EDL.
//
//    5.  Stitch each matched anchor's tokens into the global EDL as
//        an EQ run.  `DIFFu64s`'s coalescing handles run merging.
//
//  Falls back to a plain `DIFFu64s` call when no anchors exist
//  (every line repeats, or no `\n` tokens present) — token-level
//  Myers does as well as anything in that case.
//
#ifndef GRAF_BRAM_H
#define GRAF_BRAM_H

#include "abc/DIFF.h"
#include "abc/INT.h"

ok64 BRAMu64s(e32g edl, i32s work, u64cs old_hashes, u64cs new_hashes);

#endif
