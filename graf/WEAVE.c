//  WEAVE: token-level file history as a single interleaved-delta TLV
//  stream.  See WEAVE.h for the record layout and the splice-
//  canonicalization rule.
//
//  Each operation (FromBlob, Diff, Merge) writes a fresh dst weave from
//  the inputs; the caller manages a few weave instances and reuses them.
//  The heavy ops decode their inputs into transient parallel arrays on
//  BASS (`wdec`), run the token-level diff/merge there, and re-emit the
//  result through the RLE builder.
//
#include "WEAVE.h"

#include <string.h>

#include "abc/DIFF.h"
#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/TLV.h"
#include "graf/BRAM.h"
#include "graf/NEIL.h"

// u64 diff specialization for token hashlets.
#define X(M, name) M##u64##name
#include "abc/DIFFx.h"
#undef X

// --- Buffer cap ---

#define WEAVE_TLV_MAX  (96UL << 20)   // 96 MB per weave TLV stream

//  BASS acquire must NOT go through call(): call() snapshots BASS on
//  entry and rewinds on return, reclaiming the buffer just acquired
//  (see abc/PRO.h §"BASS-implicit arena macros").  Acquire directly,
//  propagating failure like call() does.
#define BACQ(expr) do { __ = (expr); if (__ != OK) return __; } while (0)

// ============================================================
//  Sorted-set primitives (sets are LE-packed u32 in I/R records)
// ============================================================

//  Decode an I/R record value (LE u32s) into `out[0..*n)`.
static ok64 weave_set_decode(u8csc raw, u32 *out, u32 cap, u32 *n) {
    u8cs r = {};
    u8csMv(r, raw);
    u32 c = 0;
    while (u8csLen(r) >= 4) {
        if (c >= cap) return WEAVEFAIL;
        u32 v = 0;
        u8sDrain32(r, &v);
        out[c++] = v;
    }
    *n = c;
    return OK;
}

static b8 weave_set_eq(u32 const *a, u32 na, u32 const *b, u32 nb) {
    if (na != nb) return NO;
    for (u32 i = 0; i < na; i++) if (a[i] != b[i]) return NO;
    return YES;
}

//  YES iff sorted set `a` is a subset of sorted set `b`.
static b8 weave_set_subset(u32 const *a, u32 na, u32 const *b, u32 nb) {
    u32 j = 0;
    for (u32 i = 0; i < na; i++) {
        while (j < nb && b[j] < a[i]) j++;
        if (j >= nb || b[j] != a[i]) return NO;
    }
    return YES;
}

//  Sorted merge-union of two sorted/deduped sets into `out`.
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
//  TLV record writers / builder
// ============================================================

//  Append one TLV record (header + value) to a buffer.  Feeding into the
//  buffer's IDLE slice advances its DATA boundary directly (commit).
static ok64 weave_rec(u8b tlv, u8 lit, u8csc val) {
    sane(tlv);
    call(TLVu8sFeed, u8bIdle(tlv), lit, val);
    done;
}

//  Append one I/R record carrying a sorted u32 set (LE-packed u32s).
static ok64 weave_rec_set(u8b tlv, u8 lit, u32 const *set, u32 n) {
    sane(tlv);
    a_pad(u8, sb, 4 * WEAVE_SET_MAX);
    for (u32 i = 0; i < n; i++) { u32 v = set[i]; call(u8sFeed32, u8bIdle(sb), &v); }
    call(weave_rec, tlv, lit, u8bDataC(sb));
    done;
}

void WEAVEBldInit(weavebld *b, weave *w) {
    zerop(b);
    b->w = w;
    WEAVEReset(w);
}

ok64 WEAVEBldPut(weavebld *b, u8csc text, u32cs iset, u32cs rset) {
    sane(b && b->w);
    u32 const *ip = (u32 const *)iset[0];
    u32        ni = (u32)u32csLen(iset);
    u32 const *rp = (u32 const *)rset[0];
    u32        nr = (u32)u32csLen(rset);
    if (ni > WEAVE_SET_MAX || nr > WEAVE_SET_MAX) return WEAVEFAIL;
    if (!b->started || !weave_set_eq(b->previ, b->npi, ip, ni)) {
        call(weave_rec_set, b->w->tlv, WEAVE_REC_I, ip, ni);
        for (u32 i = 0; i < ni; i++) b->previ[i] = ip[i];
        b->npi = ni;
    }
    if (!b->started || !weave_set_eq(b->prevr, b->npr, rp, nr)) {
        call(weave_rec_set, b->w->tlv, WEAVE_REC_R, rp, nr);
        for (u32 i = 0; i < nr; i++) b->prevr[i] = rp[i];
        b->npr = nr;
    }
    b->started = YES;
    call(weave_rec, b->w->tlv, WEAVE_REC_T, text);
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
        if (type == WEAVE_REC_I) {
            u8csc v = {val[0], val[1]};
            if (weave_set_decode(v, c->iset, WEAVE_SET_MAX, &c->ni) != OK) {
                c->bad = YES; return NO;
            }
        } else if (type == WEAVE_REC_R) {
            u8csc v = {val[0], val[1]};
            if (weave_set_decode(v, c->rset, WEAVE_SET_MAX, &c->nr) != OK) {
                c->bad = YES; return NO;
            }
        } else if (type == WEAVE_REC_T) {
            u8csMv(c->text, val);
            return YES;
        } else {
            c->bad = YES; return NO;
        }
    }
    return NO;
}

//  Emit a weave's alive byte stream (R-set-empty tokens, in order).
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
//  Transient decode to parallel arrays (heavy ops)
// ============================================================

