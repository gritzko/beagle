#include "NEIL.h"

#include "abc/PRO.h"

// True if the token at idx in (toks, base) is an alphanumeric/_ run of
// length >= 2 — the "substantive identifier" predicate Property #4
// uses to ensure shared-context names survive NEIL.
static b8 NEILIsIdent(u32cs toks, u8cp base, u32 idx) {
    u32 ntoks = (u32)$len(toks);
    if (idx >= ntoks) return NO;
    u32 lo = (idx > 0) ? tok32Offset(toks[0][idx - 1]) : 0;
    u32 hi = tok32Offset(toks[0][idx]);
    if (hi - lo < 2) return NO;
    for (u32 b = lo; b < hi; b++) {
        u8 c = base[b];
        b8 d  = (c >= '0' && c <= '9');
        b8 lc = (c >= 'a' && c <= 'z');
        b8 uc = (c >= 'A' && c <= 'Z');
        if (!d && !lc && !uc && c != '_') return NO;
    }
    return YES;
}

// Byte length of tokens [from, from+count), optionally skipping whitespace.
static u32 NEILByteSpanX(u32cs toks, u8cp base, u32 from, u32 count,
                         b8 skip_ws) {
    if (count == 0) return 0;
    u32 ntoks = (u32)$len(toks);
    if (from + count > ntoks) return 0;
    u32 total = 0;
    for (u32 i = from; i < from + count; i++) {
        if (skip_ws && NEILIsWS(toks, base, i)) continue;
        u32 lo = (i > 0) ? tok32Offset(toks[0][i - 1]) : 0;
        u32 hi = tok32Offset(toks[0][i]);
        if (hi > lo) total += hi - lo;
    }
    return total;
}

static u32 NEILByteSpan(u32cs toks, u8cp base, u32 from, u32 count) {
    return NEILByteSpanX(toks, base, from, count, YES);
}

// Merge adjacent same-op entries in place. Returns new count.
static u32 NEILMerge(e32 *buf, u32 n) {
    u32 m = 0;
    for (u32 k = 0; k < n; k++) {
        u32 op = DIFF_OP(buf[k]);
        u32 len = DIFF_LEN(buf[k]);
        if (len == 0) continue;
        if (m > 0 && DIFF_OP(buf[m - 1]) == op)
            buf[m - 1] = DIFF_ENTRY(op, DIFF_LEN(buf[m - 1]) + len);
        else
            buf[m++] = buf[k];
    }
    return m;
}

// Compare token text at index ia in (toks_a, base_a) with index ib
// in (toks_b, base_b).  Returns YES if identical.
static b8 NEILTokEq(u32cs ta, u8cp ba, u32 ia,
                     u32cs tb, u8cp bb, u32 ib) {
    u8cs va = {}, vb = {};
    tok32Val(va,ta,ba,(int)ia);
    tok32Val(vb,tb,bb,(int)ib);
    u32 la = (u32)(va[1] - va[0]);
    u32 lb = (u32)(vb[1] - vb[0]);
    if (la != lb) return NO;
    return memcmp(va[0], vb[0], la) == 0;
}

