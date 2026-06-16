//  WEAVE: token-level file history as a single interleaved-delta TLV
//  stream.  See WEAVE.h for the record layout.  Each token carries a
//  stable BIRTH-ID (seq, pos).  The whole commit DAG is replayed into ONE
//  weave: WEAVEApply diffs a commit's content against its parent-closure
//  view and inserts/deletes in place, never reordering existing tokens —
//  so concurrent branches coexist without re-derivation.
//
#include "WEAVE.h"

#include <string.h>

#include "abc/DIFF.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/TLV.h"
#include "abc/ZINT.h"
#include "graf/BRAM.h"
#include "graf/NEIL.h"

// u64 diff specialization (token hashlets AND birth-id keys).
#define X(M, name) M##u64##name
#include "abc/DIFFx.h"
#undef X

#define WEAVE_TLV_MAX  (96UL << 20)   // 96 MB per weave TLV stream

//  BASS acquire must NOT go through call() (it snapshots+rewinds BASS).
#define BACQ(expr) do { __ = (expr); if (__ != OK) return __; } while (0)

//  Wholesale DEL+INS fallback for the diff-core EDL.  See WEAVE.h.
//  Rewinds the gauge cursor to its base (dropping any partial BRAM
//  output), then appends DEL(olen)+INS(nlen) through the bounds-checked
//  DIFFu64AddEntry — no raw pointer writes, NOROOM propagates, edl[0]
//  advances so downstream n=edl[0]-edl[2] readers see the entries.
ok64 WEAVEFallbackEdl(e32g edl, u32 olen, u32 nlen) {
    sane(edl != NULL);
    edl[0] = edl[2];                       // rewind cursor to base
    call(DIFFu64AddEntry, edl, DIFF_DEL, olen);
    call(DIFFu64AddEntry, edl, DIFF_INS, nlen);
    done;
}

// ============================================================
//  TLV record writers
// ============================================================

//  Append one TLV record (header + value) to a buffer; feeding into IDLE
//  advances DATA directly (commit).
static ok64 weave_rec(u8b tlv, u8 lit, u8csc val) {
    sane(tlv);
    call(TLVu8sFeed, u8bIdle(tlv), lit, val);
    done;
}

//  Append a `T` record: a 1-byte birth-id length, ZINT128(seq, pos), then
//  the token bytes.  The length byte delimits the birth-id from the text
//  (ZINT128 itself decodes by slice length, so it can't be self-delimited
//  inline — and (0,0) encodes to zero bytes).
static ok64 weave_rec_T(u8b tlv, u32 seq, u32 pos, u8csc text) {
    sane(tlv);
    u32    idl   = ZINT128len((u64)seq, (u64)pos);
    size_t total = (size_t)1 + idl + u8csLen(text);
    //  Feeding into the buffer's IDLE slice advances DATA directly, so no
    //  u8bFed afterwards (that would double-count and leave a gap).
    if (total <= 0xff) {
        call(u8bFeed1, tlv, (u8)(WEAVE_REC_T | TLVaA));
        call(u8bFeed1, tlv, (u8)total);
    } else {
        call(u8bFeed1, tlv, WEAVE_REC_T);
        u32 t32 = (u32)total;
        call(u8sFeed32, u8bIdle(tlv), &t32);
    }
    call(u8bFeed1, tlv, (u8)idl);
    call(ZINTu8sFeed128, u8bIdle(tlv), (u64)seq, (u64)pos);
    call(u8bFeed, tlv, text);
    done;
}

//  Append an `R` record: the sorted remover seqs as blocked varints.
static ok64 weave_rec_R(u8b tlv, u32 const *set, u32 n) {
    sane(tlv);
    a_pad(u8, sb, 16 + 8 * WEAVE_SET_MAX);
    u64 tmp[WEAVE_SET_MAX];
    for (u32 i = 0; i < n; i++) tmp[i] = set[i];
    u64cs tc = {tmp, tmp + n};
    call(ZINTu8sFeedBlocked, u8bIdle(sb), tc);
    call(weave_rec, tlv, WEAVE_REC_R, u8bDataC(sb));
    done;
}

static b8 weave_set_eq(u32 const *a, u32 na, u32 const *b, u32 nb) {
    if (na != nb) return NO;
    for (u32 i = 0; i < na; i++) if (a[i] != b[i]) return NO;
    return YES;
}

//  Sorted merge-union of two sorted/deduped sets.
static ok64 weave_set_union(u32cs aset, u32 const *b, u32 nb,
                            u32 *out, u32 cap, u32 *n) {
    u32 const *a = aset[0];
    u32 na = (u32)u32csLen(aset);
    u32 i = 0, j = 0, c = 0;
    #define PUSH(V) do { u32 _v = (V); \
        if (!(c > 0 && out[c - 1] == _v)) { \
            if (c >= cap) return WEAVEFAIL; out[c++] = _v; } } while (0)
    while (i < na && j < nb) {
        if (a[i] < b[j])       { PUSH(a[i]); i++; }
        else if (b[j] < a[i])  { PUSH(b[j]); j++; }
        else                   { PUSH(a[i]); i++; j++; }
    }
    while (i < na) { PUSH(a[i]); i++; }
    while (j < nb) { PUSH(b[j]); j++; }
    #undef PUSH
    *n = c;
    return OK;
}

// ============================================================
//  Builder
// ============================================================

void WEAVEBldInit(weavebld *b, weave *w) {
    zerop(b);
    b->w = w;
    WEAVEReset(w);
}

ok64 WEAVEBldPut(weavebld *b, u8csc text, u32 seq, u32 pos, u32cs rset) {
    sane(b && b->w);
    u32 const *rp = (u32 const *)rset[0];
    u32        nr = (u32)u32csLen(rset);
    if (nr > WEAVE_SET_MAX) return WEAVEFAIL;
    if (!b->started || !weave_set_eq(b->prevr, b->npr, rp, nr)) {
        call(weave_rec_R, b->w->tlv, rp, nr);
        for (u32 i = 0; i < nr; i++) b->prevr[i] = rp[i];
        b->npr = nr;
    }
    b->started = YES;
    call(weave_rec_T, b->w->tlv, seq, pos, text);
    done;
}

// ============================================================
//  Cursor
// ============================================================

void WEAVECurInit(weavecur *c, weave const *w) {
    zerop(c);
    u8csMv(c->rest, u8bDataC(w->tlv));
}