typedef struct {
    Bu8  text;            // concatenated token bytes
    Bu32 tok;             // [ntok] cumulative end offsets
    Bu32 ipool, ioff, ilen;   // token i I-set = ipool[ioff[i] .. +ilen[i])
    Bu32 rpool, roff, rlen;
    u32  ntok;
} wdec;

//  Raw-pointer view over a filled wdec, for the algorithm loops.
typedef struct {
    u8cp  base; u32cp tok; u32 ntok;
    u32cp ipool, ioff, ilen;
    u32cp rpool, roff, rlen;
} wdp;

static void wd_view(wdp *p, wdec const *d) {
    p->base  = (u8cp)u8bDataHead(d->text);
    p->tok   = (u32cp)u32bDataHead(d->tok);
    p->ntok  = d->ntok;
    p->ipool = (u32cp)u32bDataHead(d->ipool);
    p->ioff  = (u32cp)u32bDataHead(d->ioff);
    p->ilen  = (u32cp)u32bDataHead(d->ilen);
    p->rpool = (u32cp)u32bDataHead(d->rpool);
    p->roff  = (u32cp)u32bDataHead(d->roff);
    p->rlen  = (u32cp)u32bDataHead(d->rlen);
}

static u32 wd_lo(wdp const *p, u32 i) { return i ? p->tok[i - 1] : 0; }
static u32 wd_hi(wdp const *p, u32 i) { return p->tok[i]; }
static b8  wd_alive(wdp const *p, u32 i) { return p->rlen[i] == 0; }

//  Fill a wdec whose buffers the CALLER already acquired (empty) on its
//  own BASS scope — see WEAVE_DECODE.  No BASS acquire happens here, so
//  the decoded arrays outlive this call() boundary (they live in the
//  op's scope, not this helper's).
static ok64 weave_decode_fill(wdec *d, weave const *w) {
    sane(d && w);
    weavecur c;
    WEAVECurInit(&c, w);
    u32 cur_ioff = 0, cur_ilen = 0, cur_roff = 0, cur_rlen = 0;
    b8 dirty_i = YES, dirty_r = YES;   // re-pool the set when it changed
    u32 prev_i[WEAVE_SET_MAX], prev_ni = 0;
    u32 prev_r[WEAVE_SET_MAX], prev_nr = 0;
    u32 n = 0;
    //  The cursor decodes I/R into c.iset/c.rset; we pool a fresh copy
    //  only when the set differs from the last pooled one (RLE-aware).
    while (WEAVECurNext(&c)) {
        if (dirty_i || !weave_set_eq(prev_i, prev_ni, c.iset, c.ni)) {
            cur_ioff = (u32)u32bDataLen(d->ipool);
            cur_ilen = c.ni;
            for (u32 i = 0; i < c.ni; i++) call(u32bFeed1, d->ipool, c.iset[i]);
            memcpy(prev_i, c.iset, c.ni * sizeof(u32));
            prev_ni = c.ni; dirty_i = NO;
        }
        if (dirty_r || !weave_set_eq(prev_r, prev_nr, c.rset, c.nr)) {
            cur_roff = (u32)u32bDataLen(d->rpool);
            cur_rlen = c.nr;
            for (u32 i = 0; i < c.nr; i++) call(u32bFeed1, d->rpool, c.rset[i]);
            memcpy(prev_r, c.rset, c.nr * sizeof(u32));
            prev_nr = c.nr; dirty_r = NO;
        }
        call(u8bFeed, d->text, c.text);
        call(u32bFeed1, d->tok, (u32)u8bDataLen(d->text));
        call(u32bFeed1, d->ioff, cur_ioff);
        call(u32bFeed1, d->ilen, cur_ilen);
        call(u32bFeed1, d->roff, cur_roff);
        call(u32bFeed1, d->rlen, cur_rlen);
        n++;
    }
    if (c.bad) return WEAVEFAIL;
    d->ntok = n;
    done;
}

//  Acquire (in the CALLER's scope) and fill a wdec for weave `W`.  MUST
//  expand inside the op via a macro so the BASS scratch belongs to the
//  op's `sane()` frame and survives until the op returns — a helper that
//  acquired it would have it rewound at its own call() boundary.
//  `D` is a `wdec` lvalue (pre-zeroed `= {}`); `W` is a `weave const *`.
#define WEAVE_DECODE(D, W) do {                                           \
    size_t _tl = u8bDataLen((W)->tlv);                                    \
    size_t _mt = _tl / 2 + 2, _ms = _tl / 4 + 2;                          \
    BACQ(u8bAcquire( ABC_BASS, (D).text,  _tl + 1));                      \
    BACQ(u32bAcquire(ABC_BASS, (D).tok,   _mt));                          \
    BACQ(u32bAcquire(ABC_BASS, (D).ipool, _ms));                         \
    BACQ(u32bAcquire(ABC_BASS, (D).ioff,  _mt));                          \
    BACQ(u32bAcquire(ABC_BASS, (D).ilen,  _mt));                          \
    BACQ(u32bAcquire(ABC_BASS, (D).rpool, _ms));                         \
    BACQ(u32bAcquire(ABC_BASS, (D).roff,  _mt));                          \
    BACQ(u32bAcquire(ABC_BASS, (D).rlen,  _mt));                          \
    call(weave_decode_fill, &(D), (W));                                  \
} while (0)

