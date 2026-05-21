#include "CAPOi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abc/NFA.h"
#include "abc/PRO.h"
#include "abc/SORT.h"
#include "spot/RXLITS.h"
#include "spot/SPOT.h"
#include "dog/tok/DEF.h"

// --- Grep: substring search in source text (no tree) ---

void CAPOGrepCtx(u8csc source, u32 match_pos, u32 nctx,
                  u32 *lo, u32 *hi) {
    u32 slen = (u32)$len(source);
    if (match_pos > slen) match_pos = slen;

    //  Backward: in the prefix [source[0], source[0]+match_pos), walk
    //  right-to-left and count newlines.  After crossing `nctx` of
    //  them we're standing on the '\n' that ends the line above the
    //  context window; the first byte of the window is that '\n' + 1.
    //  When the prefix runs out of newlines, we fall through to the
    //  buffer start.  See graf/NEIL.c for the canonical $rof+`p+1`
    //  shape.
    a_head(u8c, before, source, match_pos);
    u8cp lo_p = source[0];
    u32 seen = 0;
    $rof(u8c, p, before) {
        if (*p != '\n') continue;
        if (seen == nctx) { lo_p = p + 1; break; }
        seen++;
    }
    *lo = (u32)(lo_p - source[0]);

    //  Forward: in the suffix starting at match_pos, drain up to
    //  `nctx + 1` lines (own line plus `nctx` trailing context lines).
    //  u8csDrainLine handles the trailing no-'\n' tail by absorbing
    //  it and leaving `after` empty — same boundary semantics as the
    //  original index-based loop.
    a_rest(u8c, after, source, match_pos);
    u8cs unused = {};
    for (u32 i = 0; i <= nctx; i++) {
        if (u8csDrainLine(after, unused) != OK) break;
    }
    *hi = (u32)(after[0] - source[0]);
}

// --- Grep per-file callback ---

typedef struct {
    u8cs  substring;
    u32   ctx_lines;
} capo_grep_ctx;

