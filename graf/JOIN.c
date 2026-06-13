//
// JOIN.c - Token-level 3-way file merge
//

#include "JOIN.h"

#include "abc/PRO.h"
#include "graf/NEIL.h"

// Instantiate Myers diff for u64 hash sequences
#define X(M, name) M##u64##name
#include "abc/DIFFx.h"
#undef X

// --- Tokenizer callback ---

typedef struct {
    u32 **toks;
    u64 **hashes;
    u8cp base;
} join_ctx;

static ok64 join_cb(u8 tag, u8cs tok, void *vctx) {
    sane(vctx != NULL);
    join_ctx *ctx = vctx;
    u32 end = (u32)(tok[1] - ctx->base);
    call(u32bFeed1, ctx->toks, tok32Pack(tag,end));
    u64 h = RAPHash(tok);
    call(u64bFeed1, ctx->hashes, h);
    done;
}

ok64 JOINTokenize(JOINfile *jf, u8csc data, u8csc ext) {
    sane(jf != NULL);
    $set(jf->data, data);
    u64 est = $len(data);
    if (est < 256) est = 256;
    call(u32bAlloc, jf->toks, est);
    call(u64bAlloc, jf->hashes, est);
    join_ctx ctx = {
        .toks = jf->toks,
        .hashes = jf->hashes,
        .base = data[0],
    };
    TOKstate st = {.data = {data[0], data[1]}, .cb = join_cb, .ctx = &ctx};
    call(TOKLexer, &st, ext);
    done;
}

void JOINFree(JOINfile *jf) {
    if (jf == NULL) return;
    u32bFree(jf->toks);
    u64bFree(jf->hashes);
}

// --- EDL marking ---

// Walk EDL, set rm_bit on base hashes where DEL
static ok64 join_mark_base(u64bp bh, e32cs edl, u64 rm_bit) {
    sane(bh != NULL);
    u64 bi = 0;
    $for(e32c, ep, edl) {
        u32 len = DIFF_LEN(*ep);
        switch (DIFF_OP(*ep)) {
        case DIFF_EQ:
            bi += len;
            break;
        case DIFF_DEL:
            for (u32 j = 0; j < len; j++)
                bh[1][bi + j] |= rm_bit;
            bi += len;
            break;
        case DIFF_INS:
            break;
        }
    }
    done;
}

// Walk EDL, set JOIN_IN on side hashes where INS
static ok64 join_mark_side(u64bp sh, e32cs edl) {
    sane(sh != NULL);
    u64 si = 0;
    $for(e32c, ep, edl) {
        u32 len = DIFF_LEN(*ep);
        switch (DIFF_OP(*ep)) {
        case DIFF_EQ:
            si += len;
            break;
        case DIFF_INS:
            for (u32 j = 0; j < len; j++)
                sh[1][si + j] |= JOIN_IN;
            si += len;
            break;
        case DIFF_DEL:
            break;
        }
    }
    done;
}

// --- Emit token bytes ---

static ok64 join_emit(u8bp out, JOINfile const *jf, u64 ti) {
    sane(out != NULL && jf != NULL);
    u32 lo = (ti > 0) ? tok32Offset(jf->toks[1][ti - 1]) : 0;
    u32 hi = tok32Offset(jf->toks[1][ti]);
    //  24-bit tok32 offsets wrap for blobs >= 16 MiB; hi < lo would
    //  underflow hi-lo into an out-of-bounds slice fed to u8bFeed.
    test(hi >= lo, TOKBAD);
    a_rest(u8c, r, jf->data, lo);
    a_head(u8c, tok, r, hi - lo);
    call(u8bFeed, out, tok);
    done;
}

// --- 3-way merge ---