//  Emit src token `i` (from a decoded weave) verbatim through builder.
static ok64 wd_emit(weavebld *b, wdp const *p, u32 i) {
    sane(b);
    u32 lo = wd_lo(p, i), hi = wd_hi(p, i);
    u8csc t = {p->base + lo, p->base + hi};
    u32cs is = {p->ipool + p->ioff[i], p->ipool + p->ioff[i] + p->ilen[i]};
    u32cs rs = {p->rpool + p->roff[i], p->rpool + p->roff[i] + p->rlen[i]};
    return WEAVEBldPut(b, t, is, rs);
}

//  Emit src token `i` (text + I-set from `p`) with an explicit R-set.
static ok64 wd_emit_rset(weavebld *b, wdp const *p, u32 i, u32cs rs) {
    sane(b);
    u32 lo = wd_lo(p, i), hi = wd_hi(p, i);
    u8csc t = {p->base + lo, p->base + hi};
    u32cs is = {p->ipool + p->ioff[i], p->ipool + p->ioff[i] + p->ilen[i]};
    return WEAVEBldPut(b, t, is, rs);
}

//  Emit src token `i` with `add_rm` folded into its R-set (DEL case).
static ok64 wd_emit_del(weavebld *b, wdp const *p, u32 i, u32 add_rm) {
    sane(b);
    u32 lo = wd_lo(p, i), hi = wd_hi(p, i);
    u8csc t = {p->base + lo, p->base + hi};
    u32cs is = {p->ipool + p->ioff[i], p->ipool + p->ioff[i] + p->ilen[i]};
    u32 rbuf[WEAVE_SET_MAX]; u32 rn = 0;
    u32 one[1] = {add_rm};
    call(weave_set_union, p->rpool + p->roff[i], p->rlen[i], one, 1,
         rbuf, WEAVE_SET_MAX, &rn);
    u32cs rs = {rbuf, rbuf + rn};
    return WEAVEBldPut(b, t, is, rs);
}

//  Byte-equality of two decoded tokens.
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
    u32       covered;
} weave_blob_ctx;

static ok64 weave_blob_emit(weave_blob_ctx *ctx, u8csc seg) {
    sane(ctx);
    u32 one[1] = {ctx->src};
    u32cs is = {one, one + 1};
    u32cs rs = {NULL, NULL};
    call(WEAVEBldPut, ctx->b, seg, is, rs);
    done;
}

