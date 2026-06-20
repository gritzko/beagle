//  WEAVE (DOG-003): columnar, HUNK-compatible file-history weave.
//  See WEAVE.h for the model.  This file: the codec (Parse/Serialize),
//  the active-commit scope bitmap, and the tip alive-bytes scan.  The
//  ins/rms cursor + Produce/Next/Merge land next.
//
#include "WEAVE.h"

#include "abc/DIFF.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/TLV.h"
#include "dog/BRAM.h"
#include "dog/NEIL.h"

// ============================================================
//  Shared identity / anchor hash (DIS-043)
// ============================================================

//  RAPHash(commit_id ++ ordinal), host-endian bytes (12 = u64 + u32).  Used
//  for both a token's identity hash and a stored anchor, so the RGA walk can
//  match an anchor directly against an idh.  Extracted from wmerge_decode.
u64 WEAVEIdHash(u64 commit_id, u32 ordinal) {
    a_pad(u8, key, 12);
    u8bReset(key);
    (void)u8sFeed64(u8bIdle(key), &commit_id);   // fixed 12 B: never short
    (void)u8sFeed32(u8bIdle(key), &ordinal);
    return RAPHash(u8bDataC(key));
}

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
        case WEAVE_TLV_ANC: u8csMv(w->anc, v); break;   // DIS-043 RGA anchors
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
               + (size_t)$len(w->commits) * sizeof(u64)
               + (size_t)$len(w->toks) * sizeof(u64)   // 'A': one u64 per token
               + 80;
    a_carve(u8, inner, cap);
    u8csc kbytes = {(u8c *)w->toks[0], (u8c *)w->toks[1]};
    u8csc cbytes = {(u8c *)w->commits[0], (u8c *)w->commits[1]};
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_TXT, w->text);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_TOK, kbytes);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_INS, w->ins);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_RMS, w->rms);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_CMT, cbytes);
    call(TLVu8sFeed, u8bIdle(inner), WEAVE_TLV_ANC, w->anc);   // DIS-043
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
    //  DIS-043: one u64 anchor per token, lockstep with toks.  WEAVEFAIL on a
    //  short 'A'; tolerate absent 'A' (legacy blob) — caller falls back to the
    //  weave-order predecessor's idh.
    out->anchor = WEAVE_ROOT_ANCHOR;
    out->has_anchor = NO;
    if (u8csLen(c->anc) >= 8) {
        u64 ah = 0;
        u8sDrain64(c->anc, &ah);
        out->anchor = ah;
        out->has_anchor = YES;
    } else if (u8csLen(c->anc) != 0) {
        fail(WEAVEFAIL);   // partial 'A': malformed
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

//  --- output column emit (text/toks/ins/rms/anc; cum = running end) ---
typedef struct { u8b *text; u32b *toks; u8b *ins; u8b *rms; u8b *anc; u32 cum; } wnext_out;

static ok64 wnext_emit(wnext_out *o, u8csc bytes, u8 tag,
                       b8 has_in, u32 in_idx, u32cs rms, u64 anchor) {
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
    call(u8sFeed64, u8bIdle(*o->anc), &anchor);   // DIS-043 RGA anchor
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
    //  DIS-043 anchors: token 0 -> ROOT, token k -> idh of token k-1.  Every
    //  token here belongs to `commit` (the spine), so its ordinal is its index.
    u32 ntok = (u32)u32bDataLen(toks);
    a_carve(u8, anc, (size_t)ntok * sizeof(u64) + 8);
    for (u32 k = 0; k < ntok; k++) {
        u64 ah = (k == 0) ? WEAVE_ROOT_ANCHOR : WEAVEIdHash(commit, k - 1);
        call(u8sFeed64, u8bIdle(anc), &ah);
    }
    weave nw = {};
    u8csMv(nw.text, new_blob);
    nw.toks[0] = (tok32c *)u32bDataHead(toks);
    nw.toks[1] = (tok32c *)u32bDataHead(toks) + u32bDataLen(toks);
    nw.commits[0] = (u64c *)u64bDataHead(cmts);
    nw.commits[1] = (u64c *)u64bDataHead(cmts) + u64bDataLen(cmts);
    u8csMv(nw.anc, u8bDataC(anc));
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
    a_carve(u8,  o_anc,  sizeof(u64) * ((size_t)$len(w->toks) + (size_t)nlen + 1));
    wnext_out o = {.text = &o_text, .toks = &o_toks, .ins = &o_ins,
                   .rms = &o_rms, .anc = &o_anc};

    u32 *nend = (u32 *)u32bDataHead(new_end);
    u8  *ntag = (u8 *)u8bDataHead(new_tag);

    //  DIS-043: per-commit ordinal counters (in the OUTPUT commit table) and
    //  the running prev_idh (idh of the last emitted token, ROOT to start).
    //  A new token anchors on prev_idh; a carried token keeps its STORED
    //  anchor (cur.anchor), or falls back to prev_idh on a legacy blob.  Every
    //  emitted token then advances prev_idh to ITS OWN idh.
    u64 *ocd = (u64 *)u64bDataHead(out_cmts);
    a_carve(u32, ord_cnt, (size_t)u64bDataLen(out_cmts) + 1);
    for (u32 i = 0; i < (u32)u64bDataLen(out_cmts); i++) call(u32bFeed1, ord_cnt, 0);
    u32 *ordp = (u32 *)u32bDataHead(ord_cnt);
    u64 prev_idh = WEAVE_ROOT_ANCHOR;

    weave oc = *w; u32 ooff = 0; weavetok cur; b8 have = NO;
    call(old_step, &oc, &ooff, &cur, &have);

    #define ADV_IDH(INS) do { u32 _ins = (INS); u32 _ord = ordp[_ins]++;        \
        prev_idh = WEAVEIdHash(ocd[_ins], _ord); } while (0)
    #define EMIT_NEW(J) do { u32 _lo = (J) ? nend[(J) - 1] : 0, _hi = nend[(J)]; \
        a_part(u8c, _nb, new_blob, _lo, _hi - _lo); u32cs _none = {NULL, NULL};  \
        call(wnext_emit, &o, _nb, ntag[(J)], YES, new_idx, _none, prev_idh);     \
        ADV_IDH(new_idx); } while (0)
    #define CARRY() do { u64 _anc = cur.has_anchor ? cur.anchor : prev_idh;     \
        call(wnext_emit, &o, cur.text, cur.tag, cur.has_in, cur.inserter,       \
             cur.rms, _anc); ADV_IDH(cur.inserter);                             \
        call(old_step, &oc, &ooff, &cur, &have); } while (0)
    #define DELMARK() do { u32 _one = new_idx; u32cs _rs = {&_one, &_one + 1};  \
        u64 _anc = cur.has_anchor ? cur.anchor : prev_idh;                      \
        call(wnext_emit, &o, cur.text, cur.tag, cur.has_in, cur.inserter, _rs,  \
             _anc); ADV_IDH(cur.inserter);                                      \
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
    #undef ADV_IDH

    weave nw = {};
    u8csMv(nw.text, u8bDataC(o_text));
    nw.toks[0] = (tok32c *)u32bDataHead(o_toks);
    nw.toks[1] = (tok32c *)u32bDataHead(o_toks) + u32bDataLen(o_toks);
    u8csMv(nw.ins, u8bDataC(o_ins));
    u8csMv(nw.rms, u8bDataC(o_rms));
    nw.commits[0] = (u64c *)u64bDataHead(out_cmts);
    nw.commits[1] = (u64c *)u64bDataHead(out_cmts) + u64bDataLen(out_cmts);
    u8csMv(nw.anc, u8bDataC(o_anc));
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
    u64 *idh;     // identity hash RAPHash(commit-id ++ ordinal)
    u64 *anc;     // RGA left-anchor hash (in idh space)
    u32 *ord;     // tie-break: per-commit ordinal (DIS-044: index is in `ins`)
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
    a_carve(u64, anc,  nx + 1);
    a_carve(u32, ord,  nx + 1);
    a_carve(u32, cnt,  nc + 1);
    for (u32 i = 0; i < nc; i++) call(u32bFeed1, cnt, 0);
    u32 *cntp = (u32 *)u32bDataHead(cnt);
    u32 *rmap = (u32 *)remap[0];
    //  DIS-043: a legacy blob without 'A' falls back to the weave-order
    //  predecessor's idh (prev_idh), seeded ROOT.
    u64 prev_idh = WEAVE_ROOT_ANCHOR;
    weave c = *X; u32 off = 0; weavetok tk;
    while ((size_t)$len(c.toks) > 0) {
        call(WEAVEStep, &c, &off, &tk);
        u32 li = tk.inserter;
        u64 iid = X->commits[0][li];
        u32 o = cntp[li]++;
        u64 this_idh = WEAVEIdHash(iid, o);
        call(u64bFeed1, idh, this_idh);
        call(u64bFeed1, anc, tk.has_anchor ? tk.anchor : prev_idh);
        call(u32bFeed1, ord, o);
        call(u32bFeed1, end, off);
        call(u8bFeed1, tag, tk.tag);
        call(u8bFeed1, hin, (u8)tk.has_in);
        call(u32bFeed1, ins, rmap[li]);
        call(u32bFeed1, roff, (u32)u32bDataLen(rid));
        u32 rl = 0;
        $for(u32c, r, tk.rms) { call(u32bFeed1, rid, rmap[*r]); rl++; }
        call(u32bFeed1, rlen, rl);
        prev_idh = this_idh;
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
    d->anc  = (u64 *)u64bDataHead(anc);
    d->ord  = (u32 *)u32bDataHead(ord);
    done;
}