ok64 JOINMerge(u8bp out, JOINfile *base, JOINfile *ours, JOINfile *theirs) {
    sane(out && base && ours && theirs);

    u64 bn = u64bDataLen(base->hashes);
    u64 on = u64bDataLen(ours->hashes);
    u64 tn = u64bDataLen(theirs->hashes);

    // Trivial: empty base, take ours
    if (bn == 0) {
        call(u8bFeed, out, ours->data);
        done;
    }

    // Work buffer sizes
    u64 wsize = DIFFWorkSize(bn, on);
    {
        u64 w2 = DIFFWorkSize(bn, tn);
        if (w2 > wsize) wsize = w2;
    }
    u64 emax_o = DIFFEdlMaxEntries(bn, on);
    u64 emax_t = DIFFEdlMaxEntries(bn, tn);

    //  Diff workspace + two EDL buffers (e32 == u32) as separate BASS
    //  carves — no manual sub-allocation pointer math (mirrors WEAVE.c).
    a_carve(i32, work, wsize);
    a_carve(u32, edl_o_buf, emax_o);
    a_carve(u32, edl_t_buf, emax_t);

    i32s ws = {i32bHead(work), i32bTerm(work)};
    //  DIFF gauge layout is {start, end, cursor}; cursor starts at start.
    e32g edl_o = {edl_o_buf[0], edl_o_buf[3], edl_o_buf[0]};
    e32g edl_t = {edl_t_buf[0], edl_t_buf[3], edl_t_buf[0]};

    // Clear mark bits from all hashes before diffing
    {
        u64s bhm = {base->hashes[1], base->hashes[2]};
        $for(u64, hp, bhm) { *hp &= ~JOIN_MARK; }
        u64s ohm = {ours->hashes[1], ours->hashes[2]};
        $for(u64, hp, ohm) { *hp &= ~JOIN_MARK; }
        u64s thm = {theirs->hashes[1], theirs->hashes[2]};
        $for(u64, hp, thm) { *hp &= ~JOIN_MARK; }
    }

    // Compute diffs: base->ours, base->theirs
    u64cs bh = {base->hashes[1], base->hashes[2]};
    u64cs oh = {ours->hashes[1], ours->hashes[2]};
    u64cs th = {theirs->hashes[1], theirs->hashes[2]};

    call(DIFFu64s, edl_o, ws, bh, oh);
    call(DIFFu64s, edl_t, ws, bh, th);

    // NEIL semantic cleanup on both EDLs: kills false short EQs (lone
    // brackets, punctuation, lone whitespace tokens that LCS picked up
    // across genuinely divergent code), then lossless boundary shift
    // toward line/word breaks.  Without these, JOINMerge frames
    // conflicts around tiny shared tokens — `<<<<);` and similar
    // bracket-glue artifacts — instead of expanding markers to whole
    // lines.  In-place; both passes safe to ignore-on-error (the EDL
    // remains valid, just unsmoothed).
    {
        u32cs base_tv  = {base->toks[1],   base->toks[2]};
        u32cs ours_tv  = {ours->toks[1],   ours->toks[2]};
        u32cs theirs_tv= {theirs->toks[1], theirs->toks[2]};
        u8cs  base_dv  = {base->data[0],   base->data[1]};
        u8cs  ours_dv  = {ours->data[0],   ours->data[1]};
        u8cs  theirs_dv= {theirs->data[0], theirs->data[1]};
        NEILCleanup(edl_o, base_tv, ours_tv,   base_dv, ours_dv);
        NEILShift  (edl_o, base_tv, ours_tv,   base_dv, ours_dv);
        NEILCleanup(edl_t, base_tv, theirs_tv, base_dv, theirs_dv);
        NEILShift  (edl_t, base_tv, theirs_tv, base_dv, theirs_dv);
    }

    // Mark hashes
    e32cs edl_ocs = {edl_o[2], edl_o[0]};
    e32cs edl_tcs = {edl_t[2], edl_t[0]};

    call(join_mark_base, base->hashes, edl_ocs, JOIN_RM_O);
    call(join_mark_base, base->hashes, edl_tcs, JOIN_RM_T);
    call(join_mark_side, ours->hashes, edl_ocs);
    call(join_mark_side, theirs->hashes, edl_tcs);

    // Lockstep merge walk
    u64 bi = 0, oi = 0, ti = 0;

    while (oi < on || ti < tn || bi < bn) {
        b8 o_in = (oi < on) && (ours->hashes[1][oi] & JOIN_IN);
        b8 t_in = (ti < tn) && (theirs->hashes[1][ti] & JOIN_IN);

        if (o_in && t_in) {
            u64 oh_val = JOIN_HASH(ours->hashes[1][oi]);
            u64 th_val = JOIN_HASH(theirs->hashes[1][ti]);
            if (oh_val == th_val) {
                call(join_emit, out, ours, oi);
                oi++;
                ti++;
            } else {
                //  Divergent inserts — inline conflict markers:
                //  >>>>theirs||||ours<<<<
                u8cs mk_open  = u8slit(">>>>");
                u8cs mk_mid   = u8slit("||||");
                u8cs mk_close = u8slit("<<<<");
                call(u8bFeed, out, mk_open);
                while (ti < tn && (theirs->hashes[1][ti] & JOIN_IN)) {
                    call(join_emit, out, theirs, ti);
                    ti++;
                }
                call(u8bFeed, out, mk_mid);
                while (oi < on && (ours->hashes[1][oi] & JOIN_IN)) {
                    call(join_emit, out, ours, oi);
                    oi++;
                }
                call(u8bFeed, out, mk_close);
            }
        } else if (o_in) {
            call(join_emit, out, ours, oi);
            oi++;
        } else if (t_in) {
            call(join_emit, out, theirs, ti);
            ti++;
        } else if (bi < bn) {
            u64 bmark = base->hashes[1][bi] & JOIN_MARK;
            if (bmark == 0) {
                // Both kept this base token: emit from ours
                call(join_emit, out, ours, oi);
                bi++;
                oi++;
                ti++;
            } else {
                // Deleted by at least one side: skip, advance who kept it
                if (!(bmark & JOIN_RM_O)) oi++;  // ours kept, advance ours
                if (!(bmark & JOIN_RM_T)) ti++;  // theirs kept, advance theirs
                bi++;
            }
        }
    }

    done;
}
