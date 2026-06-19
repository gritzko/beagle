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
    u64 *cid;     // tie-break: inserter commit id (NOT the merged index)
    u32 *ord;     // tie-break: per-commit ordinal
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
    a_carve(u64, cid,  nx + 1);
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
        call(u64bFeed1, cid, iid);
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
    d->cid  = (u64 *)u64bDataHead(cid);
    d->ord  = (u32 *)u32bDataHead(ord);
    done;
}

//  --- DIS-043 RGA merge ---------------------------------------------------
//  rgakey: the RGA sort record per union token.  Sorted by anchor (ASC, to
//  make siblings contiguous) then the tie-break — commit-id DESCending,
//  ordinal ASCending — over the REAL (cid, ord), never the hash, so a hash
//  collision can at worst mis-order, never duplicate.
typedef struct { u64 anchor, cid; u32 ord, ui; } rgakey;

fun b8 rgakeyZ(rgakey const *a, rgakey const *b) {
    if (a->anchor != b->anchor) return a->anchor < b->anchor;
    if (a->cid != b->cid) return a->cid > b->cid;   // cid DESC
    return a->ord < b->ord;                          // ord ASC
}
#define X(M, n) M##rgakey##n
#include "abc/Bx.h"
#undef X

static int rga_cmp(void const *pa, void const *pb) {
    rgakey const *x = pa, *y = pb;
    if (x->anchor != y->anchor) return x->anchor < y->anchor ? -1 : 1;
    if (x->cid != y->cid) return x->cid > y->cid ? -1 : 1;   // cid DESC
    if (x->ord != y->ord) return x->ord < y->ord ? -1 : 1;   // ord ASC
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

//  Merge two parents' weaves into one (DIS-003 JOIN + DIS-043 RGA order).
//  The union of identities is laid out by a path-independent RGA total order
//  (each token after its immutable anchor; concurrent siblings by cid DESC,
//  ord ASC), so every merge path agrees and the JOIN matches each shared
//  identity exactly once.  `merge_commit` is reserved for an evil-merge's own
//  content (fold it with a following WEAVENext); a pure merge adds no commit.
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

    u64 nab = (u64)da.n + db.n;                // upper bound on distinct tokens

    //  (1) Build the union token set keyed by idh.  First occurrence sets the
    //  immutable fields + copies the bytes; every occurrence unions removers.
    //  Columnar parallel arrays (ABC §1): one slot per distinct identity.
    a_carve(u64, u_idh,   nab + 1);
    a_carve(u64, u_anc,   nab + 1);
    a_carve(u64, u_cid,   nab + 1);
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
    u64 *ucid = (u64 *)u64bDataHead(u_cid);
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
            ucid[ui] = d->cid[i]; uord[ui] = d->ord[i];
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

    //  (2) RGA-linearise: sort by (anchor, cid DESC, ord ASC) so siblings are
    //  contiguous, then map each anchor hash to its child group's [start,cnt).
    a_carve(rgakey, sk, (size_t)nu + 1);
    for (u32 i = 0; i < nu; i++) {
        rgakey kk = {uanc[i], ucid[i], uord[i], i};
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