static ok64 capo_grep_file_cb(void *ctx, u8csc relpath, u8csc source,
                               u8csc file_ext, u8bp mapped, path8p fpbuf) {
    sane(ctx != NULL);
    (void)fpbuf;
    capo_grep_ctx *gc = ctx;
    size_t ndl_len = (size_t)$len(gc->substring);

    if (SPOT.trace_query) {
        fprintf(stderr, "spot:   gcb %.*s len=%zu ndl=%zu first=%c\n",
            (int)$len(relpath), (char *)relpath[0],
            (size_t)$len(source), ndl_len,
            ndl_len > 0 ? gc->substring[0][0] : '?');
    }

    b8 tokenized = NO;
    Bu32 gtoks = {};
    u32cs gts = {};

    u32 prev_hi = 0;
    b8 found_any = NO;
    b8 first_hunk = YES;

    if ((size_t)$len(source) < ndl_len) goto done_file;

    a_dup(u8c, scan, source);
    while (!$empty(scan)) {
        if (u8csFindS(scan, gc->substring) != OK) break;
        u32 match_pos = (u32)(scan[0] - source[0]);
        u32 ctx_lo = 0, ctx_hi = 0;
        CAPOGrepCtx(source, match_pos, gc->ctx_lines, &ctx_lo, &ctx_hi);

        if (!found_any) {
            CAPOProgress((u8csc){});
            found_any = YES;
            if (!$empty(file_ext) && CAPOKnownExt(file_ext)) {
                size_t maxlen = $len(source) + 1;
                ok64 to = u32bMap(gtoks, maxlen);
                if (to == OK) {
                    to = SPOTTokenize(gtoks, source, file_ext);
                    if (to == OK) {
                        u32 *dts[2] = {u32bDataHead(gtoks),
                                       u32bIdleHead(gtoks)};
                        a_dup(u8c, dext, file_ext);
                        if (!$empty(dext) && *u8csHead(dext) == '.')
                            u8csUsed1(dext);
                        DEFMark(dts, source, dext);
                        tokenized = YES;
                        gts[0] = (u32cp)u32bDataHead(gtoks);
                        gts[1] = (u32cp)u32bIdleHead(gtoks);
                    } else
                        u32bUnMap(gtoks);
                }
            }
        }

        range32 hls[CAPO_MAX_HLS];
        int nhl = 0;
        hls[nhl++] = (range32){match_pos, match_pos + (u32)ndl_len};
        //  Collect additional needle hits whose match position falls
        //  inside the current context window.  Step the scan-cursor
        //  one byte past each accepted hit (same shape as the outer
        //  cursor's sp++ in the legacy loop).
        a_dup(u8c, scan2, scan);
        u8csUsed1(scan2);
        while (!$empty(scan2) && nhl < CAPO_MAX_HLS) {
            if (u8csFindS(scan2, gc->substring) != OK) break;
            u32 mp2 = (u32)(scan2[0] - source[0]);
            if (mp2 >= ctx_hi) break;
            hls[nhl++] = (range32){mp2, mp2 + (u32)ndl_len};
            u32 lo2 = 0, hi2 = 0;
            CAPOGrepCtx(source, mp2, gc->ctx_lines, &lo2, &hi2);
            if (hi2 > ctx_hi) ctx_hi = hi2;
            u8csUsed1(scan2);
        }

        b8 contiguous = (ctx_lo <= prev_hi);
        if (ctx_lo < prev_hi) ctx_lo = prev_hi;
        if (ctx_lo < ctx_hi) {
            call(CAPOBuildHunk, source, gts, ctx_lo, ctx_hi,
                 hls, nhl, file_ext, relpath,
                 !contiguous, &first_hunk);
        }
        prev_hi = ctx_hi;
        //  Resume the outer scan where the inner cursor stopped.
        u8csMv(scan, scan2);
    }

done_file:
    if (found_any) {
        if (mapped)
            LESSDefer(mapped, tokenized ? gtoks : (Bu32){});
        else if (tokenized)
            u32bUnMap(gtoks);
    } else {
        if (tokenized) u32bUnMap(gtoks);
        if (mapped) FILEUnMap(mapped);
    }
    return OK;
}

//  Inner body of CAPOGrep.  Wrapper owns hashbuf1's lifecycle so an
//  early `call(LESSArenaInit)` can't leak the trigram filter map.
static ok64 capo_grep_inner(u8csc substring, u8csc ext, u8csc reporoot,
                             u32 ctx_lines, u8css files, uri const *ref,
                             Bu64 hashbuf1) {
    sane($ok(substring) && !$empty(substring) && $ok(reporoot));

    b8 has_trigrams = NO;
    if ($len(files) == 0 && ref == NULL)
        CAPOTrigramFilter(hashbuf1, &has_trigrams, substring, reporoot);

    call(LESSArenaInit);

    capo_grep_ctx gc = {.ctx_lines = ctx_lines};
    $mv(gc.substring, substring);

    CAPOScanOpts opts = {
        .has_trigrams = has_trigrams,
        .file_fn = capo_grep_file_cb,
        .file_ctx = &gc,
    };
    if (!$empty(ext)) $mv(opts.target_ext, ext);
    if (has_trigrams) $mv(opts.tri_hashes, u64bDataC(hashbuf1));

    ok64 scan_ret = CAPORunScan(ref, files, reporoot, &opts);
    (void)scan_ret;

    CAPOProgress((u8csc){});
    if (less_nhunks > 0)
        LESSRun(less_hunks, less_nhunks);
    LESSArenaCleanup();
    done;
}

ok64 CAPOGrep(u8csc substring, u8csc ext, u8csc reporoot, u32 ctx_lines,
              u8css files, uri const *ref) {
    sane($ok(substring) && !$empty(substring) && $ok(reporoot));
    Bu64 hashbuf1 = {};
    try(capo_grep_inner, substring, ext, reporoot, ctx_lines, files, ref,
        hashbuf1);
    if (!BNULL(hashbuf1)) u64bUnMap(hashbuf1);
    done;
}

