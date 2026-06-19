//  WEAVE (DOG-003): columnar, HUNK-compatible file-history weave.
//  See WEAVE.h for the model.  This file: the codec (Parse/Serialize),
//  the active-commit scope bitmap, and the tip alive-bytes scan.  The
//  ins/rms cursor + Produce/Next/Merge land next.
//
#include "WEAVE.h"

#include "abc/DIFF.h"
#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/TLV.h"
#include "dog/BRAM.h"
#include "dog/NEIL.h"

// ============================================================
//  Codec: 'W' container <-> columnar view
// ============================================================

//  Zero-copy view over a 'W' blob: each column is a slice into `blob`.
//  'X'/'C'/'I'/'M' are byte slices; 'K'/'C' are reinterpreted as tok32/u64
//  element views (LE, like HUNK's 'K').
ok64 WEAVEParse(weave *w, u8csc blob) {
    sane(w);
    zerop(w);
    u8cs rest = {};
    u8csMv(rest, blob);
    u8  otype = 0;
    u8cs inner = {};
    call(TLVu8sDrain, rest, &otype, inner);
    if (otype != WEAVE_TLV) return WEAVEFAIL;
    while (u8csLen(inner) > 0) {
        u8  t = 0;
        u8cs v = {};
        call(TLVu8sDrain, inner, &t, v);
        switch (t) {
        case WEAVE_TLV_TXT: u8csMv(w->text, v); break;
        case WEAVE_TLV_INS: u8csMv(w->ins, v);  break;
        case WEAVE_TLV_RMS: u8csMv(w->rms, v);  break;
        case WEAVE_TLV_TOK:
            w->toks[0] = (tok32c *)v[0]; w->toks[1] = (tok32c *)v[1]; break;
        case WEAVE_TLV_CMT:
            w->commits[0] = (u64c *)v[0]; w->commits[1] = (u64c *)v[1]; break;
        default: break;   // unknown sub-record: ignore (forward-compat)
        }
    }
    done;
}

//  Serialize a weave's columns into a fresh 'W' blob in `into`.  Builders
//  normally write 'W' directly; this is the round-trip / test path.
ok64 WEAVESerialize(u8s into, weave const *w) {
    sane(w);
    size_t cap = u8csLen(w->text) + (size_t)$len(w->toks) * sizeof(tok32)
               + u8csLen(w->ins) + u8csLen(w->rms)
               + (size_t)$len(w->commits) * sizeof(u64) + 64;
    a_carve(u8, inner, cap);
    u8csc kbytes = {(u8c *)w->toks[0], (u8c *)w->toks[1]};
    u8csc cbytes = {(u8c *)w->commits[0], (u8c *)w->commits[1]};
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_TXT, w->text);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_TOK, kbytes);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_INS, w->ins);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_RMS, w->rms);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_CMT, cbytes);
    call(TLVu8sFeed, into, WEAVE_TLV, u8bDataC(inner));
    done;
}

// ============================================================
//  Scope: active-commit bitmap over commits[]
// ============================================================

//  Fill `into` (caller-acquired, >= ncommits bits) so bit i is set iff
//  i == WEAVE_SPINE or commits[i] is in `active`.  Linear membership;
//  ncommits is per-file (small), so no index needed yet.
ok64 WEAVEScope(u1b *into, weave const *w, u64cs active) {
    sane(into && w);
    u32 n = (u32)$len(w->commits);
    u1bReset(into);
    for (u32 i = 0; i < n; i++) {
        b8 on = (i == WEAVE_SPINE);
        if (!on) {
            u64 id = w->commits[0][i];
            $for(u64c, a, active) if (*a == id) { on = YES; break; }
        }
        call(u1bFeed1, into, on);
    }
    done;
}

// ============================================================
//  Tip alive bytes (rm-bit-clear scan; no ins/rms decode)
// ============================================================

//  A token is alive at the tip iff its RM bit is clear.  Emit those
//  tokens' bytes into `out` (reset on entry), in weave order.
ok64 WEAVEAlive(weave const *w, u8b out) {
    sane(w && out);
    u8bReset(out);
    u32 n  = (u32)$len(w->toks);
    u32 lo = 0;
    for (u32 i = 0; i < n; i++) {
        tok32 t  = w->toks[0][i];
        u32   hi = tok32Offset(t);
        if (!(tok32Side(t) & WEAVE_RM)) {
            a_part(u8c, seg, w->text, lo, hi - lo);
            call(u8bFeed, out, seg);
        }
        lo = hi;
    }
    done;
}

