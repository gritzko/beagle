#ifndef DOG_NEIL_H
#define DOG_NEIL_H

#include "abc/DIFF.h"
#include "dog/tok/TOK.h"

con ok64 NEILBAD = 0x1739254b28d;

// Max byte span of an EQ entry that can be killed (0 = no limit).
#ifndef NEIL_MAX_KILL
#define NEIL_MAX_KILL 64
#endif

// Check if a token is actual whitespace by its tag.  Byte-peek kept
// as a fallback for synthesized tokens that never went through a lexer.
fun b8 NEILIsWS(u32cs toks, u8cp base, u32 idx) {
    u8 tag = tok32Tag(toks[0][idx]);
    if (tag == 'W') return YES;
    u32 lo = (idx > 0) ? tok32Offset(toks[0][idx - 1]) : 0;
    u32 hi = tok32Offset(toks[0][idx]);
    if (lo >= hi) return YES;
    u8 ch = base[lo];
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

// Semantic cleanup of a token-level EDL.
// Removes false short equalities, extracts DEL/INS overlaps.
// edl      — edit description list (modified in place)
// old_toks — packed u32 tokens from old file
// new_toks — packed u32 tokens from new file
// old_src  — old file source bytes (for overlap content comparison)
// new_src  — new file source bytes (for overlap content comparison)
ok64 NEILCleanup(e32g edl, u32cs old_toks, u32cs new_toks,
                 u8csc old_src, u8csc new_src);

// Lossless boundary shift: slide edit boundaries to align with
// natural positions (line breaks, word breaks).  Does not change
// the diff semantics, only repositions EQ/edit boundaries.
// Inspired by diff-match-patch cleanupSemanticLossless.
// Should be called as the last pass, after NEILCleanup.
ok64 NEILShift(e32g edl, u32cs old_toks, u32cs new_toks,
               u8csc old_src, u8csc new_src);

// Canonicalize splices: within every maximal non-EQ run, collapse all
// the INS+DEL entries into one INS entry followed by one DEL entry —
// the "in-rm" invariant.  Drops length-0 entries.  Idempotent.  Runs
// implicitly at the tail of both `NEILCleanup` and `NEILShift`; call
// directly when consuming an EDL produced outside this pipeline.
ok64 NEILCanon(e32g edl);

#endif