// --- Regex grep: extract literal runs from regex for trigram filtering ---

// Trigram-extraction context shared with the rxlits ragel scanner.
typedef struct {
    u8 buf[1024];
    u32 len;
    u64css live;                  //  view into SPOT.runs[]
    u64 *const *hashbuf1;         //  u64b decays to u64 *const *
    b8 *has_trigrams;
} regexlits_ctx;

// For each 3-byte trigram in the literal run, intersect/seed the index.
static void regexlits_flush(regexlits_ctx *c) {
    if (c->len >= 3) {
        u32 n = (u32)$len(c->live);
        for (u32 li = 0; li + 2 < c->len; li++) {
            if (!CAPOTriChar(c->buf[li]) ||
                !CAPOTriChar(c->buf[li + 1]) ||
                !CAPOTriChar(c->buf[li + 2])) continue;
            u8 _tb[3] = {c->buf[li], c->buf[li + 1], c->buf[li + 2]};
            u8cs tri = {_tb, _tb + 3};
            u64 tri_prefix = CAPOOffPrefix(CAPOTri40(tri));
            u64cs seek_runs[CAPO_MAX_LEVELS];
            for (u32 si = 0; si < n; si++) {
                seek_runs[si][0] = c->live[0][si][0];
                seek_runs[si][1] = c->live[0][si][1];
            }
            u64css seek_iter = {seek_runs, seek_runs + n};
            HITu64Start(seek_iter);
            if (!*c->has_trigrams) {
                u64bReset(c->hashbuf1);
                CAPOCollectPaths(seek_iter, tri_prefix,
                                 u64bDataIdle(c->hashbuf1));
                *c->has_trigrams = YES;
            } else {
                u64sSort(u64bData(c->hashbuf1));
                HITu64Seek(seek_iter, &tri_prefix);
                CAPOFilterInPlace(c->hashbuf1, seek_iter, tri_prefix);
            }
        }
    }
    c->len = 0;
}

static ok64 regexlits_cb(void *ctx, u8 ch, b8 flush) {
    regexlits_ctx *c = (regexlits_ctx *)ctx;
    if (flush) { regexlits_flush(c); return OK; }
    if (c->len < sizeof(c->buf)) c->buf[c->len++] = ch;
    return OK;
}

// Walk a regex pattern, collect runs of literal characters.
// Meta chars and class escapes break a run; backslash-escaped literals stay.
// For each run >= 3 chars, extract trigrams and intersect with the index.
static void capo_regex_literals(u8csc pattern, u64css live,
                               u64b hashbuf1,
                               b8 *has_trigrams) {
    regexlits_ctx ctx = {
        .hashbuf1 = hashbuf1, .has_trigrams = has_trigrams,
    };
    ctx.live[0] = live[0];
    ctx.live[1] = live[1];
    RXLITSu8sDrain(pattern, regexlits_cb, &ctx);
}

// --- Pcre grep per-file callback ---

typedef struct {
    nfau8cs cprog;
    u32s    nfa_ws;
    u32     ctx_lines;
} capo_pcre_ctx;

//  Find the shortest accepted sub-span of `line`.  NFAu8Search has
//  already confirmed a match exists in `line`; this brute-force pair
//  of cursors locates the precise (lo, hi) for highlighting.  Falls
//  back to the full line if no sub-span matches (defensive against
//  Search/Match disagreement).  Offsets are returned relative to
//  `source_base`.  The double loop is intrinsically position-based
//  (enumerate every {start, length} pair); slice iteration buys
//  nothing here.
static void grep_match_span(nfau8cs prog, u32s ws, u8csc line,
                             u8cp source_base, u32 *lo, u32 *hi) {
    *lo = (u32)(line[0] - source_base);
    *hi = (u32)(line[1] - source_base);
    size_t llen = (size_t)$len(line);
    for (size_t so = 0; so < llen; so++) {
        for (size_t eo = so + 1; eo <= llen; eo++) {
            u8cs sub = {line[0] + so, line[0] + eo};
            if (NFAu8Match(prog, sub, ws)) {
                *lo = (u32)(sub[0] - source_base);
                *hi = (u32)(sub[1] - source_base);
                return;
            }
        }
    }
}