b8 WEAVECurNext(weavecur *c) {
    while (u8csLen(c->rest) > 0) {
        u8 type = 0;
        u8cs val = {};
        if (TLVu8sDrain(c->rest, &type, val) != OK) { c->bad = YES; return NO; }
        if (type == WEAVE_REC_R) {
            u64 tmp[WEAVE_SET_MAX];
            u64s ts = {tmp, tmp + WEAVE_SET_MAX};
            u8cs v = {};
            u8csMv(v, val);
            if (ZINTu8sDrainBlocked(v, ts) != OK) { c->bad = YES; return NO; }
            c->nr = (u32)(ts[0] - tmp);
            for (u32 i = 0; i < c->nr; i++) c->rset[i] = (u32)tmp[i];
        } else if (type == WEAVE_REC_T) {
            u8cp p = (u8cp)val[0], e = (u8cp)val[1];
            if (p >= e) { c->bad = YES; return NO; }
            u8 idl = p[0];                  // birth-id length
            if (p + 1 + idl > e) { c->bad = YES; return NO; }
            u8cs bid = {p + 1, p + 1 + idl};
            u64 seq = 0, pos = 0;
            if (ZINTu8sDrain128(bid, &seq, &pos) != OK) { c->bad = YES; return NO; }
            //  Guard the u32 narrowing: a corrupt ZINT could carry a
            //  value that silently truncates.  WEAVE_WT_SRC (0xFFFFFFFF)
            //  is the largest legitimate src and must still pass, so
            //  reject strictly above it.
            if (seq > 0xFFFFFFFFu || pos > 0xFFFFFFFFu) { c->bad = YES; return NO; }
            c->seq = (u32)seq;
            c->pos = (u32)pos;
            u8cs txt = {p + 1 + idl, e};
            u8csMv(c->text, txt);
            return YES;
        } else {
            c->bad = YES; return NO;
        }
    }
    return NO;
}

ok64 WEAVEAliveBytes(weave const *w, u8b out) {
    sane(w && out);
    u8bReset(out);
    weavecur c;
    WEAVECurInit(&c, w);
    while (WEAVECurNext(&c)) {
        if (c.nr != 0) continue;
        call(u8bFeed, out, c.text);
    }
    if (c.bad) return WEAVEFAIL;
    done;
}

// ============================================================
//  init / reset / free
// ============================================================

ok64 WEAVEInit(weave *w) {
    sane(w);
    zerop(w);
    call(u8bMap, w->tlv, WEAVE_TLV_MAX);
    done;
}

void WEAVEReset(weave *w) {
    if (!w) return;
    u8bReset(w->tlv);
}

void WEAVEFree(weave *w) {
    if (!w) return;
    if (w->tlv[0]) u8bUnMap(w->tlv);
    zerop(w);
}

// ============================================================
//  Transient decode to parallel arrays
// ============================================================

typedef struct {
    Bu8  text;
    Bu32 tok;                 // [ntok] cumulative end offsets
    Bu32 seq, pos;            // [ntok] birth-id
    Bu32 rpool, roff, rlen;   // token i R-set = rpool[roff[i] .. +rlen[i])
    u32  ntok;
} wdec;

typedef struct {
    u8cs  base;               // range-accessed: decoded token bytes
    u32cp tok; u32 ntok;      // index-accessed: cumulative end offsets
    u32cp seq, pos;           // index-accessed: birth-id columns
    u32cs rpool;              // range-accessed: R-set pool
    u32cp roff, rlen;         // index-accessed: per-token R-set window
} wdp;

static void wd_view(wdp *p, wdec const *d) {
    u8csMv(p->base, u8bDataC(d->text));
    p->tok   = (u32cp)u32bDataHead(d->tok);
    p->ntok  = d->ntok;
    p->seq   = (u32cp)u32bDataHead(d->seq);
    p->pos   = (u32cp)u32bDataHead(d->pos);
    u32csMv(p->rpool, u32bDataC(d->rpool));
    p->roff  = (u32cp)u32bDataHead(d->roff);
    p->rlen  = (u32cp)u32bDataHead(d->rlen);
}

//  Same view, over a persistent (heap-backed) weavedec.
static void wd_view_dec(wdp *p, weavedec const *d) {
    u8csMv(p->base, u8bDataC(d->text));
    p->tok   = (u32cp)u32bDataHead(d->tok);
    p->ntok  = d->ntok;
    p->seq   = (u32cp)u32bDataHead(d->seq);
    p->pos   = (u32cp)u32bDataHead(d->pos);
    u32csMv(p->rpool, u32bDataC(d->rpool));
    p->roff  = (u32cp)u32bDataHead(d->roff);
    p->rlen  = (u32cp)u32bDataHead(d->rlen);
}

static u32 wd_lo(wdp const *p, u32 i) { return i ? p->tok[i - 1] : 0; }
static u32 wd_hi(wdp const *p, u32 i) { return p->tok[i]; }
static b8  wd_alive(wdp const *p, u32 i) { return p->rlen[i] == 0; }

//  Fill a wdec whose buffers the CALLER already acquired (empty).
static ok64 weave_decode_fill(wdec *d, weave const *w) {
    sane(d && w);
    weavecur c;
    WEAVECurInit(&c, w);
    u32 cur_roff = 0, cur_rlen = 0;
    b8 dirty_r = YES;
    u32 prev_r[WEAVE_SET_MAX], prev_nr = 0;
    u32 n = 0;
    while (WEAVECurNext(&c)) {
        if (dirty_r || !weave_set_eq(prev_r, prev_nr, c.rset, c.nr)) {
            cur_roff = (u32)u32bDataLen(d->rpool);
            cur_rlen = c.nr;
            for (u32 i = 0; i < c.nr; i++) call(u32bFeed1, d->rpool, c.rset[i]);
            memcpy(prev_r, c.rset, c.nr * sizeof(u32));
            prev_nr = c.nr; dirty_r = NO;
        }
        call(u8bFeed, d->text, c.text);
        call(u32bFeed1, d->tok, (u32)u8bDataLen(d->text));
        call(u32bFeed1, d->seq, c.seq);
        call(u32bFeed1, d->pos, c.pos);
        call(u32bFeed1, d->roff, cur_roff);
        call(u32bFeed1, d->rlen, cur_rlen);
        n++;
    }
    if (c.bad) return WEAVEFAIL;
    d->ntok = n;
    done;
}

//  Acquire (in the CALLER's scope) and fill a wdec for weave `W`.
#define WEAVE_DECODE(D, W) do {                                           \
    size_t _tl = u8bDataLen((W)->tlv);                                    \
    size_t _mt = _tl / 2 + 2;                                             \
    /*  rpool holds one u32 per stored remover; a delete-heavy weave can  \
        be almost all R records, and each remover is ~1 encoded byte, so  \
        the pool can need up to _tl slots (a _tl/4 guess under-sizes and  \
        SNOROOMs once removers dominate). */                              \
    size_t _ms = _tl + 2;                                                 \
    BACQ(u8bAcquire( ABC_BASS, (D).text,  _tl + 1));                      \
    BACQ(u32bAcquire(ABC_BASS, (D).tok,   _mt));                          \
    BACQ(u32bAcquire(ABC_BASS, (D).seq,   _mt));                          \
    BACQ(u32bAcquire(ABC_BASS, (D).pos,   _mt));                          \
    BACQ(u32bAcquire(ABC_BASS, (D).rpool, _ms));                         \
    BACQ(u32bAcquire(ABC_BASS, (D).roff,  _mt));                          \
    BACQ(u32bAcquire(ABC_BASS, (D).rlen,  _mt));                          \
    call(weave_decode_fill, &(D), (W));                                  \
} while (0)

