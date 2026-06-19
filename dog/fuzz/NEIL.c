//
// NEIL fuzz test — properties of NEILCleanup + NEILShift + NEILCanon
// on EDLs produced by DIFFu64s.
//
// Input format: [old bytes] \0 [new bytes]
//
// Properties checked (any violation → assert via fail()):
//
//   1. Reconstruction.
//      Walking the post-NEIL EDL with the original old/new token streams
//      consumes exactly `ntoks(old)` from the old side and `ntoks(new)`
//      from the new side; for every EQ entry of length L the next L
//      tokens of old and new are byte-equal pairwise.
//
//   2. Token-count balance.
//      Σ EQ + Σ DEL == ntoks(old);  Σ EQ + Σ INS == ntoks(new).
//
//   3. Hygiene.
//      No length-0 entries.  No two adjacent entries with the same op
//      (NEILMerge invariant).
//
//   4. Substantive-EQ preservation.
//      For every alpha-numeric identifier of length ≥ 3 that DIFFu64s
//      placed in some EQ entry pre-NEIL (i.e. LCS classified it as
//      shared context), that identifier (by hashlet) must appear in
//      some EQ entry post-NEIL.  Catches the regression where NEIL
//      kills the `b8` / `if` / `for` shared-context tokens because
//      eq_bytes < surrounding edit bytes — see graf/BLAME.c for the
//      original repro.
//
//   5. Splice canon (in-rm).
//      Every maximal non-EQ run is exactly `[INS sum, DEL sum]` in that
//      order.  Either entry may be absent (one-sided edit) but no
//      DEL-then-INS, no rm-in-rm interleaving.
//
//   6. Shift idempotence.
//      A second NEILShift pass on the converged EDL produces the same
//      EDL byte-for-byte.
//

#include "dog/NEIL.h"
#include "JOIN.h"

#include <string.h>

#include "abc/DIFF.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/tok/TOK.h"

// Instantiate the u64 DIFF specialization (DIFFu64s, etc).
#define X(M, name) M##u64##name
#include "abc/DIFFx.h"
#undef X

#define NEIL_FUZZ_MAX 8192
#define NEIL_FUZZ_TOK_MAX 4096
#define NEIL_FUZZ_EDL_MAX 16384

// --- Helpers --------------------------------------------------------

static b8 neil_is_ident(u8cs tok) {
    if ($len(tok) < 3) return NO;
    for (u8cp p = tok[0]; p < tok[1]; p++) {
        u8 c = *p;
        b8 d = (c >= '0' && c <= '9');
        b8 lo = (c >= 'a' && c <= 'z');
        b8 up = (c >= 'A' && c <= 'Z');
        if (!d && !lo && !up && c != '_') return NO;
    }
    return YES;
}

static b8 hash_in(u64 h, u64 const *arr, u32 n) {
    for (u32 i = 0; i < n; i++) if (arr[i] == h) return YES;
    return NO;
}

// Property #1 + #2 + #3.
static ok64 neil_check_basic(e32 const *edl, u32 n,
                              JOINfile const *of, JOINfile const *nf) {
    sane(1);
    u32 oi = 0, ni = 0;
    u32 prev_op = 0xff;
    for (u32 k = 0; k < n; k++) {
        u32 op = DIFF_OP(edl[k]);
        u32 len = DIFF_LEN(edl[k]);
        if (len == 0) fail(FAILSANITY);
        if (k > 0 && op == prev_op) fail(FAILSANITY);
        prev_op = op;
        if (op == DIFF_EQ) {
            //  Pairwise byte-equal check: NEIL must never claim two
            //  non-equal tokens are EQ.
            for (u32 j = 0; j < len; j++) {
                if (oi + j >= u32bDataLen(of->toks) ||
                    ni + j >= u32bDataLen(nf->toks))
                    fail(FAILSANITY);
                u8cs ov = {}, nv = {};
                tok32Val(ov, of->toks, of->data[0], (int)(oi + j));
                tok32Val(nv, nf->toks, nf->data[0], (int)(ni + j));
                if ($len(ov) != $len(nv)) fail(FAILSANITY);
                if (memcmp(ov[0], nv[0], (size_t)$len(ov)) != 0)
                    fail(FAILSANITY);
            }
            oi += len; ni += len;
        } else if (op == DIFF_DEL) {
            oi += len;
        } else if (op == DIFF_INS) {
            ni += len;
        } else {
            fail(FAILSANITY);
        }
    }
    if (oi != u32bDataLen(of->toks)) fail(FAILSANITY);
    if (ni != u32bDataLen(nf->toks)) fail(FAILSANITY);
    done;
}

