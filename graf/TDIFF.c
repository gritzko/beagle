#include "TDIFF.h"

#include <stdio.h>
#include <string.h>

#include "abc/DIFF.h"
#include "abc/PRO.h"
#include "dog/HUNK.h"
#include "dog/TOK.h"
#include "graf/JOIN.h"
#include "graf/NEIL.h"

// --- DIFF u64 template instantiation (for u64 hash arrays) ---
#define X(M, name) M##u64##name
#include "abc/DIFFx.h"
#undef X

// --- Local helpers (lifted from spot/CAPO.c) ---

static b8 DIFFExtIs(u8cs ext_nodot, char const *a, char const *b) {
    if (u8csEmpty(ext_nodot)) return NO;
    a_cstr(a_s, a);
    if (u8csEq(ext_nodot, a_s)) return YES;
    if (b != NULL) {
        a_cstr(b_s, b);
        if (u8csEq(ext_nodot, b_s)) return YES;
    }
    return NO;
}

// Walk backward from `pos` looking for a section header line (function
// name).  Heuristic depends on file extension.
static void DIFFFindFunc(u8cs source, u32 pos, u8cs ext_nodot,
                         char *out, size_t outsz) {
    out[0] = 0;
    if ($empty(source) || pos == 0 || outsz < 2) return;
    u8cp base = source[0];
    u32 slen = (u32)$len(source);
    if (pos > slen) pos = slen;

    b8 is_md = DIFFExtIs(ext_nodot, "md", "markdown") ||
               DIFFExtIs(ext_nodot, "rst", "txt");
    b8 is_py = DIFFExtIs(ext_nodot, "py", NULL);

    u32 ls = pos;
    while (ls > 0 && base[ls - 1] != '\n') ls--;

    //  Walk lines from the one containing `pos` upward.  Examining the
    //  starting line is essential — the hunk often opens at a function
    //  header line itself (the `static foo(...) {` is shown as context),
    //  and skipping it would misattribute the hunk to the previous
    //  function.
    for (int tries = 0; tries < 200; tries++) {
        u32 le = ls;
        while (le < slen && base[le] != '\n') le++;
        if (le == ls) {
            //  Empty line — step back and retry.
            if (ls == 0) return;
            ls--;
            while (ls > 0 && base[ls - 1] != '\n') ls--;
            continue;
        }

        u8 ch = base[ls];
        u32 linelen = le - ls;

        b8 match = NO;
        if (is_md) {
            match = (ch == '#');
        } else if (is_py) {
            u8cs line = {base + ls, base + le};
            a_cstr(def_pfx, "def ");
            a_cstr(class_pfx, "class ");
            match = u8csHasPrefix(line, def_pfx) ||
                    u8csHasPrefix(line, class_pfx);
        } else {
            if (ch != '/' && ch != '*' && ch != '#' &&
                ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                 ch == '_')) {
                for (u32 j = ls; j < le; j++)
                    if (base[j] == '(') { match = YES; break; }
            }
        }

        if (match) {
            u32 copylen = linelen;
            if (copylen >= outsz) copylen = (u32)(outsz - 1);
            memcpy(out, base + ls, copylen);
            out[copylen] = 0;
            while (copylen > 0 &&
                   (out[copylen - 1] == '{' || out[copylen - 1] == ':' ||
                    out[copylen - 1] == ' ' || out[copylen - 1] == '\t' ||
                    out[copylen - 1] == '\r'))
                out[--copylen] = 0;
            return;
        }

        //  No match on this line — step back to the previous line.
        if (ls == 0) return;
        ls--;
        while (ls > 0 && base[ls - 1] != '\n') ls--;
    }
}

// Tiny arena helpers (just thin wrappers for clarity)
static u8p arena_write(Bu8 arena, void const *data, size_t len) {
    if (u8bIdleLen(arena) < len) return NULL;
    u8p p = u8bIdleHead(arena);
    memcpy(p, data, len);
    u8bFed(arena, len);
    return p;
}

static ok64 arena_alloc(Bu8 arena, u8s out, size_t len) {
    if (u8bIdleLen(arena) < len) return FAILSANITY;
    $mv(out, u8bIdle(arena));
    out[1] = out[0] + len;
    memset(out[0], 0, len);
    u8bFed(arena, len);
    return OK;
}

#define CAPOJoinToks(ts, jf) \
    u32cs ts = {(u32cp)(jf)->toks[1], (u32cp)(jf)->toks[2]}

