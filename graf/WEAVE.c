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
static ok64 weave_set_union(u32 const *a, u32 na, u32 const *b, u32 nb,
                            u32 *out, u32 cap, u32 *n) {
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
    u8cp  base; u32cp tok; u32 ntok;
    u32cp seq, pos;
    u32cp rpool, roff, rlen;
} wdp;

static void wd_view(wdp *p, wdec const *d) {
    p->base  = (u8cp)u8bDataHead(d->text);
    p->tok   = (u32cp)u32bDataHead(d->tok);
    p->ntok  = d->ntok;
    p->seq   = (u32cp)u32bDataHead(d->seq);
    p->pos   = (u32cp)u32bDataHead(d->pos);
    p->rpool = (u32cp)u32bDataHead(d->rpool);
    p->roff  = (u32cp)u32bDataHead(d->roff);
    p->rlen  = (u32cp)u32bDataHead(d->rlen);
}

static u32 wd_lo(wdp const *p, u32 i) { return i ? p->tok[i - 1] : 0; }
static u32 wd_hi(wdp const *p, u32 i) { return p->tok[i]; }
static b8  wd_alive(wdp const *p, u32 i) { return p->rlen[i] == 0; }
static u64 wd_key(wdp const *p, u32 i) {
    return ((u64)p->seq[i] << 32) | (u64)p->pos[i];
}

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

//  Emit decoded token `i` verbatim through the builder.
static ok64 wd_emit(weavebld *b, wdp const *p, u32 i) {
    sane(b);
    u32 lo = wd_lo(p, i), hi = wd_hi(p, i);
    u8csc t = {p->base + lo, p->base + hi};
    u32cs rs = {p->rpool + p->roff[i], p->rpool + p->roff[i] + p->rlen[i]};
    return WEAVEBldPut(b, t, p->seq[i], p->pos[i], rs);
}

//  Emit decoded token `i` with `add_rm` folded into its R-set (DEL).
static ok64 wd_emit_del(weavebld *b, wdp const *p, u32 i, u32 add_rm) {
    sane(b);
    u32 lo = wd_lo(p, i), hi = wd_hi(p, i);
    u8csc t = {p->base + lo, p->base + hi};
    u32 rbuf[WEAVE_SET_MAX], rn = 0;
    u32 one[1] = {add_rm};
    call(weave_set_union, p->rpool + p->roff[i], p->rlen[i], one, 1,
         rbuf, WEAVE_SET_MAX, &rn);
    u32cs rs = {rbuf, rbuf + rn};
    return WEAVEBldPut(b, t, p->seq[i], p->pos[i], rs);
}

static b8 wd_tok_eq(wdp const *a, u32 ai, wdp const *b, u32 bi) {
    u32 alo = wd_lo(a, ai), ahi = wd_hi(a, ai);
    u32 blo = wd_lo(b, bi), bhi = wd_hi(b, bi);
    if (ahi - alo != bhi - blo) return NO;
    if (ahi == alo) return YES;
    return memcmp(a->base + alo, b->base + blo, ahi - alo) == 0;
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
    TOKLexer(&st, ext);  // best effort

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
        if (c->pred(p->rpool[off + z], c->ctx)) return NO;
    return YES;
}