// Property #4.
static ok64 neil_check_idents_preserved(e32 const *pre_edl, u32 pre_n,
                                         e32 const *post_edl, u32 post_n,
                                         JOINfile const *of) {
    sane(1);

    //  Collect pre-NEIL EQ-identifier hashes.
    u64 pre_idents[1024];
    u32 pre_n_idents = 0;
    {
        u32 oi = 0;
        for (u32 k = 0; k < pre_n; k++) {
            u32 op = DIFF_OP(pre_edl[k]);
            u32 len = DIFF_LEN(pre_edl[k]);
            if (op == DIFF_EQ) {
                for (u32 j = 0; j < len && pre_n_idents < 1024; j++) {
                    u8cs v = {};
                    tok32Val(v, of->toks, of->data[0], (int)(oi + j));
                    if (!neil_is_ident(v)) continue;
                    u64 h = of->hashes[1][oi + j];
                    if (!hash_in(h, pre_idents, pre_n_idents))
                        pre_idents[pre_n_idents++] = h;
                }
                oi += len;
            } else if (op == DIFF_DEL) {
                oi += len;
            }
            // INS doesn't advance old cursor.
        }
    }
    if (pre_n_idents == 0) done;

    //  Collect post-NEIL EQ-identifier hashes.
    u64 post_idents[1024];
    u32 post_n_idents = 0;
    {
        u32 oi = 0;
        for (u32 k = 0; k < post_n; k++) {
            u32 op = DIFF_OP(post_edl[k]);
            u32 len = DIFF_LEN(post_edl[k]);
            if (op == DIFF_EQ) {
                for (u32 j = 0; j < len && post_n_idents < 1024; j++) {
                    u64 h = of->hashes[1][oi + j];
                    if (!hash_in(h, post_idents, post_n_idents))
                        post_idents[post_n_idents++] = h;
                }
                oi += len;
            } else if (op == DIFF_DEL) {
                oi += len;
            }
        }
    }

    for (u32 i = 0; i < pre_n_idents; i++) {
        if (!hash_in(pre_idents[i], post_idents, post_n_idents))
            fail(FAILSANITY);
    }
    done;
}

// Property #5.
static ok64 neil_check_canon(e32 const *edl, u32 n) {
    sane(1);
    u32 k = 0;
    while (k < n) {
        u32 op = DIFF_OP(edl[k]);
        if (op == DIFF_EQ) { k++; continue; }
        //  Maximal non-EQ run starting at k.  Allow exactly one INS
        //  followed by exactly one DEL, in that order, no repeats.
        b8 saw_ins = NO, saw_del = NO;
        while (k < n && DIFF_OP(edl[k]) != DIFF_EQ) {
            u32 op_k = DIFF_OP(edl[k]);
            if (op_k == DIFF_INS) {
                if (saw_ins || saw_del) fail(FAILSANITY);
                saw_ins = YES;
            } else if (op_k == DIFF_DEL) {
                if (saw_del) fail(FAILSANITY);
                saw_del = YES;
            } else fail(FAILSANITY);
            k++;
        }
    }
    done;
}

// --- Wrap DIFFu64s + NEIL in a single shot ---

static ok64 neil_run_diff(e32 *out_edl, u32 cap, u32 *out_n,
                           JOINfile const *of, JOINfile const *nf) {
    sane(out_edl && out_n);
    u64 olen = u64bDataLen(of->hashes);
    u64 nlen = u64bDataLen(nf->hashes);
    u64 ws_size = DIFFWorkSize(olen, nlen);
    u64 emax = DIFFEdlMaxEntries(olen, nlen);
    if (emax == 0 || emax > cap) fail(FAILSANITY);

    Bi32 work = {};
    call(i32bAllocate, work, ws_size);

    e32g edl = {out_edl, out_edl + emax, out_edl};
    i32s ws = {i32bHead(work), i32bTerm(work)};
    u64cs oh = {of->hashes[1], of->hashes[2]};
    u64cs nh = {nf->hashes[1], nf->hashes[2]};

    ok64 r = DIFFu64s(edl, ws, oh, nh);
    *out_n = (u32)(edl[0] - edl[2]);
    i32bFree(work);
    return r;
}

// --- Entry point ---