// ============================================================
//  Sequential read: consume one token from a weave copy
// ============================================================

//  Advance `c` (a consuming copy of the weave) by one token, filling
//  `out`.  text comes off the front of c->text (length = this token's
//  end offset minus the running `*off`); the inserter is one LE-u32 from
//  c->ins when the IN bit is set (else the spine); removers are a u32cs
//  view into c->rms when the RM bit is set (a count + N when custom).
//  Call only while c->toks is non-empty; OK per token, WEAVEFAIL if the
//  stream is malformed (so it composes with call()/try()).
ok64 WEAVEStep(weave *c, u32 *off, weavetok *out) {
    sane(c && off && out);
    zerop(out);
    if (c->toks[0] >= c->toks[1]) fail(WEAVEFAIL);
    tok32 t = *c->toks[0];
    c->toks[0]++;                       // consume one token (slice-head advance)
    u32 hi = tok32Offset(t);
    if (hi < *off) fail(WEAVEFAIL);
    u32 len = hi - *off;
    *off = hi;
    if ((size_t)len > u8csLen(c->text)) fail(WEAVEFAIL);
    a_head(u8c, tb, c->text, len);
    u8csMv(out->text, tb);
    (void)u8csUsed(c->text, len);
    out->tag = tok32Tag(t);
    u8 side = tok32Side(t);
    out->has_in = (side & WEAVE_IN) ? YES : NO;
    out->inserter = WEAVE_SPINE;
    if (side & WEAVE_IN) {
        if (u8csLen(c->ins) < 4) fail(WEAVEFAIL);
        u8sDrain32(c->ins, &out->inserter);
    }
    if (side & WEAVE_RM) {
        u32 n = 1;
        if (tok32Custom(t)) {
            if (u8csLen(c->rms) < 4) fail(WEAVEFAIL);
            u8sDrain32(c->rms, &n);
        }
        if (u8csLen(c->rms) < (size_t)n * 4) fail(WEAVEFAIL);
        u32c *rb = (u32c *)c->rms[0];
        u32cs rs = {rb, rb + n};
        $mv(out->rms, rs);
        (void)u8csUsed(c->rms, (size_t)n * 4);
    }
    done;
}

// ============================================================
//  Produce: file bytes at any rev (scope-classified alive)
// ============================================================

//  Emit (into `out`, reset on entry) every token visible in `scope`:
//  inserter bit set AND no remover bit set.  scope bit 0 (spine) is
//  always set by WEAVEScope, so spine tokens pass unless a remover is
//  reachable.  Order is weave order = file order.
ok64 WEAVEProduce(weave const *w, weavescope scope, u8b out) {
    sane(w && out);
    u8bReset(out);
    weave c = *w;
    u32 off = 0;
    weavetok tk = {};
    while ((size_t)$len(c.toks) > 0) {
        call(WEAVEStep, &c, &off, &tk);
        if (!u1At(scope, tk.inserter)) continue;     // inserter unreachable
        b8 dead = NO;
        $for(u32c, r, tk.rms) if (u1At(scope, *r)) { dead = YES; break; }
        if (dead) continue;
        call(u8bFeed, out, tk.text);
    }
    done;
}

// ============================================================
//  WEAVENext: fold one blob into the weave (linear chain step)
// ============================================================

//  Line-coherent tokenizer (mirrors graf's weave_blob_cb): split each lexer
//  token at '\n' so the diff's line anchors work.  Per segment append a
//  tok32 (tag, side EQ, cumulative end); optionally the end, tag, and
//  RAPHash (pass NULL to skip a column).
typedef struct {
    u8cp  base;
    u32b *toks;    // tok32(tag,end)            (required)
    u32b *ends;    // cumulative end offset     (or NULL)
    u8b  *tags;    // tag                       (or NULL)
    u64b *hashes;  // RAPHash(segment)          (or NULL)
    u32   covered;
} wnext_ctx;