//  --- DIS-043 RGA merge + DIS-044 causal tie-break ------------------------
//  rgakey: the RGA sort record per union token.  Sorted by anchor (ASC, to
//  make siblings contiguous) then the tie-break — DIS-044: the merged commit
//  INDEX (a path-independent CAUSAL topo rank, ancestor < descendant), not the
//  raw 60-bit hashlet (arbitrary, so a base could outrank its edits and strand
//  a replace-edit's token).  Among purely-concurrent commits the topo merge
//  orders by hashlet, so cidx DESC == the old cid DESC sibling order.  ordinal
//  ASC within a commit.  cidx is a real index, so no hash collision to dodge.
typedef struct { u64 anchor; u32 cidx, ord, ui; } rgakey;

fun b8 rgakeyZ(rgakey const *a, rgakey const *b) {
    if (a->anchor != b->anchor) return a->anchor < b->anchor;
    if (a->cidx != b->cidx) return a->cidx > b->cidx;   // commit index DESC
    return a->ord < b->ord;                              // ord ASC
}
#define X(M, n) M##rgakey##n
#include "abc/Bx.h"
#undef X

static int rga_cmp(void const *pa, void const *pb) {
    rgakey const *x = pa, *y = pb;
    if (x->anchor != y->anchor) return x->anchor < y->anchor ? -1 : 1;
    if (x->cidx != y->cidx) return x->cidx > y->cidx ? -1 : 1;   // index DESC
    if (x->ord != y->ord) return x->ord < y->ord ? -1 : 1;       // ord ASC
    return 0;
}

//  Open-addressing u64 -> u32 map (linear probe).  Key 0 (ROOT/idh-zero) is
//  tracked separately.  `key[i]==0` means empty.  Carve must be zerob'd by
//  the caller (BASS reuses memory across rewound carves).
typedef struct { u64 *key; u32 *val; u32 mask; b8 has0; u32 v0; } u64map;

static void u64map_put(u64map *m, u64 k, u32 v) {
    if (k == 0) { m->has0 = YES; m->v0 = v; return; }
    u32 i = (u32)k & m->mask;
    while (m->key[i] != 0) { if (m->key[i] == k) { m->val[i] = v; return; } i = (i + 1) & m->mask; }
    m->key[i] = k; m->val[i] = v;
}
static b8 u64map_get(u64map const *m, u64 k, u32 *out) {
    if (k == 0) { if (m->has0) *out = m->v0; return m->has0; }
    u32 i = (u32)k & m->mask;
    while (m->key[i] != 0) { if (m->key[i] == k) { *out = m->val[i]; return YES; } i = (i + 1) & m->mask; }
    return NO;
}
static u32 u64map_cap(u64 n) {   // smallest power of two > 2n, min 8
    u32 c = 8;
    while ((u64)c <= 2 * n + 1) c <<= 1;
    return c;
}