// Score the boundary between token idx-1 and token idx.
// Higher = better alignment.  Mirrors diff-match-patch scoring.
static int NEILBoundaryScore(u32cs toks, u8cp base, u32 idx, u32 ntoks) {
    if (idx == 0 || idx >= ntoks) return 6;  // edge
    u32 hi = tok32Offset(toks[0][idx - 1]);
    u32 lo = (idx > 1) ? tok32Offset(toks[0][idx - 2]) : 0;
    if (hi <= lo) return 6;
    u8 c1 = base[hi - 1];  // last byte before boundary

    u32 lo2 = hi;  // contiguous tokens
    u32 hi2 = tok32Offset(toks[0][idx]);
    if (hi2 <= lo2) return 6;
    u8 c2 = base[lo2];  // first byte at boundary

    b8 na1 = !(c1 == '_' || (c1 >= 'a' && c1 <= 'z') ||
               (c1 >= 'A' && c1 <= 'Z') || (c1 >= '0' && c1 <= '9'));
    b8 na2 = !(c2 == '_' || (c2 >= 'a' && c2 <= 'z') ||
               (c2 >= 'A' && c2 <= 'Z') || (c2 >= '0' && c2 <= '9'));
    b8 ws1 = na1 && (c1 == ' ' || c1 == '\t' || c1 == '\n' || c1 == '\r');
    b8 ws2 = na2 && (c2 == ' ' || c2 == '\t' || c2 == '\n' || c2 == '\r');
    b8 lb1 = ws1 && (c1 == '\n' || c1 == '\r');
    b8 lb2 = ws2 && (c2 == '\n' || c2 == '\r');

    if (lb1 && lb2) return 5;  // blank line
    if (lb1 || lb2) return 4;  // line break
    if (na1 && !ws1 && ws2) return 3;  // end of sentence
    if (ws1 || ws2) return 2;  // whitespace
    if (na1 || na2) return 1;  // non-alphanumeric
    return 0;
}