static ok64 weave_blob_cb(u8 tag, u8cs tok, void *vctx) {
    sane(vctx);
    (void)tag;
    weave_blob_ctx *ctx = vctx;
    //  Split any token containing '\n' so each emitted token sits on
    //  exactly one line (windowing in WEAVEEmitDiff classifies by start
    //  line; a multi-line token would orphan its later lines).
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

    weave_blob_ctx ctx = {.b = &b, .base = data[0], .src = src, .covered = 0};
    TOKstate st = {.data = {data[0], data[1]}, .cb = weave_blob_cb, .ctx = &ctx};
    TOKLexer(&st, ext);  // best effort

    //  Tail-fill: cover any bytes the lexer didn't tokenize (binary /
    //  mid-input bail), split at every '\n'.
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
//  WEAVEDiff
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

    //  Empty src: copy nu wholesale, restamping in={src_commit}.
    if (s.ntok == 0) {
        for (u32 i = 0; i < nuv.ntok; i++) {
            u32 lo = wd_lo(&nuv, i), hi = wd_hi(&nuv, i);
            u8csc t = {nuv.base + lo, nuv.base + hi};
            u32 one[1] = {src_commit}; u32cs is = {one, one + 1};
            u32cs rs = {NULL, NULL};
            call(WEAVEBldPut, &bld, t, is, rs);
        }
        done;
    }

    //  Hashlets for nu (all alive) and for alive src tokens.  Plus alive
    //  text + offset-only toks views for NEIL.
    Bu64 alive_h = {}, nu_h = {};
    Bu8  alive_text = {};
    Bu32 alive_toks = {};
    Bi32 work = {};
    Bu32 edlbuf = {};
    BACQ(u64bAcquire(ABC_BASS, alive_h, s.ntok + 1));
    BACQ(u64bAcquire(ABC_BASS, nu_h, nuv.ntok + 1));
    {
        size_t stl = u8bDataLen(sd.text);
        BACQ(u8bAcquire(ABC_BASS, alive_text, stl + 1));
    }
    BACQ(u32bAcquire(ABC_BASS, alive_toks, s.ntok + 1));
    for (u32 i = 0; i < nuv.ntok; i++) {
        u32 lo = wd_lo(&nuv, i), hi = wd_hi(&nuv, i);
        u8csc tb = {nuv.base + lo, nuv.base + hi};
        call(u64bFeed1, nu_h, RAPHash(tb));
    }
    {
        u32 cum = 0;
        for (u32 i = 0; i < s.ntok; i++) {
            if (!wd_alive(&s, i)) continue;
            u32 lo = wd_lo(&s, i), hi = wd_hi(&s, i);
            u8csc tb = {s.base + lo, s.base + hi};
            call(u64bFeed1, alive_h, RAPHash(tb));
            call(u8bFeed, alive_text, tb);
            cum += (hi - lo);
            call(u32bFeed1, alive_toks, tok32Pack('S', cum));  // tag unused
        }
    }

    u64 olen = u64bDataLen(alive_h);
    u64 nlen = (u64)nuv.ntok;
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
        if (olen > 0) { *edlg[1]++ = DIFF_ENTRY(DIFF_DEL, (u32)olen); }
        if (nlen > 0) { *edlg[1]++ = DIFF_ENTRY(DIFF_INS, (u32)nlen); }
        call(NEILCanon, edlg);
    } else {
        u32cs at_view = {(u32cp)u32bDataHead(alive_toks),
                         (u32cp)u32bDataHead(alive_toks) + u32bDataLen(alive_toks)};
        u32cs nt_view = {nuv.tok, nuv.tok + nuv.ntok};
        u8cs  at_text = {u8bDataHead(alive_text),
                         u8bDataHead(alive_text) + u8bDataLen(alive_text)};
        u8cs  nt_text = {nuv.base, nuv.base + (size_t)u8bDataLen(nd.text)};
        NEILCleanup(edlg, at_view, nt_view, at_text, nt_text);
        NEILShift  (edlg, at_view, nt_view, at_text, nt_text);
    }
    e32c *ep = edlbuf[0];
    e32c *ee = edlg[0];

    //  Walk the canonical EDL (EQ + INS-then-DEL runs).
    u32 wi = 0;   // cursor in full src (incl dead)
    u32 ni = 0;   // cursor in nu

    while (ep < ee) {
        u32 op  = DIFF_OP(*ep);
        u32 len = DIFF_LEN(*ep);

        if (op == DIFF_EQ) {
            for (u32 j = 0; j < len; j++) {
                while (wi < s.ntok && !wd_alive(&s, wi)) {
                    call(wd_emit, &bld, &s, wi); wi++;
                }
                if (wi < s.ntok) { call(wd_emit, &bld, &s, wi); wi++; }
                ni++;
            }
            ep++;
            continue;
        }

        //  Non-EQ run totals.
        u32 sum_ins = 0, sum_del = 0;
        while (ep < ee && DIFF_OP(*ep) != DIFF_EQ) {
            u32 l = DIFF_LEN(*ep);
            if (DIFF_OP(*ep) == DIFF_INS) sum_ins += l; else sum_del += l;
            ep++;
        }

        //  Prefix/suffix lift: recover byte-equal boundary tokens as EQ
        //  context (carry src's own provenance) — same role as before.
        u32 adi[1]; (void)adi;
        u32 prefix = 0, suffix = 0;
        if (sum_del > 0 && sum_ins > 0) {
            //  Collect indices of the next sum_del alive src tokens.
            //  Bounded scan; reuse a small BASS buffer.
            Bu32 adi_buf = {};
            BACQ(u32bAcquire(ABC_BASS, adi_buf, sum_del));
            u32 *adip = (u32 *)u32bDataHead(adi_buf);
            u32  adi_n = 0;
            u32  wi_p = wi;
            while (adi_n < sum_del && wi_p < s.ntok) {
                if (wd_alive(&s, wi_p)) adip[adi_n++] = wi_p;
                wi_p++;
            }
            u32 lim = (sum_ins < sum_del) ? sum_ins : sum_del;
            while (prefix < lim && prefix < adi_n) {
                u32 ni_p = ni + prefix, wi_q = adip[prefix];
                u32 nlo = wd_lo(&nuv, ni_p), nhi = wd_hi(&nuv, ni_p);
                u32 slo = wd_lo(&s, wi_q),  shi = wd_hi(&s, wi_q);
                if (nhi - nlo != shi - slo) break;
                if (memcmp(nuv.base + nlo, s.base + slo, nhi - nlo) != 0) break;
                prefix++;
            }
            u32 max_sf = lim - prefix;
            while (suffix < max_sf) {
                u32 ni_p = ni + sum_ins - 1 - suffix;
                u32 wi_q = adip[sum_del - 1 - suffix];
                u32 nlo = wd_lo(&nuv, ni_p), nhi = wd_hi(&nuv, ni_p);
                u32 slo = wd_lo(&s, wi_q),  shi = wd_hi(&s, wi_q);
                if (nhi - nlo != shi - slo) break;
                if (memcmp(nuv.base + nlo, s.base + slo, nhi - nlo) != 0) break;
                suffix++;
            }
        }

        //  Prefix lift → EQ context.
        for (u32 j = 0; j < prefix; j++) {
            while (wi < s.ntok && !wd_alive(&s, wi)) { call(wd_emit, &bld, &s, wi); wi++; }
            if (wi < s.ntok) { call(wd_emit, &bld, &s, wi); wi++; }
            ni++;
        }
        //  Remaining INS: nu tokens, I={src_commit}, R={}.
        u32 remain_ins = sum_ins - prefix - suffix;
        for (u32 j = 0; j < remain_ins; j++) {
            u32 lo = wd_lo(&nuv, ni), hi = wd_hi(&nuv, ni);
            u8csc t = {nuv.base + lo, nuv.base + hi};
            u32 one[1] = {src_commit}; u32cs is = {one, one + 1};
            u32cs rs = {NULL, NULL};
            call(WEAVEBldPut, &bld, t, is, rs);
            ni++;
        }
        //  Remaining DEL: alive src tokens gain src_commit in R-set.
        u32 remain_del = sum_del - prefix - suffix;
        for (u32 j = 0; j < remain_del; j++) {
            while (wi < s.ntok && !wd_alive(&s, wi)) { call(wd_emit, &bld, &s, wi); wi++; }
            if (wi < s.ntok) { call(wd_emit_del, &bld, &s, wi, src_commit); wi++; }
        }
        //  Suffix lift → EQ context.
        for (u32 j = 0; j < suffix; j++) {
            while (wi < s.ntok && !wd_alive(&s, wi)) { call(wd_emit, &bld, &s, wi); wi++; }
            if (wi < s.ntok) { call(wd_emit, &bld, &s, wi); wi++; }
            ni++;
        }
    }

    //  Tail: drain remaining src tokens verbatim.
    while (wi < s.ntok) { call(wd_emit, &bld, &s, wi); wi++; }
    done;
}