//  --- Emit sink: TLV builder + optional carried-decode capture --------
//
//  Every token the diff core emits goes through `wsink_put`, which feeds
//  the TLV builder AND, when `cap` is non-NULL, appends the same token to
//  a persistent `weavedec` so the next replay step reads it without re-
//  decoding dst's TLV (GET-001).  The capture mirrors weave_decode_fill's
//  column layout, but the R-set is always materialized per token here
//  (no run-length dedup) — that is exactly what a wdp view expects.
typedef struct { weavebld *b; weavedec *cap; } wsink;

static ok64 wsink_put(wsink *k, u8csc text, u32 seq, u32 pos, u32cs rset) {
    sane(k && k->b);
    call(WEAVEBldPut, k->b, text, seq, pos, rset);
    weavedec *d = k->cap;
    if (d != NULL) {
        u32 roff = (u32)u32bDataLen(d->rpool);
        u32 nr = (u32)u32csLen(rset);
        for (u32 i = 0; i < nr; i++) call(u32bFeed1, d->rpool, ((u32 const *)rset[0])[i]);
        call(u8bFeed, d->text, text);
        call(u32bFeed1, d->tok, (u32)u8bDataLen(d->text));
        call(u32bFeed1, d->seq, seq);
        call(u32bFeed1, d->pos, pos);
        call(u32bFeed1, d->roff, roff);
        call(u32bFeed1, d->rlen, nr);
        d->ntok++;
    }
    done;
}

//  Emit decoded token `i` verbatim through the sink.
static ok64 wd_emit(wsink *k, wdp const *p, u32 i) {
    sane(k);
    u32 lo = wd_lo(p, i), hi = wd_hi(p, i);
    a_part(u8c, t, p->base, lo, hi - lo);
    a_part(u32c, rs, p->rpool, p->roff[i], p->rlen[i]);
    return wsink_put(k, t, p->seq[i], p->pos[i], rs);
}

//  Emit decoded token `i` with `add_rm` folded into its R-set (DEL).
static ok64 wd_emit_del(wsink *k, wdp const *p, u32 i, u32 add_rm) {
    sane(k);
    u32 lo = wd_lo(p, i), hi = wd_hi(p, i);
    a_part(u8c, t, p->base, lo, hi - lo);
    u32 rbuf[WEAVE_SET_MAX], rn = 0;
    u32 one[1] = {add_rm};
    a_part(u32c, cur, p->rpool, p->roff[i], p->rlen[i]);
    call(weave_set_union, cur, one, 1, rbuf, WEAVE_SET_MAX, &rn);
    u32cs rs = {rbuf, rbuf + rn};
    return wsink_put(k, t, p->seq[i], p->pos[i], rs);
}


// ============================================================
//  WEAVEFromBlob
// ============================================================

typedef struct {
    weavebld *b;
    u8cp      base;
    u32       src;
    u32       pos;       // running token ordinal
    u32       covered;
} weave_blob_ctx;

static ok64 weave_blob_emit(weave_blob_ctx *ctx, u8csc seg) {
    sane(ctx);
    u32cs rs = {NULL, NULL};
    call(WEAVEBldPut, ctx->b, seg, ctx->src, ctx->pos, rs);
    ctx->pos++;
    done;
}

static ok64 weave_blob_cb(u8 tag, u8cs tok, void *vctx) {
    sane(vctx);
    (void)tag;
    weave_blob_ctx *ctx = vctx;
    if ($len(tok) > 1 && memchr(tok[0], '\n', (size_t)$len(tok)) != NULL) {
        u8c *p = tok[0];
        u8c *e = tok[1];
        while (p < e) {
            u8c *q = p;
            while (q < e && *q != '\n') q++;
            u8c *seg_end = (q < e) ? q + 1 : e;
            u8csc seg = {p, seg_end};
            call(weave_blob_emit, ctx, seg);
            p = seg_end;
        }
        ctx->covered = (u32)(e - ctx->base);
        done;
    }
    u8csc seg = {tok[0], tok[1]};
    call(weave_blob_emit, ctx, seg);
    ctx->covered = (u32)(tok[1] - ctx->base);
    done;
}

ok64 WEAVEFromBlob(weave *w, u8cs data, u8cs ext, u32 src) {
    sane(w);
    weavebld b;
    WEAVEBldInit(&b, w);
    if ($empty(data)) done;

    weave_blob_ctx ctx = {.b = &b, .base = data[0], .src = src, .pos = 0, .covered = 0};
    TOKstate st = {.data = {data[0], data[1]}, .cb = weave_blob_cb, .ctx = &ctx};
    //  TOKLexer returns the callback's ok64.  weave_blob_cb fails only
    //  when WEAVEBldPut overflows (NOROOM) — a real error we must not
    //  swallow, else the tail loop below would re-emit tokens on top of a
    //  partially-built weave.  A lexer that doesn't cover `ext` still
    //  returns OK (covered stays short) and the tail loop handles the
    //  uncovered remainder — that fallback is preserved.
    call(TOKLexer, &st, ext);

    u32 total = (u32)$len(data);
    u32 lo = ctx.covered;
    while (lo < total) {
        u32 hi = lo;
        while (hi < total && data[0][hi] != '\n') hi++;
        if (hi < total) hi++;
        u8csc piece = {data[0] + lo, data[0] + hi};
        call(weave_blob_emit, &ctx, piece);
        lo = hi;
    }
    done;
}

// ============================================================
//  Diff/apply core — shared by WEAVEDiff and WEAVEApply
// ============================================================
//
//  Both diff a BASELINE view of `s` against `nuv` and rebuild into `bld`,
//  in `s`'s token order: baseline tokens that survive pass through, ones
//  the diff deletes gain `seq` as a remover, new tokens are inserted
//  (birth-id (seq, pos)) anchored before the next baseline token, and any
//  token NOT in the baseline (other branches / already dead) passes
//  through untouched and never moves.  The only difference between the two
//  callers is WHICH tokens form the baseline (`isbase`):
//    - WEAVEDiff:  baseline = all alive tokens (linear chain step).
//    - WEAVEApply: baseline = the commit's parent-closure view (one weave,
//                  k-parent DAG replay), so existing tokens are never
//                  re-derived/reordered — the SCCS-weave invariant.

typedef b8 (*weave_basefn)(wdp const *p, u32 i, void *ctx);

static b8 wbase_alive(wdp const *p, u32 i, void *ctx) {
    (void)ctx;
    return p->rlen[i] == 0;
}

typedef struct { WEAVEsetfn pred; void *ctx; } wbase_clos;

//  Baseline = alive in the parent-closure view: inserter reachable AND no
//  remover reachable.  Pure predicate (no seq==0 spine special case — the
//  caller's predicate decides every seq, including any bootstrap).
static b8 wbase_closure(wdp const *p, u32 i, void *vctx) {
    wbase_clos *c = vctx;
    if (!c->pred || !c->pred(p->seq[i], c->ctx)) return NO;
    u32 off = p->roff[i], n = p->rlen[i];
    for (u32 z = 0; z < n; z++)
        if (c->pred(p->rpool[0][off + z], c->ctx)) return NO;
    return YES;
}