static ok64 weave_diff_core(weavebld *bld, wdp const *s, wdp const *nuv,
                            u32 seq, weave_basefn isbase, void *bctx) {
    sane(bld);
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
        u8csc tb = {nuv->base + lo, nuv->base + hi};
        call(u64bFeed1, nu_h, RAPHash(tb));
    }
    {
        u32 cum = 0;
        for (u32 i = 0; i < s->ntok; i++) {
            if (!isbase(s, i, bctx)) continue;
            u32 lo = wd_lo(s, i), hi = wd_hi(s, i);
            u8csc tb = {s->base + lo, s->base + hi};
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
            u8csc t = {nuv->base + lo, nuv->base + hi};
            u32cs rs = {NULL, NULL};
            call(WEAVEBldPut, bld, t, seq, ins_pos++, rs);
        }
        for (u32 i = 0; i < s->ntok; i++) call(wd_emit, bld, s, i);
        done;
    }
    if (nlen == 0) {
        for (u32 i = 0; i < s->ntok; i++) {
            if (isbase(s, i, bctx)) { call(wd_emit_del, bld, s, i, seq); }
            else                    { call(wd_emit, bld, s, i); }
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
        edlg[1] = edlg[0];
        *edlg[1]++ = DIFF_ENTRY(DIFF_DEL, (u32)olen);
        *edlg[1]++ = DIFF_ENTRY(DIFF_INS, (u32)nlen);
        call(NEILCanon, edlg);
    } else {
        u32cs at_view = {(u32cp)u32bDataHead(alive_toks),
                         (u32cp)u32bDataHead(alive_toks) + u32bDataLen(alive_toks)};
        u32cs nt_view = {nuv->tok, nuv->tok + nuv->ntok};
        u8cs  at_text = {u8bDataHead(alive_text),
                         u8bDataHead(alive_text) + u8bDataLen(alive_text)};
        size_t nu_text_len = (nuv->ntok > 0) ? wd_hi(nuv, nuv->ntok - 1) : 0;
        u8cs  nt_text = {(u8cp)nuv->base, (u8cp)nuv->base + nu_text_len};
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
                while (wi < s->ntok && !isbase(s, wi, bctx)) { call(wd_emit, bld, s, wi); wi++; }
                if (wi < s->ntok) { call(wd_emit, bld, s, wi); wi++; }
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
        while (wi < s->ntok && !isbase(s, wi, bctx)) { call(wd_emit, bld, s, wi); wi++; }
        for (u32 j = 0; j < sum_ins; j++) {
            u32 lo = wd_lo(nuv, ni), hi = wd_hi(nuv, ni);
            u8csc t = {nuv->base + lo, nuv->base + hi};
            u32cs rs = {NULL, NULL};
            call(WEAVEBldPut, bld, t, seq, ins_pos++, rs);
            ni++;
        }
        for (u32 j = 0; j < sum_del; j++) {
            while (wi < s->ntok && !isbase(s, wi, bctx)) { call(wd_emit, bld, s, wi); wi++; }
            if (wi < s->ntok) { call(wd_emit_del, bld, s, wi, seq); wi++; }
        }
    }
    while (wi < s->ntok) { call(wd_emit, bld, s, wi); wi++; }
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
    call(weave_diff_core, &bld, &s, &nuv, src_commit, wbase_alive, NULL);
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
    call(weave_diff_core, &bld, &s, &nuv, seq, wbase_closure, &clos);
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

ok64 WEAVEEmitDiff(weave const *w, u8cs name,
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
    u8cp text = p.base;

    #define RSET(i) ((u32cs){p.rpool + p.roff[i], p.rpool + p.roff[i] + p.rlen[i]})

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
        u8 tag = weave_diff_classify(p.seq[i], RSET(i),
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

    a_carve(u8,  outtext, 16UL << 20);
    a_carve(u32, outtoks, 1UL << 16);
    a_carve(u8,  outuri,  1UL << 12);

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
            u8csc _empty_sym = {NULL, NULL};                           \
            if (HUNKu8sMakeURI(u8bIdle(outuri), name,                  \
                               _empty_sym, win_lo + 1) != OK) {        \
                u8bReset(outuri);                                      \
                (void)u8bFeed(outuri, _diff_scheme);                   \
                (void)u8bFeed(outuri, name);                           \
            }                                                          \
            hunk hk = {};                                             \
            hk.uri[0] = (u8 *)u8bDataHead(outuri);                    \
            hk.uri[1] = (u8 *)u8bDataHead(outuri) + u8bDataLen(outuri);\
            hk.text[0] = u8bDataHead(outtext);                        \
            hk.text[1] = u8bDataHead(outtext) + u8bDataLen(outtext);  \
            hk.toks[0] = (tok32c *)u32bDataHead(outtoks);             \
            hk.toks[1] = (tok32c *)u32bDataHead(outtoks)              \
                       + u32bDataLen(outtoks);                        \
            ok64 _r = cb(&hk, cb_ctx);                                \
            if (_r != OK) ret = _r;                                   \
            u8bReset(outtext);                                        \
            u32bReset(outtoks);                                       \
            hunk_open = NO;                                           \
        }                                                             \
    } while (0)

    for (u32 i = 0; i < ntok; i++) {
        u8 tag = weave_diff_classify(p.seq[i], RSET(i),
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
            u8cs tb = {text + lo, text + hi};
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
    #undef RSET
    return ret;
}

#define WEAVE_FULL_HUNK_MAX (1UL << 20)

ok64 WEAVEEmitFull(weave const *w, u8cs name,
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
    u8cp text = p.base;

    #define RSET(i) ((u32cs){p.rpool + p.roff[i], p.rpool + p.roff[i] + p.rlen[i]})

    a_carve(u8,  outtext, 16UL << 20);
    a_carve(u32, outtoks, 1UL << 16);
    a_carve(u8,  outuri,  1UL << 12);

    ok64 ret = OK;
    b8   hunk_open = NO;
    u32  hunk_start_line = 0;
    u32  cur_line = 0;

    #define FLUSH_FULL_HUNK() do {                                       \
        if (hunk_open) {                                                 \
            u8bReset(outuri);                                            \
            u8csc _empty_sym = {NULL, NULL};                             \
            if (HUNKu8sMakeURI(u8bIdle(outuri), name,                    \
                               _empty_sym, hunk_start_line + 1) != OK) { \
                u8bReset(outuri);                                        \
                (void)u8bFeed(outuri, name);                             \
            }                                                            \
            hunk hk = {};                                               \
            hk.uri[0]  = (u8 *)u8bDataHead(outuri);                     \
            hk.uri[1]  = (u8 *)u8bDataHead(outuri) + u8bDataLen(outuri);\
            hk.text[0] = u8bDataHead(outtext);                          \
            hk.text[1] = u8bDataHead(outtext) + u8bDataLen(outtext);    \
            hk.toks[0] = (tok32c *)u32bDataHead(outtoks);               \
            hk.toks[1] = (tok32c *)u32bDataHead(outtoks)                \
                       + u32bDataLen(outtoks);                          \
            ok64 _r = cb(&hk, cb_ctx);                                  \
            if (_r != OK) ret = _r;                                     \
            u8bReset(outtext);                                          \
            u32bReset(outtoks);                                         \
            hunk_open = NO;                                             \
            hunk_start_line = cur_line;                                 \
        }                                                               \
    } while (0)

    for (u32 i = 0; i < ntok; i++) {
        u8 tag = weave_diff_classify(p.seq[i], RSET(i),
                                     in_from, from_ctx, in_to, to_ctx);
        if (tag == 0) continue;
        u32 lo = wd_lo(&p, i), hi = wd_hi(&p, i);
        u32 nl = 0;
        for (u32 b = lo; b < hi; b++) if (text[b] == '\n') nl++;
        if (hunk_open && u8bDataLen(outtext) + (hi - lo) > WEAVE_FULL_HUNK_MAX)
            FLUSH_FULL_HUNK();
        u8cs tb = {text + lo, text + hi};
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
    #undef RSET
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

    #define EMITTOK(i) do { u32 _lo = wd_lo(&p,(i)), _hi = wd_hi(&p,(i)); \
        u8cs _tb = {p.base + _lo, p.base + _hi}; call(u8bFeed, out, _tb); } while (0)

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
