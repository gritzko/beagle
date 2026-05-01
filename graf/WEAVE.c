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
#include "graf/NEIL.h"

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

    //  Project alive src into three parallel views — hashlets (for the
    //  LCS), text bytes, and tok32 entries (rebased offsets).  The text
    //  + toks views are needed by NEIL's predicates (`NEILIsWS` peeks
    //  bytes; the boundary-shift pass scans for line/word breaks).
    //  Without NEIL the LCS happily matches whitespace tokens between
    //  unrelated lines, producing a weave where a deleted-then-inserted
    //  pair shares its surrounding `' '` / `\n` tokens — a meaningless
    //  "context" that pollutes both the diff hunks and downstream blame.
    Bu64 alive_h     = {};
    Bu8  alive_text  = {};
    Bu32 alive_toks  = {};
    Bi32 work        = {};
    Bu32 edlbuf      = {};

    __ = u64bAlloc(alive_h, src_len + 1); if (__ != OK) goto cleanup;
    u32 src_text_len = (u32)u8bDataLen(src->text);
    if (src_text_len == 0) src_text_len = 1;
    __ = u8bMap (alive_text, src_text_len + 1); if (__ != OK) goto cleanup;
    __ = u32bAlloc(alive_toks, src_len + 1);    if (__ != OK) goto cleanup;
    {
        u32 cum = 0;
        for (u32 i = 0; i < src_len; i++) {
            if (src_irm[i].rm != 0) continue;
            __ = u64bFeed1(alive_h, src_hash[i]); if (__ != OK) goto cleanup;
            u32 lo = (i == 0) ? 0 : tok32Offset(src_toks[i - 1]);
            u32 hi = tok32Offset(src_toks[i]);
            u8cs tb = {src_text + lo, src_text + hi};
            __ = u8bFeed(alive_text, tb); if (__ != OK) goto cleanup;
            cum += (hi - lo);
            u8 tag = tok32Tag(src_toks[i]);
            __ = u32bFeed1(alive_toks, tok32Pack(tag, cum));
            if (__ != OK) goto cleanup;
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
    } else {
        //  NEIL semantic cleanup on the EDL: kill false short equalities
        //  (typically lone whitespace / punctuation tokens that LCS-
        //  matched across unrelated content) and lossless boundary shift
        //  to align edits with line/word boundaries.  Both passes
        //  in-place.  The `walk` block below re-reads `edlg[0]` so it
        //  picks up the cleaned size.
        u32cs at_view = {(u32cp)u32bDataHead(alive_toks),
                         (u32cp)u32bDataHead(alive_toks) +
                                u32bDataLen(alive_toks)};
        u32cs nt_view = {nu_toks, nu_toks_e};
        u8cs  at_text = {u8bDataHead(alive_text),
                         u8bDataHead(alive_text) + u8bDataLen(alive_text)};
        u8cs  nt_text = {nu_text, nu_text + (size_t)u8bDataLen(nu->text)};
        NEILCleanup(edlg, at_view, nt_view, at_text, nt_text);
        NEILShift  (edlg, at_view, nt_view, at_text, nt_text);
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
    if (alive_text[0]) u8bUnMap(alive_text);
    u32bFree(alive_toks);
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

// --- WEAVEEmitDiff ---
//
//  Emits one or more hunks per file, each covering a cluster of changed
//  lines flanked by `WEAVE_CTX_LINES` of context (the conventional
//  unified-diff `@@ -X,Y +A,B @@` shape minus the line-count header).
//  Per kept token (anything `alive_from || alive_to`):
//    alive_to && !alive_from → 'I' (inserted on to-side)
//    alive_from && !alive_to → 'D' (deleted on to-side)
//    alive_from && alive_to  → ' ' (context, unchanged)
//  Tokens carry their lexer tag in the hunk's `toks` (syntax stream),
//  and the I/D/' ' classification in `hili`.  Both arrays tile the
//  hunk's `text` exactly.  See `dog/HUNK.c` for the renderer.
//  TODO: NEIL-style cleanup happens upstream in `WEAVEDiff` (ride the
//  EDL there); WEAVEEmitDiff trusts its weave's classification.

#define WEAVE_CTX_LINES 3

//  Per-token classification + line tracking, used by both passes.
//  Returns the kind of `'I'` / `'D'` / `' '`, or 0 to skip.
static u8 weave_diff_classify(inrm e,
                               WEAVEsetfn in_from, void *from_ctx,
                               WEAVEsetfn in_to,   void *to_ctx) {
    b8 af = in_from(e.in, from_ctx) &&
            (e.rm == 0 || !in_from(e.rm, from_ctx));
    b8 at = in_to  (e.in, to_ctx) &&
            (e.rm == 0 || !in_to  (e.rm, to_ctx));
    if (at && !af) return 'I';
    if (af && !at) return 'D';
    if (af && at)  return ' ';
    return 0;
}

ok64 WEAVEEmitDiff(weave const *w, u8cs name,
                   WEAVEsetfn in_from, void *from_ctx,
                   WEAVEsetfn in_to,   void *to_ctx,
                   HUNKcb cb, void *cb_ctx) {
    sane(w && cb && in_from && in_to);

    u32cp toks   = (u32cp)w->toks[1];
    u32cp toks_e = (u32cp)w->toks[2];
    u32   ntok   = (u32)(toks_e - toks);
    inrmcp irm   = (inrmcp)w->inrm[1];
    u8cp   text  = (u8cp)w->text[1];
    if (ntok == 0) done;

    //  Pass 1: walk kept tokens, count newlines (line index in the
    //  merged stream), mark every line that contains an I or D token.
    //  total_lines is one past the highest line index we'll see.
    u32 total_lines_est = 1;
    {
        u32 tlen = (u32)u8bDataLen(w->text);
        for (u32 b = 0; b < tlen; b++) if (text[b] == '\n') total_lines_est++;
    }

    Bu8 changed = {};
    call(u8bMap, changed, total_lines_est + 4);
    memset(u8bDataHead(changed), 0, total_lines_est);
    u8bFed(changed, total_lines_est);

    u8 *cmark = (u8 *)u8bDataHead(changed);
    u32 cur_line = 0;
    for (u32 i = 0; i < ntok; i++) {
        u8 tag = weave_diff_classify(irm[i], in_from, from_ctx,
                                            in_to,   to_ctx);
        if (tag == 0) continue;

        u32 lo = (i == 0) ? 0 : tok32Offset(toks[i - 1]);
        u32 hi = tok32Offset(toks[i]);
        u32 nl = 0;
        for (u32 b = lo; b < hi; b++) if (text[b] == '\n') nl++;

        if (tag == 'I' || tag == 'D') {
            for (u32 l = cur_line; l <= cur_line + nl && l < total_lines_est; l++)
                cmark[l] = 1;
        }
        cur_line += nl;
    }
    u32 total_lines = cur_line + 1;
    if (total_lines > total_lines_est) total_lines = total_lines_est;

    //  Pass 2: cluster changed lines into visible windows.  Adjacent
    //  changed regions whose context (CTX_LINES on each side) overlaps
    //  fold into one window.
    Bu32 windows = {};
    call(u32bMap, windows, (total_lines + 4) * 2);
    u32 *wbuf = (u32 *)u32bDataHead(windows);
    u32 nwin = 0;
    {
        u32 i = 0;
        while (i < total_lines) {
            if (!cmark[i]) { i++; continue; }
            u32 cluster_first = i;
            u32 cluster_last  = i;
            i++;
            //  Extend cluster while the next change is within
            //  2*CTX_LINES (i.e. its context overlaps ours).
            while (i < total_lines) {
                if (cmark[i]) { cluster_last = i; i++; continue; }
                u32 j = i;
                while (j < total_lines && !cmark[j] &&
                       j - cluster_last <= 2 * WEAVE_CTX_LINES) j++;
                if (j < total_lines && cmark[j]) {
                    cluster_last = j; i = j + 1;
                } else break;
            }
            u32 lo = (cluster_first > WEAVE_CTX_LINES)
                     ? cluster_first - WEAVE_CTX_LINES : 0;
            u32 hi = cluster_last + WEAVE_CTX_LINES;
            if (hi >= total_lines) hi = total_lines - 1;
            wbuf[nwin * 2]     = lo;
            wbuf[nwin * 2 + 1] = hi;
            nwin++;
        }
    }
    u32bFed(windows, nwin * 2);

    if (nwin == 0) {
        u8bUnMap(changed);
        u32bUnMap(windows);
        done;
    }

    //  Pass 3: emit one hunk per window.  Walk tokens once, accumulate
    //  output for the active window, flush + advance when we cross a
    //  window boundary.
    Bu8  outtext = {};
    Bu32 outtoks = {};
    Bu32 outhili = {};
    call(u8bMap,  outtext, 16UL << 20);
    call(u32bMap, outtoks, 1UL << 16);
    call(u32bMap, outhili, 1UL << 16);

    ok64 ret = OK;
    u32 wi = 0;          // active window index
    u32 win_lo = wbuf[0];
    u32 win_hi = wbuf[1];
    cur_line = 0;
    u8 last_hili = 0;    // last 'I'/'D'/' ' tag in current hunk
    b8 hunk_open = NO;

    #define FLUSH_HUNK() do {                                          \
        if (hunk_open) {                                               \
            if (last_hili != 0) {                                      \
                u32bFeed1(outhili,                                     \
                    tok32Pack(last_hili, (u32)u8bDataLen(outtext)));   \
            }                                                          \
            hunk hk = {};                                              \
            $mv(hk.uri, name);                                         \
            hk.text[0] = u8bDataHead(outtext);                         \
            hk.text[1] = u8bDataHead(outtext) + u8bDataLen(outtext);   \
            hk.toks[0] = (tok32c *)u32bDataHead(outtoks);              \
            hk.toks[1] = (tok32c *)u32bDataHead(outtoks)               \
                       + u32bDataLen(outtoks);                         \
            hk.hili[0] = (tok32c *)u32bDataHead(outhili);              \
            hk.hili[1] = (tok32c *)u32bDataHead(outhili)               \
                       + u32bDataLen(outhili);                         \
            ok64 _r = cb(&hk, cb_ctx);                                 \
            if (_r != OK) ret = _r;                                    \
            u8bReset(outtext);                                         \
            u32bReset(outtoks);                                        \
            u32bReset(outhili);                                        \
            last_hili = 0;                                             \
            hunk_open = NO;                                            \
        }                                                              \
    } while (0)

    for (u32 i = 0; i < ntok; i++) {
        u8 tag = weave_diff_classify(irm[i], in_from, from_ctx,
                                            in_to,   to_ctx);
        if (tag == 0) continue;

        u32 lo = (i == 0) ? 0 : tok32Offset(toks[i - 1]);
        u32 hi = tok32Offset(toks[i]);
        u32 nl = 0;
        for (u32 b = lo; b < hi; b++) if (text[b] == '\n') nl++;

        //  Advance past finished windows.
        while (wi < nwin && cur_line > win_hi) {
            FLUSH_HUNK();
            wi++;
            if (wi < nwin) {
                win_lo = wbuf[wi * 2];
                win_hi = wbuf[wi * 2 + 1];
            }
        }
        if (wi >= nwin) break;

        //  Token's start line determines window membership.  We treat
        //  any token whose start line is inside the window as visible.
        if (cur_line >= win_lo && cur_line <= win_hi) {
            if (last_hili != 0 && last_hili != tag) {
                ok64 fo = u32bFeed1(outhili,
                    tok32Pack(last_hili, (u32)u8bDataLen(outtext)));
                if (fo != OK) { ret = fo; goto cleanup; }
            }
            last_hili = tag;

            u8cs tb = {text + lo, text + hi};
            ok64 fo = u8bFeed(outtext, tb);
            if (fo != OK) { ret = fo; goto cleanup; }
            //  Carry the lexer's syntax tag through into the hunk's
            //  toks stream so the renderer can colour by token type.
            u8 syntag = tok32Tag(toks[i]);
            fo = u32bFeed1(outtoks,
                tok32Pack(syntag, (u32)u8bDataLen(outtext)));
            if (fo != OK) { ret = fo; goto cleanup; }
            hunk_open = YES;
        }
        cur_line += nl;
    }

    //  Final flush: any tokens past the last window are dropped; the
    //  open window (if any) emits its accumulated hunk now.
    FLUSH_HUNK();

    #undef FLUSH_HUNK

cleanup:
    u8bUnMap(outtext);
    u32bUnMap(outtoks);
    u32bUnMap(outhili);
    u8bUnMap(changed);
    u32bUnMap(windows);
    return ret;
}