//  Emit one union token (its bytes from `utxt`, its remover slice from `urid`)
//  with its STORED anchor.  Removers are already a deduped union.
static ok64 wmerge_emit1(wnext_out *o, u8b utxt, u32b urid, u8 tag, b8 hasin,
                         u32 ins, u32 tlo, u32 thi, u32 roff, u32 rlen, u64 anchor) {
    sane(o);
    a_part(u8c, bytes, u8bDataC(utxt), tlo, thi - tlo);
    u32c *rb = (u32c *)u32bDataHead(urid) + roff;
    u32cs rms = {rb, rb + rlen};
    call(wnext_emit, o, bytes, tag, hasin, ins, rms, anchor);
    done;
}

//  DIS-044: build the merged commit table in a DETERMINISTIC CAUSAL order so a
//  token's commit INDEX is a real topo rank — causal (ancestor < descendant)
//  AND path-independent (same whichever parent is `a`).  Each parent's
//  commits[] is already ancestors-first, so this is a stable two-way merge of
//  two causally-ordered lists: a head is READY when every commit before it in
//  EITHER list is already emitted (i.e. it is not buried in the other list's
//  unconsumed tail); among ready heads pick the SMALLER hashlet (a symmetric
//  tie-break for concurrent commits).  Shared commits advance both cursors.
//  Fills `mc` (merged ids), `ra`/`rb` (parent-local idx -> merged idx).  Counts
//  are per-file (small): the O(n^2) tail scan matches the rest of this file.
static ok64 wmerge_commits(u64b mc, u32b ra, u32b rb,
                           u64csc ca, u64csc cb) {
    sane(1);
    u32 na = (u32)$len(ca), nb = (u32)$len(cb);
    u32 pa = 0, pb = 0;
    while (pa < na || pb < nb) {
        b8 hasa = (pa < na), hasb = (pb < nb);
        u64 ha = hasa ? ca[0][pa] : 0, hb = hasb ? cb[0][pb] : 0;
        //  A head is ready iff it is NOT still pending later in the other list.
        b8 ra_rdy = NO, rb_rdy = NO;
        if (hasa) {
            ra_rdy = YES;
            for (u32 k = pb; k < nb; k++) if (cb[0][k] == ha) { ra_rdy = (k == pb); break; }
        }
        if (hasb) {
            rb_rdy = YES;
            for (u32 k = pa; k < na; k++) if (ca[0][k] == hb) { rb_rdy = (k == pa); break; }
        }
        //  Choose the ready head with the smaller hashlet; if (malformed)
        //  neither is ready, force progress on the smaller head to avoid a
        //  deadlock (a real ordering conflict between the two lists).
        b8 pick_a;
        if (ra_rdy && rb_rdy) pick_a = (ha <= hb);
        else if (ra_rdy)      pick_a = YES;
        else if (rb_rdy)      pick_a = NO;
        else                  pick_a = hasa && (!hasb || ha <= hb);
        u64 chosen = pick_a ? ha : hb;
        u32 idx = (u32)u64bDataLen(mc);
        call(u64bFeed1, mc, chosen);
        if (pa < na && ca[0][pa] == chosen) { call(u32bFeed1, ra, idx); pa++; }
        if (pb < nb && cb[0][pb] == chosen) { call(u32bFeed1, rb, idx); pb++; }
    }
    done;
}

