//  WEAVE: token-level file history as four parallel arrays.
//  See WEAVE.h for the data layout and the splice-canonicalization rule.
//
//  Each operation (FromBlob, Diff, Merge) writes a fresh dst weave from
//  the inputs; the caller manages three weave instances and reuses them.
//
#include "WEAVE.h"

#include <string.h>

#include "abc/DIFF.h"
#include "abc/PRO.h"

// u64 diff specialization for token hashlets.
#define X(M, name) M##u64##name
#include "abc/DIFFx.h"
#undef X

// --- Buffer caps ---

#define WEAVE_TEXT_MAX  (64UL << 20)   // 64 MB token-text storage
#define WEAVE_TOK_MAX   (1 << 20)      // 1M tokens per weave

// --- init / reset / free ---

ok64 WEAVEInit(weave *w) {
    sane(w);
    memset(w, 0, sizeof(*w));
    call(u8bMap,   w->text,     WEAVE_TEXT_MAX);
    call(u32bMap,  w->toks,     WEAVE_TOK_MAX);
    call(u64bMap,  w->hashlets, WEAVE_TOK_MAX);
    call(inrmbMap, w->inrm,     WEAVE_TOK_MAX);
    done;
}

void WEAVEReset(weave *w) {
    if (!w) return;
    u8bReset  (w->text);
    u32bReset (w->toks);
    u64bReset (w->hashlets);
    inrmbReset(w->inrm);
}

void WEAVEFree(weave *w) {
    if (!w) return;
    if (w->text[0])     u8bUnMap  (w->text);
    if (w->toks[0])     u32bUnMap (w->toks);
    if (w->hashlets[0]) u64bUnMap (w->hashlets);
    if (w->inrm[0])     inrmbUnMap(w->inrm);
    memset(w, 0, sizeof(*w));
}

// --- Tokenization helper for FromBlob ---

typedef struct {
    weave *w;
    u8cp   base;
} weave_blob_ctx;

static ok64 weave_blob_cb(u8 tag, u8cs tok, void *vctx) {
    sane(vctx);
    weave_blob_ctx *ctx = vctx;
    u32 end = (u32)(tok[1] - ctx->base);
    call(u32bFeed1, ctx->w->toks,     tok32Pack(tag, end));
    call(u64bFeed1, ctx->w->hashlets, RAPHash(tok));
    done;
}

ok64 WEAVEFromBlob(weave *w, u8cs data, u8cs ext, u32 src) {
    sane(w);
    WEAVEReset(w);
    if ($empty(data)) done;

    //  Tokenizer writes tok32 end-offsets relative to data[0].  We
    //  then copy data verbatim into w->text, so those offsets are
    //  correct in w->text coordinates as well.
    weave_blob_ctx ctx = {.w = w, .base = data[0]};
    TOKstate st = {.data = {data[0], data[1]}, .cb = weave_blob_cb, .ctx = &ctx};
    TOKLexer(&st, ext);  // best effort

    //  Fallback: lexer produced no tokens (binary / unknown ext).
    if (u32bDataLen(w->toks) == 0) {
        u32 end = (u32)$len(data);
        call(u32bFeed1, w->toks, tok32Pack('S', end));
        call(u64bFeed1, w->hashlets, RAPHash(data));
    }

    //  Append data bytes to w->text.  Offsets in w->toks now match.
    call(u8bFeed, w->text, data);

    //  Stamp inrm for every token: { in = src, rm = 0 }.
    u32 n = (u32)u32bDataLen(w->toks);
    for (u32 i = 0; i < n; i++) {
        inrm e = {.in = src, .rm = 0};
        call(inrmbFeed1, w->inrm, e);
    }
    done;
}

// --- Compose helpers (write into dst) ---

//  Append token i from a source (text+toks+hashlets) into dst, with the
//  given inrm.  Token i's bytes span [lo, hi) in src_text where
//  lo = tok32Offset(src_toks[i-1]) (or 0 for i==0) and
//  hi = tok32Offset(src_toks[i]).  dst's tok32 records the new
//  cumulative end-offset within dst->text.
static ok64 weave_append(weave *dst,
                         u8cp  src_text,
                         u32cp src_toks,
                         u64cp src_hash,
                         u32   i,
                         inrm  e) {
    sane(dst);
    u32 lo = (i == 0) ? 0 : tok32Offset(src_toks[i - 1]);
    u32 hi = tok32Offset(src_toks[i]);
    u8cs ts = {src_text + lo, src_text + hi};
    u8 tag = tok32Tag(src_toks[i]);
    call(u8bFeed, dst->text, ts);
    u32 new_end = (u32)u8bDataLen(dst->text);
    call(u32bFeed1,  dst->toks,     tok32Pack(tag, new_end));
    call(u64bFeed1,  dst->hashlets, src_hash[i]);
    call(inrmbFeed1, dst->inrm,     e);
    done;
}

// --- WEAVEDiff ---