static ok64 wnext_tok_cb(u8 tag, u8cs tok, void *vctx) {
    sane(vctx);
    wnext_ctx *c = vctx;
    u8c *p = tok[0], *e = tok[1];
    while (p < e) {
        u8c *q = p;
        while (q < e && *q != '\n') q++;
        u8c *seg_end = (q < e) ? q + 1 : e;
        u32 end = (u32)(seg_end - c->base);
        call(u32bFeed1, *c->toks, tok32PackSide(tag, TOK_SIDE_EQ, end));
        if (c->ends) call(u32bFeed1, *c->ends, end);
        if (c->tags) call(u8bFeed1, *c->tags, tag);
        if (c->hashes) { u8csc seg = {p, seg_end}; call(u64bFeed1, *c->hashes, RAPHash(seg)); }
        p = seg_end;
    }
    c->covered = (u32)(e - c->base);
    done;
}

static ok64 wnext_tokenize(wnext_ctx *c, u8csc blob, u8csc ext) {
    sane(c);
    if ($empty(blob)) done;
    c->base = blob[0];
    TOKstate st = {.data = {blob[0], blob[1]}, .cb = wnext_tok_cb, .ctx = c};
    ok64 lo = TOKLexer(&st, ext);
    if (lo != OK) return lo;
    u32 total = (u32)$len(blob), cur = c->covered;
    while (cur < total) {                       // uncovered tail (unknown ext)
        u32 hi = cur;
        while (hi < total && blob[0][hi] != '\n') hi++;
        if (hi < total) hi++;
        call(u32bFeed1, *c->toks, tok32PackSide('S', TOK_SIDE_EQ, hi));
        if (c->ends) call(u32bFeed1, *c->ends, hi);
        if (c->tags) call(u8bFeed1, *c->tags, 'S');
        if (c->hashes) { a_part(u8c, seg, blob, cur, hi - cur); call(u64bFeed1, *c->hashes, RAPHash(seg)); }
        cur = hi;
    }
    done;
}

//  --- output column emit (text/toks/ins/rms; cum = running end offset) ---
typedef struct { u8b *text; u32b *toks; u8b *ins; u8b *rms; u32 cum; } wnext_out;

static ok64 wnext_emit(wnext_out *o, u8csc bytes, u8 tag,
                       b8 has_in, u32 in_idx, u32cs rms) {
    sane(o);
    u32 nrms = (u32)$len(rms);
    u8 side = (u8)((has_in ? WEAVE_IN : 0) | (nrms ? WEAVE_RM : 0));
    o->cum += (u32)u8csLen(bytes);
    call(u8bFeed, *o->text, bytes);
    u32 tk = tok32PackSide(tag, side, o->cum);
    if (nrms > 1) tk = tok32SetCustom(tk, 1);
    call(u32bFeed1, *o->toks, tk);
    if (has_in) call(u8sFeed32, u8bIdle(*o->ins), &in_idx);
    if (nrms > 1) { u32 n = nrms; call(u8sFeed32, u8bIdle(*o->rms), &n); }
    $for(u32c, r, rms) { u32 v = *r; call(u8sFeed32, u8bIdle(*o->rms), &v); }
    done;
}

//  Advance the old-token stream; *have = a token was read into *cur.
static ok64 old_step(weave *oc, u32 *ooff, weavetok *cur, b8 *have) {
    sane(oc);
    *have = ((size_t)$len(oc->toks) > 0);
    if (*have) call(WEAVEStep, oc, ooff, cur);
    done;
}

static ok64 wnext_from_blob(u8s into, u8csc new_blob, u8csc ext, u64 commit) {
    sane(into != NULL);
    a_carve(u32, toks, (size_t)$len(new_blob) + 16);
    wnext_ctx c = {.toks = &toks};
    call(wnext_tokenize, &c, new_blob, ext);
    a_carve(u64, cmts, 1);
    call(u64bFeed1, cmts, commit);
    weave nw = {};
    u8csMv(nw.text, new_blob);
    nw.toks[0] = (tok32c *)u32bDataHead(toks);
    nw.toks[1] = (tok32c *)u32bDataHead(toks) + u32bDataLen(toks);
    nw.commits[0] = (u64c *)u64bDataHead(cmts);
    nw.commits[1] = (u64c *)u64bDataHead(cmts) + u64bDataLen(cmts);
    call(WEAVESerialize, into, &nw);
    done;
}