static ok64 capo_pcre_file_cb(void *ctx, u8csc relpath, u8csc source,
                                u8csc file_ext, u8bp mapped, path8p fpbuf) {
    sane(ctx != NULL);
    (void)fpbuf;
    capo_pcre_ctx *pc = ctx;

    b8 tokenized = NO;
    Bu32 gtoks = {};
    u32cs gts = {};

    u32 prev_hi = 0;
    b8 found_any = NO;
    b8 first_hunk = YES;

    a_dup(u8c, cur, source);
    u8cs line = {};
    while (u8csDrainLine(cur, line) == OK) {
        if (!NFAu8Search(pc->cprog, line, pc->nfa_ws)) continue;

        u32 match_pos = (u32)(line[0] - source[0]);
        u32 ctx_lo = 0, ctx_hi = 0;
        CAPOGrepCtx(source, match_pos, pc->ctx_lines, &ctx_lo, &ctx_hi);

        if (!found_any) {
            CAPOProgress((u8csc){});
            found_any = YES;
            if (!$empty(file_ext) && CAPOKnownExt(file_ext)) {
                size_t maxlen = $len(source) + 1;
                ok64 to = u32bMap(gtoks, maxlen);
                if (to == OK) {
                    to = SPOTTokenize(gtoks, source, file_ext);
                    if (to == OK) {
                        u32 *dts[2] = {u32bDataHead(gtoks),
                                       u32bIdleHead(gtoks)};
                        a_dup(u8c, dext, file_ext);
                        if (!$empty(dext) && *u8csHead(dext) == '.')
                            u8csUsed1(dext);
                        DEFMark(dts, source, dext);
                        tokenized = YES;
                        gts[0] = (u32cp)u32bDataHead(gtoks);
                        gts[1] = (u32cp)u32bIdleHead(gtoks);
                    } else
                        u32bUnMap(gtoks);
                }
            }
        }

        range32 hls[CAPO_MAX_HLS];
        int nhl = 0;
        {
            u32 hl_lo = 0, hl_hi = 0;
            grep_match_span(pc->cprog, pc->nfa_ws, line,
                            source[0], &hl_lo, &hl_hi);
            hls[nhl++] = (range32){hl_lo, hl_hi};
        }

        //  Look for more matches in lines that still fall inside the
        //  current context window.  `cur` already points one line
        //  past the primary hit.
        a_dup(u8c, scan2, cur);
        u8cs line2 = {};
        while (nhl < CAPO_MAX_HLS &&
               u8csDrainLine(scan2, line2) == OK) {
            u32 mp2 = (u32)(line2[0] - source[0]);
            if (mp2 >= ctx_hi) break;
            if (!NFAu8Search(pc->cprog, line2, pc->nfa_ws)) continue;
            u32 hl_lo = 0, hl_hi = 0;
            grep_match_span(pc->cprog, pc->nfa_ws, line2,
                            source[0], &hl_lo, &hl_hi);
            hls[nhl++] = (range32){hl_lo, hl_hi};
            u32 lo2 = 0, hi2 = 0;
            CAPOGrepCtx(source, mp2, pc->ctx_lines, &lo2, &hi2);
            if (hi2 > ctx_hi) ctx_hi = hi2;
        }

        b8 contiguous = (ctx_lo <= prev_hi);
        if (ctx_lo < prev_hi) ctx_lo = prev_hi;
        if (ctx_lo < ctx_hi) {
            call(CAPOBuildHunk, source, gts, ctx_lo, ctx_hi,
                 hls, nhl, file_ext, relpath,
                 !contiguous, &first_hunk);
        }
        prev_hi = ctx_hi;

        //  Resume the outer scan at the end of the context window so
        //  the lines just emitted aren't re-considered.
        size_t pos = (size_t)(cur[0] - source[0]);
        if (prev_hi > pos) u8csUsed(cur, prev_hi - pos);
    }

    if (found_any) {
        if (mapped)
            LESSDefer(mapped, tokenized ? gtoks : (Bu32){});
        else if (tokenized)
            u32bUnMap(gtoks);
    } else {
        if (tokenized) u32bUnMap(gtoks);
        if (mapped) FILEUnMap(mapped);
    }
    return OK;
}

