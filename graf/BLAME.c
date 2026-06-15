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

//  Sentinel `src` for the worktree shadow version (uncommitted edits) —
//  shared with the DIFF projector via `WEAVE_WT_SRC` in WEAVE.h.
#define BLAME_WT_SRC WEAVE_WT_SRC

// --- Author table: gen → commit identity (sha + author time) ---
//
//  BLAME-005: a run-hunk needs the commit's full SHA-1 (for its
//  `commit:?<sha40>` URI) and author time (the hunk `ts`).  `author`/
//  `date` are kept human-readable strings for diagnostics; the rendered
//  output derives its labels from the URI + ts via the shared renderer.

typedef struct {
    u64   commit_hashlet;
    sha1  sha;        // full commit SHA-1 (8-char hashlet → commit:? URI)
    ron60 ts;         // author time, ron60-packed (hunk `ts` field)
    char  author[48];
    char  date[12];   // YYYY-MM-DD
    char  subject[80];// first line of the commit message (URI fragment)
} blame_author;

// --- Fetch author + date from commit via keeper ---

static void blame_fetch_author(blame_author *ba, keeper *k,
                                u64 commit_hashlet) {
    ba->author[0] = 0;
    ba->date[0] = 0;
    ba->ts = 0;
    ba->sha = (sha1){};
    ba->subject[0] = 0;

    //  MEM-018: this helper is invoked once per folded commit by the
    //  weave step callback, which GRAFFileWeave fires via a raw fn-ptr
    //  (no call()/try() boundary).  A per-call `u8bAcquire(ABC_BASS, …,
    //  1 MiB)` would therefore never rewind — 1 MiB/commit piles up on
    //  BASS until `a_carve` returns NOROOM and authors silently go
    //  blank.  Read the commit body into the long-lived, reset-per-use
    //  `GRAF.obj_buf` singleton instead: it is free at the callback
    //  point (GRAFBlobAtCommit / blame_descend_leaf have finished with
    //  it before the step fires) and costs no BASS at all.
    Bu8 *cbuf = &GRAF.obj_buf;
    u8bReset(*cbuf);
    u8 obj_type = 0;
    if (KEEPGet(commit_hashlet,
                DAG_H60_HEXLEN, *cbuf, &obj_type) != OK ||
        obj_type != DOG_OBJ_COMMIT) {
        return;
    }

    //  Full commit SHA-1 from the canonical body — feeds the
    //  `commit:?<8hex>#<subject>` hunk URI (8-char hashlet; matches the
    //  sha COMMIT-001 resolves).
    KEEPObjSha(&ba->sha, DOG_OBJ_COMMIT, u8bDataC(*cbuf));

    //  Walk via the shared commit-body parser; pick name + ts from
    //  the author header.
    a_dup(u8c, scan, u8bDataC(*cbuf));
    u8cs field = {}, value = {};
    while (GITu8sDrainCommit(scan, field, value) == OK) {
        if (u8csEmpty(field)) {
            //  Blank line → `value` is the message body; its first line
            //  is the subject (rides the hunk URI fragment).
            if (!u8csEmpty(value)) {
                a_dup(u8c, msc, value);
                u8cp end = (u8csFind(msc, '\n') == OK) ? msc[0] : value[1];
                u8cs subj = {value[0], end};
                size_t sl = u8csLen(subj);
                if (sl >= sizeof(ba->subject)) sl = sizeof(ba->subject) - 1;
                if (sl > 0) memcpy(ba->subject, value[0], sl);
                ba->subject[sl] = 0;
            }
            break;
        }
        if (!u8csEq(field, GIT_FIELD_AUTHOR)) continue;
        u8csc value_c = {value[0], value[1]};
        u8cs name = {}, email = {};
        ron60 ts_r = 0;
        GITu8sIdent(value_c, name, email, &ts_r);
        size_t nl = u8csLen(name);
        if (nl >= sizeof(ba->author)) nl = sizeof(ba->author) - 1;
        if (nl > 0) memcpy(ba->author, name[0], nl);
        ba->author[nl] = 0;
        ba->ts = ts_r;
        if (ts_r != 0) {
            struct tm tm = {};
            if (RONToTime(ts_r, &tm, NULL) == OK) {
                snprintf(ba->date, sizeof(ba->date), "%04d-%02d-%02d",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
            }
        }
        //  No break: keep draining to the blank line so the subject
        //  capture above runs.
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

//  Resolve `filepath`'s leaf blob hashlet at the tree `root_h` by
//  reading each path component's child OID — index-first (BLAME-002:
//  `DAGChildStep` over the materialised (tree,name)→child edges, zero
//  inflate), falling back to a keeper tree inflate + entry parse only
//  when the index lacks/contradicts the edge (legacy store or a 60-bit
//  collision).  Each component hashlet is compared to `prev[]` (the
//  last folded version's chain, used iff `have_prev`); the descent
//  STOPS at the first component equal to prev — the subtree, hence the
//  file, is unchanged ⇒ BLAME_DESC_SAME — unless `full` (an anchor,
//  which must fold regardless and so descends to the leaf).  Fills
//  `cur[]` (cur[0]=root_h, then one hashlet per consumed segment) and
//  `*cur_n`; on CHANGED sets `*out_leaf_h` to the leaf blob's 60-bit
//  hashlet.  `*infl` counts tree inflates for GRAF_BLAME_STATS (the
//  number drops to ~0 once descent is fully index-served).  The commit
//  object is never inflated — the root tree comes from the index
//  (`tree_hs`).
static int blame_descend_leaf(u64 *out_leaf_h, u64 root_h, u8cs filepath,
                              wh128css runs, b8 use_index,
                              u64 const *prev, u32 prev_n, b8 have_prev,
                              u64 *cur, u32 *cur_n, b8 full, u32 *infl) {
    Bu8 *tb = &GRAF.tree_buf;
    cur[0] = root_h;
    u32 lvl = 0;
    u64 parent_h = root_h;       // tree whose entries we're resolving
    b8  tb_loaded = NO;          // is *tb the inflate of parent_h?

    a_dup(u8c, rest, filepath);
    while (!u8csEmpty(rest)) {
        u8c const *start = rest[0];
        a_dup(u8c, scan, rest);
        (void)u8csFind(scan, '/');
        u8cs name = {start, scan[0]};
        rest[0] = scan[0];
        if (!u8csEmpty(rest)) u8csUsed1(rest);   // step past '/'
        if (u8csEmpty(name)) continue;

        //  Resolve `name`'s child within `parent_h`.
        u64 child_h = 0;
        u8  child_type = 0;
        b8  found = NO;

        //  Index-first (no inflate).  `use_index` is NO under the
        //  GRAF_BLAME_NOINDEX test toggle, forcing the keeper-inflate
        //  fallback so the two descents can be diff-compared.
        u8csc namec = {name[0], name[1]};
        ok64 di = use_index
                      ? DAGChildStep(runs, parent_h, namec, &child_h, &child_type)
                      : DAGNONE;
        if (di == OK) {
            found = YES;
            tb_loaded = NO;       // *tb no longer matches parent_h
        } else {
            //  Fallback: inflate the parent tree (once) and scan it.
            if (!tb_loaded) {
                u8 otype = 0;
                u8bReset(*tb);
                (*infl)++;
                if (KEEPGet(parent_h, DAG_H60_HEXLEN, *tb, &otype) != OK ||
                    otype != DOG_OBJ_TREE)
                    return BLAME_DESC_ABSENT;
                tb_loaded = YES;
            }
            sha1 child = {};
            u32 mode = 0;
            a_dup(u8c, body, u8bDataC(*tb));
            u8cs ent = {}, esha = {};
            while (GITu8sDrainTree(body, ent, esha, &mode) == OK) {
                u8cs ename = {};
                if (GITu8sFileSplit(ent, NULL, ename) != OK) continue;
                if (!u8csEq(ename, name)) continue;
                (void)sha1Drain(esha, &child);
                found = YES;
                break;
            }
            if (found) {
                child_h = WHIFFHashlet60(&child);
                child_type = ((mode & 0170000) == 0040000)
                                 ? DAG_T_TREE : DAG_T_BLOB;
            }
        }
        if (!found) return BLAME_DESC_ABSENT;

        if (lvl + 1 >= BLAME_MAX_PATH_LEV) return BLAME_DESC_ABSENT;
        lvl++;
        cur[lvl] = child_h;

        //  Early-stop: identical component ⇒ file unchanged below it.
        if (!full && have_prev && lvl <= prev_n && child_h == prev[lvl]) {
            *cur_n = lvl;
            return BLAME_DESC_SAME;
        }

        if (u8csEmpty(rest)) {            // leaf reached, OID differs
            //  A directory where a file was expected is absent for this
            //  path (blame fetches the leaf as a blob).
            if (child_type == DAG_T_TREE) return BLAME_DESC_ABSENT;
            *out_leaf_h = child_h;
            *cur_n = lvl;
            return BLAME_DESC_CHANGED;
        }

        //  Descend into the subtree for the next segment.
        if (child_type != DAG_T_TREE) return BLAME_DESC_ABSENT;
        parent_h = child_h;
        tb_loaded = NO;
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
    ok64 go = GRAFOpen(NO);
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

    //  BLAME-002: index-first path descent (DAGChildStep).  The
    //  GRAF_BLAME_NOINDEX env var forces the keeper-inflate fallback —
    //  used by graf/test/blame-identical.sh to prove the two descents
    //  produce byte-identical blame.
    b8 use_index = getenv("GRAF_BLAME_NOINDEX") == NULL;

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
            u64 leaf_h = 0;
            int dr = blame_descend_leaf(&leaf_h, tree_hs[i], filepath, runs,
                                        use_index,
                                        prev_oids, prev_n, have_prev,
                                        cur_oids, &cur_n, is_anchor,
                                        &dbg_treeinfl);
            if (dr == BLAME_DESC_ABSENT) continue;
            if (dr == BLAME_DESC_SAME)   continue;   // unchanged (anchors never SAME)
            u8bReset(*cur_blob);
            u8 bt = 0;
            if (KEEPGet(leaf_h, DAG_H60_HEXLEN, *cur_blob, &bt) != OK ||
                bt != DOG_OBJ_BLOB)
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

        //  BLAME-004: fold via try() (not a bare call) so the transient
        //  BASS scratch each WEAVE step decodes (sd/nd columns + diff
        //  work arrays, sized by the accumulating weave) is rewound per
        //  iteration.  Called bare, those acquisitions pile up across
        //  every folded version and exhaust ABC_BASS — a large/long-
        //  history file then BNOROOMs mid-walk.  The woven result lives
        //  in each weave's own mmap'd TLV, which survives the rewind.
        try(WEAVEFromBlob, wnu, new_data, ext, sc);
        ret = __;
        if (ret != OK) break;
        if (use_closures) {
            //  Diff against THIS commit's parent closure (precise — a
            //  concurrent branch already in the weave passes through).
            bctx.clo = &closures[(size_t)i * words];
            try(WEAVEApply, wdst, wsrc, wnu, sc, blame_base_pred, &bctx);
        } else {
            try(WEAVEDiff, wdst, wsrc, wnu, sc);
        }
        ret = __;
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


//  BLAME-005: emit one maximal commit-run as a content hunk.
//    uri  = `commit:?<sha40>` (URIMake query form — the same shape
//           COMMIT-001's link resolves; bro makes it a click target).
//    ts   = the run commit's author time; verb = 0 (verbless — the
//           banner navigates a `commit:` projection, not an action).
//    text = the run's verbatim source bytes; toks = syntax tags from a
//           fresh dog/TOK pass over that text (the weave stores none).
//  Streamed via GRAFHunkEmit as produced — no whole-file accumulation.
//  Empty runs and the synthetic worktree layer (no sha) emit a bare
//  uri-less content hunk so unattributed lines still render.
static ok64 blame_flush_run(blame_author const *ba, u8cs run, u8cs ext) {
    sane(ba);
    if ($empty(run)) done;

    a_pad(u8, uri, 160);
    if (ba->commit_hashlet && ba->commit_hashlet != (u64)BLAME_WT_SRC) {
        //  `commit:?<8-hashlet>#<subject>` — short hashlet in the query,
        //  the commit message subject in the fragment (per the URI model).
        a_sha1hex(sha_hex, &ba->sha);
        a_head(u8c, h8, sha_hex, 8);
        a_cstr(scheme, "commit");
        u8cs none = {};
        a_cstr(subj, ba->subject);
        call(URIMake, u8bIdle(uri), scheme, none, none, h8, subj);
    }

    //  Re-tokenize the run for syntax highlighting (the weave keeps no
    //  lexer tags — see graf/WEAVE.h).  Best-effort: an unknown ext or a
    //  tokenizer hiccup just yields an untagged body, still well-formed.
    a_carve(u32, toks, (size_t)($len(run) + 16));
    tok32cs toks_view = {NULL, NULL};
    if (!$empty(ext) && TOKKnownExt(ext)) {
        u32 *begin = u32bIdleHead(toks);
        if (HUNKu32bTokenize(toks, run, ext) == OK) {
            toks_view[0] = (tok32 const *)begin;
            toks_view[1] = (tok32 const *)u32bIdleHead(toks);
        }
    }

    hunk hk = {};
    hk.ts   = ba->ts;
    //  Verbless banner: the hunk URI is a projection (`commit:?<sha>`)
    //  we navigate to, not an action — so the title bar reads
    //  `<date> commit:?<sha>#<subject>`, no `blame` verb.  ts stays for
    //  the date column.
    u8csMv(hk.uri, u8bDataC(uri));
    u8csMv(hk.text, run);
    hk.toks[0] = toks_view[0];
    hk.toks[1] = toks_view[1];
    call(GRAFHunkEmit, &hk, NULL);
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

    //  BLAME-005: emit one content hunk per maximal commit-run.  The
    //  per-line walk (inserter `seq` at each BOL) is unchanged — only the
    //  EMIT regroups: consecutive lines sharing a commit accumulate into
    //  `runbuf`, flushed as a `{ts, uri=commit:?<sha40>}` (verbless)
    //  hunk the instant the attributing commit changes.  No fixed-width
    //  gutter, no per-line `L`-tok — the commit rides the hunk header, the
    //  shared renderer (HUNKu8sFeed*) draws/highlights/wraps the body.
    u8cs ext = {};
    PATHu8sExt(ext, filepath);

    weavecur wcur;
    WEAVECurInit(&wcur, wsrc);

    //  Per-run scratch: one run's verbatim source.  Reset (not re-carved)
    //  per flush so a long file streams within a bounded buffer.
    a_carve(u8, runbuf, 16UL << 20);

    blame_author const *run_ba = NULL;   // commit owning the open run
    b8  at_bol = YES;

    ok64 ret = OK;
    while (WEAVECurNext(&wcur)) {
        if (wcur.nr != 0) continue;
        //  Inserter for blame attribution: the token's birth-id seq.
        u32 in_rep = wcur.seq;

        u8cp tp = (u8cp)wcur.text[0];
        u8cp te = (u8cp)wcur.text[1];

        if (at_bol) {
            blame_author const *ba = blame_lookup_in(authors, nauthors, in_rep);
            if (!ba) ba = &blame_unknown;
            //  Commit boundary at a line start ⇒ close the open run.
            if (run_ba &&
                ba->commit_hashlet != run_ba->commit_hashlet) {
                try(blame_flush_run, run_ba, u8bDataC(runbuf), ext);
                ret = __;
                if (ret != OK) break;
                u8bReset(runbuf);
            }
            run_ba = ba;
            at_bol = NO;
        }

        //  Append the token's bytes verbatim, tracking line starts so the
        //  next BOL re-checks the commit.
        while (tp < te) {
            u8cp nl = tp;
            while (nl < te && *nl != '\n') nl++;
            if (nl < te) {
                u8cs chunk = {tp, nl + 1};
                u8bFeed(runbuf, chunk);
                tp = nl + 1;
                at_bol = YES;
                if (tp < te) at_bol = NO;   // more bytes ⇒ same commit
            } else {
                u8cs chunk = {tp, te};
                u8bFeed(runbuf, chunk);
                tp = te;
            }
        }
    }

    //  Flush the trailing run (terminating its body with a newline so the
    //  renderer frames it cleanly).
    a_dup(u8c, tail, u8bDataC(runbuf));
    if (ret == OK && run_ba && !$empty(tail)) {
        if (*(tail[1] - 1) != '\n') (void)u8bFeed1(runbuf, '\n');
        ret = blame_flush_run(run_ba, u8bDataC(runbuf), ext);
    }

    WEAVEFree(&wA);
    WEAVEFree(&wB);
    WEAVEFree(&wnu);
    GRAFArenaCleanup();
    return ret;
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
                   u8cs from, u8cs to, b8 full, u8cs navver) {
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

    return GRAFDiff2Layer(filepath, ext, from_data, to_data, full, navver);
}