//  Merge two parents' weaves into one (DIS-003 JOIN + DIS-043 RGA order +
//  DIS-044 causal tie-break).  The union of identities is laid out by a
//  path-independent RGA total order (each token after its immutable anchor;
//  concurrent siblings by merged commit INDEX DESC, ord ASC — the index is a
//  causal topo rank from wmerge_commits, so a base never outranks its edits),
//  so every merge path agrees and the JOIN matches each shared identity exactly
//  once.  `merge_commit` is reserved for an evil-merge's own content (fold it
//  with a following WEAVENext); a pure merge adds no commit.
ok64 WEAVEMerge(u8s into, weave const *a, weave const *b, u64 merge_commit) {
    sane(into != NULL);
    (void)merge_commit;
    if (a == NULL || WEAVEEmpty(a)) { if (b) { weave nb = *b; return WEAVESerialize(into, &nb); } fail(WEAVEFAIL); }
    if (b == NULL || WEAVEEmpty(b)) { weave na = *a; return WEAVESerialize(into, &na); }

    //  Merged commit table in DETERMINISTIC CAUSAL (topo) order (DIS-044), so a
    //  token's commit INDEX is a path-independent causal rank; remap_a/remap_b
    //  map each parent's local index to the merged index.
    a_carve(u64, mc, (size_t)$len(a->commits) + (size_t)$len(b->commits) + 1);
    a_carve(u32, ra, (size_t)$len(a->commits) + 1);
    a_carve(u32, rb, (size_t)$len(b->commits) + 1);
    call(wmerge_commits, mc, ra, rb, a->commits, b->commits);

    wmdec da = {}, db = {};
    { ok64 r = wmerge_decode(a, u32bDataC(ra), &da); if (r != OK) return r; }
    { ok64 r = wmerge_decode(b, u32bDataC(rb), &db); if (r != OK) return r; }

    u64 nab = (u64)da.n + db.n;                // upper bound on distinct tokens

    //  (1) Build the union token set keyed by idh.  First occurrence sets the
    //  immutable fields + copies the bytes; every occurrence unions removers.
    //  Columnar parallel arrays (ABC §1): one slot per distinct identity.
    a_carve(u64, u_idh,   nab + 1);
    a_carve(u64, u_anc,   nab + 1);
    a_carve(u32, u_ord,   nab + 1);
    a_carve(u32, u_tlo,   nab + 1);
    a_carve(u32, u_thi,   nab + 1);
    a_carve(u8,  u_tag,   nab + 1);
    a_carve(u8,  u_hin,   nab + 1);
    a_carve(u32, u_ins,   nab + 1);
    a_carve(u32, u_roff,  nab + 1);
    a_carve(u32, u_rlen,  nab + 1);
    a_carve(u8,  utxt, u8csLen(a->text) + u8csLen(b->text) + 1);
    a_carve(u32, urid, u8csLen(a->rms) / 4 + u8csLen(b->rms) / 4 + 1);
    u32 ucap = u64map_cap(nab);
    a_carve(u64, ukey, ucap);
    a_carve(u32, uval, ucap);
    zerob(ukey);
    u64map umap = {(u64 *)u64bDataHead(ukey), (u32 *)u32bDataHead(uval), ucap - 1, NO, 0};
    u64 *uidh = (u64 *)u64bDataHead(u_idh), *uanc = (u64 *)u64bDataHead(u_anc);
    u32 *uord = (u32 *)u32bDataHead(u_ord), *utlo = (u32 *)u32bDataHead(u_tlo);
    u32 *uthi = (u32 *)u32bDataHead(u_thi), *uins = (u32 *)u32bDataHead(u_ins);
    u32 *uroff = (u32 *)u32bDataHead(u_roff), *urlen = (u32 *)u32bDataHead(u_rlen);
    u8  *utag = (u8 *)u8bDataHead(u_tag), *uhin = (u8 *)u8bDataHead(u_hin);
    u32 nu = 0;

    //  Pass 1: distinct identities + immutable fields (no removers yet).
    for (u32 side = 0; side < 2; side++) {
        wmdec const *d = side ? &db : &da;
        for (u32 i = 0; i < d->n; i++) {
            u32 ui = 0;
            if (u64map_get(&umap, d->idh[i], &ui)) continue;
            u32 lo = i ? d->end[i - 1] : 0, hi = d->end[i];
            ui = nu++;
            a_part(u8c, bytes, d->text, lo, hi - lo);
            u32 tlo = (u32)u8bDataLen(utxt);
            call(u8bFeed, utxt, bytes);
            uidh[ui] = d->idh[i]; uanc[ui] = d->anc[i];
            uord[ui] = d->ord[i];
            utlo[ui] = tlo; uthi[ui] = (u32)u8bDataLen(utxt);
            utag[ui] = d->tag[i]; uhin[ui] = d->hasin[i]; uins[ui] = d->ins[i];
            u64map_put(&umap, d->idh[i], ui);
        }
    }
    //  Pass 2: per entry (in creation order) gather the UNION of removers from
    //  every matching token on both sides, appending one contiguous run to
    //  urid.  Processing entries in order keeps each run at urid's tail.
    for (u32 ui = 0; ui < nu; ui++) {
        uroff[ui] = (u32)u32bDataLen(urid); urlen[ui] = 0;
        for (u32 side = 0; side < 2; side++) {
            wmdec const *d = side ? &db : &da;
            for (u32 i = 0; i < d->n; i++) {
                if (d->idh[i] != uidh[ui]) continue;
                for (u32 k = 0; k < d->rlen[i]; k++) {
                    u32 rv = d->rid[d->roff[i] + k];
                    u32 *ridd = (u32 *)u32bDataHead(urid);
                    b8 dup = NO;
                    for (u32 m = 0; m < urlen[ui]; m++) if (ridd[uroff[ui] + m] == rv) { dup = YES; break; }
                    if (dup) continue;
                    call(u32bFeed1, urid, rv); urlen[ui]++;
                }
            }
        }
    }

    //  (2) RGA-linearise: sort by (anchor, commit-index DESC, ord ASC) so
    //  siblings are contiguous (DIS-044: the merged INDEX `uins` is the causal,
    //  path-independent tie-break — not the raw `ucid` hashlet), then map each
    //  anchor hash to its child group's [start,cnt).
    a_carve(rgakey, sk, (size_t)nu + 1);
    for (u32 i = 0; i < nu; i++) {
        rgakey kk = {uanc[i], uins[i], uord[i], i};
        call(rgakeybFeed1, sk, kk);
    }
    rgakey *skd = (rgakey *)rgakeybDataHead(sk);
    rgakey *sks[2] = {skd, skd + nu};
    $sort(sks, rga_cmp);
    //  Map anchor-hash -> first sorted index of its child group; count derives
    //  from the contiguous run sharing that anchor.
    u32 gcap = u64map_cap(nu);
    a_carve(u64, gkey, gcap);
    a_carve(u32, gval, gcap);
    zerob(gkey);
    u64map gmap = {(u64 *)u64bDataHead(gkey), (u32 *)u32bDataHead(gval), gcap - 1, NO, 0};
    for (u32 i = 0; i < nu; i++) {
        u64 an = skd[i].anchor;
        u32 prev = 0;
        if (!u64map_get(&gmap, an, &prev)) u64map_put(&gmap, an, i);
    }

    //  Output columns (RGA depth-first order from ROOT).
    a_carve(u8,  o_text, u8csLen(a->text) + u8csLen(b->text) + 1);
    a_carve(u32, o_toks, (size_t)nu + 1);
    a_carve(u8,  o_ins,  4 * ((size_t)nu + 1));
    a_carve(u8,  o_rms,  u8csLen(a->rms) + u8csLen(b->rms)
                         + 4 * ((size_t)nu + 1));
    a_carve(u8,  o_anc,  sizeof(u64) * ((size_t)nu + 1));
    wnext_out o = {.text = &o_text, .toks = &o_toks, .ins = &o_ins,
                   .rms = &o_rms, .anc = &o_anc};

    //  (3) Emit depth-first from ROOT via an EXPLICIT stack (slice + a top
    //  index; LIFO, so a node's children pop after the node).  Push a group's
    //  siblings in REVERSE sorted order so popping yields (cid DESC, ord ASC).
    //  Capacity nu+1 — every union token is pushed exactly once (each has one
    //  anchor and appears in exactly one child group).
    //  Stack capacity 2*nu+1: an idh hash collision (anchor accidentally ==
    //  another token's idh) could push a node more than once; the `seen` guard
    //  then emits each token at most ONCE (a collision mis-orders, never
    //  duplicates — the ticket's invariant), so the extra pushes are bounded.
    a_carve(u32, stack, 2 * (size_t)nu + 1);
    a_carve(u8,  seen,  (size_t)nu + 1);
    for (u32 i = 0; i < nu; i++) call(u8bFeed1, seen, 0);
    u8 *seenp = (u8 *)u8bDataHead(seen);
    u32 *stk = (u32 *)u32bIdleHead(stack);
    u32 top = 0;
    #define PUSH_CHILDREN(ANCHOR) do {                                  \
        u32 _gs = 0;                                                    \
        if (u64map_get(&gmap, (ANCHOR), &_gs)) {                        \
            u32 _e = _gs;                                              \
            while (_e < nu && skd[_e].anchor == (ANCHOR)) _e++;         \
            for (u32 _j = _e; _j-- > _gs;)                              \
                if (top < 2 * nu) stk[top++] = skd[_j].ui;              \
        } } while (0)
    PUSH_CHILDREN(WEAVE_ROOT_ANCHOR);
    u32 emitted = 0;
    while (top > 0) {
        u32 ui = stk[--top];                       // pop
        if (seenp[ui]) continue;                   // collision: emit once
        seenp[ui] = 1;
        call(wmerge_emit1, &o, utxt, urid, utag[ui], uhin[ui], uins[ui],
             utlo[ui], uthi[ui], uroff[ui], urlen[ui], uanc[ui]);
        emitted++;
        PUSH_CHILDREN(uidh[ui]);
    }
    #undef PUSH_CHILDREN
    //  Every union token must reach the output (no orphan anchor).  An anchor
    //  pointing outside the union would strand a subtree; treat as malformed.
    if (emitted != nu) fail(WEAVEFAIL);

    weave nw = {};
    u8csMv(nw.text, u8bDataC(o_text));
    nw.toks[0] = (tok32c *)u32bDataHead(o_toks);
    nw.toks[1] = (tok32c *)u32bDataHead(o_toks) + u32bDataLen(o_toks);
    u8csMv(nw.ins, u8bDataC(o_ins));
    u8csMv(nw.rms, u8bDataC(o_rms));
    nw.commits[0] = (u64c *)u64bDataHead(mc);
    nw.commits[1] = (u64c *)u64bDataHead(mc) + u64bDataLen(mc);
    u8csMv(nw.anc, u8bDataC(o_anc));
    call(WEAVESerialize, into, &nw);
    done;
}

