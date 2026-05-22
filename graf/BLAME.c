//  BLAME: token-level blame via keeper object store + DAG index.
//
//  Walks file history via DAG index (PATH_VER + PREV_BLOB chain),
//  fetches blobs via KEEPGet, builds a weave from successive blob
//  versions, renders blame annotations per line.
//
//  WEAVE DIFF: resolves refs via KEEPWalk, fetches blobs, runs
//  pairwise token-level diff via WEAVEDiff (delegated through
//  graf/DIFFREF.c GRAFDiff2Layer).
//
#include "GRAF.h"
#include "BLOB.h"
#include "DAG.h"
#include "WEAVE.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/UTF8.h"
#include "dog/HUNK.h"
#include "dog/WHIFF.h"
#include "dog/git/GIT.h"
#include "keeper/REFS.h"

#define BLAME_MAX_VERS 256
#define BLAME_MAX_AUTHORS 256

//  Anchor → U-token: write `diff:?<10-hex-hashlet>` after the just-
//  emitted anchor span and pack a 'U' tok past those bytes.  Bytes
//  are zero-width in the renderer; bro's click handler executes
//  `be --tlv diff:?<hex>` and KEEPResolveRef widens the hashlet
//  prefix to a full commit sha.  No-op when `toks` is the zero
//  slice (plain mode).
static void blame_pack_uri_diff_sha(Bu32 toks, u8b out, u64 hashlet) {
    if (!$ok(toks)) return;
    a_cstr(prefix, "diff:?");
    (void)u8bFeed(out, prefix);
    a_pad(u8, hex, 10);
    (void)WHIFFHexFeed40(hex_idle, hashlet);
    (void)u8bFeed(out, u8bDataC(hex));
    (void)u32bFeed1(toks, tok32Pack('U', (u32)u8bDataLen(out)));
}

//  Sentinel `src` for the worktree shadow version (uncommitted edits) —
//  shared with the DIFF projector via `WEAVE_WT_SRC` in WEAVE.h.
#define BLAME_WT_SRC WEAVE_WT_SRC

// --- Author table: gen → author + date ---

typedef struct {
    u32  gen;
    u64  commit_hashlet;
    char author[48];
    char date[12];   // YYYY-MM-DD
} blame_author;

// --- UTF-8 aware fixed-width feed: truncate to N codepoints, pad right ---

static void blame_fixfeed(u8bp out, u8cs src, u32 maxcols, u8cs after) {
    u32 cols = 0;
    u8c *p = src[0];
    while (p < src[1] && cols < maxcols) {
        u8 len = UTF8_LEN[((u8)*p) >> 4];
        if (p + len > src[1]) break;
        u8cs cp = {p, p + len};
        (void)u8bFeed(out, cp);
        p += len;
        cols++;
    }
    while (cols < maxcols) { (void)u8bFeed1(out, ' '); cols++; }
    (void)u8bFeed(out, after);
}

// --- Compact date feed: "3Jun" if same year, "2023" if different ---

static char const *MONTH_ABBR[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
};

//  ISO date is "YYYY-MM-DD" (fixed format produced by
//  blame_fetch_author); parse digit-by-digit, no sscanf.
static void blame_compact_feed(u8bp out, u8cs iso_date, int cur_year) {
    if (u8csLen(iso_date) < 10) return;
    u8cp p = iso_date[0];
    if (p[4] != '-' || p[7] != '-') return;
    int y = (p[0]-'0')*1000 + (p[1]-'0')*100 + (p[2]-'0')*10 + (p[3]-'0');
    int m = (p[5]-'0')*10 + (p[6]-'0');
    int d = (p[8]-'0')*10 + (p[9]-'0');
    a_pad(u8, buf, 8);
    if (y == cur_year && m >= 1 && m <= 12) {
        i64 dd = d;
        (void)utf8sFeedInt(buf_idle, &dd);
        a_cstr(mon, MONTH_ABBR[m - 1]);
        (void)u8bFeed(buf, mon);
    } else {
        i64 yy = y;
        (void)utf8sFeedInt(buf_idle, &yy);
    }
    (void)u8bFeed(out, u8bDataC(buf));
}