static ok64 weave_diff_core(wsink *k, wdp const *s, wdp const *nuv,
                            u32 seq, weave_basefn isbase, void *bctx) {
    sane(k);
    u32 ins_pos = 0;   // ordinal of INS tokens within this step

    Bu64 alive_h = {}, nu_h = {};
    Bu8  alive_text = {};
    Bu32 alive_toks = {};
    Bi32 work = {};
    Bu32 edlbuf = {};
    BACQ(u64bAcquire(ABC_BASS, alive_h, s->ntok + 1));
    BACQ(u64bAcquire(ABC_BASS, nu_h, nuv->ntok + 1));
    size_t s_text_len = (s->ntok > 0) ? wd_hi(s, s->ntok - 1) : 0;
    BACQ(u8bAcquire(ABC_BASS, alive_text, s_text_len + 1));
    BACQ(u32bAcquire(ABC_BASS, alive_toks, s->ntok + 1));
    for (u32 i = 0; i < nuv->ntok; i++) {
        u32 lo = wd_lo(nuv, i), hi = wd_hi(nuv, i);
        a_part(u8c, tb, nuv->base, lo, hi - lo);
        call(u64bFeed1, nu_h, RAPHash(tb));
    }
    {
        u32 cum = 0;
        for (u32 i = 0; i < s->ntok; i++) {
            if (!isbase(s, i, bctx)) continue;
            u32 lo = wd_lo(s, i), hi = wd_hi(s, i);
            a_part(u8c, tb, s->base, lo, hi - lo);
            call(u64bFeed1, alive_h, RAPHash(tb));
            call(u8bFeed, alive_text, tb);
            cum += (hi - lo);
            call(u32bFeed1, alive_toks, tok32Pack('S', cum));
        }
    }

    u64 olen = u64bDataLen(alive_h);
    u64 nlen = (u64)nuv->ntok;

    //  Degenerate baselines — emit directly (an empty side makes the EDL
    //  gauge degenerate).  No baseline ⇒ insert all content, then pass src
    //  through verbatim.  No content ⇒ delete every baseline token, pass
    //  the rest through.
    if (olen == 0) {
        for (u32 i = 0; i < nuv->ntok; i++) {
            u32 lo = wd_lo(nuv, i), hi = wd_hi(nuv, i);
            a_part(u8c, t, nuv->base, lo, hi - lo);
            u32cs rs = {NULL, NULL};
            call(wsink_put, k, t, seq, ins_pos++, rs);
        }
        for (u32 i = 0; i < s->ntok; i++) call(wd_emit, k, s, i);
        done;
    }
    if (nlen == 0) {
        for (u32 i = 0; i < s->ntok; i++) {
            if (isbase(s, i, bctx)) { call(wd_emit_del, k, s, i, seq); }
            else                    { call(wd_emit, k, s, i); }
        }
        done;
    }

    u64 work_sz = DIFFWorkSize(olen, nlen);
    u64 edl_sz  = DIFFEdlMaxEntries(olen, nlen);
    if (work_sz > 0) BACQ(i32bAcquire(ABC_BASS, work,   work_sz));
    if (edl_sz  > 0) BACQ(u32bAcquire(ABC_BASS, edlbuf, edl_sz));

    u64cs oh = {u64bDataHead(alive_h), u64bDataHead(alive_h) + olen};
    u64cs nh = {u64bDataHead(nu_h), u64bDataHead(nu_h) + nlen};
    e32g edlg = {edlbuf[0], edlbuf[3], edlbuf[0]};
    i32s ws = {i32bHead(work), i32bTerm(work)};
    ok64 diff_o = BRAMu64s(edlg, ws, oh, nh);
    if (diff_o != OK) {
        //  BRAM ran out of room (or otherwise failed): discard its partial
        //  output and emit a wholesale DEL(olen)+INS(nlen) through the
        //  bounds-checked gauge.  Cap = olen+nlen >= 2 here (both sides
        //  non-empty per the degenerate guards above), so this always
        //  fits; the checked path propagates NOROOM instead of an OOB
        //  raw write if it ever doesn't.
        call(WEAVEFallbackEdl, edlg, (u32)olen, (u32)nlen);
        call(NEILCanon, edlg);
    } else {
        u32cs at_view = {(u32cp)u32bDataHead(alive_toks),
                         (u32cp)u32bDataHead(alive_toks) + u32bDataLen(alive_toks)};
        u32cs nt_view = {nuv->tok, nuv->tok + nuv->ntok};
        u8cs  at_text = {u8bDataHead(alive_text),
                         u8bDataHead(alive_text) + u8bDataLen(alive_text)};
        size_t nu_text_len = (nuv->ntok > 0) ? wd_hi(nuv, nuv->ntok - 1) : 0;
        a_part(u8c, nt_text, nuv->base, 0, nu_text_len);
        NEILCleanup(edlg, at_view, nt_view, at_text, nt_text);
        NEILShift  (edlg, at_view, nt_view, at_text, nt_text);
    }
    e32c *ep = edlbuf[0];
    e32c *ee = edlg[0];

    u32 wi = 0, ni = 0;
    while (ep < ee) {
        u32 op  = DIFF_OP(*ep);
        u32 len = DIFF_LEN(*ep);
        if (op == DIFF_EQ) {
            for (u32 j = 0; j < len; j++) {
                while (wi < s->ntok && !isbase(s, wi, bctx)) { call(wd_emit, k, s, wi); wi++; }
                if (wi < s->ntok) { call(wd_emit, k, s, wi); wi++; }
                ni++;
            }
            ep++;
            continue;
        }
        u32 sum_ins = 0, sum_del = 0;
        while (ep < ee && DIFF_OP(*ep) != DIFF_EQ) {
            u32 l = DIFF_LEN(*ep);
            if (DIFF_OP(*ep) == DIFF_INS) sum_ins += l; else sum_del += l;
            ep++;
        }
        //  Flush this side's existing non-baseline tokens up to the anchor
        //  BEFORE inserting — a concurrent side already in the weave (e.g.
        //  the base/ours run in a merge conflict) then renders ahead of
        //  this commit's inserts.  Inserts still land before the next
        //  baseline token, so anchoring is unchanged; recovery is
        //  order-independent (the sides are in different closures).
        while (wi < s->ntok && !isbase(s, wi, bctx)) { call(wd_emit, k, s, wi); wi++; }
        for (u32 j = 0; j < sum_ins; j++) {
            u32 lo = wd_lo(nuv, ni), hi = wd_hi(nuv, ni);
            a_part(u8c, t, nuv->base, lo, hi - lo);
            u32cs rs = {NULL, NULL};
            call(wsink_put, k, t, seq, ins_pos++, rs);
            ni++;
        }
        for (u32 j = 0; j < sum_del; j++) {
            while (wi < s->ntok && !isbase(s, wi, bctx)) { call(wd_emit, k, s, wi); wi++; }
            if (wi < s->ntok) { call(wd_emit_del, k, s, wi, seq); wi++; }
        }
    }
    while (wi < s->ntok) { call(wd_emit, k, s, wi); wi++; }
    done;
}

// ============================================================
//  WEAVEDiff (linear chain step: baseline = all alive tokens)
// ============================================================