// ============================================================
//  Emit (DOG-004): port graf's WEAVEEmit* onto the columnar weave.
//  Classification drives off the scope BITMAP (WEAVEScope/WEAVEStep)
//  instead of graf's WEAVEsetfn callbacks; output stays HUNK records.
// ============================================================

//  16 MiB weave-text cap (DIFF-007): past this the token re-tokenise and
//  the side-overlay blow up, so fall back to a coarse blob-level emit and
//  REPORT a `capped` status row — never a silently-empty result.
#define WEAVE_TEXT_CAP   (16UL << 20)
//  Whole-file (Full) hunk-body cap: keep individual hunks bounded so the
//  renderer never holds an unbounded body (graf carried the same knob).
#define WEAVE_FULL_HUNK_MAX (1UL << 20)
#define WEAVE_CTX_LINES  3

//  Flat per-token decode of a whole weave, materialised once by a
//  WEAVEStep walk so the emit logic indexes tokens like graf's `wdp`
//  (random access by i).  Carves persist in the CALLER's frame — call
//  weave_emit_decode DIRECTLY, never via call().
typedef struct {
    u32  n;
    u8cs text;     // concatenated token bytes, weave order (== w->text)
    u32 *end;      // cumulative end offset per token
    u8  *tag;      // stored syntax tag per token
    u32 *ins;      // inserter commit index per token
    u32 *roff;     // remover-pool offset per token
    u32 *rlen;     // remover count per token
    u32 *rid;      // remover commit-index pool
} wedec;

static ok64 weave_emit_decode(weave const *w, wedec *d) {
    sane(w && d);
    zerop(d);
    u32 nx = (u32)$len(w->toks);
    a_carve(u32, end,  nx + 1);
    a_carve(u8,  tag,  nx + 1);
    a_carve(u32, ins,  nx + 1);
    a_carve(u32, roff, nx + 1);
    a_carve(u32, rlen, nx + 1);
    a_carve(u32, rid,  u8csLen(w->rms) / 4 + 1);
    weave c = *w; u32 off = 0; weavetok tk;
    while ((size_t)$len(c.toks) > 0) {
        call(WEAVEStep, &c, &off, &tk);
        call(u32bFeed1, end, off);
        call(u8bFeed1, tag, tk.tag);
        call(u32bFeed1, ins, tk.inserter);
        call(u32bFeed1, roff, (u32)u32bDataLen(rid));
        u32 rl = 0;
        $for(u32c, r, tk.rms) { call(u32bFeed1, rid, *r); rl++; }
        call(u32bFeed1, rlen, rl);
    }
    d->n = nx;
    u8csMv(d->text, w->text);
    d->end  = (u32 *)u32bDataHead(end);
    d->tag  = (u8 *)u8bDataHead(tag);
    d->ins  = (u32 *)u32bDataHead(ins);
    d->roff = (u32 *)u32bDataHead(roff);
    d->rlen = (u32 *)u32bDataHead(rlen);
    d->rid  = (u32 *)u32bDataHead(rid);
    done;
}

fun u32 we_lo(wedec const *d, u32 i) { return i ? d->end[i - 1] : 0; }
fun u32 we_hi(wedec const *d, u32 i) { return d->end[i]; }

//  A token is alive in `scope` iff its inserter bit is set AND no remover
//  bit is set.  WEAVEScope always sets bit 0 (the spine), so a spine
//  token (inserter == WEAVE_SPINE) passes unless a reachable remover kills
//  it — exactly graf's weave_scope_alive predicate, bitmap-driven.
static b8 weave_scope_alive(wedec const *d, u32 i, weavescope scope) {
    if (!u1At(scope, d->ins[i])) return NO;
    for (u32 z = 0; z < d->rlen[i]; z++)
        if (u1At(scope, d->rid[d->roff[i] + z])) return NO;
    return YES;
}

//  Diff classify: 'I' inserted (alive in `to`, not `from`), 'D' deleted
//  (alive in `from`, not `to`), ' ' context (both), 0 invisible (neither).
static u8 weave_diff_classify(wedec const *d, u32 i,
                              weavescope from, weavescope to) {
    b8 af = weave_scope_alive(d, i, from);
    b8 at = weave_scope_alive(d, i, to);
    if (at && !af) return 'I';
    if (af && !at) return 'D';
    if (af && at)  return ' ';
    return 0;
}