// --- Fetch author + date from commit via keeper ---

static void blame_fetch_author(blame_author *ba, keeper *k,
                                u64 commit_hashlet) {
    ba->author[0] = 0;
    ba->date[0] = 0;

    Bu8 cbuf = {};
    if (u8bMap(cbuf, 1UL << 20) != OK) return;
    u8 obj_type = 0;
    if (KEEPGet(commit_hashlet,
                DAG_H60_HEXLEN, cbuf, &obj_type) != OK ||
        obj_type != DOG_OBJ_COMMIT) {
        u8bUnMap(cbuf);
        return;
    }

    //  Walk via the shared commit-body parser; pick name + ts from
    //  the author header.
    a_dup(u8c, scan, u8bDataC(cbuf));
    u8cs field = {}, value = {};
    while (GITu8sDrainCommit(scan, field, value) == OK) {
        if (u8csEmpty(field)) break;
        if (!u8csEq(field, GIT_FIELD_AUTHOR)) continue;
        u8csc value_c = {value[0], value[1]};
        u8cs name = {}, email = {};
        ron60 ts_r = 0;
        GITu8sIdent(value_c, name, email, &ts_r);
        size_t nl = u8csLen(name);
        if (nl >= sizeof(ba->author)) nl = sizeof(ba->author) - 1;
        if (nl > 0) memcpy(ba->author, name[0], nl);
        ba->author[nl] = 0;
        if (ts_r != 0) {
            struct tm tm = {};
            if (RONToTime(ts_r, &tm, NULL) == OK) {
                snprintf(ba->date, sizeof(ba->date), "%04d-%02d-%02d",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
            }
        }
        break;
    }
    u8bUnMap(cbuf);
}

// Forward decl: ref-scoped blob fetch lives at end of file.
static ok64 blame_read_blob(u8bp buf, keeper *k, u8cs ref, u8cs filepath);

#define BLAME_ANC_SIZE  (1u << 18)   // 256K slots ≈ 4MB, power of 2

// --- Find author for a token by its u32 hashlet (low 32 of commit_hashlet) ---

static blame_author const *blame_lookup_in(blame_author const *authors,
                                            u32 nauthors, u32 in_h32) {
    for (u32 i = 0; i < nauthors; i++)
        if ((u32)authors[i].commit_hashlet == in_h32) return &authors[i];
    return NULL;
}

static blame_author const blame_unknown = {.gen = 0, .commit_hashlet = 0, .author = "?", .date = ""};

// --- Shared weave builder ---
//
//  Public via GRAF.h.  Walks the file's commit history (ancestor
//  closure of `tip_h`, or all commits when `tip_h == 0`), oldest-first,
//  byte-deduping adjacent versions, and folds each kept blob into the
//  weave via `WEAVEFromBlob` + `WEAVEDiff`.  When `wt_src != 0`, also
//  folds the on-disk worktree bytes as a final layer (skipped silently
//  on missing-file or byte-identical-to-prev).  `cb` (optional) fires
//  once per kept layer so callers (BLAME) can populate side tables.

ok64 GRAFFileWeave(weave *wsrc, weave *wdst, weave *wnu,
                   weave **out_final,
                   keeper *k, u8cs filepath, u64 tip_h,
                   u8cs reporoot, u32 wt_src,
                   GRAFweaveStepCb cb, void *cb_ctx) {
    sane(wsrc && wdst && wnu && out_final && k && $ok(filepath));

    //  Open the DAG index.  The CLI entry point may already have
    //  opened graf in rw mode — GRAFOpen then returns GRAFOPEN (or
    //  GRAFOPENRO on a downgrade attempt), which is NOT an error.
    //  We only own the handle (and must close it ourselves) when the
    //  open actually succeeded here.
    ok64 go = GRAFOpen(k->h, NO);
    b8 own_open = (go == OK);
    if (go != OK && go != GRAFOPEN && go != GRAFOPENRO) return go;

    //  Ancestor closure of the tip, topologically sorted parents-first.
    //  No PATH_VER pre-filter — the blob fetch loop below skips commits
    //  where the path is absent and dedups byte-identical adjacent
    //  versions.  When tip_h == 0 (caller couldn't resolve a tip), fall
    //  back to all commits recorded in the index.
    Bwh128 ancestors = {};
    ok64 ao = wh128bAllocate(ancestors, BLAME_ANC_SIZE);
    if (ao != OK) {
        if (own_open) GRAFClose();
        return ao;
    }
    wh128css runs = {NULL, NULL};
    GRAFRuns(runs);
    if (tip_h != 0) {
        DAGAncestors(ancestors, runs, tip_h);
    } else {
        DAGAllCommits(ancestors, runs);
    }

    size_t anc_cap = (size_t)(wh128bTerm(ancestors) -
                              wh128bHead(ancestors));
    Bu8 ord_buf = {};
    u64 *ordered = NULL;
    u32  nord    = 0;
    if (anc_cap > 0 && u8bMap(ord_buf, anc_cap * sizeof(u64)) == OK) {
        ordered = (u64 *)u8bDataHead(ord_buf);
        nord = DAGTopoSort(ordered, (u32)anc_cap, ancestors, runs);
    }

    // Two mapped blob buffers, swap each iteration
    #define GRAF_FW_BLOB_MAX (16UL << 20)  // 16 MB per blob
    Bu8 blob_a = {}, blob_b = {};
    ok64 ma = u8bMap(blob_a, GRAF_FW_BLOB_MAX);
    ok64 mb = u8bMap(blob_b, GRAF_FW_BLOB_MAX);
    if (ma != OK || mb != OK) {
        if (blob_a[0]) u8bUnMap(blob_a);
        if (blob_b[0]) u8bUnMap(blob_b);
        if (ord_buf[0]) u8bUnMap(ord_buf);
        wh128bFree(ancestors);
        if (own_open) GRAFClose();
        return (ma != OK) ? ma : mb;
    }
    Bu8 *cur_blob = &blob_a, *prev_blobp = &blob_b;

    u8cs ext = {};
    PATHu8sExt(ext, filepath);

    //  Per-topo-position metadata, scanned in lockstep with `ordered[]`:
    //    tree_hs[i]    - root-tree hashlet of ordered[i] (0 on index miss).
    //    npar_arr[i]   - number of parent edges in the index (≥2 ⇒ merge).
    //    child_count[i]- in-set children of ordered[i] (≥2 ⇒ fork).
    //  Computed in one pre-pass: DAGCommitTree + DAGParents per commit,
    //  with the standard back-scan to fold parents into child_count
    //  (LOG.c idiom).  All three arrays are optional — if the alloc
    //  fails we fall back to "fold every commit," which is correct but
    //  slow.
    Bu8 th_buf = {}, np_buf = {}, cc_buf = {};
    u64 *tree_hs    = NULL;
    u32 *npar_arr   = NULL;
    u32 *child_count = NULL;
    if (nord > 0) {
        if (u8bMap(th_buf, nord * sizeof(u64)) == OK)
            tree_hs = (u64 *)u8bDataHead(th_buf);
        if (u8bMap(np_buf, nord * sizeof(u32)) == OK)
            npar_arr = (u32 *)u8bDataHead(np_buf);
        if (u8bMap(cc_buf, nord * sizeof(u32)) == OK)
            child_count = (u32 *)u8bDataHead(cc_buf);
    }
    if (tree_hs && npar_arr && child_count) {
        for (u32 i = 0; i < nord; i++) {
            tree_hs[i] = DAGCommitTree(runs, ordered[i]);
            wh64 par_buf[16] = {};
            wh64s parents = {par_buf, par_buf + 16};
            wh64 *pbase = parents[0];
            DAGParents(runs, parents, DAGPack(DAG_T_COMMIT, ordered[i]));
            npar_arr[i] = (u32)(parents[0] - pbase);
            for (wh64 *p = pbase; p < parents[0]; p++) {
                u64 ph = DAGHashlet(*p);
                for (u32 j = i; j > 0; j--) {
                    if (ordered[j - 1] == ph) {
                        child_count[j - 1]++;
                        break;
                    }
                }
            }
        }
    }

    ok64 ret = OK;
    b8 have_prev = NO;
    u64 prev_root_h = 0;   // root-tree hashlet of the last folded layer
    for (u32 i = 0; i < nord; i++) {
        u64 commit_h = ordered[i];

        //  Anchor: always fold, even if content is unchanged — preserves
        //  weave structure at the first folded layer, the topo tail, and
        //  every fork/merge node.
        b8 is_anchor = !have_prev ||
                       (i == nord - 1) ||
                       (npar_arr    && npar_arr[i]    >= 2) ||
                       (child_count && child_count[i] >= 2);

        //  Index-side skip: same root tree as the last folded layer ⇒
        //  bit-identical content at every path ⇒ no need to fetch.
        //  Tree-hashlet 0 means the commit isn't indexed; fall through
        //  to the reliable keeper path.  Anchors bypass the skip.
        if (!is_anchor && have_prev &&
            tree_hs && tree_hs[i] != 0 && tree_hs[i] == prev_root_h)
            continue;

        u8bReset(*cur_blob);
        ok64 fo = GRAFBlobAtCommit(*cur_blob, commit_h, filepath);
        if (fo != OK) continue;

        // Byte-level dedup safety net for non-anchors when the index
        // side didn't help (different root tree, identical leaf bytes).
        if (have_prev && !is_anchor) {
            a_dup(u8c, cur_data,  u8bDataC(*cur_blob));
            a_dup(u8c, prev_data, u8bDataC(*prev_blobp));
            if (u8csEq(cur_data, prev_data)) continue;
        }

        u32 sc = (u32)commit_h;
        if (cb) {
            ok64 co = cb(sc, commit_h, cb_ctx);
            if (co != OK) { ret = co; break; }
        }

        a_dup(u8c, new_data, u8bDataC(*cur_blob));

        ret = WEAVEFromBlob(wnu, new_data, ext, sc);
        if (ret != OK) break;
        ret = WEAVEDiff(wdst, wsrc, wnu, sc);
        if (ret != OK) break;
        weave *wtmp = wsrc; wsrc = wdst; wdst = wtmp;

        // Swap blob buffers (prev kept for next iter's byte-dedup).
        Bu8 *tmp = cur_blob; cur_blob = prev_blobp; prev_blobp = tmp;
        have_prev = YES;
        if (tree_hs) prev_root_h = tree_hs[i];
    }

    //  --- Worktree shadow version ---
    //  When wt_src != 0, read the on-disk file at reporoot/filepath
    //  and fold it into the weave with src=wt_src.  Skipped silently
    //  if the file is missing (deleted in worktree) or identical to
    //  the last kept committed version.
    if (ret == OK && wt_src != 0 && $ok(reporoot)) {
        a_path(wt_path, reporoot, filepath);
        u8bp wt_mapped = NULL;
        ok64 wto = FILEMapRO(&wt_mapped, $path(wt_path));
        if (wto == OK && wt_mapped) {
            a_dup(u8c, wt_data, u8bDataC(wt_mapped));
            b8 same = NO;
            if (have_prev) {
                a_dup(u8c, prev_data, u8bDataC(*prev_blobp));
                if (u8csEq(wt_data, prev_data)) same = YES;
            }
            if (!same) {
                if (cb) {
                    ok64 co = cb(wt_src, 0, cb_ctx);
                    if (co != OK) ret = co;
                }
                if (ret == OK) {
                    ok64 fbo = WEAVEFromBlob(wnu, wt_data, ext, wt_src);
                    if (fbo == OK) {
                        ok64 dfo = WEAVEDiff(wdst, wsrc, wnu, wt_src);
                        if (dfo == OK) {
                            weave *wtmp = wsrc; wsrc = wdst; wdst = wtmp;
                        }
                    }
                }
            }
            u8bUnMap(wt_mapped);
        }
    }

    *out_final = wsrc;

    u8bUnMap(blob_a);
    u8bUnMap(blob_b);
    if (cc_buf[0]) u8bUnMap(cc_buf);
    if (np_buf[0]) u8bUnMap(np_buf);
    if (th_buf[0]) u8bUnMap(th_buf);
    if (ord_buf[0]) u8bUnMap(ord_buf);
    wh128bFree(ancestors);
    if (own_open) GRAFClose();
    return ret;
}

// --- Public entry ---

//  Step callback context for GRAFBlame: populates the per-row
//  authors[] table consumed by the rendering loop below.
typedef struct {
    blame_author *authors;
    u32          *nauthors;
    u32           cap;
    keeper       *k;
} blame_step_ctx;

static ok64 blame_step_cb(u32 src_id, u64 commit_h, void *vctx) {
    sane(vctx);
    blame_step_ctx *bs = vctx;
    if (*bs->nauthors >= bs->cap) done;
    blame_author *a = &bs->authors[*bs->nauthors];
    a->gen = 0;
    if (commit_h == 0) {
        // Worktree layer: no keeper lookup, synthetic label.
        a->commit_hashlet = (u64)src_id;
        snprintf(a->author, sizeof(a->author), "(worktree)");
        a->date[0] = 0;
    } else {
        a->commit_hashlet = commit_h;
        blame_fetch_author(a, bs->k, commit_h);
    }
    (*bs->nauthors)++;
    done;
}

ok64 GRAFBlame(u8cs filepath, u64 tip_h, u8cs reporoot) {
    sane($ok(filepath) && $ok(reporoot));
    keeper *k = &KEEP;

    call(GRAFArenaInit);

    blame_author authors[BLAME_MAX_AUTHORS] = {};
    u32 nauthors = 0;

    //  Three weave instances per the WEAVE.h API: src holds the
    //  accumulated history, dst receives each step's composition,
    //  nu is rebuilt fresh for every blob version.
    weave wA = {}, wB = {}, wnu = {};
    call(WEAVEInit, &wA);
    call(WEAVEInit, &wB);
    call(WEAVEInit, &wnu);

    blame_step_ctx bs = {.authors  = authors,
                         .nauthors = &nauthors,
                         .cap      = BLAME_MAX_AUTHORS,
                         .k        = k};
    weave *wsrc = NULL;
    ok64 fwo = GRAFFileWeave(&wA, &wB, &wnu, &wsrc, k, filepath, tip_h,
                              reporoot, BLAME_WT_SRC,
                              blame_step_cb, &bs);
    if (fwo != OK) {
        WEAVEFree(&wA);
        WEAVEFree(&wB);
        WEAVEFree(&wnu);
        GRAFArenaCleanup();
        return fwo;
    }
    if (wsrc == NULL) wsrc = &wA;

    // Render blame: "hashlet name date code"
    #define BLAME_HW 7   // hashlet width
    #define BLAME_NW 12  // name width
    #define BLAME_DW 5   // date width
    #define BLAME_PW (BLAME_HW + 1 + BLAME_NW + 1 + BLAME_DW + 1)
    #define CLR_HASH "\033[38;5;108m"
    #define CLR_NAME "\033[38;5;103m"
    #define CLR_DATE "\033[38;5;245m"
    #define CLR_OFF  "\033[0m"

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int cur_year = tm ? tm->tm_year + 1900 : 2026;
    b8 tty = graf_out_fd >= 0 && graf_emit == HUNKu8sFeed;

    u32cp w_toks   = (u32cp)wsrc->toks[1];
    u32cp w_toks_e = (u32cp)wsrc->toks[2];
    u32   wlen     = (u32)(w_toks_e - w_toks);
    inrmcp w_irm   = (inrmcp)wsrc->inrm[1];
    u8cp  w_text   = (u8cp)wsrc->text[1];

    Bu8 outbuf = {};
    call(u8bMap, outbuf, 16UL << 20);

    //  TLV mode: emit a toks stream so each row's hashlet column is a
    //  clickable anchor — `diff:?<hashlet>` rides in a 'U' token right
    //  after the anchor.  Plain mode keeps toks empty; the renderers
    //  drop the URI bytes either way.
    Bu32 toks_buf = {};
    if (tty) call(u32bAllocate, toks_buf, BLAME_MAX_AUTHORS * 4);

    u32 prev_in = 0;        // 0 means "no previous row yet"
    b8  have_prev_in = NO;
    b8  at_bol = YES;

    a_cstr(sp1, " ");
    a_cstr(empty, "");

    #define EMIT_BLANK do {                                           \
        for (u32 _j = 0; _j < BLAME_PW; _j++) u8bFeed1(outbuf, ' ');  \
    } while(0)

    for (u32 wi = 0; wi < wlen; wi++) {
        if (w_irm[wi].rm != 0) continue;

        u32 tlo = (wi == 0) ? 0 : tok32Offset(w_toks[wi - 1]);
        u32 thi = tok32Offset(w_toks[wi]);
        u8cp tp = w_text + tlo;
        u8cp te = w_text + thi;

        if (at_bol) {
            blame_author const *ba = blame_lookup_in(authors, nauthors, w_irm[wi].in);
            if (!ba) ba = &blame_unknown;
            b8 diff_commit = !have_prev_in || prev_in != w_irm[wi].in;

            if (diff_commit) {
                //  Hash column: "wt" for worktree, first 7 hex of the
                //  hashlet otherwise, blank when no source.
                a_pad(u8, hex, 10);
                u8cs hash_field = {};
                if (ba->commit_hashlet == (u64)BLAME_WT_SRC) {
                    a_cstr(wt, "wt");
                    (void)u8bFeed(hex, wt);
                    hash_field[0] = u8bDataHead(hex);
                    hash_field[1] = u8bIdleHead(hex);
                } else if (ba->commit_hashlet) {
                    (void)WHIFFHexFeed40(hex_idle, ba->commit_hashlet);
                    a_dup(u8c, full, u8bDataC(hex));
                    hash_field[0] = full[0];
                    hash_field[1] = full[0] + BLAME_HW;
                }
                a_cstr(name_field, ba->author);
                a_cstr(date_field, ba->date);
                a_pad(u8, cd, 8);
                blame_compact_feed(cd, date_field, cur_year);
                if (tty) { a_cstr(c, CLR_HASH); (void)u8bFeed(outbuf, c); }
                //  Anchor pass: emit BLAME_HW chars + L-tok, then the
                //  U-token URI, then the column-trailing space.  In
                //  plain mode the URI bytes never reach text (helper
                //  no-ops on the null toks slice).
                blame_fixfeed(outbuf, hash_field, BLAME_HW, empty);
                if (tty)
                    (void)u32bFeed1(toks_buf,
                                    tok32Pack('L', (u32)u8bDataLen(outbuf)));
                if (tty && ba->commit_hashlet
                        && ba->commit_hashlet != (u64)BLAME_WT_SRC)
                    blame_pack_uri_diff_sha(toks_buf, outbuf,
                                            ba->commit_hashlet);
                (void)u8bFeed(outbuf, sp1);
                if (tty) { a_cstr(c, CLR_NAME); (void)u8bFeed(outbuf, c); }
                blame_fixfeed(outbuf, name_field, BLAME_NW, sp1);
                if (tty) { a_cstr(c, CLR_DATE); (void)u8bFeed(outbuf, c); }
                blame_fixfeed(outbuf, u8bDataC(cd), BLAME_DW, sp1);
                if (tty) { a_cstr(c, CLR_OFF);  (void)u8bFeed(outbuf, c); }
                prev_in = w_irm[wi].in;
                have_prev_in = YES;
            } else {
                EMIT_BLANK;
            }
            at_bol = NO;
        }

        while (tp < te) {
            u8cp nl = tp;
            while (nl < te && *nl != '\n') nl++;
            if (nl < te) {
                u8cs chunk = {tp, nl + 1};
                u8bFeed(outbuf, chunk);
                tp = nl + 1;
                at_bol = YES;
                if (tp < te) { EMIT_BLANK; at_bol = NO; }
            } else {
                u8cs chunk = {tp, te};
                u8bFeed(outbuf, chunk);
                tp = te;
            }
        }
    }

    if (!at_bol) {
        u8bFeed1(outbuf, '\n');
    }

    #undef EMIT_BLANK

    {
        a_pad(u8, title, 128);
        (void)u8bFeed(title, filepath);
        a_cstr(suffix, " (blame)");
        (void)u8bFeed(title, suffix);
        hunk hk = {};
        hk.uri[0]  = u8bDataHead(title);
        hk.uri[1]  = u8bIdleHead(title);
        hk.text[0] = u8bDataHead(outbuf);
        hk.text[1] = u8bIdleHead(outbuf);
        if (tty) {
            hk.toks[0] = (tok32 const *)u32bDataHead(toks_buf);
            hk.toks[1] = (tok32 const *)u32bIdleHead(toks_buf);
        }
        call(GRAFHunkEmit, &hk, NULL);
    }

    if (tty) u32bFree(toks_buf);
    u8bUnMap(outbuf);
    WEAVEFree(&wA);
    WEAVEFree(&wB);
    WEAVEFree(&wnu);
    GRAFArenaCleanup();
    done;
}