ok64 WEAVEDiff(weave *dst, weave const *src, weave const *nu, u32 src_commit) {
    sane(dst);
    if (!src || !nu) return FAILSANITY;

    wdec sd = {}, nd = {};
    WEAVE_DECODE(sd, src);
    WEAVE_DECODE(nd, nu);
    wdp s = {}, nuv = {};
    wd_view(&s, &sd);
    wd_view(&nuv, &nd);

    weavebld bld;
    WEAVEBldInit(&bld, dst);
    wsink k = {&bld, NULL};
    call(weave_diff_core, &k, &s, &nuv, src_commit, wbase_alive, NULL);
    done;
}

// ============================================================
//  Carried-decode linear replay (GET-001)
// ============================================================

//  Capacity reserves for a carried decode.  The accumulator weave can
//  grow to WEAVE_TLV_MAX of TLV; its decode columns are bounded by that:
//  the text never exceeds the TLV, and each per-token column has at most
//  one entry per token (≤ TLV bytes worth of records).  rpool holds one
//  u32 per stored remover, also ≤ TLV slots.  mmap with MAP_NORESERVE
//  (u8bMap/u32bMap) reserves VA cheaply and pages in on demand, so these
//  generous reserves cost nothing until actually written.
#define WEAVE_DEC_TEXT_MAX  WEAVE_TLV_MAX
#define WEAVE_DEC_TOK_MAX   (WEAVE_TLV_MAX + 2)

ok64 WEAVEDecInit(weavedec *d) {
    sane(d);
    zerop(d);
    //  MEM-040: a mid-init bMap failure must roll back the regions
    //  already mapped — else 1..6 of the 7 mmaps leak.  try() lets the
    //  error fall through to WEAVEDecFree, which unmaps only the
    //  regions whose [0] is set (the ones that succeeded above).
    try(u8bMap,  d->text,  WEAVE_DEC_TEXT_MAX);    nedo goto rollback;
    try(u32bMap, d->tok,   WEAVE_DEC_TOK_MAX);     nedo goto rollback;
    try(u32bMap, d->seq,   WEAVE_DEC_TOK_MAX);     nedo goto rollback;
    try(u32bMap, d->pos,   WEAVE_DEC_TOK_MAX);     nedo goto rollback;
    try(u32bMap, d->rpool, WEAVE_DEC_TOK_MAX);     nedo goto rollback;
    try(u32bMap, d->roff,  WEAVE_DEC_TOK_MAX);     nedo goto rollback;
    try(u32bMap, d->rlen,  WEAVE_DEC_TOK_MAX);     nedo goto rollback;
    done;
rollback:
    WEAVEDecFree(d);   // unmaps the regions that succeeded; zerops d
    return __;
}

void WEAVEDecReset(weavedec *d) {
    if (!d) return;
    u8bReset(d->text);
    u32bReset(d->tok);  u32bReset(d->seq);  u32bReset(d->pos);
    u32bReset(d->rpool); u32bReset(d->roff); u32bReset(d->rlen);
    d->ntok = 0;
}

void WEAVEDecFree(weavedec *d) {
    if (!d) return;
    if (d->text[0])  u8bUnMap(d->text);
    if (d->tok[0])   u32bUnMap(d->tok);
    if (d->seq[0])   u32bUnMap(d->seq);
    if (d->pos[0])   u32bUnMap(d->pos);
    if (d->rpool[0]) u32bUnMap(d->rpool);
    if (d->roff[0])  u32bUnMap(d->roff);
    if (d->rlen[0])  u32bUnMap(d->rlen);
    zerop(d);
}

//  dst (TLV) = `src` (carried decode) diffed against `nu`; `dst_dec`
//  (reset on entry) receives dst's decode, captured token-by-token as
//  the diff core emits — no TLV re-parse of the accumulator (GET-001).
ok64 WEAVEDiffCarry(weave *dst, weavedec *dst_dec,
                    weavedec const *src, weave const *nu, u32 src_commit) {
    sane(dst && dst_dec && src);
    if (!nu) return FAILSANITY;

    wdec nd = {};
    WEAVE_DECODE(nd, nu);
    wdp s = {}, nuv = {};
    wd_view_dec(&s, src);
    wd_view(&nuv, &nd);

    WEAVEDecReset(dst_dec);
    weavebld bld;
    WEAVEBldInit(&bld, dst);
    wsink k = {&bld, dst_dec};
    call(weave_diff_core, &k, &s, &nuv, src_commit, wbase_alive, NULL);
    done;
}

// ============================================================
//  WEAVEApply (one weave, k-parent DAG replay step)
// ============================================================
//
//  dst = src with commit `seq`'s edit applied.  The baseline is src's
//  alive view restricted to `base`/`base_ctx` (the commit's parent
//  closure): diff that against `nu`'s tokens, delete-mark survivors the
//  commit dropped, insert its new tokens anchored before the following
//  baseline token.  Tokens outside the baseline (concurrent branches,
//  already-dead) are kept verbatim and never reordered, so concurrent
//  inserts get a fixed position from replay order — no re-derivation, no
//  cyclic transposition.  `base == NULL` ⇒ empty baseline (root commit:
//  every nu token is inserted).  `nu` is the commit's content pre-
//  tokenized the same way the weave's tokens were (caller's choice of
//  lexer), exactly as WEAVEDiff takes its `nu`.
ok64 WEAVEApply(weave *dst, weave const *src, weave const *nu,
                u32 seq, WEAVEsetfn base, void *base_ctx) {
    sane(dst);
    if (!src || !nu) return FAILSANITY;

    wdec sd = {}, nd = {};
    WEAVE_DECODE(sd, src);
    WEAVE_DECODE(nd, nu);
    wdp s = {}, nuv = {};
    wd_view(&s, &sd);
    wd_view(&nuv, &nd);

    weavebld bld;
    WEAVEBldInit(&bld, dst);
    wbase_clos clos = {base, base_ctx};
    wsink k = {&bld, NULL};
    call(weave_diff_core, &k, &s, &nuv, seq, wbase_closure, &clos);
    done;
}

// ============================================================
//  Emission: classification by inserter seq
// ============================================================

static b8 weave_scope_alive(u32 seq, u32cs rset,
                            WEAVEsetfn pred, void *ctx) {
    if (!(seq == 0 || pred(seq, ctx))) return NO;     // inserter unreachable
    $for(u32c, r, rset) { if (*r == 0 || pred(*r, ctx)) return NO; }
    return YES;
}

static u8 weave_diff_classify(u32 seq, u32cs rset,
                              WEAVEsetfn in_from, void *from_ctx,
                              WEAVEsetfn in_to,   void *to_ctx) {
    b8 af = weave_scope_alive(seq, rset, in_from, from_ctx);
    b8 at = weave_scope_alive(seq, rset, in_to,   to_ctx);
    if (at && !af) return 'I';
    if (af && !at) return 'D';
    if (af && at)  return ' ';
    return 0;
}

#define WEAVE_CTX_LINES 3