FUZZ(u8, NEILfuzz) {
    sane(1);
    if ($len(input) > NEIL_FUZZ_MAX || $len(input) < 4) done;

    //  Split on the first \0.
    u8cp sep = NULL;
    $for(u8c, p, input) {
        if (*p == 0) { sep = p; break; }
    }
    if (sep == NULL) done;

    a_head(u8c, old_data, input, sep - input[0]);
    a_rest(u8c, after_sep, input, sep - input[0] + 1);
    if ($empty(old_data) || $empty(after_sep)) done;
    a_head(u8c, new_data, after_sep, $len(after_sep));

    u8csc c_ext = {(u8cp)"c", (u8cp)"c" + 1};

    JOINfile of = {}, nf = {};
    ok64 to = JOINTokenize(&of, old_data, c_ext);
    if (to == OK) to = JOINTokenize(&nf, new_data, c_ext);
    if (to != OK) {
        JOINFree(&of); JOINFree(&nf); done;
    }
    u64 olen = u64bDataLen(of.hashes);
    u64 nlen = u64bDataLen(nf.hashes);
    if (olen == 0 || nlen == 0 ||
        olen > NEIL_FUZZ_TOK_MAX || nlen > NEIL_FUZZ_TOK_MAX) {
        JOINFree(&of); JOINFree(&nf); done;
    }
    u64 emax = DIFFEdlMaxEntries(olen, nlen);
    if (emax > NEIL_FUZZ_EDL_MAX) {
        JOINFree(&of); JOINFree(&nf); done;
    }

    //  Allocate three EDL buffers — pre-NEIL, post-NEIL, and a shift-
    //  idempotence scratch copy — back to back.
    u8 *mem[4] = {};
    if (u8bAlloc(mem, emax * sizeof(e32) * 3) != OK) {
        JOINFree(&of); JOINFree(&nf); done;
    }
    e32 *pre_edl  = (e32 *)mem[1];
    e32 *post_edl = pre_edl  + emax;
    e32 *shift_edl = post_edl + emax;

    u32 pre_n = 0;
    ok64 dr = neil_run_diff(pre_edl, (u32)emax, &pre_n, &of, &nf);
    if (dr != OK || pre_n == 0) {
        u8bFree(mem); JOINFree(&of); JOINFree(&nf); done;
    }

    //  Run NEIL on a copy.
    memcpy(post_edl, pre_edl, pre_n * sizeof(e32));
    e32g edl_g = {post_edl, post_edl + emax, post_edl};
    edl_g[0] = post_edl + pre_n;

    u32cs old_ts_view = {(u32cp)of.toks[1], (u32cp)of.toks[2]};
    u32cs new_ts_view = {(u32cp)nf.toks[1], (u32cp)nf.toks[2]};
    u8cs old_src_view = {old_data[0], old_data[1]};
    u8cs new_src_view = {new_data[0], new_data[1]};
    NEILCleanup(edl_g, old_ts_view, new_ts_view,
                old_src_view, new_src_view);
    NEILShift  (edl_g, old_ts_view, new_ts_view,
                old_src_view, new_src_view);
    u32 post_n = (u32)(edl_g[0] - edl_g[2]);

    //  Property checks.
    ok64 c1 = neil_check_basic(post_edl, post_n, &of, &nf);
    ok64 c4 = (c1 == OK)
        ? neil_check_idents_preserved(pre_edl, pre_n,
                                       post_edl, post_n, &of)
        : c1;
    ok64 c5 = (c4 == OK)
        ? neil_check_canon(post_edl, post_n)
        : c4;

    //  Property #6: shift idempotence.
    ok64 c6 = OK;
    if (c5 == OK) {
        memcpy(shift_edl, post_edl, post_n * sizeof(e32));
        e32g sg = {shift_edl, shift_edl + emax, shift_edl};
        sg[0] = shift_edl + post_n;
        NEILShift(sg, old_ts_view, new_ts_view,
                  old_src_view, new_src_view);
        u32 shift_n = (u32)(sg[0] - sg[2]);
        if (shift_n != post_n ||
            memcmp(shift_edl, post_edl, post_n * sizeof(e32)) != 0)
            c6 = FAILSANITY;
    }

    u8bFree(mem);
    JOINFree(&of);
    JOINFree(&nf);

    if (c1 != OK) fail(c1);
    if (c4 != OK) fail(c4);
    if (c5 != OK) fail(c5);
    if (c6 != OK) fail(c6);
    done;
}