// ============================================================
//  WEAVEMerge
// ============================================================

ok64 WEAVEMerge(weave *dst, weave const *a, weave const *b) {
    sane(dst);
    if (!a || !b) return FAILSANITY;

    wdec ad = {}, bd = {};
    WEAVE_DECODE(ad, a);
    WEAVE_DECODE(bd, b);
    wdp av = {}, bv = {};
    wd_view(&av, &ad);
    wd_view(&bv, &bd);

    weavebld bld;
    WEAVEBldInit(&bld, dst);

    if (av.ntok == 0) {
        for (u32 i = 0; i < bv.ntok; i++) call(wd_emit, &bld, &bv, i);
        done;
    }
    if (bv.ntok == 0) {
        for (u32 i = 0; i < av.ntok; i++) call(wd_emit, &bld, &av, i);
        done;
    }

    //  Hashlets over the FULL streams, biased by min(I-set) so the LCS
    //  only matches tokens sharing byte content AND a common stamp — this
    //  keeps genuinely-distinct same-byte tokens (e.g. two base commas)
    //  apart.  A token whose I-set was union'd to a superset (different
    //  min) lands in a non-EQ run vs its un-union'd self; the disjoint-
    //  tail rescue's proper-subset rule reconciles those.
    Bu64 ah = {}, bh = {};
    Bi32 work = {};
    Bu32 edlbuf = {};
    BACQ(u64bAcquire(ABC_BASS, ah, av.ntok + 1));
    BACQ(u64bAcquire(ABC_BASS, bh, bv.ntok + 1));
    for (u32 i = 0; i < av.ntok; i++) {
        u32 lo = wd_lo(&av, i), hi = wd_hi(&av, i);
        u8csc tb = {av.base + lo, av.base + hi};
        u32 mn = av.ilen[i] ? av.ipool[av.ioff[i]] : 0;   // sorted => min
        call(u64bFeed1, ah, RAPHash(tb) * 0x9e3779b97f4a7c15ULL + (u64)mn);
    }
    for (u32 i = 0; i < bv.ntok; i++) {
        u32 lo = wd_lo(&bv, i), hi = wd_hi(&bv, i);
        u8csc tb = {bv.base + lo, bv.base + hi};
        u32 mn = bv.ilen[i] ? bv.ipool[bv.ioff[i]] : 0;
        call(u64bFeed1, bh, RAPHash(tb) * 0x9e3779b97f4a7c15ULL + (u64)mn);
    }

    u64 olen = (u64)av.ntok, nlen = (u64)bv.ntok;
    u64 work_sz = DIFFWorkSize(olen, nlen);
    u64 edl_sz  = DIFFEdlMaxEntries(olen, nlen);
    if (work_sz > 0) BACQ(i32bAcquire(ABC_BASS, work,   work_sz));
    if (edl_sz  > 0) BACQ(u32bAcquire(ABC_BASS, edlbuf, edl_sz));

    u64cs ahs = {u64bDataHead(ah), u64bDataHead(ah) + olen};
    u64cs bhs = {u64bDataHead(bh), u64bDataHead(bh) + nlen};
    e32g edlg = {edlbuf[0], edlbuf[3], edlbuf[0]};
    i32s ws = {i32bHead(work), i32bTerm(work)};
    ok64 diff_o = DIFFu64s(edlg, ws, ahs, bhs);
    if (diff_o != OK) {
        edlg[1] = edlg[0];
        if (olen > 0) { *edlg[1]++ = DIFF_ENTRY(DIFF_DEL, (u32)olen); }
        if (nlen > 0) { *edlg[1]++ = DIFF_ENTRY(DIFF_INS, (u32)nlen); }
    }
    //  No NEILCleanup/Shift here: the provenance-mixed hash already gates
    //  false matches, and killing short EQ runs would split same-identity
    //  shared tokens (same I-set, same bytes) out of the EQ-reconcile pass
    //  — leaving duplicate copies that revive together on recovery
    //  (graf/fuzz/WEAVE2).  Just canonicalize splice order.
    call(NEILCanon, edlg);

    e32c *ep = edlbuf[0];
    e32c *ee = edlg[0];
    u32 ai = 0, bi = 0;

    while (ep < ee) {
        u32 op  = DIFF_OP(*ep);
        u32 len = DIFF_LEN(*ep);

        if (op == DIFF_EQ) {
            for (u32 j = 0; j < len; j++) {
                if (ai >= av.ntok || bi >= bv.ntok) break;
                //  Reconcile: I=union, R=union; text from a (byte-equal).
                u32 ibuf[WEAVE_SET_MAX], rbuf[WEAVE_SET_MAX], inn = 0, rnn = 0;
                call(weave_set_union,
                     av.ipool + av.ioff[ai], av.ilen[ai],
                     bv.ipool + bv.ioff[bi], bv.ilen[bi],
                     ibuf, WEAVE_SET_MAX, &inn);
                call(weave_set_union,
                     av.rpool + av.roff[ai], av.rlen[ai],
                     bv.rpool + bv.roff[bi], bv.rlen[bi],
                     rbuf, WEAVE_SET_MAX, &rnn);
                u32 lo = wd_lo(&av, ai), hi = wd_hi(&av, ai);
                u8csc t = {av.base + lo, av.base + hi};
                u32cs is = {ibuf, ibuf + inn};
                u32cs rs = {rbuf, rbuf + rnn};
                call(WEAVEBldPut, &bld, t, is, rs);
                ai++; bi++;
            }
            ep++;
            continue;
        }

        //  Non-EQ run (canonical INS-then-DEL): a-run [ai,ai+sum_del),
        //  b-run [bi,bi+sum_ins).
        u32 sum_del = 0, sum_ins = 0;
        while (ep < ee && DIFF_OP(*ep) != DIFF_EQ) {
            u32 l = DIFF_LEN(*ep);
            if (DIFF_OP(*ep) == DIFF_INS) sum_ins += l; else sum_del += l;
            ep++;
        }

        //  Count alive tokens on each side of the run.
        u32 a_alive = 0, b_alive = 0;
        for (u32 j = 0; j < sum_del; j++) if (wd_alive(&av, ai + j)) a_alive++;
        for (u32 j = 0; j < sum_ins; j++) if (wd_alive(&bv, bi + j)) b_alive++;

        //  Longest byte-equal ALIVE prefix shared by the two sides
        //  (skipping dead tokens).  We DEDUP (union the inserters into
        //  one token) ONLY when the shorter side's alive tokens are
        //  fully contained as that prefix — i.e. the same edit reached
        //  both sides (concurrent identical insert / foster absorption).
        //  A merely-partial overlap (two different blocks sharing a
        //  leading whitespace/brace) is a genuine conflict: both sides
        //  emit in full so WEAVEEmitMerged frames them with markers.
        u32 matched = 0;
        {
            u32 pa = 0, pb = 0;
            while (pa < sum_del && pb < sum_ins) {
                while (pa < sum_del && !wd_alive(&av, ai + pa)) pa++;
                while (pb < sum_ins && !wd_alive(&bv, bi + pb)) pb++;
                if (pa >= sum_del || pb >= sum_ins) break;
                if (!wd_tok_eq(&av, ai + pa, &bv, bi + pb)) break;
                matched++; pa++; pb++;
            }
        }
        u32 alive_min = (a_alive < b_alive) ? a_alive : b_alive;
        b8 agree = (matched > 0 && matched == alive_min);

        u32 ja = 0, jb = 0;
        if (agree) {
            //  Emit the matched alive prefix once with unioned inserters,
            //  carrying each side's interspersed dead tokens (a then b).
            u32 paired = 0;
            while (paired < matched) {
                while (ja < sum_del && !wd_alive(&av, ai + ja)) {
                    call(wd_emit, &bld, &av, ai + ja); ja++;
                }
                while (jb < sum_ins && !wd_alive(&bv, bi + jb)) {
                    call(wd_emit, &bld, &bv, bi + jb); jb++;
                }
                u32 ibuf[WEAVE_SET_MAX], inn = 0;
                call(weave_set_union,
                     av.ipool + av.ioff[ai + ja], av.ilen[ai + ja],
                     bv.ipool + bv.ioff[bi + jb], bv.ilen[bi + jb],
                     ibuf, WEAVE_SET_MAX, &inn);
                u32 lo = wd_lo(&av, ai + ja), hi = wd_hi(&av, ai + ja);
                u8csc t = {av.base + lo, av.base + hi};
                u32cs is = {ibuf, ibuf + inn};
                u32cs rs = {NULL, NULL};   // both alive => union empty
                call(WEAVEBldPut, &bld, t, is, rs);
                ja++; jb++; paired++;
            }
        }
        //  Disjoint tail.  3-way reconciliation of a byte-equal (a,b) pair
        //  that is really ONE logical token:
        //    * equal I-sets + OPPOSITE aliveness — same base token, kept on
        //      one side, deleted on the other → delete wins.
        //    * one I-set a PROPER SUBSET of the other — the same token at
        //      two union stages ({1} vs {0,1}); the min-mix hash put them in
        //      different runs (graf/fuzz/WEAVE2).
        //  Equal I-sets + SAME aliveness are two distinct same-stamp tokens
        //  (e.g. two base commas) — left alone; fully-disjoint I-sets are
        //  genuine divergence for the conflict framing.
        //
        //  ORDER: theirs is walked IN ORDER (a reconciled pair emits at the
        //  THEIRS position, not the ours position) so the merge preserves
        //  theirs' token order — else a later re-merge sees a transposed
        //  stream the LCS can't realign, duplicating the moved token
        //  (graf/test/WEAVE2 remerge_order_preserved).  Ours-only tokens
        //  lead, keeping conflict framing ours-first.
        u32 a_base = ai + ja, a_n = sum_del - ja;
        u32 b_base = bi + jb, b_n = sum_ins - jb;
        //  owner[ka] = theirs-index that ABSORBS ours-token ka, or NONE.
        //  A theirs-token may absorb MANY ours-tokens: a union token
        //  (e.g. a{0,2}) is the same logical token as each of its
        //  components (a{0}, a{2}), so it must claim them ALL and union
        //  their provenance — claiming only one leaves the rest as
        //  duplicates that revive together (graf/fuzz/WEAVE2).
        Bu32 owner_buf = {};
        u32 *owner = NULL;
        if (a_n > 0 && b_n > 0) {
            BACQ(u32bAcquire(ABC_BASS, owner_buf, a_n));
            owner = (u32 *)u32bDataHead(owner_buf);
            for (u32 ka = 0; ka < a_n; ka++) owner[ka] = 0xFFFFFFFFu;
            for (u32 kb = 0; kb < b_n; kb++) {
                u32 bx = b_base + kb;
                for (u32 ka = 0; ka < a_n; ka++) {
                    if (owner[ka] != 0xFFFFFFFFu) continue;
                    u32 ax = a_base + ka;
                    u32 const *ia = av.ipool + av.ioff[ax];
                    u32 const *ib = bv.ipool + bv.ioff[bx];
                    u32 na = av.ilen[ax], nb = bv.ilen[bx];
                    b8 ieq = weave_set_eq(ia, na, ib, nb);
                    b8 sub = weave_set_subset(ia, na, ib, nb) ||
                             weave_set_subset(ib, nb, ia, na);
                    b8 opp = wd_alive(&av, ax) != wd_alive(&bv, bx);
                    if (!((ieq && opp) || (sub && !ieq))) continue;
                    if (!wd_tok_eq(&av, ax, &bv, bx)) continue;
                    owner[ka] = kb;
                }
            }
        }
        //  anchor[kb] = the smallest ours-index theirs-token kb absorbs,
        //  or NONE.  Reconciled pairs are ANCHORS that pin the merge order.
        Bu32 anchor_buf = {};
        u32 *anchor = NULL;
        if (b_n > 0) {
            BACQ(u32bAcquire(ABC_BASS, anchor_buf, b_n));
            anchor = (u32 *)u32bDataHead(anchor_buf);
            for (u32 kb = 0; kb < b_n; kb++) anchor[kb] = 0xFFFFFFFFu;
            for (u32 ka = 0; owner && ka < a_n; ka++) {
                if (owner[ka] == 0xFFFFFFFFu) continue;
                u32 kb = owner[ka];
                if (anchor[kb] == 0xFFFFFFFFu || ka < anchor[kb]) anchor[kb] = ka;
            }
        }
        //  Segmented 3-way walk.  Between consecutive anchors emit
        //  ours-only then theirs-only (conflict ours-first); at each anchor
        //  emit ONE reconciled token (theirs ∪ every ours-token it absorbs)
        //  at the theirs position — preserving BOTH sides' order around
        //  shared tokens (graf/test/WEAVE2 merge_order_ours_only_after_anchor).
        u32 prev_ka = 0, prev_kb = 0;
        for (u32 kb = 0; kb < b_n; kb++) {
            if (!anchor || anchor[kb] == 0xFFFFFFFFu) continue;
            u32 ka = anchor[kb];
            if (ka < prev_ka) ka = prev_ka;   // clamp crossing anchors
            for (u32 x = prev_ka; x < ka; x++)
                if (owner[x] == 0xFFFFFFFFu) call(wd_emit, &bld, &av, a_base + x);
            for (u32 y = prev_kb; y < kb; y++)
                if (anchor[y] == 0xFFFFFFFFu) call(wd_emit, &bld, &bv, b_base + y);
            //  Reconciled token: union theirs bx with every ours it owns.
            u32 bx = b_base + kb;
            u32 acc_i[WEAVE_SET_MAX], acc_in = bv.ilen[bx];
            u32 acc_r[WEAVE_SET_MAX], acc_rn = bv.rlen[bx];
            for (u32 z = 0; z < acc_in; z++) acc_i[z] = bv.ipool[bv.ioff[bx] + z];
            for (u32 z = 0; z < acc_rn; z++) acc_r[z] = bv.rpool[bv.roff[bx] + z];
            for (u32 x = 0; x < a_n; x++) {
                if (owner[x] != kb) continue;
                u32 ax = a_base + x;
                u32 tmp[WEAVE_SET_MAX], tn = 0;
                call(weave_set_union, acc_i, acc_in,
                     av.ipool + av.ioff[ax], av.ilen[ax], tmp, WEAVE_SET_MAX, &tn);
                memcpy(acc_i, tmp, tn * sizeof(u32)); acc_in = tn;
                call(weave_set_union, acc_r, acc_rn,
                     av.rpool + av.roff[ax], av.rlen[ax], tmp, WEAVE_SET_MAX, &tn);
                memcpy(acc_r, tmp, tn * sizeof(u32)); acc_rn = tn;
            }
            u32 lo = wd_lo(&bv, bx), hi = wd_hi(&bv, bx);
            u8csc t = {bv.base + lo, bv.base + hi};
            u32cs is = {acc_i, acc_i + acc_in};
            u32cs rs = {acc_r, acc_r + acc_rn};
            call(WEAVEBldPut, &bld, t, is, rs);
            prev_ka = ka + 1; prev_kb = kb + 1;
        }
        //  Trailing segment: ours-only then theirs-only.
        for (u32 x = prev_ka; x < a_n; x++)
            if (!owner || owner[x] == 0xFFFFFFFFu) call(wd_emit, &bld, &av, a_base + x);
        for (u32 y = prev_kb; y < b_n; y++)
            if (!anchor || anchor[y] == 0xFFFFFFFFu) call(wd_emit, &bld, &bv, b_base + y);
        ai += sum_del; bi += sum_ins;
    }

    while (ai < av.ntok) { call(wd_emit, &bld, &av, ai); ai++; }
    while (bi < bv.ntok) { call(wd_emit, &bld, &bv, bi); bi++; }
    done;
}