//  Overlay real syntax tags onto a diff hunk's side-only tok stream.
//  `text` is the assembled hunk bytes; `sides` carries one tok32 per
//  source segment (its diff SIDE kept, tag ignored).  The weave stores
//  the per-token stored tag, but a hunk re-slices arbitrary token runs,
//  so — like graf — we re-tokenise `text` for syntax and write into
//  `out` (reset first) the union segmentation: each emitted tok32 takes
//  the syntax tag of its covering syntax token and the diff side of its
//  covering side segment.  Best-effort; on a hiccup the neutral 'S' tag
//  stands so the body always renders.
static ok64 weave_overlay_syntax(u32b out, u8csc text, u8csc ext,
                                 tok32cs sides) {
    sane(out != NULL);
    u32bReset(out);
    u32 ns = (u32)$len(sides);
    if (ns == 0 || $empty(text)) done;

    a_carve(u32, syn, (size_t)($len(text) + 16));
    u32 nt = 0;
    if (!$empty(ext) && TOKKnownExt(ext) &&
        HUNKu32bTokenize(syn, text, ext) == OK)
        nt = (u32)u32bDataLen(syn);
    tok32c *st = (tok32c *)u32bDataHead(syn);
    tok32c *sd = sides[0];

    u32 tlen = (u32)$len(text);
    u32 pos = 0, i = 0, j = 0;
    while (pos < tlen) {
        u8  side = (i < ns) ? tok32Side(sd[i]) : TOK_SIDE_EQ;
        u8  tag  = (j < nt) ? tok32Tag(st[j])  : 'S';
        u32 a = (i < ns) ? tok32Offset(sd[i]) : tlen;
        u32 b = (j < nt) ? tok32Offset(st[j]) : tlen;
        u32 nb = a < b ? a : b;
        if (nb <= pos) break;   // defensive: strictly-increasing offsets
        call(u32bFeed1, out, tok32PackSide(tag, side, nb));
        if (a == nb && i < ns) i++;
        if (b == nb && j < nt) j++;
        pos = nb;
    }
    done;
}

//  Compose a hunk URI `<scheme><name>?<navver>#L<lineno>` into `uri`
//  (reset first), preserving DIFF-003/004 behaviour: a non-empty scheme
//  is prepended; `navver` rides as a query; the start line is 1-based.
static ok64 weave_emit_uri(u8b uri, u8cs scheme, u8cs name, u8cs navver,
                           u32 lineno) {
    sane(uri != NULL);
    u8bReset(uri);
    if (!u8csEmpty(scheme)) call(u8bFeed, uri, scheme);
    call(u8bFeed, uri, name);
    if (!u8csEmpty(navver)) {
        call(u8bFeed1, uri, '?');
        call(u8bFeed, uri, navver);
    }
    u8csc empty = {NULL, NULL};
    call(HUNKu8sMakeURI, u8bIdle(uri), empty, empty, lineno + 1);
    done;
}

//  Emit one hunk (uri/text/sides) through `cb`: overlay syntax onto the
//  side-only stream, then hand a borrowed `hunk` to the callback.
static ok64 weave_emit_flush(HUNKcb cb, void *ctx, u8b uri,
                             u8b text, u32b sides, u32b combined, u8cs ext) {
    sane(cb != NULL);
    a_dup(u8c, htext, u8bDataC(text));
    tok32cs sd = {(tok32c *)u32bDataHead(sides),
                  (tok32c *)u32bDataHead(sides) + u32bDataLen(sides)};
    call(weave_overlay_syntax, combined, htext, ext, sd);
    hunk hk = {};
    hk.uri[0]  = u8bDataHead(uri);
    hk.uri[1]  = u8bDataHead(uri) + u8bDataLen(uri);
    hk.text[0] = u8bDataHead(text);
    hk.text[1] = u8bDataHead(text) + u8bDataLen(text);
    hk.toks[0] = (tok32c *)u32bDataHead(combined);
    hk.toks[1] = (tok32c *)u32bDataHead(combined) + u32bDataLen(combined);
    call(cb, &hk, ctx);
    done;
}

//  Coarse large-file fallback (DIFF-007): a weave whose text exceeds the
//  16 MiB cap can't carry valid tok32 offsets (24-bit end offsets wrap
//  past 16 MiB), so per-token decode/classify/overlay is impossible —
//  emit ONE whole-file hunk of the raw `text` column (weave order, no
//  per-token diff sides) plus a `capped` status row, never a silently
//  empty result.  `to` is unused: a coarse blob-level view has no scope.
static ok64 weave_emit_capped(weave const *w, u8cs scheme, u8cs name,
                              u8cs navver, HUNKcb cb, void *ctx) {
    sane(w && cb != NULL);
    a_carve(u8, uri, u8csLen(name) + u8csLen(navver) + 64);
    call(weave_emit_uri, uri, scheme, name, navver, 0);
    {
        hunk hk = {};
        hk.uri[0]  = u8bDataHead(uri);
        hk.uri[1]  = u8bDataHead(uri) + u8bDataLen(uri);
        hk.text[0] = (u8 *)w->text[0];
        hk.text[1] = (u8 *)w->text[1];
        call(cb, &hk, ctx);
    }
    //  `capped` status row: a hunk with verb=hunk + a status URI, empty
    //  body, so the renderer prints one machine-parseable report line.
    a_carve(u8, srow, u8csLen(name) + 16);
    a_cstr(cap, "capped:");
    call(u8bFeed, srow, cap);
    call(u8bFeed, srow, name);
    {
        hunk hk = {};
        hk.verb = HUNK_VERB_HUNK;
        hk.uri[0] = u8bDataHead(srow);
        hk.uri[1] = u8bDataHead(srow) + u8bDataLen(srow);
        call(cb, &hk, ctx);
    }
    done;
}