// --- Weave diff: resolve (ref, filepath) → blob via path descent ---

// Resolve `ref` + `filepath` to the blob content at that path.
// Descends path segments one at a time (O(depth) tree loads) rather
// than walking the full tree.
static ok64 blame_read_blob(u8bp buf, keeper *k, u8cs ref, u8cs filepath) {
    sane(buf && k);

    //  Pick `#<sha>` for an all-hex ref (KEEPResolveTree's fragment fast
    //  path handles full + short shas via `WHIFFHexHashlet60`); `?<ref>`
    //  for everything else (REFS-resolved name).
    b8 hex_only = !$empty(ref);
    if (hex_only) {
        for (u8cp p = ref[0]; p < ref[1]; p++) {
            u8 c = *p;
            b8 d = (c >= '0' && c <= '9');
            b8 lo = (c >= 'a' && c <= 'f');
            b8 up = (c >= 'A' && c <= 'F');
            if (!d && !lo && !up) { hex_only = NO; break; }
        }
    }
    uri target = {};
    a_pad(u8, ubuf, 512);
    u8bFeed1(ubuf, hex_only ? '#' : '?');
    call(u8bFeed, ubuf, ref);
    a_dup(u8c, udata, u8bData(ubuf));
    target.data[0] = udata[0];
    target.data[1] = udata[1];
    if (hex_only) {
        target.fragment[0] = udata[0] + 1;
        target.fragment[1] = udata[1];
    } else {
        target.query[0] = udata[0] + 1;
        target.query[1] = udata[1];
    }

    sha1 cur = {};
    call(KEEPResolveTree, &target, &cur);

    u8cs rest = {filepath[0], filepath[1]};
    while (!$empty(rest)) {
        u8cp slash = rest[0];
        while (slash < rest[1] && *slash != '/') slash++;
        u8cs name = {rest[0], slash};
        call(GRAFTreeStep, &cur, name);
        rest[0] = (slash < rest[1]) ? slash + 1 : slash;
    }

    u8 btype = 0;
    call(KEEPGetExact, &cur, buf, &btype);
    if (btype != DOG_OBJ_BLOB) fail(KEEPNONE);
    done;
}