// ============================================================
//  WEAVEReplay
// ============================================================

ok64 WEAVEReplay(weave *dst,
                 weave const *const *parents, u32 nparents,
                 u8cs result_blob, u8cs ext,
                 u32 merge_in) {
    sane(dst && parents && nparents >= 1);

    weave blob_w = {}, m0 = {}, m1 = {};
    call(WEAVEInit, &blob_w);
    ok64 ret = WEAVEInit(&m0);
    if (ret != OK) { WEAVEFree(&blob_w); return ret; }
    ret = WEAVEInit(&m1);
    if (ret != OK) { WEAVEFree(&blob_w); WEAVEFree(&m0); return ret; }

    weave const *merged = parents[0];
    if (nparents >= 2) {
        ret = WEAVEMerge(&m0, parents[0], parents[1]);
        weave *cur = &m0, *nxt = &m1;
        for (u32 k = 2; ret == OK && k < nparents; k++) {
            ret = WEAVEMerge(nxt, cur, parents[k]);
            weave *tmp = cur; cur = nxt; nxt = tmp;
        }
        merged = cur;
    }
    if (ret == OK) ret = WEAVEFromBlob(&blob_w, result_blob, ext, merge_in);
    if (ret == OK) ret = WEAVEDiff(dst, merged, &blob_w, merge_in);

    WEAVEFree(&blob_w);
    WEAVEFree(&m0);
    WEAVEFree(&m1);
    return ret;
}

