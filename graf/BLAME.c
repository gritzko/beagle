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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/UTF8.h"
#include "dog/DOG.h"
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
    a_pad(u8, hex, 10);
    (void)WHIFFHexFeed40(hex_idle, hashlet);
    GRAFEmitDiffUri(toks, out, u8bDataC(hex));
}

//  Sentinel `src` for the worktree shadow version (uncommitted edits) —
//  shared with the DIFF projector via `WEAVE_WT_SRC` in WEAVE.h.
#define BLAME_WT_SRC WEAVE_WT_SRC

// --- Author table: gen → author + date ---

typedef struct {
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

    //  Non-sane'd helper called from inside a parent call() frame —
    //  BASS unwind happens at that boundary.
    Bu8 cbuf = {};
    if (u8bAcquire(ABC_BASS, cbuf, 1UL << 20) != OK) return;
    u8 obj_type = 0;
    if (KEEPGet(commit_hashlet,
                DAG_H60_HEXLEN, cbuf, &obj_type) != OK ||
        obj_type != DOG_OBJ_COMMIT) {
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

static blame_author const blame_unknown = {.commit_hashlet = 0, .author = "?", .date = ""};

// --- Shared weave builder ---
//
//  Public via GRAF.h.  Walks the file's commit history (ancestor
//  closure of `tip_h`, or all commits when `tip_h == 0`), oldest-first,
//  byte-deduping adjacent versions, and folds each kept blob into the
//  weave via `WEAVEFromBlob` + `WEAVEDiff`.  When `wt_src != 0`, also
//  folds the on-disk worktree bytes as a final layer (skipped silently
//  on missing-file or byte-identical-to-prev).  `cb` (optional) fires
//  once per kept layer so callers (BLAME) can populate side tables.

//  Per-commit baseline predicate for the single-weave replay: a token's
//  inserter `seq` ((u32) commit hashlet) is in the current commit's
//  parent closure iff its topo index is set in `clo` (this commit's
//  ancestor bitmap).  `map` is an open-addressed (seq -> idx+1) lookup
//  built over the topo order (rare (u32) collisions keep the first).
typedef struct {
    u32 const *mapkey;
    u32 const *mapval;   // 0 = empty, else idx+1
    u32        mapcap;   // power of two
    u64 const *clo;      // current commit's closure row (set per iteration)
} blame_base_ctx;

static b8 blame_base_pred(u32 seq, void *vctx) {
    blame_base_ctx const *c = vctx;
    u32 h = (u32)((seq * 0x9e3779b1u) >> 8) & (c->mapcap - 1);
    while (c->mapval[h] != 0) {
        if (c->mapkey[h] == seq) {
            u32 idx = c->mapval[h] - 1;
            return (c->clo[idx >> 6] >> (idx & 63)) & 1u;
        }
        h = (h + 1) & (c->mapcap - 1);
    }
    return NO;
}

#define BLAME_MAX_PATH_LEV 64   // path-depth cap for the OID early-stop

//  Top-down path descent result.
enum { BLAME_DESC_CHANGED = 0, BLAME_DESC_SAME, BLAME_DESC_ABSENT };

//  Resolve `filepath`'s blob OID at the tree `root_h` by reading each
//  path component's git OID out of its PARENT tree — no blob inflate.
//  Each component hashlet is compared to `prev[]` (the last folded
//  version's chain, used iff `have_prev`); the descent STOPS at the
//  first component equal to prev — the subtree, hence the file, is
//  unchanged ⇒ BLAME_DESC_SAME — unless `full` (an anchor, which must
//  fold regardless and so descends to the leaf).  Fills `cur[]`
//  (cur[0]=root_h, then one hashlet per consumed segment) and `*cur_n`;
//  on CHANGED sets `*out_leaf` to the leaf blob sha.  `*infl` counts
//  tree inflates for GRAF_BLAME_STATS.  The commit object is never
//  inflated — the root tree comes from the index (`tree_hs`).
static int blame_descend_leaf(sha1 *out_leaf, u64 root_h, u8cs filepath,
                              u64 const *prev, u32 prev_n, b8 have_prev,
                              u64 *cur, u32 *cur_n, b8 full, u32 *infl) {
    Bu8 *tb = &GRAF.tree_buf;
    cur[0] = root_h;
    u32 lvl = 0;

    u8 otype = 0;
    u8bReset(*tb);
    (*infl)++;
    if (KEEPGet(root_h, DAG_H60_HEXLEN, *tb, &otype) != OK ||
        otype != DOG_OBJ_TREE)
        return BLAME_DESC_ABSENT;

    a_dup(u8c, rest, filepath);
    while (!u8csEmpty(rest)) {
        u8c const *start = rest[0];
        a_dup(u8c, scan, rest);
        (void)u8csFind(scan, '/');
        u8cs name = {start, scan[0]};
        rest[0] = scan[0];
        if (!u8csEmpty(rest)) u8csUsed1(rest);   // step past '/'
        if (u8csEmpty(name)) continue;

        //  Find `name` in the current tree body → child sha.
        sha1 child = {};
        b8 found = NO;
        a_dup(u8c, body, u8bDataC(*tb));
        u8cs field = {}, esha = {};
        while (GITu8sDrainTree(body, field, esha, NULL) == OK) {
            u8cs ename = {};
            if (GITu8sFileSplit(field, NULL, ename) != OK) continue;
            if (!u8csEq(ename, name)) continue;
            (void)sha1Drain(esha, &child);
            found = YES;
            break;
        }
        if (!found) return BLAME_DESC_ABSENT;

        if (lvl + 1 >= BLAME_MAX_PATH_LEV) return BLAME_DESC_ABSENT;
        u64 child_h = WHIFFHashlet60(&child);
        lvl++;
        cur[lvl] = child_h;

        //  Early-stop: identical component ⇒ file unchanged below it.
        if (!full && have_prev && lvl <= prev_n && child_h == prev[lvl]) {
            *cur_n = lvl;
            return BLAME_DESC_SAME;
        }

        if (u8csEmpty(rest)) {            // leaf reached, OID differs
            *out_leaf = child;
            *cur_n = lvl;
            return BLAME_DESC_CHANGED;
        }

        //  Descend into the subtree.
        u8bReset(*tb);
        (*infl)++;
        if (KEEPGetExact(&child, *tb, &otype) != OK || otype != DOG_OBJ_TREE)
            return BLAME_DESC_ABSENT;
    }
    return BLAME_DESC_ABSENT;             // empty path
}

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

    //  Per-call scratch on BASS — auto-rewound at caller's call() return.
    //  Use direct *bAcquire (not a_carve) so a failure path can still run
    //  the `if (own_open) GRAFClose()` epilogue rather than short-circuit.
    Bwh128 ancestors = {};
    ok64 ao = wh128bAcquire(ABC_BASS, ancestors, BLAME_ANC_SIZE);
    if (ao != OK) {
        if (own_open) GRAFClose();
        return ao;
    }
    zerob(ancestors);  // hash set — must be zero-init
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
    if (anc_cap > 0 && u8bAcquire(ABC_BASS, ord_buf, anc_cap * sizeof(u64)) == OK) {
        ordered = (u64 *)u8bDataHead(ord_buf);
        nord = DAGTopoSort(ordered, (u32)anc_cap, ancestors, runs);
    }

    // Two BASS-carved blob buffers, swap each iteration
    #define GRAF_FW_BLOB_MAX (16UL << 20)  // 16 MB per blob
    Bu8 blob_a = {}, blob_b = {};
    ok64 ma = u8bAcquire(ABC_BASS, blob_a, GRAF_FW_BLOB_MAX);
    ok64 mb = u8bAcquire(ABC_BASS, blob_b, GRAF_FW_BLOB_MAX);
    if (ma != OK || mb != OK) {
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
        if (u8bAcquire(ABC_BASS, th_buf, nord * sizeof(u64)) == OK)
            tree_hs = (u64 *)u8bDataHead(th_buf);
        if (u8bAcquire(ABC_BASS, np_buf, nord * sizeof(u32)) == OK)
            npar_arr = (u32 *)u8bDataHead(np_buf);
        if (u8bAcquire(ABC_BASS, cc_buf, nord * sizeof(u32)) == OK)
            child_count = (u32 *)u8bDataHead(cc_buf);
    }

    //  Per-commit ancestor-closure bitmaps for PRECISE single-weave
    //  replay: each commit diffs against its parent CLOSURE, not just the
    //  previous topo layer — so a merge correctly sees BOTH parents' lines
    //  as already present (else the second parent's lines mis-attribute to
    //  the merge).  Bounded to ~128MB (≈32k commits); beyond that, fall
    //  back to the linear accumulate (WEAVEDiff vs previous).
    Bu8 clo_buf = {}, mk_buf = {}, mv_buf = {};
    u64 *closures = NULL;     // nord rows of `words` u64
    u32 *mapkey = NULL, *mapval = NULL;
    u32  words = (nord + 63) / 64;
    u32  mapcap = 1;
    while (mapcap < nord * 2 + 1) mapcap <<= 1;
    b8 use_closures = NO;
    if (tree_hs && npar_arr && child_count && nord > 0 &&
        (size_t)nord * words * 8 <= (128UL << 20)) {
        if (u8bAcquire(ABC_BASS, clo_buf, (size_t)nord * words * 8) == OK &&
            u8bAcquire(ABC_BASS, mk_buf, (size_t)mapcap * 4) == OK &&
            u8bAcquire(ABC_BASS, mv_buf, (size_t)mapcap * 4) == OK) {
            closures = (u64 *)u8bDataHead(clo_buf);
            mapkey   = (u32 *)u8bDataHead(mk_buf);
            mapval   = (u32 *)u8bDataHead(mv_buf);
            memset(closures, 0, (size_t)nord * words * 8);
            memset(mapval, 0, (size_t)mapcap * 4);
            for (u32 i = 0; i < nord; i++) {
                u32 seq = (u32)ordered[i];
                u32 h = (u32)((seq * 0x9e3779b1u) >> 8) & (mapcap - 1);
                while (mapval[h] != 0) h = (h + 1) & (mapcap - 1);
                mapkey[h] = seq; mapval[h] = i + 1;   // first occurrence wins
            }
            use_closures = YES;
        }
    }

    if (tree_hs && npar_arr && child_count) {
        for (u32 i = 0; i < nord; i++) {
            tree_hs[i] = DAGCommitTree(runs, ordered[i]);
            wh64 par_buf[16] = {};
            wh64s parents = {par_buf, par_buf + 16};
            wh64 *pbase = parents[0];
            DAGParents(runs, parents, DAGPack(DAG_T_COMMIT, ordered[i]));
            npar_arr[i] = (u32)(parents[0] - pbase);
            u64 *clo_i = use_closures ? &closures[(size_t)i * words] : NULL;
            if (clo_i) clo_i[i >> 6] |= (u64)1 << (i & 63);   // self
            for (wh64 *p = pbase; p < parents[0]; p++) {
                u64 ph = DAGHashlet(*p);
                for (u32 j = i; j > 0; j--) {
                    if (ordered[j - 1] == ph) {
                        child_count[j - 1]++;
                        if (clo_i) {
                            u64 const *clo_p = &closures[(size_t)(j - 1) * words];
                            for (u32 w = 0; w < words; w++) clo_i[w] |= clo_p[w];
                        }
                        break;
                    }
                }
            }
        }
    }

    blame_base_ctx bctx = {mapkey, mapval, mapcap, NULL};

    ok64 ret = OK;
    b8 have_prev = NO;
    u64 prev_root_h = 0;   // root-tree hashlet of the last folded layer
    u64 prev_oids[BLAME_MAX_PATH_LEV] = {};   // path-OID chain of last fold
    u32 prev_n = 0;
    u32 dbg_blobfetch = 0, dbg_fold = 0, dbg_treeinfl = 0;  // GRAF_BLAME_STATS
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

        //  Resolve the file's blob OID top-down, stopping at the first
        //  unchanged subtree (no blob inflate); fetch the blob content
        //  only when it actually changed (or at an anchor).  `tree_hs[i]`
        //  is the commit's root tree from the index — no commit inflate.
        //  Every ancestor commit carries a (COMMIT,TREE) edge, so
        //  tree_hs[i] != 0 here; the fallback covers a missing array.
        u64 cur_oids[BLAME_MAX_PATH_LEV];
        u32 cur_n = 0;
        if (tree_hs && tree_hs[i] != 0) {
            sha1 leaf = {};
            int dr = blame_descend_leaf(&leaf, tree_hs[i], filepath,
                                        prev_oids, prev_n, have_prev,
                                        cur_oids, &cur_n, is_anchor,
                                        &dbg_treeinfl);
            if (dr == BLAME_DESC_ABSENT) continue;
            if (dr == BLAME_DESC_SAME)   continue;   // unchanged (anchors never SAME)
            u8bReset(*cur_blob);
            u8 bt = 0;
            if (KEEPGetExact(&leaf, *cur_blob, &bt) != OK || bt != DOG_OBJ_BLOB)
                continue;
            dbg_blobfetch++;
        } else {
            //  Fallback: no index tree info — old commit-based fetch +
            //  byte-dedup (correct, just not faster).
            u8bReset(*cur_blob);
            if (GRAFBlobAtCommit(*cur_blob, commit_h, filepath) != OK) continue;
            dbg_blobfetch++;
            if (have_prev && !is_anchor) {
                a_dup(u8c, cur_data,  u8bDataC(*cur_blob));
                a_dup(u8c, prev_data, u8bDataC(*prev_blobp));
                if (u8csEq(cur_data, prev_data)) continue;
            }
        }

        u32 sc = (u32)commit_h;
        if (cb) {
            ok64 co = cb(sc, commit_h, cb_ctx);
            if (co != OK) { ret = co; break; }
        }

        a_dup(u8c, new_data, u8bDataC(*cur_blob));

        ret = WEAVEFromBlob(wnu, new_data, ext, sc);
        if (ret != OK) break;
        if (use_closures) {
            //  Diff against THIS commit's parent closure (precise — a
            //  concurrent branch already in the weave passes through).
            bctx.clo = &closures[(size_t)i * words];
            ret = WEAVEApply(wdst, wsrc, wnu, sc, blame_base_pred, &bctx);
        } else {
            ret = WEAVEDiff(wdst, wsrc, wnu, sc);
        }
        if (ret != OK) break;
        dbg_fold++;
        weave *wtmp = wsrc; wsrc = wdst; wdst = wtmp;

        // Swap blob buffers (prev kept for next iter's byte-dedup).
        Bu8 *tmp = cur_blob; cur_blob = prev_blobp; prev_blobp = tmp;
        have_prev = YES;
        if (tree_hs) prev_root_h = tree_hs[i];
        //  Remember this folded version's path-OID chain for the next
        //  commit's early-stop comparison.
        if (cur_n > 0 && cur_n < BLAME_MAX_PATH_LEV) {
            for (u32 q = 0; q <= cur_n; q++) prev_oids[q] = cur_oids[q];
            prev_n = cur_n;
        }
    }

    //  Opt-in perf counters (GRAF_BLAME_STATS): folds = layers actually
    //  woven; blobfetch = file-content inflates.  The fix's win shows as
    //  blobfetch dropping from ~nord to ~folds.  Off by default.
    if (getenv("GRAF_BLAME_STATS"))
        fprintf(stderr, "BLAMESTATS nord=%u folds=%u blobfetch=%u treeinfl=%u\n",
                nord, dbg_fold, dbg_blobfetch, dbg_treeinfl);

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
    //  ANSI in the body when the output is anything other than plain
    //  (TLV → bro passes it through to the terminal; Color → rendered
    //  directly; Plain → strip).
    b8 tty = graf_out_fd >= 0 && HUNKMode != HUNKOutPlain;

    weavecur wcur;
    WEAVECurInit(&wcur, wsrc);

    a_carve(u8, outbuf, 16UL << 20);

    //  TLV mode: emit a toks stream so each row's hashlet column is a
    //  clickable anchor — `diff:?<hashlet>` rides in a 'U' token right
    //  after the anchor.  Plain mode keeps toks empty; the renderers
    //  drop the URI bytes either way.
    Bu32 toks_buf = {};
    if (tty) {
        __ = u32bAcquire(ABC_BASS, toks_buf, BLAME_MAX_AUTHORS * 4);
        if (__ != OK) {
            WEAVEFree(&wA); WEAVEFree(&wB); WEAVEFree(&wnu);
            GRAFArenaCleanup();
            return __;
        }
    }

    u32 prev_in = 0;        // 0 means "no previous row yet"
    b8  have_prev_in = NO;
    b8  at_bol = YES;

    a_cstr(sp1, " ");
    a_cstr(empty, "");

    #define EMIT_BLANK do {                                           \
        for (u32 _j = 0; _j < BLAME_PW; _j++) u8bFeed1(outbuf, ' ');  \
    } while(0)

    while (WEAVECurNext(&wcur)) {
        if (wcur.nr != 0) continue;
        //  Inserter for blame attribution: the token's birth-id seq.
        u32 in_rep = wcur.seq;

        u8cp tp = (u8cp)wcur.text[0];
        u8cp te = (u8cp)wcur.text[1];

        if (at_bol) {
            blame_author const *ba = blame_lookup_in(authors, nauthors, in_rep);
            if (!ba) ba = &blame_unknown;
            b8 diff_commit = !have_prev_in || prev_in != in_rep;

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
                prev_in = in_rep;
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
    //  for everything else (REFS-resolved name).  URI-001 §"one rule":
    //  branch-FIRST — a ref whose name is all-hex but exists as a REFS
    //  name (`GRAFRefIsName`) takes the `?<ref>` path, never the hashlet.
    b8 hex_only = DOGIsHashlet(ref) && GRAFRefIsName(ref) != OK;
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
    call(GRAFPathDescend, &cur, filepath);

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

    a_carve(u8, from_buf, 16UL << 20);
    a_carve(u8, to_buf,   16UL << 20);

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

    return GRAFDiff2Layer(filepath, ext, from_data, to_data);
}