// Emit a single hunk via cb after a "deleted" or "new" file shortcut.
static ok64 emit_whole_file(Bu8 arena, char const *dispname,
                            u8cs data, u32cs toks, u8 hili_tag,
                            HUNKcb cb, void *ctx) {
    sane(cb != NULL);
    hunk hk = {};
    u32 dlen = (u32)$len(data);

    if (dispname) {
        u8cs dp = {(u8cp)dispname, (u8cp)dispname + strlen(dispname)};
        u8cs nosym = {};
        u8gp ug = u8aOpen(arena);
        HUNKu8sMakeURI(u8gRest(ug), dp, nosym, 0);
        u8cs uri_s = {};
        u8aClose(arena, uri_s);
        $mv(hk.uri, uri_s);
    }

    u8p txp = arena_write(arena, data[0], dlen);
    if (txp) { hk.text[0] = txp; hk.text[1] = txp + dlen; }

    if (!$empty(toks)) {
        size_t tkn = (size_t)((u8cp)toks[1] - (u8cp)toks[0]);
        u8p tkp = arena_write(arena, toks[0], tkn);
        if (tkp) {
            hk.toks[0] = (u32cp)tkp;
            hk.toks[1] = (u32cp)(tkp + tkn);
        }
    }

    u32 tag_tok = tok32Pack(hili_tag, dlen);
    u8p hp = arena_write(arena, &tag_tok, sizeof(u32));
    if (hp) {
        hk.hili[0] = (u32cp)hp;
        hk.hili[1] = (u32cp)(hp + sizeof(u32));
    }

    return cb(&hk, ctx);
}