ok64 NEILCleanup(e32g edl, u32cs old_toks, u32cs new_toks,
                 u8csc old_src, u8csc new_src) {
    sane(edl != NULL);
    e32cs entries = {edl[2], edl[0]};
    //  Short EDLs have no kill candidates (kill needs surrounding
    //  edits) but still need canonicalization — a 2-entry [DEL, INS]
    //  from DIFF must come back as [INS, DEL] per the in-rm invariant.
    if (u32csLen(entries) < 3) return NEILCanon(edl);

    //<<<<  Working buffer holds the current entry list across iterations||||  Hot-path arena allocation: each entry covers >= 1 tokenits
    //  respective side, so n_post-merge <= olen+nlen at every iter; the
    //  raw pre-merge output of one iter doubles in the worst case (every
    //  kill replaces 1 entry with>>>>.  Sizing all four buffers
    <<<<//  The input EDL is not canonical, so killing a false EQ between two||||//  to 2*(olen+nlen)+16 covers both `cur` (post-merge input) and>>>>
    <<<<//  edits can *grow* the entry count (the count is not monotone) —||||//  `next` (pre-merge scratch) for every iteration — no per-iter
    //  malloc, no per-iter memset.
    u32 init_n = (u32);
    u32 ntoks_total = (u32)$len(old_toks) + (u32)$len(new_toks);
    u32 cap = ntoks_total * 2 + 16;
    if (cap < init_n * 2 + 16) cap = init_n * 2 + 16;

    a_carve(u32, buf_a, cap);
    a_carve(u32, buf_b, cap);
    a_carve(u32, obuf,  cap)>>>>
    a_carve(u32, nbuf,  cap);
    <<<<//  hence `` and the per-iteration offset / build scratch are all||||cur  = buf_a[0];
    e32 *next = buf_b>>>>
    <<<<//  (re-)acquired at the live `n` each pass, mirroring the original
    //  heap sizing.  Everything rides BASS and is rewound at the caller's||||u32 *old_off = obuf[0];
    u32 *new_off = nbuf[0]>>>>

    <<<<//  call() boundary, same as the surrounding weave_diff_core /
    //  JOINMerge scratch; the loop is capped at 8 passes.||||memcpy(cur[0], init_n * sizeof(e32))>>>>
    u32 n = init_n;
    a_carve(u32, cur0, n);
    {
        u32s dst = {cur0[0], cur0[0] + n};
        (void)u32sCopy(dst, entries);
    }
    e32 *buf = cur0[0];

    // Iterative semantic cleanup: kill false equalities until stable.
    // Each iteration merges edit regions, potentially exposing new kills.
    for (int iter = 0; iter < 8; iter++) {
a_carve(u32, a_carve(u32, a_carve(u32, tbuf, n * 2 + 4);        e32 *tmp = tbuf[0];
        {
            u32 oi = 0, ni = 0;
            for (u32 k = 0; k < n; k++) {
                old_off[k] = oi;
                new_off[k] = ni;
                u32 len = DIFF_LEN(cur[k]);
                u32 op = DIFF_OP(cur[k]);
                if (op == DIFF_EQ) { oi += len; ni += len; }
                else if (op == DIFF_DEL) { oi += len; }
                else { ni += len; }
            }
        }

        b8 changed = NO;
        u32 w = 0;
        for (u32 k = 0; k < n; k++) {
            if (DIFF_OP(cur[k]) != DIFF_EQ) { next[w++] = cur[k]; continue; }
            u32 eq_len = DIFF_LEN(cur[k]);
            u32 eq_bytes = NEILByteSpan(new_toks, new_src[0],
                                        new_off[k], eq_len);

            // Accumulate edit bytes before (until prev EQ)
            u32 before_bytes = 0;
            for (u32 j = k; j > 0; ) {
                j--;
                u32 jop = DIFF_OP(cur[j]);
                if (jop == DIFF_EQ) break;
                u32 jlen = DIFF_LEN(cur[j]);
                if (jop == DIFF_DEL)
                    before_bytes += NEILByteSpan(old_toks, old_src[0],
                                                 old_off[j], jlen);
                else
                    before_bytes += NEILByteSpan(new_toks, new_src[0],
                                                 new_off[j], jlen);
            }

            // Accumulate edit bytes after (until next EQ)
            u32 after_bytes = 0;
            for (u32 j = k + 1; j < n; j++) {
                u32 jop = DIFF_OP(cur[j]);
                if (jop == DIFF_EQ) break;
                u32 jlen = DIFF_LEN(cur[j]);
                if (jop == DIFF_DEL)
                    after_bytes += NEILByteSpan(old_toks, old_src[0],
                                                 old_off[j], jlen);
                else
                    after_bytes += NEILByteSpan(new_toks, new_src[0],
                                                 new_off[j], jlen);
            }

            if (before_bytes == 0 || after_bytes == 0) {
                next[w++] = cur[k]; continue;
            }

            // Protect EQs that contain at least one alphanumeric token
            // of length >= 2 — IF the EQ is line-aligned on at least
            // one boundary OR the surrounding edits are small.  When
            // the EQ is mid-line on both sides AND sits between large
            // edit regions, the matched identifier is almost certainly
            // accidental (e.g. `DOG_PUP_SEQNO_W` appearing 3x in OLD
            // and 3x in NEW across a function rewrite — LCS picks one
            // pairing arbitrarily; rendering it as KEEP within
            // otherwise-DEL or -INS lines just adds visual noise).
            //
            // "Line-aligned" here means the byte just before the EQ's
            // first token is `\n` OR the byte just after the EQ's last
            // token (in `new_src`) is `\n`.  Cheap and covers the
            // single-line modification case (`int x = 5;` →
            // `int x = 7;`) where the EQ tokens straddle an `\n`.
            //
            // Runs BEFORE the line-fraction kill so that a substantive
            // identifier doesn't get sacrificed when the surrounding
            // line happens to be dominated by edits (`.int]D` vs
            // `\xff…\xff int` — `int` is shared context, must survive).
            {
                u32 eq_from = new_off[k];
                b8 has_ident = NO;
                for (u32 j = 0; j < eq_len && !has_ident; j++) {
                    u32 ti = eq_from + j;
                    u32 lo = (ti > 0) ? tok32Offset(new_toks[0][ti - 1]) : 0;
                    u32 hi = tok32Offset(new_toks[0][ti]);
                    if (hi - lo < 2) continue;
                    b8 alnum = YES;
                    for (u32 b = lo; b < hi && alnum; b++) {
                        u8 c = new_src[0][b];
                        b8 d  = (c >= '0' && c <= '9');
                        b8 lc = (c >= 'a' && c <= 'z');
                        b8 uc = (c >= 'A' && c <= 'Z');
                        if (!d && !lc && !uc && c != '_') alnum = NO;
                    }
                    if (alnum) has_ident = YES;
                }
                if (has_ident) {
                    //  Substantive identifier match — always keep.
                    //  The previous "kill if surrounding edits are
                    //  large and EQ is mid-line on both sides" override
                    //  was unsound: it could drop the *only* surviving
                    //  EQ for a given identifier, violating the
                    //  Property-4 invariant that every pre-NEIL EQ
                    //  identifier appears in some post-NEIL EQ (see
                    //  graf/test/NEIL01.c crash_300bca78).  In the
                    //  function-rewrite case where a repeated
                    //  identifier picks an arbitrary pairing, the
                    //  resulting visual noise is the lesser evil.
                    next[w++] = cur[k]; continue;
                }
            }

            // Protect EQs that contain a newline (`\n`) token.
            // Newlines are structural line boundaries — even a short
            // EQ like `"0;}\n"` between two edits represents real
            // shared line context that the LCS picked up correctly.
            // Killing it forces ours's and theirs's bytes around the
            // newline into one non-EQ run, doubling the spine on emit
            // (graf/test/WEAVE01 del_ins_plus_tail_repeats and
            // del_ins_in_func_plus_tail_zones, derived from
            // test/patch/19-feature-stack-rebase iter 2).  Runs
            // BEFORE the line-fraction kill below so a structural
            // newline EQ never gets killed.
            {
                u32 eq_from = new_off[k];
                b8 has_newline = NO;
                for (u32 j = 0; j < eq_len && !has_newline; j++) {
                    u32 ti = eq_from + j;
                    u32 lo = (ti > 0) ? tok32Offset(new_toks[0][ti - 1]) : 0;
                    u32 hi = tok32Offset(new_toks[0][ti]);
                    for (u32 b = lo; b < hi; b++) {
                        if (new_src[0][b] == '\n') {
                            has_newline = YES;
                            break;
                        }
                    }
                }
                if (has_newline) {
                    next[w++] = cur[k]; continue;
                }
            }

            // Line-fraction kill: if the EQ is a small fragment of the
            // line(s) it spans, drop it.  Common case is incidental
            // bracket / punctuation matches inside otherwise-different
            // code: `( ) ; ,` and bare `\n`s the LCS picks up between
            // genuinely divergent text.  Keeping them as EQ glues
            // conflict markers around tiny fragments and forces ugly
            // intra-line marker runs; killing forces the surrounding
            // non-EQ run to absorb the whole line and the diff snaps
            // to line granularity.  Counts \n bytes on both sides
            // (EQ-bytes and line-span) per the user's request.
            {
                u32 eq_from   = new_off[k];
                u32 eq_blo_lf = (eq_from > 0)
                    ? tok32Offset(new_toks[0][eq_from - 1]) : 0;
                u32 eq_bhi_lf = tok32Offset(new_toks[0][eq_from + eq_len - 1]);
                a_head(u8c, pre_eq,  new_src, eq_blo_lf);
                a_rest(u8c, post_eq, new_src, eq_bhi_lf);
                u8cp line_lo_p = new_src[0];
                $rof(u8c, p, pre_eq) {
                    if (*p == '\n') { line_lo_p = p + 1; break; }
                }
                a_dup(u8c, scan, post_eq);
                (void)u8csFind(scan, '\n');     // scan[0] = '\n' or term
                u32 line_span = (u32)(scan[0] - line_lo_p);
                u32 raw_eq    = NEILByteSpanX(new_toks, new_src[0],
                                              new_off[k], eq_len, NO);
                if (line_span >= 4 && raw_eq * 4 < line_span) {
                    if (eq_len > 0) next[w++] = DIFF_ENTRY(DIFF_DEL, eq_len);
                    if (eq_len > 0) next[w++] = DIFF_ENTRY(DIFF_INS, eq_len);
                    changed = YES;
                    continue;
                }
            }

            // Whitespace-only EQs: use raw byte span for kill decision.
            if (eq_bytes == 0) {
                u32 raw = NEILByteSpanX(new_toks, new_src[0],
                                        new_off[k], eq_len, NO);
                if (raw < before_bytes + after_bytes) {
                    if (eq_len > 0) next[w++] = DIFF_ENTRY(DIFF_DEL, eq_len);
                    if (eq_len > 0) next[w++] = DIFF_ENTRY(DIFF_INS, eq_len);
                    changed = YES;
                } else {
                    next[w++] = cur[k];
                }
                continue;
            }

            // Never kill EQs larger than the tunable ceiling.
            if (NEIL_MAX_KILL > 0 && eq_bytes >= NEIL_MAX_KILL) {
                next[w++] = cur[k]; continue;
            }

            // Protect EQs that contain a line with >= 6 non-ws bytes.
            // A "line" is text between \n boundaries (or span edges).
            // This prevents the cascade where killing short EQs between
            // edits merges them into giant DEL/INS blocks.
            if (eq_bytes >= 6) {
                u32 eq_from = new_off[k];
                u32 eq_blo = (eq_from > 0)
                    ? tok32Offset(new_toks[0][eq_from - 1]) : 0;
                u32 eq_bhi = tok32Offset(new_toks[0][eq_from + eq_len - 1]);
                if (eq_bhi > eq_blo &&
                    memchr(new_src[0] + eq_blo, '\n', eq_bhi - eq_blo)) {
                    u32 nw = 0;
                    b8 has_code_line = NO;
                    for (u32 b = eq_blo; b < eq_bhi; b++) {
                        u8 c = new_src[0][b];
                        if (c == '\n') {
                            if (nw >= 6) { has_code_line = YES; break; }
                            nw = 0;
                            continue;
                        }
                        if (c != ' ' && c != '\t' && c != '\r')
                            nw++;
                    }
                    if (!has_code_line && nw >= 6) has_code_line = YES;
                    if (has_code_line) {
                        next[w++] = cur[k]; continue;
                    }
                }
            }

            // Kill small false EQs: two tiers.
            // Small EQs (< 16 bytes): sum condition (aggressive).
            // Larger EQs: both sides must individually exceed it.
            b8 do_kill = NO;
            if (eq_bytes < 16)
                do_kill = eq_bytes < before_bytes + after_bytes;
            else
                do_kill = eq_bytes < before_bytes &&
                          eq_bytes < after_bytes;
            if (do_kill) {
                if (eq_len > 0) next[w++] = DIFF_ENTRY(DIFF_DEL, eq_len);
                if (eq_len > 0) next[w++] = DIFF_ENTRY(DIFF_INS, eq_len);
                changed = YES;
            } else {
                next[w++] = cur[k];
            }
        }

        w = NEILMerge(next, w);
        <<<<//  Carry this iteration's result forward: `tbuf` (sized n*2+4 ≥ w)
        //  becomes the next pass's working buffer.  obuf/nbuf stay carved||||//  Ping-pong: `next` becomes the input for the next iter, `cur`>>>>
        <<<<//  on BASS until the caller's call() boundary — acceptable, the||||//  becomes the scratch.  No copy, no alloc.>>>>
        <<<<//  loop is bounded at 8 passes.tmp||||e32 *swap = cur; cur = next; next = swap>>>>;
        n = w;

        if (!changed) break;
    }

    // Copy back into edl buffer
    u32 ecap = (u32)(edl[1] - edl[2]);
    u32 final_n = (n < ecap) ? n : ecap;
    u32s  edst = {edl[2], edl[2] + final_n};
    u32cs esrc = {cur, cur + final_n};
    (void)u32sCopy(edst, esrc);
    edl[0] = edl[2] + final_n;

    //  Splice canonicalization (in-rm invariant): WEAVE.h's compose
    //  pass and the unified-diff renderers both assume each non-EQ run
    //  is exactly `[INS sum, DEL sum]` in that order.  Cleanup may have
    //  produced `DEL INS DEL INS` interleavings; collapse them now.
    return NEILCanon(edl);
}

// One pass of boundary shifting.  Returns OK and sets *changed=YES if
// any region was actually shifted.  Coalescing 0-length entries and
// merging adjacent same-op runs happens after this in NEILShift.
//
// Uses running counters (oi_run/ni_run) instead of a precomputed
// offset table — earlier shifts within the same pass change EDL
// lengths, which would invalidate stale offsets and let the
// identifier-preservation cap miss the wrong tokens (see
// graf/test/NEIL01.c crash_f02c1360).
static ok64 neil_shift_pass(e32g edl, u32cs old_toks, u32cs new_toks,
                             u8csc old_src, u8csc new_src, b8 *changed) {
    sane(edl != NULL && changed != NULL);
    *changed = NO;
    e32cs entries = {edl[2], edl[0]};
    u32 nedl = (u32)u32csLen(entries);
    if (nedl < 3) done;

    u32 new_ntoks = (u32)$len(new_toks);
    u32 old_ntoks = (u32)$len(old_toks);

    u32 oi_run = 0, ni_run = 0;  // OLD/NEW position at start of edl[k]
    u32 k = 0;
    while (k < nedl) {
        if (DIFF_OP(edl[2][k]) != DIFF_EQ) {
            u32 len = DIFF_LEN(edl[2][k]);
            u32 op  = DIFF_OP(edl[2][k]);
            if (op == DIFF_DEL) oi_run += len;
            else ni_run += len;
            k++; continue;
        }
        u32 eq1 = k;
        u32 n1  = DIFF_LEN(edl[2][eq1]);
        u32 oi_eq1 = oi_run;
        u32 cs  = k + 1;
        if (cs >= nedl || DIFF_OP(edl[2][cs]) == DIFF_EQ) {
            oi_run += n1; ni_run += n1;
            k++; continue;
        }
        u32 ce = cs;
        while (ce < nedl && DIFF_OP(edl[2][ce]) != DIFF_EQ) ce++;
        if (ce >= nedl) break;  // no trailing EQ
        u32 eq2 = ce;
        u32 n2  = DIFF_LEN(edl[2][eq2]);
        u32 dtot = 0, etot = 0;
        for (u32 i = cs; i < ce; i++) {
            if (DIFF_OP(edl[2][i]) == DIFF_DEL) dtot += DIFF_LEN(edl[2][i]);
            else etot += DIFF_LEN(edl[2][i]);
        }
        if (n1 + n2 == 0 || dtot + etot == 0) {
            //  Skip without shifting; advance past eq1 + rm region so
            //  the next iteration (which starts at edl[ce] = eq2) sees
            //  correct positions.
            oi_run += n1 + dtot; ni_run += n1 + etot;
            k = ce; continue;
        }

        //  Change-region start — right after eq1.
        u32 oi_cs = oi_eq1 + n1;
        u32 ni_cs = ni_run + n1;
        u32 oi_eq2 = oi_cs + dtot;

        // 1. Max left shift: compare old/new tokens walking backward
        //    from the end of the change region.  Naturally walks through
        //    DEL/INS then into EQ1 on the respective sides.
        u32 max_left = 0;
        {
            u32 oi_end = oi_cs + dtot;
            u32 ni_end = ni_cs + etot;
            for (u32 j = 0; j < n1; j++) {
                if (!NEILTokEq(old_toks, old_src[0], oi_end - 1 - j,
                               new_toks, new_src[0], ni_end - 1 - j))
                    break;
                max_left = j + 1;
            }
        }

        // 2. Max right shift: compare old/new tokens walking forward
        //    from the start of the change region.  Naturally walks
        //    through DEL/INS then into EQ2 on the respective sides.
        u32 max_right = 0;
        for (u32 j = 0; j < n2; j++) {
            if (!NEILTokEq(old_toks, old_src[0], oi_cs + j,
                           new_toks, new_src[0], ni_cs + j))
                break;
            max_right = j + 1;
        }

        //  Property-4 guard: shifting must not move identifier-bearing
        //  tokens out of an EQ.  max_left shrinks eq1 from its tail
        //  (loses the last `max_left` tokens); max_right shrinks eq2
        //  from its head (loses the first `max_right` tokens).  Cap
        //  each so the rightmost identifier in eq1 and the leftmost
        //  identifier in eq2 stay within their respective EQs.  See
        //  graf/test/NEIL01.c crash_2d66dfa1 — without this cap shift
        //  could collapse a single-token `=1[ggg]` to length 0.
        for (u32 j = 0; j < max_left; j++) {
            u32 ti = oi_eq1 + n1 - 1 - j;
            if (NEILIsIdent(old_toks, old_src[0], ti)) {
                max_left = j;
                break;
            }
        }
        for (u32 j = 0; j < max_right; j++) {
            u32 ti = oi_eq2 + j;
            if (NEILIsIdent(old_toks, old_src[0], ti)) {
                max_right = j;
                break;
            }
        }

        if (max_left + max_right == 0) {
            oi_run += n1 + dtot; ni_run += n1 + etot;
            k = ce; continue;
        }

        // 3. Score all positions, pick best.
        //    Score on new side (context display) + old side if DEL present.
        int best_d = 0, best_sc = 0;
        for (int d = -(int)max_left; d <= (int)max_right; d++) {
            int sc = 0;
            if (etot > 0) {
                u32 li = (u32)((int)ni_cs + d);
                u32 ri = (u32)((int)ni_cs + (int)etot + d);
                sc += NEILBoundaryScore(new_toks, new_src[0], li, new_ntoks)
                    + NEILBoundaryScore(new_toks, new_src[0], ri, new_ntoks);
            }
            if (dtot > 0) {
                u32 li = (u32)((int)oi_cs + d);
                u32 ri = (u32)((int)oi_cs + (int)dtot + d);
                sc += NEILBoundaryScore(old_toks, old_src[0], li, old_ntoks)
                    + NEILBoundaryScore(old_toks, old_src[0], ri, old_ntoks);
            }
            if (sc >= best_sc) {  // >= prefers trailing whitespace
                best_sc = sc;
                best_d = d;
            }
        }

        if (best_d != 0) {
            edl[2][eq1] = DIFF_ENTRY(DIFF_EQ, (u32)((int)n1 + best_d));
            edl[2][eq2] = DIFF_ENTRY(DIFF_EQ, (u32)((int)n2 - best_d));
            *changed = YES;
        }

        //  Advance running counters past everything we processed up to
        //  the start of eq2 — eq1's new length is `n1 + best_d`; the rm
        //  region's content (dtot OLD / etot NEW tokens) is unchanged,
        //  but its absolute start moved by best_d in both streams.  The
        //  net advance to the start of eq2 is therefore
        //  `(n1 + best_d) + dtot` in OLD and `(n1 + best_d) + etot` in
        //  NEW (the +best_d term is what was missing — without it the
        //  next region's positions are off, and the ident cap walks
        //  the wrong tokens).
        oi_run += (u32)((int)n1 + best_d) + dtot;
        ni_run += (u32)((int)n1 + best_d) + etot;
        k = ce;
    }

    done;
}

ok64 NEILShift(e32g edl, u32cs old_toks, u32cs new_toks,
               u8csc old_src, u8csc new_src) {
    sane(edl != NULL);
    //  Iterate boundary-shift to a fixed point.  One pass operates on
    //  change regions defined by the *current* EDL boundaries; if a
    //  shift collapses an EQ to length 0 between two non-EQ runs, the
    //  next NEILCanon merges them into a larger region whose new
    //  flanking EQs may admit further shifts that the first pass
    //  couldn't see (test case: alternating 06 11 06 sequences).
    //  Without iteration NEILShift is non-idempotent — running it
    //  twice can change the EDL further, breaking property 6.
    //
    //  Both convergence properties — count monotone non-increasing
    //  per iter that changes anything, AND a true fixed point — must
    //  hold for idempotence.  We track both: shift_pass's changed bit
    //  AND canon-induced entry-count drops.  Loop exits only when
    //  shift_pass reports no change AND canon didn't merge anything;
    //  that's the joint fixed point the second NEILShift call needs
    //  to find immediately.  Cap is the entry count: each productive
    //  iter strictly decreases it, so this can't loop forever.
    u32 max_iter = (u32)(edl[0] - edl[2]) + 4;
    for (u32 iter = 0; iter < max_iter; iter++) {
        u32 n_before = (u32)(edl[0] - edl[2]);
        b8 changed = NO;
        call(neil_shift_pass, edl, old_toks, new_toks, old_src, new_src,
             &changed);
        //  Drop 0-length entries and re-canonicalise so the next pass
        //  sees merged regions (this is also where post-shift
        //  `[DEL, INS]` orderings get collapsed to `[INS, DEL]` and
        //  non-EQ runs separated by len-0 EQs get merged).
        call(NEILCanon, edl);
        u32 n_after = (u32)(edl[0] - edl[2]);
        if (!changed && n_before == n_after) done;
    }
    done;
}

ok64 NEILCanon(e32g edl) {
    sane(edl != NULL);
    u32 n = (u32)(edl[0] - edl[2]);
    if (n == 0) done;

    <<<<a_carve(u32||||a_carve(u32>>>>, buf, n + 4);
    e32 *out = buf[0];
    u32 w = 0;

    u32 k = 0;
    while (k < n) {
        e32 e = edl[2][k];
        u32 op = DIFF_OP(e);
        u32 len = DIFF_LEN(e);
        if (len == 0) { k++; continue; }
        if (op == DIFF_EQ) {
            //  Coalesce adjacent EQ entries (NEIL passes don't usually
            //  produce them, but the canon is the right place to enforce
            //  it).  Keeps downstream walkers simple.
            if (w > 0 && DIFF_OP(out[w - 1]) == DIFF_EQ)
                out[w - 1] = DIFF_ENTRY(DIFF_EQ,
                                        DIFF_LEN(out[w - 1]) + len);
            else
                out[w++] = e;
            k++;
            continue;
        }
        //  Maximal non-EQ run starting at k.  Length-0 entries (of
        //  any op, including EQ) are transparent — `neil_shift_pass`
        //  can collapse an EQ to length 0 between two non-EQ runs,
        //  and breaking on it would leave the two runs as separate
        //  `(INS,DEL)` pairs in the output, violating in-rm canon.
        u32 sum_ins = 0, sum_del = 0;
        while (k < n) {
            e32 ek = edl[2][k];
            u32 op_k  = DIFF_OP(ek);
            u32 len_k = DIFF_LEN(ek);
            if (len_k == 0) { k++; continue; }
            if (op_k == DIFF_EQ) break;
            if (op_k == DIFF_INS) sum_ins += len_k;
            else if (op_k == DIFF_DEL) sum_del += len_k;
            k++;
        }
        //  Emit `INS sum, DEL sum` — the in-rm invariant.  Either may
        //  be zero (one-sided edit); skip empty entries.
        if (sum_ins > 0) out[w++] = DIFF_ENTRY(DIFF_INS, sum_ins);
        if (sum_del > 0) out[w++] = DIFF_ENTRY(DIFF_DEL, sum_del);
    }

    u32 ecap = (u32)(edl[1] - edl[2]);
    u32 final_n = (w < ecap) ? w : ecap;
    u32s  edst = {edl[2], edl[2] + final_n};
    u32cs esrc = {out, out + final_n};
    (void)u32sCopy(edst, esrc);
    edl[0] = edl[2] + final_n;

    done;
}