//  Inner body of CAPOPcreGrep.  Owns no buffers: the wrapper allocates
//  `nfa_ws_bb` (via u32bAlloc inside) and `hashbuf1` (via u64bMap), and
//  unconditionally frees both after we return — so an early `call(...)`
//  in the middle of setup can't leak.
static ok64 capo_pcre_inner(u8csc pattern, u8csc ext, u8csc reporoot,
                             u32 ctx_lines, u8css files, uri const *ref,
                             Bu32 nfa_ws_bb, Bu64 hashbuf1) {
    sane($ok(pattern) && !$empty(pattern) && $ok(reporoot));

    // Compile regex
    nfau8 prog_buf[512];
    u32 patch_buf[512];
    nfau8g prog = {prog_buf, prog_buf + 512, prog_buf};
    u32 *ws_patch[2] = {patch_buf, patch_buf + 512};
    a_dup(u8c,pat,pattern);
    ok64 co = NFAu8Compile(prog, pat, ws_patch);
    if (co != OK) {
        fprintf(stderr, "spot: bad regex: %s\n", ok64str(co));
        return co;
    }
    nfau8cs cprog = {prog[2], prog[0]};
    u16 nstates = NFAu8States(cprog);

    u64 wsz = NFAu8WorkSize(nstates);
    call(u32bAlloc, nfa_ws_bb, wsz);
    u32s nfa_ws = {};
    $mv(nfa_ws, u32bIdle(nfa_ws_bb));

    // Trigram filtering for regex
    b8 has_trigrams = NO;
    if ($len(files) == 0 && ref == NULL) {
        ok64 ao = u64bMap(hashbuf1, 1ULL << 27);
        if (ao == OK) {
            u64css live = {};
            CAPORuns(live);
            if (!$empty(live))
                capo_regex_literals(pattern, live, hashbuf1, &has_trigrams);
            if (has_trigrams && u64bDataLen(hashbuf1) > 0)
                u64sSort(u64bData(hashbuf1));
        }
    }

    call(LESSArenaInit);

    capo_pcre_ctx pc = {.ctx_lines = ctx_lines};
    $mv(pc.cprog, cprog);
    $mv(pc.nfa_ws, nfa_ws);

    CAPOScanOpts opts = {
        .has_trigrams = has_trigrams,
        .file_fn = capo_pcre_file_cb,
        .file_ctx = &pc,
    };
    if (!$empty(ext)) $mv(opts.target_ext, ext);
    if (has_trigrams) $mv(opts.tri_hashes, u64bDataC(hashbuf1));

    (void)CAPORunScan(ref, files, reporoot, &opts);

    CAPOProgress((u8csc){});
    if (less_nhunks > 0)
        LESSRun(less_hunks, less_nhunks);
    LESSArenaCleanup();
    done;
}

ok64 CAPOPcreGrep(u8csc pattern, u8csc ext, u8csc reporoot, u32 ctx_lines,
                   u8css files, uri const *ref) {
    sane($ok(pattern) && !$empty(pattern) && $ok(reporoot));
    Bu32 nfa_ws_bb = {};
    Bu64 hashbuf1 = {};
    try(capo_pcre_inner, pattern, ext, reporoot, ctx_lines, files, ref,
        nfa_ws_bb, hashbuf1);
    if (!BNULL(nfa_ws_bb)) u32bFree(nfa_ws_bb);
    if (!BNULL(hashbuf1))  u64bUnMap(hashbuf1);
    done;
}