// ============================================================
//  Emission: classification helpers over decoded sets
// ============================================================

static b8 weave_scope_alive(u32cs iset, u32cs rset,
                            WEAVEsetfn pred, void *ctx) {
    b8 ins = NO;
    $for(u32c, p, iset) { if (*p == 0 || pred(*p, ctx)) { ins = YES; break; } }
    if (!ins) return NO;
    $for(u32c, p, rset) { if (*p == 0 || pred(*p, ctx)) return NO; }
    return YES;
}

static u8 weave_diff_classify(u32cs iset, u32cs rset,
                              WEAVEsetfn in_from, void *from_ctx,
                              WEAVEsetfn in_to,   void *to_ctx) {
    b8 af = weave_scope_alive(iset, rset, in_from, from_ctx);
    b8 at = weave_scope_alive(iset, rset, in_to,   to_ctx);
    if (at && !af) return 'I';
    if (af && !at) return 'D';
    if (af && at)  return ' ';
    return 0;
}

// --- WEAVEEmitDiff ---

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

    #define ISET(i) ((u32cs){p.ipool + p.ioff[i], p.ipool + p.ioff[i] + p.ilen[i]})
    #define RSET(i) ((u32cs){p.rpool + p.roff[i], p.rpool + p.roff[i] + p.rlen[i]})

    //  Pass 1: mark changed lines.
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
        u8 tag = weave_diff_classify(ISET(i), RSET(i),
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

    //  Pass 2: cluster changed lines into windows.
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

    //  Pass 3: emit one hunk per window.
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
        u8 tag = weave_diff_classify(ISET(i), RSET(i),
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
    #undef ISET
    #undef RSET
    return ret;
}

// --- WEAVEEmitFull ---

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

    #define ISET(i) ((u32cs){p.ipool + p.ioff[i], p.ipool + p.ioff[i] + p.ilen[i]})
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
        u8 tag = weave_diff_classify(ISET(i), RSET(i),
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
    #undef ISET
    #undef RSET
    return ret;
}

// --- WEAVEEmitMerged ---

#define WEAVE_EMIT_MAX_PREDS  32
#define WEAVE_EMIT_MAX_GROUPS 32

static u32 weave_emit_membership(u32cs iset,
                                 WEAVEsetfn const *preds, void *const *ctxs,
                                 u32 npreds, u32 spine_mask) {
    $for(u32c, q, iset) if (*q == 0) return spine_mask;   // spine
    u32 m = 0;
    for (u32 pi = 0; pi < npreds; pi++) {
        if (!preds[pi]) continue;
        $for(u32c, q, iset) {
            if (preds[pi](*q, ctxs ? ctxs[pi] : NULL)) { m |= (1u << pi); break; }
        }
    }
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

    #define ISET(i) ((u32cs){p.ipool + p.ioff[i], p.ipool + p.ioff[i] + p.ilen[i]})
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
        u32 m = weave_emit_membership(ISET(i), preds, ctxs, npreds, spine_mask);
        if (m == spine_mask) { EMITTOK(i); i++; continue; }

        u32 run_lo = i, run_hi = i;
        while (run_hi < ntok) {
            if (!wd_alive(&p, run_hi)) { run_hi++; continue; }
            u32 mm = weave_emit_membership(ISET(run_hi), preds, ctxs,
                                           npreds, spine_mask);
            if (mm == spine_mask) break;
            run_hi++;
        }

        b8 conflict = NO;
        u32 groups[WEAVE_EMIT_MAX_GROUPS];
        u32 ngroups = 0;
        for (u32 j = run_lo; j < run_hi && !conflict; j++) {
            if (!wd_alive(&p, j)) continue;
            u32 mj = weave_emit_membership(ISET(j), preds, ctxs, npreds, spine_mask);
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
            u32 mj = weave_emit_membership(ISET(j), preds, ctxs, npreds, spine_mask);
            b8 dup = NO;
            for (u32 k = 0; k < ngroups; k++) if (groups[k] == mj) { dup = YES; break; }
            if (!dup && ngroups < WEAVE_EMIT_MAX_GROUPS) groups[ngroups++] = mj;
        }
        call(u8bFeed, out, mk_open);
        for (u32 g = 0; g < ngroups; g++) {
            if (g > 0) call(u8bFeed, out, mk_mid);
            for (u32 j = run_lo; j < run_hi; j++) {
                if (!wd_alive(&p, j)) continue;
                u32 mj = weave_emit_membership(ISET(j), preds, ctxs, npreds, spine_mask);
                if (mj != groups[g]) continue;
                EMITTOK(j);
            }
        }
        call(u8bFeed, out, mk_close);
        i = run_hi;
    }
    #undef ISET
    #undef EMITTOK
    done;
}