//  Windowed diff: emit only changed-line windows (WEAVE_CTX_LINES of
//  context) as `diff:<name>?<navver>#L<line>` hunks.  Byte-parity with
//  graf's WEAVEEmitDiff where uncapped.
ok64 WEAVEEmitDiff(weave const *w, u8cs name, u8cs navver,
                   weavescope from, weavescope to, HUNKcb cb, void *ctx) {
    sane(w && cb != NULL);
    if (WEAVEEmpty(w)) done;
    if (u8csLen(w->text) > WEAVE_TEXT_CAP) {
        a_cstr(dscheme, "diff:");
        return weave_emit_capped(w, dscheme, name, navver, cb, ctx);
    }

    wedec d = {};
    { ok64 r = weave_emit_decode(w, &d); if (r != OK) return r; }
    u32 ntok = d.n;
    if (ntok == 0) done;
    u8c *text = d.text[0];

    //  Mark changed lines: a line carrying an 'I'/'D' token (and its
    //  immediate predecessor line) is changed.  total_lines_est bounds it.
    u32 total_lines_est = 1;
    { u32 tlen = (u32)u8csLen(d.text);
      for (u32 b = 0; b < tlen; b++) if (text[b] == '\n') total_lines_est++; }
    a_carve(u8, changed, total_lines_est + 4);
    u8bReset(changed);
    for (u32 z = 0; z < total_lines_est; z++) call(u8bFeed1, changed, 0);
    u8 *cmark = (u8 *)u8bDataHead(changed);
    u32 cur_line = 0;
    for (u32 i = 0; i < ntok; i++) {
        u8 tag = weave_diff_classify(&d, i, from, to);
        if (tag == 0) continue;
        u32 lo = we_lo(&d, i), hi = we_hi(&d, i);
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

    //  Coalesce changed lines into [lo,hi] windows with context.
    a_carve(u32, windows, (total_lines + 4) * 2);
    u32 *wbuf = (u32 *)u32bIdleHead(windows);
    u32 nwin = 0;
    { u32 i = 0;
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
        wbuf[nwin * 2] = lo; wbuf[nwin * 2 + 1] = hi; nwin++;
      } }
    if (nwin == 0) done;

    a_carve(u8,  outtext,  u8csLen(d.text) + 1);
    a_carve(u32, outtoks,  (size_t)ntok + 1);
    a_carve(u32, combined, 2 * (size_t)ntok + (size_t)u8csLen(d.text) + 16);
    a_carve(u8,  outuri,   u8csLen(name) + u8csLen(navver) + 64);
    u8cs ext = {};
    PATHu8sExt(ext, name);
    a_cstr(dscheme, "diff:");

    u32 wi = 0, win_lo = wbuf[0], win_hi = wbuf[1];
    cur_line = 0;
    b8 hunk_open = NO;

    #define FLUSH() do { if (hunk_open) {                                 \
        call(weave_emit_uri, outuri, dscheme, name, navver, win_lo);      \
        call(weave_emit_flush, cb, ctx, outuri, outtext, outtoks,         \
             combined, ext);                                              \
        u8bReset(outtext); u32bReset(outtoks); hunk_open = NO; } } while (0)

    for (u32 i = 0; i < ntok; i++) {
        u8 tag = weave_diff_classify(&d, i, from, to);
        if (tag == 0) continue;
        u32 lo = we_lo(&d, i), hi = we_hi(&d, i);
        u32 nl = 0;
        for (u32 b = lo; b < hi; b++) if (text[b] == '\n') nl++;
        while (wi < nwin && cur_line > win_hi) {
            FLUSH();
            wi++;
            if (wi < nwin) { win_lo = wbuf[wi * 2]; win_hi = wbuf[wi * 2 + 1]; }
        }
        if (wi >= nwin) break;
        if (cur_line >= win_lo && cur_line <= win_hi) {
            a_part(u8c, tb, d.text, lo, hi - lo);
            call(u8bFeed, outtext, tb);
            u8 side = (tag == 'I') ? TOK_SIDE_IN
                    : (tag == 'D') ? TOK_SIDE_RM : TOK_SIDE_EQ;
            call(u32bFeed1, outtoks,
                 tok32PackSide('S', side, (u32)u8bDataLen(outtext)));
            hunk_open = YES;
        }
        cur_line += nl;
    }
    FLUSH();
    #undef FLUSH
    done;
}

//  Whole-file diff: every visible token, change-tagged.  `scheme` (e.g.
//  `diff:` or empty for `cat:`) selects the renderer path (DIFF-003).
ok64 WEAVEEmitFull(weave const *w, u8cs name, u8cs scheme, u8cs navver,
                   weavescope from, weavescope to, HUNKcb cb, void *ctx) {
    sane(w && cb != NULL);
    if (WEAVEEmpty(w)) done;
    if (u8csLen(w->text) > WEAVE_TEXT_CAP)
        return weave_emit_capped(w, scheme, name, navver, cb, ctx);

    wedec d = {};
    { ok64 r = weave_emit_decode(w, &d); if (r != OK) return r; }
    u32 ntok = d.n;
    if (ntok == 0) done;

    a_carve(u8,  outtext,  u8csLen(d.text) + 1);
    a_carve(u32, outtoks,  (size_t)ntok + 1);
    a_carve(u32, combined, 2 * (size_t)ntok + (size_t)u8csLen(d.text) + 16);
    a_carve(u8,  outuri,   u8csLen(name) + u8csLen(navver) + 64);
    u8cs ext = {};
    PATHu8sExt(ext, name);

    b8 hunk_open = NO;
    u32 hunk_start_line = 0, cur_line = 0;

    #define FLUSH() do { if (hunk_open) {                                    \
        call(weave_emit_uri, outuri, scheme, name, navver, hunk_start_line); \
        call(weave_emit_flush, cb, ctx, outuri, outtext, outtoks,            \
             combined, ext);                                                 \
        u8bReset(outtext); u32bReset(outtoks); hunk_open = NO;               \
        hunk_start_line = cur_line; } } while (0)

    for (u32 i = 0; i < ntok; i++) {
        u8 tag = weave_diff_classify(&d, i, from, to);
        if (tag == 0) continue;
        u32 lo = we_lo(&d, i), hi = we_hi(&d, i);
        u32 nl = 0;
        for (u32 b = lo; b < hi; b++) if (d.text[0][b] == '\n') nl++;
        if (hunk_open && u8bDataLen(outtext) + (hi - lo) > WEAVE_FULL_HUNK_MAX)
            FLUSH();
        a_part(u8c, tb, d.text, lo, hi - lo);
        call(u8bFeed, outtext, tb);
        u8 side = (tag == 'I') ? TOK_SIDE_IN
                : (tag == 'D') ? TOK_SIDE_RM : TOK_SIDE_EQ;
        call(u32bFeed1, outtoks,
             tok32PackSide('S', side, (u32)u8bDataLen(outtext)));
        hunk_open = YES;
        cur_line += nl;
    }
    FLUSH();
    #undef FLUSH
    done;
}