ok64 GRAFWeaveDiff(u8cs filepath, u8cs reporoot,
                   u8cs from, u8cs to) {
    sane($ok(filepath));
    keeper *k = &KEEP;
    (void)reporoot;

    Bu8 from_buf = {}, to_buf = {};
    call(u8bMap, from_buf, 16UL << 20);
    call(u8bMap, to_buf,   16UL << 20);

    //  Fetch the `to` blob.  Empty `to` ref means HEAD (legacy
    //  callers); diff: dispatch always supplies an explicit ref.
    {
        u8cs to_ref = {};
        u8csMv(to_ref, u8csEmpty(to) ? GIT_HEAD_LIT : to);
        blame_read_blob(to_buf, k, to_ref, filepath);
    }

    //  Fetch the `from` blob if specified; empty → empty buffer →
    //  GRAFDiff2Layer treats it as a brand-new file (all-INS).
    if (!$empty(from))
        blame_read_blob(from_buf, k, from, filepath);

    a_dup(u8c, from_data, u8bData(from_buf));
    a_dup(u8c, to_data,   u8bData(to_buf));

    u8cs ext = {};
    PATHu8sExt(ext, filepath);

    ok64 ret = GRAFDiff2Layer(filepath, ext, from_data, to_data);

    u8bUnMap(from_buf);
    u8bUnMap(to_buf);
    return ret;
}