//  Overlay real syntax tags onto a diff hunk's side-only tok stream.
//  `text` is the assembled hunk bytes; `sides` carries one tok32 per
//  source segment (its tag is the neutral 'S' placeholder, ignored — its
//  diff SIDE is kept), covering `text` end-to-end.  The weave stores no
//  lexer tag (graf/WEAVE.h), so — like blame (graf/BLAME.c) — we
//  re-tokenize `text` for syntax, then write into `out` (reset first)
//  the union segmentation: each emitted tok32 takes the syntax tag of
//  its covering syntax token and the diff side of its covering side
//  segment.  Bytes the lexer doesn't reach (unknown ext, tail) keep the
//  neutral 'S' tag.  Best-effort: on any hiccup `out` still ends up a
//  well-formed side-only stream (the body always renders).
static ok64 weave_overlay_syntax(u32b out, u8csc text, u8csc ext,
                                 tok32cs sides) {
    sane(out != NULL);
    u32bReset(out);
    u32 ns = (u32)$len(sides);
    if (ns == 0 || $empty(text)) done;

    //  Syntax pass into a scratch buffer (best-effort; nt stays 0 on an
    //  unknown ext or a tokenizer hiccup, leaving the neutral tag).
    a_carve(u32, syn, (size_t)($len(text) + 16));
    u32 nt = 0;
    if (!$empty(ext) && TOKKnownExt(ext) &&
        HUNKu32bTokenize(syn, text, ext) == OK)
        nt = (u32)u32bDataLen(syn);
    tok32c *st = (tok32c *)u32bDataHead(syn);
    tok32c *sd = sides[0];

    //  Sweep both end-offset streams together; cut at every boundary.
    u32 tlen = (u32)$len(text);
    u32 pos = 0, i = 0, j = 0;
    while (pos < tlen) {
        u8  side = (i < ns) ? tok32Side(sd[i]) : TOK_SIDE_EQ;
        u8  tag  = (j < nt) ? tok32Tag(st[j])  : 'S';
        u32 a = (i < ns) ? tok32Offset(sd[i]) : tlen;
        u32 b = (j < nt) ? tok32Offset(st[j]) : tlen;
        u32 nb = a < b ? a : b;
        if (nb <= pos) break;   //  defensive: strictly-increasing offsets
        call(u32bFeed1, out, tok32PackSide(tag, side, nb));
        if (a == nb && i < ns) i++;
        if (b == nb && j < nt) j++;
        pos = nb;
    }
    done;
}

ok64 WEAVEEmitDiff(weave const *w, u8cs name, u8cs navver,
                   WEAVEsetfn in_from, void *from_ctx,
                   WEAVEsetfn in_to,   void *to_ctx,
                   HUNKcb cb, void *cb_ctx) {
    sane(w && cb && in_from && in_to);

    wdec d = {};
    WEAVE_DECODE(d, w);
    wdp p = {};
    wd_view(&p, &d);
    u32 ntok = p.ntok;
    if (ntok == 0) done;
    u8cp text = p.base[0];

    u32 total_lines_est = 1;
    {
        u32 tlen = (u32)u8bDataLen(d.text);
        for (u32 b = 0; b < tlen; b++) if (text[b] == '\n') total_lines_est++;
    }
    a_carve(u8, changed, total_lines_est + 4);
    memset(changed[0], 0, total_lines_est + 4);
    u8bFed(changed, total_lines_est);
    u8 *cmark = (u8 *)u8bDataHead(changed);
    u32 cur_line = 0;
    for (u32 i = 0; i < ntok; i++) {
        a_part(u32c, rs_i, p.rpool, p.roff[i], p.rlen[i]);
        u8 tag = weave_diff_classify(p.seq[i], rs_i,
                                     in_from, from_ctx, in_to, to_ctx);
        if (tag == 0) continue;
        u32 lo = wd_lo(&p, i), hi = wd_hi(&p, i);
        u32 nl = 0;
        for (u32 b = lo; b < hi; b++) if (text[b] == '\n') nl++;
        if (tag == 'I' || tag == 'D') {
            if (cur_line > 0) cmark[cur_line - 1] = 1;
            for (u32 l = cur_line; l <= cur_line + nl && l < total_lines_est; l++)
                cmark[l] = 1;
        }
        cur_line += nl;
    }
    u32 total_lines = cur_line + 1;
    if (total_lines > total_lines_est) total_lines = total_lines_est;

    a_carve(u32, windows, (total_lines + 4) * 2);
    u32 *wbuf = (u32 *)u32bDataHead(windows);
    u32 nwin = 0;
    {
        u32 i = 0;
        while (i < total_lines) {
            if (!cmark[i]) { i++; continue; }
            u32 cluster_first = i, cluster_last = i;
            i++;
            while (i < total_lines) {
                if (cmark[i]) { cluster_last = i; i++; continue; }
                u32 j = i;
                while (j < total_lines && !cmark[j] &&
                       j - cluster_last <= 2 * WEAVE_CTX_LINES) j++;
                if (j < total_lines && cmark[j]) { cluster_last = j; i = j + 1; }
                else break;
            }
            u32 lo = (cluster_first > WEAVE_CTX_LINES)
                     ? cluster_first - WEAVE_CTX_LINES : 0;
            u32 hi = cluster_last + WEAVE_CTX_LINES;
            if (hi >= total_lines) hi = total_lines - 1;
            wbuf[nwin * 2] = lo;
            wbuf[nwin * 2 + 1] = hi;
            nwin++;
        }
    }
    u32bFed(windows, nwin * 2);
    if (nwin == 0) done;

    a_carve(u8,  outtext,  16UL << 20);
    a_carve(u32, outtoks,  1UL << 16);
    a_carve(u32, combined, 1UL << 18);   //  side ∪ syntax segmentation
    a_carve(u8,  outuri,   1UL << 12);
    u8cs ext = {};
    PATHu8sExt(ext, name);

    ok64 ret = OK;
    u32 wi = 0;
    u32 win_lo = wbuf[0], win_hi = wbuf[1];
    cur_line = 0;
    b8 hunk_open = NO;

    #define FLUSH_HUNK() do {                                          \
        if (hunk_open) {                                               \
            u8bReset(outuri);                                          \
            a_cstr(_diff_scheme, "diff:");                             \
            (void)u8bFeed(outuri, _diff_scheme);                       \
            (void)u8bFeed(outuri, name);                               \
            if (!u8csEmpty(navver)) {                                  \
                (void)u8bFeed1(outuri, '?');                           \
                (void)u8bFeed(outuri, navver);                         \
            }                                                          \
            u8csc _empty_path = {NULL, NULL};                          \
            u8csc _empty_sym  = {NULL, NULL};                          \
            (void)HUNKu8sMakeURI(u8bIdle(outuri), _empty_path,         \
                                 _empty_sym, win_lo + 1);              \
            u8csc _htext = {u8bDataHead(outtext),                      \
                            u8bDataHead(outtext) + u8bDataLen(outtext)};\
            tok32cs _sides = {(tok32c *)u32bDataHead(outtoks),         \
                              (tok32c *)u32bDataHead(outtoks)          \
                              + u32bDataLen(outtoks)};                 \
            (void)weave_overlay_syntax(combined, _htext, ext, _sides); \
            hunk hk = {};                                             \
            hk.uri[0] = (u8 *)u8bDataHead(outuri);                    \
            hk.uri[1] = (u8 *)u8bDataHead(outuri) + u8bDataLen(outuri);\
            hk.text[0] = u8bDataHead(outtext);                        \
            hk.text[1] = u8bDataHead(outtext) + u8bDataLen(outtext);  \
            hk.toks[0] = (tok32c *)u32bDataHead(combined);            \
            hk.toks[1] = (tok32c *)u32bDataHead(combined)             \
                       + u32bDataLen(combined);                       \
            ok64 _r = cb(&hk, cb_ctx);                                \
            if (_r != OK) ret = _r;                                   \
            u8bReset(outtext);                                        \
            u32bReset(outtoks);                                       \
            hunk_open = NO;                                           \
        }                                                             \
    } while (0)

    for (u32 i = 0; i < ntok; i++) {
        a_part(u32c, rs_i, p.rpool, p.roff[i], p.rlen[i]);
        u8 tag = weave_diff_classify(p.seq[i], rs_i,
                                     in_from, from_ctx, in_to, to_ctx);
        if (tag == 0) continue;
        u32 lo = wd_lo(&p, i), hi = wd_hi(&p, i);
        u32 nl = 0;
        for (u32 b = lo; b < hi; b++) if (text[b] == '\n') nl++;
        while (wi < nwin && cur_line > win_hi) {
            FLUSH_HUNK();
            wi++;
            if (wi < nwin) { win_lo = wbuf[wi * 2]; win_hi = wbuf[wi * 2 + 1]; }
        }
        if (wi >= nwin) break;
        if (cur_line >= win_lo && cur_line <= win_hi) {
            //  Flush before the next token overflows a fixed carve: a
            //  fully-rewritten file collapses into one window whose token
            //  stream can exceed outtext/outtoks (mirrors WEAVEEmitFull).
            if (hunk_open &&
                (u8bIdleLen(outtext) < (size_t)(hi - lo) ||
                 u32bIdleLen(outtoks) < 1)) {
                FLUSH_HUNK();
                win_lo = cur_line;   // continuation hunk starts here
            }
            a_part(u8c, tb, p.base, lo, hi - lo);
            ok64 fo = u8bFeed(outtext, tb);
            if (fo != OK) { ret = fo; goto cleanup; }
            u8 side = (tag == 'I') ? TOK_SIDE_IN
                    : (tag == 'D') ? TOK_SIDE_RM : TOK_SIDE_EQ;
            fo = u32bFeed1(outtoks,
                tok32PackSide('S', side, (u32)u8bDataLen(outtext)));
            if (fo != OK) { ret = fo; goto cleanup; }
            hunk_open = YES;
        }
        cur_line += nl;
    }
    FLUSH_HUNK();
    #undef FLUSH_HUNK