//  Per-token membership mask: bit g set iff the token's inserter is
//  reachable in groups[g].  The spine (bit 0 of every scope) sets every
//  group, so a base token reads as shared (== all-groups), never framed.
static u32 weave_emit_membership(wedec const *d, u32 i,
                                 weavescope const *groups, u32 ngroups) {
    u32 m = 0;
    for (u32 g = 0; g < ngroups; g++)
        if (u1At(groups[g], d->ins[i])) m |= (1u << g);
    return m;
}

//  Gather (into reset `dst`) the bytes of every alive token in
//  [run_lo,run_hi) whose membership equals `gmask` — the byte-equality
//  collapse oracle for re-absorbed (foster/cherry) content.
static ok64 weave_gather_group(u8b dst, wedec const *d, u32 run_lo, u32 run_hi,
                               u32 gmask, weavescope const *groups, u32 ngroups) {
    sane(dst != NULL);
    u8bReset(dst);
    for (u32 j = run_lo; j < run_hi; j++) {
        if (d->rlen[j] != 0) continue;     // not alive at the merged tip
        if (weave_emit_membership(d, j, groups, ngroups) != gmask) continue;
        a_part(u8c, tb, d->text, we_lo(d, j), we_hi(d, j) - we_lo(d, j));
        call(u8bFeed, dst, tb);
    }
    done;
}

//  Conflict-aware render: walk the merged weave's alive tokens; a run
//  whose membership masks disagree across groups is framed with render-
//  time `<<<<`/`||||`/`>>>>` markers (never stored).  Byte-equal groups
//  (foster/cherry re-absorption) collapse to one un-framed copy.  `out`
//  is reset on entry.
ok64 WEAVEEmitMerged(weave const *w, weavescope const *groups, u32 ngroups,
                     u8b out) {
    sane(w && out);
    if (ngroups > 32) return WEAVEFAIL;
    u8bReset(out);
    if (WEAVEEmpty(w)) done;

    wedec d = {};
    { ok64 r = weave_emit_decode(w, &d); if (r != OK) return r; }
    u32 ntok = d.n;
    if (ntok == 0) done;

    //  spine_mask = all group bits (a token reachable in every group is
    //  shared, not a conflict).  The spine bit (commits[0]) is always set
    //  in every scope, so base tokens reach it.
    u32 spine_mask = (ngroups == 0) ? 0
                   : (ngroups == 32 ? 0xFFFFFFFFu : ((1u << ngroups) - 1u));

    a_carve(u8, cgA, u8csLen(d.text) + 1);
    a_carve(u8, cgB, u8csLen(d.text) + 1);
    a_cstr(mk_open,  "<<<<");
    a_cstr(mk_mid,   "||||");
    a_cstr(mk_close, ">>>>");

    #define EMITTOK(i) do { a_part(u8c, _tb, d.text, we_lo(&d,(i)),       \
        we_hi(&d,(i)) - we_lo(&d,(i))); call(u8bFeed, out, _tb); } while (0)

    u32 i = 0;
    while (i < ntok) {
        if (d.rlen[i] != 0) { i++; continue; }   // dead at merged tip
        u32 m = weave_emit_membership(&d, i, groups, ngroups);
        if (m == spine_mask) { EMITTOK(i); i++; continue; }

        //  Divergent run: spans until the next shared (spine_mask) token.
        u32 run_lo = i, run_hi = i;
        while (run_hi < ntok) {
            if (d.rlen[run_hi] != 0) { run_hi++; continue; }
            u32 mm = weave_emit_membership(&d, run_hi, groups, ngroups);
            if (mm == spine_mask) break;
            run_hi++;
        }

        //  Conflict iff two distinct masks share no group (disjoint sides).
        b8 conflict = NO;
        u32 groups_seen[32]; u32 ngseen = 0;
        for (u32 j = run_lo; j < run_hi && !conflict; j++) {
            if (d.rlen[j] != 0) continue;
            u32 mj = weave_emit_membership(&d, j, groups, ngroups);
            for (u32 k = 0; k < ngseen; k++)
                if ((groups_seen[k] & mj) == 0) { conflict = YES; break; }
            if (conflict) break;
            b8 dup = NO;
            for (u32 k = 0; k < ngseen; k++) if (groups_seen[k] == mj) { dup = YES; break; }
            if (!dup && ngseen < 32) groups_seen[ngseen++] = mj;
        }

        if (!conflict) {
            for (u32 j = run_lo; j < run_hi; j++)
                if (d.rlen[j] == 0) EMITTOK(j);
            i = run_hi;
            continue;
        }

        //  Re-collect every distinct mask in order for the framed render.
        ngseen = 0;
        for (u32 j = run_lo; j < run_hi; j++) {
            if (d.rlen[j] != 0) continue;
            u32 mj = weave_emit_membership(&d, j, groups, ngroups);
            b8 dup = NO;
            for (u32 k = 0; k < ngseen; k++) if (groups_seen[k] == mj) { dup = YES; break; }
            if (!dup && ngseen < 32) groups_seen[ngseen++] = mj;
        }

        //  Byte-equality collapse: if every divergent group emits the same
        //  bytes (content re-absorbed under a different birth-id), emit it
        //  once with no markers — not a real conflict.
        if (ngseen >= 2) {
            call(weave_gather_group, cgA, &d, run_lo, run_hi, groups_seen[0],
                 groups, ngroups);
            a_dup(u8c, ga, u8bDataC(cgA));
            b8 all_eq = YES;
            for (u32 g = 1; g < ngseen && all_eq; g++) {
                call(weave_gather_group, cgB, &d, run_lo, run_hi, groups_seen[g],
                     groups, ngroups);
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
        for (u32 g = 0; g < ngseen; g++) {
            if (g > 0) call(u8bFeed, out, mk_mid);
            for (u32 j = run_lo; j < run_hi; j++) {
                if (d.rlen[j] != 0) continue;
                if (weave_emit_membership(&d, j, groups, ngroups) != groups_seen[g])
                    continue;
                EMITTOK(j);
            }
        }
        call(u8bFeed, out, mk_close);
        i = run_hi;
    }
    #undef EMITTOK
    done;
}