ok64 DIFFu8cs(Bu8 arena,
              u8cs old_data, u8cs new_data,
              u8cs ext_nodot, char const *dispname,
              HUNKcb cb, void *ctx) {
    sane(cb != NULL);

    // Reset arena to start
    if (arena[0] != NULL)
        ((u8 **)arena)[2] = arena[1];

    // --- Special cases: empty old (new file) or empty new (deleted file)

    if ($empty(old_data) && $empty(new_data)) return OK;

    if ($empty(old_data)) {
        // New file: all content is INS
        JOINfile new_f = {};
        ok64 o = JOINTokenize(&new_f, new_data, ext_nodot);
        if (o == OK) {
            CAPOJoinToks(new_ts, &new_f);
            o = emit_whole_file(arena, dispname, new_data,
                                new_ts, 'I', cb, ctx);
        }
        JOINFree(&new_f);
        return o;
    }

    if ($empty(new_data)) {
        // Deleted file: all content is DEL
        JOINfile old_f = {};
        ok64 o = JOINTokenize(&old_f, old_data, ext_nodot);
        if (o == OK) {
            CAPOJoinToks(old_ts, &old_f);
            o = emit_whole_file(arena, dispname, old_data,
                                old_ts, 'D', cb, ctx);
        }
        JOINFree(&old_f);
        return o;
    }

    // --- Tokenize both sides
    JOINfile old_f = {}, new_f = {};
    ok64 o = JOINTokenize(&old_f, old_data, ext_nodot);
    if (o != OK) goto cleanup;
    o = JOINTokenize(&new_f, new_data, ext_nodot);
    if (o != OK) goto cleanup;

    {
        u64 on = u64bDataLen(old_f.hashes);
        u64 nn = u64bDataLen(new_f.hashes);

        u64 wsize = DIFFWorkSize(on, nn);
        u64 emax = DIFFEdlMaxEntries(on, nn);
        u64 total = wsize * sizeof(i32) + emax * sizeof(e32);
        u8 *mem[4] = {};
        o = u8bAlloc(mem, total);
        if (o != OK) goto cleanup;

        i32p workp = (i32p)mem[1];
        i32s ws = {workp, workp + wsize};
        e32 *edl_buf = (e32 *)(workp + wsize);
        e32g edl = {edl_buf, edl_buf + emax, edl_buf};

        CAPOJoinToks(old_ts, &old_f);
        CAPOJoinToks(new_ts, &new_f);

        u64cs oh = {old_f.hashes[1], old_f.hashes[2]};
        u64cs nh = {new_f.hashes[1], new_f.hashes[2]};
        o = DIFFu64s(edl, ws, oh, nh);
        if (o != OK) { u8bFree(mem); goto cleanup; }

        NEILCleanup(edl, old_ts, new_ts, old_data, new_data);
        NEILShift(edl, old_ts, new_ts, old_data, new_data);

        #define CTX_LINES 3
        u32 nedl = (u32)(edl[0] - edl[2]);


        // Phase 1: visible-line intervals
        Bu32 visbuf = {};
        u32 *vis = NULL;
        u32 nvis = 0;
        if (nedl > 0 && u32bAlloc(visbuf, 2 * nedl) == OK)
            vis = visbuf[0];
        if (vis != NULL) {
            u64 sni = 0;
            u32 nl = 0;
            for (u32 k = 0; k < nedl; k++) {
                e32 e = edl[2][k];
                u32 elen = DIFF_LEN(e);
                u32 op = DIFF_OP(e);
                if (op == DIFF_EQ) {
                    for (u32 j = 0; j < elen; j++) {
                        u8cs v = {};
                        tok32Val(v, new_ts, new_f.data[0], (int)(sni + j));
                        $for(u8c, cp, v)
                            if (*cp == '\n') nl++;
                    }
                    sni += elen;
                } else if (op == DIFF_INS) {
                    u32 sl = nl;
                    for (u32 j = 0; j < elen; j++) {
                        u8cs v = {};
                        tok32Val(v, new_ts, new_f.data[0], (int)(sni + j));
                        $for(u8c, cp, v)
                            if (*cp == '\n') nl++;
                    }
                    u32 lo = (sl > CTX_LINES) ? sl - CTX_LINES : 0;
                    u32 hi = nl + CTX_LINES;
                    vis[nvis * 2] = lo;
                    vis[nvis * 2 + 1] = hi;
                    nvis++;
                    sni += elen;
                } else {  // DIFF_DEL
                    u32 lo = (nl > CTX_LINES) ? nl - CTX_LINES : 0;
                    u32 hi = nl + CTX_LINES;
                    vis[nvis * 2] = lo;
                    vis[nvis * 2 + 1] = hi;
                    nvis++;
                }
            }
            u32 m = 0;
            for (u32 i = 0; i < nvis; i++) {
                u32 lo = vis[i * 2], hi = vis[i * 2 + 1];
                if (m > 0 && lo <= vis[(m - 1) * 2 + 1]) {
                    if (hi > vis[(m - 1) * 2 + 1])
                        vis[(m - 1) * 2 + 1] = hi;
                } else {
                    vis[m * 2] = lo;
                    vis[m * 2 + 1] = hi;
                    m++;
                }
            }
            nvis = m;
        }

        // Phase 2: walk EDL, build hunks via cb
        u32 old_len = (u32)$len(old_data);
        u32 new_len = (u32)$len(new_data);
        u32 arena_need = old_len + new_len;
        u32 max_toks = (u32)($len(old_ts) + $len(new_ts) + 100);
        u8s diff_text_s = {};
        u8p dtxp = NULL;
        u32p dtokp = NULL, dhilp = NULL;
        hunk cur_hunk_v = {};
        hunk *cur_hunk = NULL;

        if (arena_need > 0 &&
            arena_alloc(arena, diff_text_s, arena_need) == OK) {
            dtxp = diff_text_s[0];
            u8s toks_arena = {}, hili_arena = {};
            if (arena_alloc(arena, toks_arena, max_toks * sizeof(u32)) == OK &&
                arena_alloc(arena, hili_arena, max_toks * sizeof(u32)) == OK) {
                dtokp = (u32p)toks_arena[0];
                dhilp = (u32p)hili_arena[0];
            }
        }

        #define DIFF_COPY_TOK(toks_s, base, idx, hflag) do {   \
            u8cs _v = {};                                       \
            tok32Val(_v, toks_s, base, (int)(idx));             \
            u32 _n = (u32)$len(_v);                             \
            u8 _tag = tok32Tag((toks_s)[0][(idx)]);             \
            u32 _eoff = (u32)(dtxp - (u8p)cur_hunk->text[0]) + _n; \
            memcpy(dtxp, _v[0], _n);                            \
            dtxp += _n;                                         \
            if (dtokp) *dtokp++ = tok32Pack(_tag, _eoff);       \
            if (dhilp) *dhilp++ = tok32Pack(                    \
                (hflag) ? (hflag) : 'A', _eoff);                \
        } while(0)

        // Start a new hunk; emit the previous one (if any) via cb.
        #define DIFF_START_HUNK(boff) do {                      \
            if (cur_hunk != NULL) {                             \
                cur_hunk->text[1] = dtxp;                       \
                cur_hunk->toks[1] = (u32cp)dtokp;               \
                cur_hunk->hili[1] = (u32cp)dhilp;               \
                cb(cur_hunk, ctx);                              \
            }                                                   \
            cur_hunk = &cur_hunk_v;                             \
            *cur_hunk = (hunk){};                           \
            {                                                   \
                char _funcname[256] = {};                        \
                DIFFFindFunc(new_data, (boff), ext_nodot,       \
                             _funcname, sizeof(_funcname));     \
                u8cs _dp = {};                                  \
                if (dispname) {                                 \
                    _dp[0] = (u8cp)dispname;                    \
                    _dp[1] = (u8cp)dispname + strlen(dispname); \
                }                                               \
                u8cs _fn = {};                                  \
                if (_funcname[0]) {                             \
                    _fn[0] = (u8cp)_funcname;                   \
                    _fn[1] = (u8cp)_funcname + strlen(_funcname); \
                }                                               \
                u8gp _ug = u8aOpen(arena);                      \
                HUNKu8sMakeURI(u8gRest(_ug), _dp, _fn, 0);      \
                u8cs _uri = {};                                 \
                u8aClose(arena, _uri);                          \
                $mv(cur_hunk->uri, _uri);                       \
            }                                                   \
            cur_hunk->text[0] = dtxp;                           \
            cur_hunk->toks[0] = (u32cp)dtokp;                   \
            cur_hunk->hili[0] = (u32cp)dhilp;                   \
        } while(0)

        #define DIFF_COPY_LINE_PREFIX(boff) do {                \
            u32 _ls = (boff);                                   \
            while (_ls > 0 && new_data[0][_ls-1] != '\n') _ls--;\
            if (_ls < (boff)) {                                 \
                u32 _pn = (boff) - _ls;                         \
                u32 _eoff = (u32)(dtxp - (u8p)cur_hunk->text[0]) + _pn; \
                memcpy(dtxp, new_data[0] + _ls, _pn);           \
                dtxp += _pn;                                    \
                if (dtokp) *dtokp++ = tok32Pack('S', _eoff);    \
                if (dhilp) *dhilp++ = tok32Pack('A', _eoff);    \
            }                                                   \
        } while(0)

        u64 oi = 0, ni = 0;
        u32 cur_line = 0, cur_iv = 0;
        b8 in_gap = YES;

        for (u32 k = 0; k < nedl; ) {
            e32 e = edl[2][k];
            if (DIFF_OP(e) == DIFF_EQ) {
                u32 len = DIFF_LEN(e);
                for (u32 j = 0; j < len; j++) {
                    while (cur_iv < nvis &&
                           vis[cur_iv * 2 + 1] < cur_line)
                        cur_iv++;
                    b8 show = (vis == NULL) ||
                        (cur_iv < nvis &&
                         cur_line >= vis[cur_iv * 2] &&
                         cur_line <= vis[cur_iv * 2 + 1]);
                    if (show) {
                        if (in_gap) {
                            u32 boff = (ni > 0) ? tok32Offset(new_ts[0][ni-1]) : 0;
                            DIFF_START_HUNK(boff);
                            DIFF_COPY_LINE_PREFIX(boff);
                            in_gap = NO;
                        }
                        DIFF_COPY_TOK(new_ts, new_f.data[0], ni, 0);
                    } else {
                        in_gap = YES;
                    }
                    u8cs v = {};
                    tok32Val(v, new_ts, new_f.data[0], (int)ni);
                    $for(u8c, cp, v)
                        if (*cp == '\n') cur_line++;
                    oi++; ni++;
                }
                k++;
            } else {
                u32 kend = k;
                while (kend < nedl && DIFF_OP(edl[2][kend]) != DIFF_EQ)
                    kend++;

                u32 del_total = 0, ins_total = 0;
                b8 ws_only = YES;
                for (u32 kk = k; kk < kend; kk++) {
                    u32 klen = DIFF_LEN(edl[2][kk]);
                    if (DIFF_OP(edl[2][kk]) == DIFF_DEL) {
                        del_total += klen;
                        for (u32 j = 0; j < klen && ws_only; j++)
                            if (!NEILIsWS(old_ts, old_f.data[0],
                                          oi + del_total - klen + j))
                                ws_only = NO;
                    } else {
                        ins_total += klen;
                        for (u32 j = 0; j < klen && ws_only; j++)
                            if (!NEILIsWS(new_ts, new_f.data[0],
                                          ni + ins_total - klen + j))
                                ws_only = NO;
                    }
                }

                if (ws_only && ins_total > 0) {
                    for (u32 j = 0; j < ins_total; j++) {
                        while (cur_iv < nvis &&
                               vis[cur_iv * 2 + 1] < cur_line)
                            cur_iv++;
                        b8 show = (vis == NULL) ||
                            (cur_iv < nvis &&
                             cur_line >= vis[cur_iv * 2] &&
                             cur_line <= vis[cur_iv * 2 + 1]);
                        if (show) {
                            if (in_gap) {
                                u64 ti = ni + j;
                                u32 boff = (ti > 0)
                                    ? tok32Offset(new_ts[0][ti-1]) : 0;
                                DIFF_START_HUNK(boff);
                                DIFF_COPY_LINE_PREFIX(boff);
                                in_gap = NO;
                            }
                            DIFF_COPY_TOK(new_ts, new_f.data[0], ni + j, 0);
                        } else {
                            in_gap = YES;
                        }
                        u8cs v = {};
                        tok32Val(v, new_ts, new_f.data[0], (int)(ni + j));
                        $for(u8c, cp, v)
                            if (*cp == '\n') cur_line++;
                    }
                    oi += del_total;
                    ni += ins_total;
                    k = kend;
                    continue;
                }

                u32 prefix = 0;
                {
                    u32 lim = (del_total < ins_total)
                              ? del_total : ins_total;
                    u64 ti = oi, tj = ni;
                    while (prefix < lim) {
                        u8cs ov = {}, nv = {};
                        tok32Val(ov, old_ts, old_f.data[0], (int)ti);
                        tok32Val(nv, new_ts, new_f.data[0], (int)tj);
                        if (!u8csEq(ov, nv)) break;
                        prefix++; ti++; tj++;
                    }
                }

                u32 suffix = 0;
                {
                    u32 lim = (del_total < ins_total)
                              ? del_total : ins_total;
                    lim -= prefix;
                    u64 ti = oi + del_total - 1;
                    u64 tj = ni + ins_total - 1;
                    while (suffix < lim) {
                        u8cs ov = {}, nv = {};
                        tok32Val(ov, old_ts, old_f.data[0], (int)ti);
                        tok32Val(nv, new_ts, new_f.data[0], (int)tj);
                        if (!u8csEq(ov, nv)) break;
                        suffix++; ti--; tj--;
                    }
                }

                u64 base_oi = oi, base_ni = ni;

                for (u32 j = 0; j < prefix; j++) {
                    while (cur_iv < nvis &&
                           vis[cur_iv * 2 + 1] < cur_line)
                        cur_iv++;
                    b8 show = (vis == NULL) ||
                        (cur_iv < nvis &&
                         cur_line >= vis[cur_iv * 2] &&
                         cur_line <= vis[cur_iv * 2 + 1]);
                    if (show) {
                        if (in_gap) {
                            u64 ti = base_ni + j;
                            u32 boff = (ti > 0)
                                ? tok32Offset(new_ts[0][ti-1]) : 0;
                            DIFF_START_HUNK(boff);
                            DIFF_COPY_LINE_PREFIX(boff);
                            in_gap = NO;
                        }
                        DIFF_COPY_TOK(new_ts, new_f.data[0], base_ni + j, 0);
                    } else {
                        in_gap = YES;
                    }
                    u8cs v = {};
                    tok32Val(v, new_ts, new_f.data[0], (int)(base_ni + j));
                    $for(u8c, cp, v)
                        if (*cp == '\n') cur_line++;
                }

                if (in_gap) {
                    u32 boff = (base_ni > 0)
                        ? tok32Offset(new_ts[0][base_ni-1]) : 0;
                    DIFF_START_HUNK(boff);
                    in_gap = NO;
                }

                u64 toi = base_oi;
                for (u32 kk = k; kk < kend; kk++) {
                    if (DIFF_OP(edl[2][kk]) == DIFF_DEL) {
                        u32 dlen = DIFF_LEN(edl[2][kk]);
                        for (u32 j = 0; j < dlen; j++) {
                            u32 pos = (u32)(toi - base_oi);
                            if (pos >= prefix &&
                                pos < del_total - suffix) {
                                DIFF_COPY_TOK(old_ts, old_f.data[0],
                                              toi, 'D');
                            }
                            toi++;
                        }
                    }
                }

                // Insert synthetic newline between DEL and INS when the
                // DEL block contains a newline (spans multiple lines) but
                // doesn't end with one.  This prevents concatenation of
                // old and new content on the same output line.
                b8 del_has_nl = NO;
                for (u64 di = base_oi + prefix;
                     di < base_oi + del_total - suffix && !del_has_nl; di++) {
                    u8cs dv = {};
                    tok32Val(dv, old_ts, old_f.data[0], (int)di);
                    $for(u8c, cp, dv)
                        if (*cp == '\n') { del_has_nl = YES; break; }
                }
                if (del_total > prefix + suffix &&
                    ins_total > prefix + suffix &&
                    del_has_nl) {
                    u64 last_del = base_oi + del_total - suffix - 1;
                    u8cs ldv = {};
                    tok32Val(ldv, old_ts, old_f.data[0], (int)last_del);
                    if (!$empty(ldv) && *(ldv[1] - 1) != '\n') {
                        u64 ins_ti = base_ni + prefix;
                        u32 ins_boff = (ins_ti > 0)
                            ? tok32Offset(new_ts[0][ins_ti - 1]) : 0;
                        u32 ls = ins_boff;
                        while (ls > 0 && new_data[0][ls - 1] != '\n')
                            ls--;
                        u32 _nloff = (u32)(dtxp - (u8p)cur_hunk->text[0]) + 1;
                        *dtxp++ = '\n';
                        if (dtokp) *dtokp++ = tok32Pack('W', _nloff);
                        if (dhilp) *dhilp++ = tok32Pack('D', _nloff);
                        if (ls < ins_boff) {
                            u32 pn = ins_boff - ls;
                            u32 _poff = _nloff + pn;
                            memcpy(dtxp, new_data[0] + ls, pn);
                            dtxp += pn;
                            if (dtokp) *dtokp++ = tok32Pack('S', _poff);
                            //  Tag 'A' (context), not 'I'.  These bytes
                            //  are the start of the new file's line up to
                            //  the first INS token — they exist verbatim
                            //  on the old side too (the run's common
                            //  prefix), so colouring them as inserted
                            //  paints unchanged tokens (e.g. `b8`) green
                            //  on the `+` line.
                            if (dhilp) *dhilp++ = tok32Pack('A', _poff);
                        }
                    }
                }

                u64 tni = base_ni;
                for (u32 kk = k; kk < kend; kk++) {
                    if (DIFF_OP(edl[2][kk]) == DIFF_INS) {
                        u32 ilen = DIFF_LEN(edl[2][kk]);
                        for (u32 j = 0; j < ilen; j++) {
                            u32 pos = (u32)(tni - base_ni);
                            if (pos >= prefix &&
                                pos < ins_total - suffix) {
                                DIFF_COPY_TOK(new_ts, new_f.data[0],
                                              tni, 'I');
                                u8cs v = {};
                                tok32Val(v, new_ts, new_f.data[0], (int)tni);
                                $for(u8c, cp, v)
                                    if (*cp == '\n') cur_line++;
                            }
                            tni++;
                        }
                    }
                }

                for (u32 j = 0; j < suffix; j++) {
                    u64 sn = base_ni + ins_total - suffix + j;
                    while (cur_iv < nvis &&
                           vis[cur_iv * 2 + 1] < cur_line)
                        cur_iv++;
                    b8 show = (vis == NULL) ||
                        (cur_iv < nvis &&
                         cur_line >= vis[cur_iv * 2] &&
                         cur_line <= vis[cur_iv * 2 + 1]);
                    if (show) {
                        DIFF_COPY_TOK(new_ts, new_f.data[0], sn, 0);
                    } else {
                        in_gap = YES;
                    }
                    u8cs v = {};
                    tok32Val(v, new_ts, new_f.data[0], (int)sn);
                    $for(u8c, cp, v)
                        if (*cp == '\n') cur_line++;
                }

                oi = toi;
                ni = tni;
                k = kend;
            }
        }

        // Finalize last hunk
        if (cur_hunk != NULL) {
            cur_hunk->text[1] = dtxp;
            cur_hunk->toks[1] = (u32cp)dtokp;
            cur_hunk->hili[1] = (u32cp)dhilp;
            cb(cur_hunk, ctx);
        }

        #undef DIFF_COPY_TOK
        #undef DIFF_START_HUNK
        #undef DIFF_COPY_LINE_PREFIX
        #undef CTX_LINES

        u32bFree(visbuf);
        u8bFree(mem);
    }

cleanup:
    JOINFree(&old_f);
    JOINFree(&new_f);
    return o;
}