//  Diff fold: diff w's tip-alive view against new_blob (RAPHash + BRAM +
//  NEIL).  Survivors carry through; dropped survivors gain `commit` as a
//  remover; new tokens insert with inserter=commit; dead tokens pass
//  through verbatim (the SCCS-weave invariant).
static ok64 wnext_diff(u8s into, weave const *w, u8csc new_blob, u8csc ext,
                       u64 commit) {
    sane(into != NULL);

    //  commits = old ++ [commit] (reuse if present); new_idx = its slot.
    a_carve(u64, out_cmts, (size_t)$len(w->commits) + 1);
    u32 new_idx = (u32)$len(w->commits);
    b8 found = NO; u32 ci = 0;
    $for(u64c, cid, w->commits) {
        call(u64bFeed1, out_cmts, *cid);
        if (*cid == commit) { found = YES; new_idx = ci; }
        ci++;
    }
    if (!found) call(u64bFeed1, out_cmts, commit);

    //  Pass 1: tip-alive baseline (hashes + tok offsets + text) for the diff.
    a_carve(u64, alive_h,  (size_t)$len(w->toks) + 1);
    a_carve(u32, alive_tk, (size_t)$len(w->toks) + 1);
    a_carve(u8,  alive_tx, u8csLen(w->text) + 1);
    {
        weave oc = *w; u32 ooff = 0; weavetok ot; u32 acum = 0;
        while ((size_t)$len(oc.toks) > 0) {
            call(WEAVEStep, &oc, &ooff, &ot);
            if ((size_t)$len(ot.rms) != 0) continue;   // dead: not baseline
            call(u64bFeed1, alive_h, RAPHash(ot.text));
            call(u8bFeed, alive_tx, ot.text);
            acum += (u32)u8csLen(ot.text);
            call(u32bFeed1, alive_tk, tok32PackSide(ot.tag, TOK_SIDE_EQ, acum));
        }
    }

    //  New tokens: hashes (BRAM) + tok offsets / ends / tags (NEIL + emit).
    a_carve(u32, new_tk,  (size_t)$len(new_blob) + 16);
    a_carve(u32, new_end, (size_t)$len(new_blob) + 16);
    a_carve(u8,  new_tag, (size_t)$len(new_blob) + 16);
    a_carve(u64, new_h,   (size_t)$len(new_blob) + 16);
    {
        wnext_ctx c = {.toks = &new_tk, .ends = &new_end,
                       .tags = &new_tag, .hashes = &new_h};
        call(wnext_tokenize, &c, new_blob, ext);
    }

    u64 olen = u64bDataLen(alive_h);
    u64 nlen = u64bDataLen(new_h);

    //  Output columns.
    a_carve(u8,  o_text, u8csLen(w->text) + u8csLen(new_blob) + 1);
    a_carve(u32, o_toks, (size_t)$len(w->toks) + (size_t)nlen + 1);
    a_carve(u8,  o_ins,  4 * ((size_t)$len(w->toks) + (size_t)nlen + 1));
    //  rms: carry forward every existing remover byte (a merged token may
    //  hold many removers — count + N indices, > 8 bytes), plus up to one
    //  fresh remover (4 bytes) per output token from a DELMARK.
    a_carve(u8,  o_rms,  u8csLen(w->rms)
                         + 4 * ((size_t)$len(w->toks) + (size_t)nlen + 1));
    wnext_out o = {.text = &o_text, .toks = &o_toks, .ins = &o_ins, .rms = &o_rms};

    u32 *nend = (u32 *)u32bDataHead(new_end);
    u8  *ntag = (u8 *)u8bDataHead(new_tag);

    weave oc = *w; u32 ooff = 0; weavetok cur; b8 have = NO;
    call(old_step, &oc, &ooff, &cur, &have);

    #define EMIT_NEW(J) do { u32 _lo = (J) ? nend[(J) - 1] : 0, _hi = nend[(J)]; \
        a_part(u8c, _nb, new_blob, _lo, _hi - _lo); u32cs _none = {NULL, NULL};  \
        call(wnext_emit, &o, _nb, ntag[(J)], YES, new_idx, _none); } while (0)
    #define CARRY() do { call(wnext_emit, &o, cur.text, cur.tag, cur.has_in,    \
        cur.inserter, cur.rms); call(old_step, &oc, &ooff, &cur, &have); } while (0)
    #define DELMARK() do { u32 _one = new_idx; u32cs _rs = {&_one, &_one + 1};  \
        call(wnext_emit, &o, cur.text, cur.tag, cur.has_in, cur.inserter, _rs); \
        call(old_step, &oc, &ooff, &cur, &have); } while (0)

    if (olen == 0) {                       // no baseline: insert all, pass old
        for (u32 j = 0; j < nlen; j++) EMIT_NEW(j);
        while (have) CARRY();
    } else if (nlen == 0) {                // no content: delete the baseline
        while (have) { if ((size_t)$len(cur.rms) == 0) DELMARK(); else CARRY(); }
    } else {
        u64 work_sz = DIFFWorkSize(olen, nlen);
        u64 edl_sz  = DIFFEdlMaxEntries(olen, nlen);
        a_carve(i32, work,   work_sz ? work_sz : 1);
        a_carve(u32, edlbuf, edl_sz ? edl_sz : 1);
        a_dup(u64c, oh, u64bDataC(alive_h));
        a_dup(u64c, nh, u64bDataC(new_h));
        e32g edlg = {edlbuf[0], edlbuf[3], edlbuf[0]};
        i32s ws = {i32bHead(work), i32bTerm(work)};
        ok64 diff_o = BRAMu64s(edlg, ws, oh, nh);
        if (diff_o != OK) {
            call(BRAMFallbackEdl, edlg, (u32)olen, (u32)nlen);
            call(NEILCanon, edlg);
        } else {
            a_dup(u32c, at_view, u32bDataC(alive_tk));
            a_dup(u32c, nt_view, u32bDataC(new_tk));
            a_dup(u8c,  at_text, u8bDataC(alive_tx));
            NEILCleanup(edlg, at_view, nt_view, at_text, new_blob);
            NEILShift  (edlg, at_view, nt_view, at_text, new_blob);
        }
        e32c *ep = edlbuf[0];
        e32c *ee = edlg[0];
        u32 ni = 0;
        while (ep < ee) {
            u32 op = DIFF_OP(*ep), len = DIFF_LEN(*ep);
            if (op == DIFF_EQ) {
                for (u32 j = 0; j < len; j++) {
                    while (have && (size_t)$len(cur.rms) != 0) CARRY();
                    if (have) CARRY();
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
            while (have && (size_t)$len(cur.rms) != 0) CARRY();
            for (u32 j = 0; j < sum_ins; j++) { EMIT_NEW(ni); ni++; }
            for (u32 j = 0; j < sum_del; j++) {
                while (have && (size_t)$len(cur.rms) != 0) CARRY();
                if (have) DELMARK();
            }
        }
        while (have) CARRY();
    }
    #undef EMIT_NEW
    #undef CARRY
    #undef DELMARK

    weave nw = {};
    u8csMv(nw.text, u8bDataC(o_text));
    nw.toks[0] = (tok32c *)u32bDataHead(o_toks);
    nw.toks[1] = (tok32c *)u32bDataHead(o_toks) + u32bDataLen(o_toks);
    u8csMv(nw.ins, u8bDataC(o_ins));
    u8csMv(nw.rms, u8bDataC(o_rms));
    nw.commits[0] = (u64c *)u64bDataHead(out_cmts);
    nw.commits[1] = (u64c *)u64bDataHead(out_cmts) + u64bDataLen(out_cmts);
    call(WEAVESerialize, into, &nw);
    done;
}

ok64 WEAVENext(u8s into, weave const *w, u8csc new_blob, u8csc ext, u64 commit) {
    sane(into != NULL);
    if (w == NULL || WEAVEEmpty(w))
        return wnext_from_blob(into, new_blob, ext, commit);
    return wnext_diff(into, w, new_blob, ext, commit);
}

// ============================================================
//  WEAVEMerge: identity-keyed union of two parent weaves
// ============================================================

//  Decoded view of one parent for the merge.  Per token: cumulative end
//  offset (byte range), tag, has_in, inserter as a MERGED commit index,
//  the identity hash (RAPHash of <inserter-commit-id, per-commit ordinal>
//  — equal iff the same logical token), and removers as MERGED indices.
typedef struct {
    u32  n;
    u8cs text;
    u32 *end, *ins, *roff, *rlen, *rid;
    u8  *tag, *hasin;
    u64 *idh;
} wmdec;

//  Decode `X` into `d` (carves persist in the CALLER's frame — call this
//  DIRECTLY, never via call(), or BASS would be rewound underneath it).
//  `remap[local commit idx]` maps to the merged commit index.
static ok64 wmerge_decode(weave const *X, u32cs remap, wmdec *d) {
    sane(X && d);
    zerop(d);
    u32 nx = (u32)$len(X->toks);
    u32 nc = (u32)$len(X->commits);
    a_carve(u32, end,  nx + 1);
    a_carve(u32, ins,  nx + 1);
    a_carve(u32, roff, nx + 1);
    a_carve(u32, rlen, nx + 1);
    a_carve(u32, rid,  u8csLen(X->rms) / 4 + 1);
    a_carve(u8,  tag,  nx + 1);
    a_carve(u8,  hin,  nx + 1);
    a_carve(u64, idh,  nx + 1);
    a_carve(u32, cnt,  nc + 1);
    for (u32 i = 0; i < nc; i++) call(u32bFeed1, cnt, 0);
    u32 *cntp = (u32 *)u32bDataHead(cnt);
    u32 *rmap = (u32 *)remap[0];
    a_pad(u8, key, 12);
    weave c = *X; u32 off = 0; weavetok tk;
    while ((size_t)$len(c.toks) > 0) {
        call(WEAVEStep, &c, &off, &tk);
        u32 li = tk.inserter;
        u64 iid = X->commits[0][li];
        u32 ord = cntp[li]++;
        u8bReset(key);
        call(u8sFeed64, u8bIdle(key), &iid);
        call(u8sFeed32, u8bIdle(key), &ord);
        call(u64bFeed1, idh, RAPHash(u8bDataC(key)));
        call(u32bFeed1, end, off);
        call(u8bFeed1, tag, tk.tag);
        call(u8bFeed1, hin, (u8)tk.has_in);
        call(u32bFeed1, ins, rmap[li]);
        call(u32bFeed1, roff, (u32)u32bDataLen(rid));
        u32 rl = 0;
        $for(u32c, r, tk.rms) { call(u32bFeed1, rid, rmap[*r]); rl++; }
        call(u32bFeed1, rlen, rl);
    }
    d->n = nx;
    u8csMv(d->text, X->text);
    d->end  = (u32 *)u32bDataHead(end);
    d->ins  = (u32 *)u32bDataHead(ins);
    d->roff = (u32 *)u32bDataHead(roff);
    d->rlen = (u32 *)u32bDataHead(rlen);
    d->rid  = (u32 *)u32bDataHead(rid);
    d->tag  = (u8 *)u8bDataHead(tag);
    d->hasin = (u8 *)u8bDataHead(hin);
    d->idh  = (u64 *)u64bDataHead(idh);
    done;
}

//  Emit token `i` of `d`, unioning its removers with `other` (the matching
//  token's removers from the far side; empty for a one-sided token).
//  Add `v` to `un` if not already present (sorted/dedup not needed —
//  remover sets are tiny).
static ok64 wmerge_addrm(u32b un, u32 v) {
    sane(1);
    u32 *d = (u32 *)u32bDataHead(un);
    for (u32 m = 0; m < (u32)u32bDataLen(un); m++) if (d[m] == v) done;
    call(u32bFeed1, un, v);
    done;
}

static ok64 wmerge_emit(wnext_out *o, wmdec const *d, u32 i, u32cs other) {
    sane(o && d);
    u32 lo = i ? d->end[i - 1] : 0, hi = d->end[i];
    a_part(u8c, bytes, d->text, lo, hi - lo);
    a_pad(u32, un, 256);
    for (u32 k = 0; k < d->rlen[i]; k++) call(wmerge_addrm, un, d->rid[d->roff[i] + k]);
    for (u32 k = 0; k < (u32)$len(other); k++) call(wmerge_addrm, un, other[0][k]);
    u32cs uns = {(u32c *)u32bDataHead(un),
                 (u32c *)u32bDataHead(un) + u32bDataLen(un)};
    call(wnext_emit, o, bytes, d->tag[i], d->hasin[i], d->ins[i], uns);
    done;
}

//  Open-addressing membership set over identity hashes (linear probe).  Used
//  only to answer "is this token shared with the other parent?".  0 is a
//  valid hash, tracked separately; `mask` = cap-1, cap a power of two.
typedef struct { u64 *slot; u32 mask; b8 zero; } idset;

static void idset_add(idset *s, u64 h) {
    if (h == 0) { s->zero = YES; return; }
    u32 i = (u32)h & s->mask;
    while (s->slot[i] != 0) { if (s->slot[i] == h) return; i = (i + 1) & s->mask; }
    s->slot[i] = h;
}
static b8 idset_has(idset const *s, u64 h) {
    if (h == 0) return s->zero;
    u32 i = (u32)h & s->mask;
    while (s->slot[i] != 0) { if (s->slot[i] == h) return YES; i = (i + 1) & s->mask; }
    return NO;
}
static u32 idset_cap(u64 n) {   // smallest power of two > 2n, min 8
    u32 c = 8;
    while ((u64)c <= 2 * n + 1) c <<= 1;
    return c;
}

//  Merge two parents' weaves into one (DIS-003: shared tokens align by
//  identity, removers union, side-only tokens splice in via the EDL).
//  `merge_commit` is reserved for an evil-merge's own content (fold it with
//  a following WEAVENext); a pure merge references no new commit.
ok64 WEAVEMerge(u8s into, weave const *a, weave const *b, u64 merge_commit) {
    sane(into != NULL);
    (void)merge_commit;
    if (a == NULL || WEAVEEmpty(a)) { if (b) { weave nb = *b; return WEAVESerialize(into, &nb); } fail(WEAVEFAIL); }
    if (b == NULL || WEAVEEmpty(b)) { weave na = *a; return WEAVESerialize(into, &na); }

    //  Merged commit table = a.commits ++ (b.commits not already present);
    //  remap_a/remap_b map each parent's local index to the merged index.
    a_carve(u64, mc, (size_t)$len(a->commits) + (size_t)$len(b->commits) + 1);
    a_carve(u32, ra, (size_t)$len(a->commits) + 1);
    a_carve(u32, rb, (size_t)$len(b->commits) + 1);
    { u32 i = 0; $for(u64c, c, a->commits) { call(u64bFeed1, mc, *c); call(u32bFeed1, ra, i); i++; } }
    $for(u64c, c, b->commits) {
        u32 idx = (u32)u64bDataLen(mc), k = 0; b8 f = NO;
        u64 *mcd = (u64 *)u64bDataHead(mc);
        for (k = 0; k < (u32)u64bDataLen(mc); k++) if (mcd[k] == *c) { idx = k; f = YES; break; }
        if (!f) call(u64bFeed1, mc, *c);
        call(u32bFeed1, rb, idx);
    }

    wmdec da = {}, db = {};
    { ok64 r = wmerge_decode(a, u32bDataC(ra), &da); if (r != OK) return r; }
    { ok64 r = wmerge_decode(b, u32bDataC(rb), &db); if (r != OK) return r; }

    u64 olen = da.n, nlen = db.n;
    a_carve(u8,  o_text, u8csLen(a->text) + u8csLen(b->text) + 1);
    a_carve(u32, o_toks, (size_t)olen + (size_t)nlen + 1);
    a_carve(u8,  o_ins,  4 * ((size_t)olen + (size_t)nlen + 1));
    //  rms: a merged token's removers are the UNION of both sides, so the
    //  worst case is every remover byte from a and b, plus a count word per
    //  output token (4 bytes) when the union has >1 entry.
    a_carve(u8,  o_rms,  u8csLen(a->rms) + u8csLen(b->rms)
                         + 4 * ((size_t)olen + (size_t)nlen + 1));
    wnext_out o = {.text = &o_text, .toks = &o_toks, .ins = &o_ins, .rms = &o_rms};
    u32cs none = {NULL, NULL};

    //  Identity-keyed merge-JOIN, not an LCS diff (DIS-003 fix).  A token
    //  shared by both parents has identical (commit-id, ordinal) identity AND
    //  — because every descendant weave keeps a commit's whole token set in
    //  insertion order, and insertion-only edits never reorder — appears in
    //  the SAME relative order in both.  So a two-pointer scan pairs EVERY
    //  shared token (an LCS only pairs a subsequence, dropping matches whose
    //  context diverges -> the criss-cross duplication).  Membership sets say
    //  whether a head is shared; a-only then b-only runs splice between shared
    //  anchors (deterministic order); shared tokens emit once, removers union.
    u32 capA = idset_cap(olen), capB = idset_cap(nlen);
    a_carve(u64, sa, capA);
    a_carve(u64, sb, capB);
    u64 *sad = (u64 *)u64bIdleHead(sa);
    u64 *sbd = (u64 *)u64bIdleHead(sb);
    memset(sad, 0, (size_t)capA * sizeof(u64));
    memset(sbd, 0, (size_t)capB * sizeof(u64));
    idset setA = {sad, capA - 1, NO}, setB = {sbd, capB - 1, NO};
    for (u32 k = 0; k < (u32)olen; k++) idset_add(&setA, da.idh[k]);
    for (u32 k = 0; k < (u32)nlen; k++) idset_add(&setB, db.idh[k]);

    //  Concurrent (one-sided) tokens MUST be interleaved by a path-independent
    //  key, never by parent role: "a-only before b-only" makes the order of two
    //  sibling inserts depend on which parent is left/right, so re-merging two
    //  weaves that ordered the same pair oppositely desyncs the join and
    //  duplicates.  Order by commit-id (then intra-commit order, preserved) —
    //  a total order every merge along every path agrees on.  a-only and
    //  b-only commit sets are disjoint (a commit present in a parent brings ALL
    //  its tokens), so the keys never tie.
    u64 *mcd = (u64 *)u64bDataHead(mc);
    u32 ia = 0, ib = 0;
    while (ia < olen || ib < nlen) {
        b8 amatch = ia < olen && idset_has(&setB, da.idh[ia]);
        b8 bmatch = ib < nlen && idset_has(&setA, db.idh[ib]);
        if (ia < olen && ib < nlen && da.idh[ia] == db.idh[ib]) {
            u32cs orh = {db.rid + db.roff[ib], db.rid + db.roff[ib] + db.rlen[ib]};
            call(wmerge_emit, &o, &da, ia, orh);   // shared anchor: union removers
            ia++; ib++;
        } else if (ib >= nlen) {
            call(wmerge_emit, &o, &da, ia, none); ia++;
        } else if (ia >= olen) {
            call(wmerge_emit, &o, &db, ib, none); ib++;
        } else if (!amatch && !bmatch) {           // both concurrent: cid order
            if (mcd[da.ins[ia]] <= mcd[db.ins[ib]]) {
                call(wmerge_emit, &o, &da, ia, none); ia++;
            } else {
                call(wmerge_emit, &o, &db, ib, none); ib++;
            }
        } else if (!amatch) {                      // a-only precedes b's anchor
            call(wmerge_emit, &o, &da, ia, none); ia++;
        } else if (!bmatch) {                      // b-only precedes a's anchor
            call(wmerge_emit, &o, &db, ib, none); ib++;
        } else {
            //  Both heads are anchors but differ — a consistent commit-id
            //  order should preclude this; break by commit-id to progress
            //  (the DWEAVE fuzzer guards correctness).
            if (mcd[da.ins[ia]] <= mcd[db.ins[ib]]) {
                call(wmerge_emit, &o, &da, ia, none); ia++;
            } else {
                call(wmerge_emit, &o, &db, ib, none); ib++;
            }
        }
    }

    weave nw = {};
    u8csMv(nw.text, u8bDataC(o_text));
    nw.toks[0] = (tok32c *)u32bDataHead(o_toks);
    nw.toks[1] = (tok32c *)u32bDataHead(o_toks) + u32bDataLen(o_toks);
    u8csMv(nw.ins, u8bDataC(o_ins));
    u8csMv(nw.rms, u8bDataC(o_rms));
    nw.commits[0] = (u64c *)u64bDataHead(mc);
    nw.commits[1] = (u64c *)u64bDataHead(mc) + u64bDataLen(mc);
    call(WEAVESerialize, into, &nw);
    done;
}