cleanup:
    return ret;
}

#define WEAVE_FULL_HUNK_MAX (1UL << 20)

ok64 WEAVEEmitFull(weave const *w, u8cs name, u8cs scheme, u8cs navver,
                   WEAVEsetfn in_from, void *from_ctx,
                   WEAVEsetfn in_to,   void *to_ctx,
                   HUNKcb cb, void *cb_ctx) {
    sane(w && cb && in_from && in_to);

    wdec d = {};
    WEAVE_DECODE(d, w);
    wdp p = {};
    wd_view(&p, &d);
    u32 ntok = p.ntok;
    if (ntok == 0) done;
    u8cp text = p.base[0];

    a_carve(u8,  outtext,  16UL << 20);
    a_carve(u32, outtoks,  1UL << 16);
    a_carve(u32, combined, 1UL << 18);   //  side ∪ syntax segmentation
    a_carve(u8,  outuri,   1UL << 12);
    u8cs ext = {};
    PATHu8sExt(ext, name);

    ok64 ret = OK;
    b8   hunk_open = NO;
    u32  hunk_start_line = 0;
    u32  cur_line = 0;

    //  DIFF-003: a non-empty `scheme` (e.g. `diff:`) is prepended to the
    //  hunk URI so the renderer (HUNKu8sFeedText's `hunk_uri_is_diff`)
    //  routes the whole-file hunk through the unified-diff +/- formatter
    //  exactly like a windowed `diff:` hunk.  `cat:` passes empty scheme
    //  so its whole-file hunk renders as plain syntax-highlighted text.
    //  Either way the body's tokens carry real syntax tags (overlaid
    //  below) plus the diff side, so syntax AND change hili both paint.
    #define FLUSH_FULL_HUNK() do {                                       \
        if (hunk_open) {                                                 \
            u8bReset(outuri);                                            \
            (void)u8bFeed(outuri, scheme);                              \
            (void)u8bFeed(outuri, name);                                 \
            if (!u8csEmpty(navver)) {                                     \
                (void)u8bFeed1(outuri, '?');                              \
                (void)u8bFeed(outuri, navver);                           \
            }                                                            \
            u8csc _empty_path = {NULL, NULL};                            \
            u8csc _empty_sym  = {NULL, NULL};                            \
            (void)HUNKu8sMakeURI(u8bIdle(outuri), _empty_path,           \
                                 _empty_sym, hunk_start_line + 1);       \
            u8csc _htext = {u8bDataHead(outtext),                        \
                            u8bDataHead(outtext) + u8bDataLen(outtext)}; \
            tok32cs _sides = {(tok32c *)u32bDataHead(outtoks),           \
                              (tok32c *)u32bDataHead(outtoks)            \
                              + u32bDataLen(outtoks)};                   \
            (void)weave_overlay_syntax(combined, _htext, ext, _sides);   \
            hunk hk = {};                                               \
            hk.uri[0]  = (u8 *)u8bDataHead(outuri);                     \
            hk.uri[1]  = (u8 *)u8bDataHead(outuri) + u8bDataLen(outuri);\
            hk.text[0] = u8bDataHead(outtext);                          \
            hk.text[1] = u8bDataHead(outtext) + u8bDataLen(outtext);    \
            hk.toks[0] = (tok32c *)u32bDataHead(combined);              \
            hk.toks[1] = (tok32c *)u32bDataHead(combined)               \
                       + u32bDataLen(combined);                         \
            ok64 _r = cb(&hk, cb_ctx);                                  \
            if (_r != OK) ret = _r;                                     \
            u8bReset(outtext);                                          \
            u32bReset(outtoks);                                         \
            hunk_open = NO;                                             \
            hunk_start_line = cur_line;                                 \
        }                                                               \
    } while (0)

    for (u32 i = 0; i < ntok; i++) {
        a_part(u32c, rs_i, p.rpool, p.roff[i], p.rlen[i]);
        u8 tag = weave_diff_classify(p.seq[i], rs_i,
                                     in_from, from_ctx, in_to, to_ctx);
        if (tag == 0) continue;
        u32 lo = wd_lo(&p, i), hi = wd_hi(&p, i);
        u32 nl = 0;
        for (u32 b = lo; b < hi; b++) if (text[b] == '\n') nl++;
        if (hunk_open && u8bDataLen(outtext) + (hi - lo) > WEAVE_FULL_HUNK_MAX)
            FLUSH_FULL_HUNK();
        a_part(u8c, tb, p.base, lo, hi - lo);
        ok64 fo = u8bFeed(outtext, tb);
        if (fo != OK) { ret = fo; goto cleanup; }
        u8 side = (tag == 'I') ? TOK_SIDE_IN
                : (tag == 'D') ? TOK_SIDE_RM : TOK_SIDE_EQ;
        fo = u32bFeed1(outtoks,
            tok32PackSide('S', side, (u32)u8bDataLen(outtext)));
        if (fo != OK) { ret = fo; goto cleanup; }
        hunk_open = YES;
        cur_line += nl;
    }
    FLUSH_FULL_HUNK();
    #undef FLUSH_FULL_HUNK