ok64 WEAVEDiff(weave *dst, weave const *src, weave const *nu, u32 src_commit) {
    sane(dst);
    if (!src || !nu) return FAILSANITY;
    WEAVEReset(dst);

    //  Direct const reads via b[1] (DATA head) and b[2] (DATA end).
    u32cp  src_toks = (u32cp)src->toks[1];
    u32cp  src_toks_e = (u32cp)src->toks[2];
    u32    src_len = (u32)(src_toks_e - src_toks);
    u8cp   src_text = (u8cp)src->text[1];
    u64cp  src_hash = (u64cp)src->hashlets[1];
    inrmcp src_irm  = (inrmcp)src->inrm[1];

    u32cp  nu_toks  = (u32cp)nu->toks[1];
    u32cp  nu_toks_e = (u32cp)nu->toks[2];
    u32    nu_len   = (u32)(nu_toks_e - nu_toks);
    u8cp   nu_text  = (u8cp)nu->text[1];
    u64cp  nu_hash  = (u64cp)nu->hashlets[1];

    //  Empty src: copy nu wholesale, restamping in=src_commit on each.
    if (src_len == 0) {
        for (u32 i = 0; i < nu_len; i++) {
            inrm e = {.in = src_commit, .rm = 0};
            call(weave_append, dst, nu_text, nu_toks, nu_hash, i, e);
        }
        done;
    }

    //  Project alive src hashlets for diff input.
    Bu64 alive_h = {};
    Bi32 work    = {};
    Bu32 edlbuf  = {};

    __ = u64bAlloc(alive_h, src_len + 1); if (__ != OK) goto cleanup;
    for (u32 i = 0; i < src_len; i++) {
        if (src_irm[i].rm == 0) {
            __ = u64bFeed1(alive_h, src_hash[i]); if (__ != OK) goto cleanup;
        }
    }

    u64 olen = u64bDataLen(alive_h);
    u64 nlen = (u64)nu_len;

    u64 work_sz = DIFFWorkSize(olen, nlen);
    u64 edl_sz  = DIFFEdlMaxEntries(olen, nlen);
    if (work_sz > 0) { __ = i32bAllocate(work,   work_sz); if (__ != OK) goto cleanup; }
    if (edl_sz  > 0) { __ = u32bAllocate(edlbuf, edl_sz);  if (__ != OK) goto cleanup; }

    u64cs oh = {u64bDataHead(alive_h), u64bDataHead(alive_h) + olen};
    u64cs nh = {nu_hash, nu_hash + nlen};

    e32g edlg = {edlbuf[0], edlbuf[3], edlbuf[0]};
    i32s ws = {i32bHead(work), i32bTerm(work)};
    ok64 diff_o = DIFFu64s(edlg, ws, oh, nh);
    if (diff_o != OK) {
        //  Fallback: replace alive prefix wholesale.
        edlg[1] = edlg[0];
        if (olen > 0) { *edlg[1]++ = DIFF_ENTRY(DIFF_DEL, (u32)olen); }
        if (nlen > 0) { *edlg[1]++ = DIFF_ENTRY(DIFF_INS, (u32)nlen); }
    }
    e32c *ep = edlbuf[0];
    e32c *ee = edlg[0];

    //  Walk EDL, canonicalizing each non-EQ run to INS-then-DEL on the fly.
    u32 wi = 0;  // cursor in src (full, includes dead tokens)
    u32 ni = 0;  // cursor in nu (full)

    while (ep < ee) {
        u32 op  = DIFF_OP(*ep);
        u32 len = DIFF_LEN(*ep);

        if (op == DIFF_EQ) {
            for (u32 j = 0; j < len; j++) {
                //  Carry any dead src tokens through.
                while (wi < src_len && src_irm[wi].rm != 0) {
                    __ = weave_append(dst, src_text, src_toks, src_hash,
                                      wi, src_irm[wi]);
                    if (__ != OK) goto cleanup;
                    wi++;
                }
                if (wi < src_len) {
                    __ = weave_append(dst, src_text, src_toks, src_hash,
                                      wi, src_irm[wi]);
                    if (__ != OK) goto cleanup;
                    wi++;
                }
                ni++;   // EQ advances both src-alive cursor and nu cursor
            }
            ep++;
            continue;
        }

        //  Non-EQ run: gather totals, then emit INS first then DEL.
        u32 sum_ins = 0, sum_del = 0;
        while (ep < ee && DIFF_OP(*ep) != DIFF_EQ) {
            u32 l = DIFF_LEN(*ep);
            if (DIFF_OP(*ep) == DIFF_INS) sum_ins += l;
            else                           sum_del += l;
            ep++;
        }

        //  INS: append nu tokens with in=src_commit, rm=0.
        for (u32 j = 0; j < sum_ins; j++) {
            inrm e = {.in = src_commit, .rm = 0};
            __ = weave_append(dst, nu_text, nu_toks, nu_hash, ni, e);
            if (__ != OK) goto cleanup;
            ni++;
        }

        //  DEL: drain dead, then mark each alive token rm=src_commit.
        for (u32 j = 0; j < sum_del; j++) {
            while (wi < src_len && src_irm[wi].rm != 0) {
                __ = weave_append(dst, src_text, src_toks, src_hash,
                                  wi, src_irm[wi]);
                if (__ != OK) goto cleanup;
                wi++;
            }
            if (wi < src_len) {
                inrm e = src_irm[wi];
                e.rm = src_commit;
                __ = weave_append(dst, src_text, src_toks, src_hash, wi, e);
                if (__ != OK) goto cleanup;
                wi++;
            }
        }
    }

    //  Tail: drain any remaining src tokens (all should be dead, but
    //  copy verbatim regardless to keep the invariant that dst
    //  contains every src token plus any inserts).
    while (wi < src_len) {
        __ = weave_append(dst, src_text, src_toks, src_hash, wi, src_irm[wi]);
        if (__ != OK) goto cleanup;
        wi++;
    }

cleanup:
    u64bFree(alive_h);
    i32bFree(work);
    u32bFree(edlbuf);
    return __;
}

// --- WEAVEMerge: stub ---

ok64 WEAVEMerge(weave *dst, weave const *a, weave const *b) {
    sane(dst);
    (void)a; (void)b;
    return WEAVEFAIL;  // TODO: concurrent-branch weave merge
}