cleanup:
    return ret;
}

#define WEAVE_EMIT_MAX_PREDS  32
#define WEAVE_EMIT_MAX_GROUPS 32

static u32 weave_emit_membership(u32 seq,
                                 WEAVEsetfn const *preds, void *const *ctxs,
                                 u32 npreds, u32 spine_mask) {
    if (seq == 0) return spine_mask;
    u32 m = 0;
    for (u32 pi = 0; pi < npreds; pi++)
        if (preds[pi] && preds[pi](seq, ctxs ? ctxs[pi] : NULL)) m |= (1u << pi);
    return m;
}

//  Concatenate (into the reset buffer `dst`) the bytes of every alive
//  token in [run_lo, run_hi) whose emit-membership equals `gmask`.
//  Used by WEAVEEmitMerged's byte-equality collapse below.
static ok64 weave_gather_group(u8b dst, wdp const *p, u32 run_lo, u32 run_hi,
                               u32 gmask, WEAVEsetfn const *preds,
                               void *const *ctxs, u32 npreds, u32 spine_mask) {
    sane(p && dst);
    u8bReset(dst);
    for (u32 j = run_lo; j < run_hi; j++) {
        if (!wd_alive(p, j)) continue;
        if (weave_emit_membership(p->seq[j], preds, ctxs, npreds, spine_mask)
            != gmask)
            continue;
        u32 lo = wd_lo(p, j), hi = wd_hi(p, j);
        a_part(u8c, tb, p->base, lo, hi - lo);
        call(u8bFeed, dst, tb);
    }
    done;
}

ok64 WEAVEEmitMerged(weave const *w,
                     WEAVEsetfn const *preds, void *const *ctxs,
                     u32 npreds, u8b out) {
    sane(w && out);
    if (npreds > WEAVE_EMIT_MAX_PREDS) return WEAVEFAIL;
    u8bReset(out);

    wdec d = {};
    WEAVE_DECODE(d, w);
    wdp p = {};
    wd_view(&p, &d);
    u32 ntok = p.ntok;
    if (ntok == 0) done;

    //  Scratch for the byte-equality collapse (see the conflict branch):
    //  a divergent group's bytes are at most the whole decoded text.
    a_carve(u8, cgA, u8bDataLen(d.text) + 1);
    a_carve(u8, cgB, u8bDataLen(d.text) + 1);

    #define EMITTOK(i) do { u32 _lo = wd_lo(&p,(i)), _hi = wd_hi(&p,(i)); \
        a_part(u8c, _tb, p.base, _lo, _hi - _lo); call(u8bFeed, out, _tb); } while (0)

    u32 spine_mask =
        (npreds == 0) ? 0
        : (npreds == 32 ? 0xFFFFFFFFu : ((1u << npreds) - 1u));

    a_cstr(mk_open,  "<<<<");
    a_cstr(mk_mid,   "||||");
    a_cstr(mk_close, ">>>>");

    u32 i = 0;
    while (i < ntok) {
        if (!wd_alive(&p, i)) { i++; continue; }
        u32 m = weave_emit_membership(p.seq[i], preds, ctxs, npreds, spine_mask);
        if (m == spine_mask) { EMITTOK(i); i++; continue; }

        u32 run_lo = i, run_hi = i;
        while (run_hi < ntok) {
            if (!wd_alive(&p, run_hi)) { run_hi++; continue; }
            u32 mm = weave_emit_membership(p.seq[run_hi], preds, ctxs,
                                           npreds, spine_mask);
            if (mm == spine_mask) break;
            run_hi++;
        }

        b8 conflict = NO;
        u32 groups[WEAVE_EMIT_MAX_GROUPS];
        u32 ngroups = 0;
        for (u32 j = run_lo; j < run_hi && !conflict; j++) {
            if (!wd_alive(&p, j)) continue;
            u32 mj = weave_emit_membership(p.seq[j], preds, ctxs, npreds, spine_mask);
            for (u32 k = 0; k < ngroups; k++)
                if ((groups[k] & mj) == 0) { conflict = YES; break; }
            if (conflict) break;
            b8 dup = NO;
            for (u32 k = 0; k < ngroups; k++) if (groups[k] == mj) { dup = YES; break; }
            if (!dup && ngroups < WEAVE_EMIT_MAX_GROUPS) groups[ngroups++] = mj;
        }

        if (!conflict) {
            for (u32 j = run_lo; j < run_hi; j++) {
                if (!wd_alive(&p, j)) continue;
                EMITTOK(j);
            }
            i = run_hi;
            continue;
        }

        ngroups = 0;
        for (u32 j = run_lo; j < run_hi; j++) {
            if (!wd_alive(&p, j)) continue;
            u32 mj = weave_emit_membership(p.seq[j], preds, ctxs, npreds, spine_mask);
            b8 dup = NO;
            for (u32 k = 0; k < ngroups; k++) if (groups[k] == mj) { dup = YES; break; }
            if (!dup && ngroups < WEAVE_EMIT_MAX_GROUPS) groups[ngroups++] = mj;
        }

        //  Byte-equality collapse (FOSTER.plan.md #1).  Content
        //  re-absorbed under a different WEAVE birth-id (foster /
        //  cherry-pick) surfaces as disjoint ours-only / theirs-only
        //  groups that are nevertheless byte-identical — not a real
        //  conflict.  If every group emits the same bytes, emit it once
        //  with no markers.
        if (ngroups >= 2) {
            call(weave_gather_group, cgA, &p, run_lo, run_hi, groups[0],
                 preds, ctxs, npreds, spine_mask);
            a_dup(u8c, ga, u8bDataC(cgA));
            b8 all_eq = YES;
            for (u32 g = 1; g < ngroups && all_eq; g++) {
                call(weave_gather_group, cgB, &p, run_lo, run_hi, groups[g],
                     preds, ctxs, npreds, spine_mask);
                a_dup(u8c, gb, u8bDataC(cgB));
                if (!u8csEq(ga, gb)) all_eq = NO;
            }
            if (all_eq) {
                call(u8bFeed, out, u8bDataC(cgA));
                i = run_hi;
                continue;
            }
        }
        call(u8bFeed, out, mk_open);
        for (u32 g = 0; g < ngroups; g++) {
            if (g > 0) call(u8bFeed, out, mk_mid);
            for (u32 j = run_lo; j < run_hi; j++) {
                if (!wd_alive(&p, j)) continue;
                u32 mj = weave_emit_membership(p.seq[j], preds, ctxs, npreds, spine_mask);
                if (mj != groups[g]) continue;
                EMITTOK(j);
            }
        }
        call(u8bFeed, out, mk_close);
        i = run_hi;
    }
    #undef EMITTOK
    done;
}
