//  GET: URI-driven single-tip blob/tree READ.
//
//  `path?<sha>`   → blob bytes at commit `<sha>`.
//  `dir/?<sha>`   → tree object body at commit `<sha>`.
//  Multi-tip merge URIs (`path?A&B...`) are retired — merge is PATCH
//  territory.  Callers use `GRAFMergeWtFileTunable` directly.
//
//  This file also hosts the WEAVE-based merge helpers
//  (`GRAFMergeWtFile`, `GRAFMergeWtFileTunable`, `GRAFMerge3Bytes`)
//  that PATCH / REBASE drive.  Layered here for shared use of
//  `build_tip_weave_tunable` and `emit_alive_bytes`.
//
#include "GRAF.h"

#include <string.h>

#include "BLOB.h"
#include "DAG.h"
#include "WEAVE.h"

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/DOG.h"
#include "dog/WHIFF.h"
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/RESOLVE.h"

con ok64 GETFAIL   = 0x1039d3ca495;
con ok64 GETBAD    = 0x40e74b28d;
con ok64 GETNOTIPS = 0x1039d5d875265c;

#define GET_MAX_TIPS   8
#define GET_MAX_VERS   200000
#define GET_ANC_SIZE   (1u << 18)     // 256K slots
#define GET_BLOB_MAX   (16UL << 20)   // 16 MB / blob
#define GET_TREE_MAX_ENTRIES 4096
#define GET_TREE_ARENA (1UL << 20)    // 1 MB for interned names/modes

typedef struct {
    sha1 sha;         // full commit sha
    u64  h40;         // 40-bit hashlet
    u32  gen;         // DAG generation (0 if not indexed)
} get_tip;

// --- Resolve one ref chunk to (sha, h40, gen) ---
//
//  Front-door for projector URIs (`tree:`, `blob:`, etc.).  Routes
//  through KEEPResolveRef so the URI accepts every shape the CLI
//  understands: full sha, hashlet prefix, absolute / relative branch
//  paths.  After resolution we insist on COMMIT-typed output: graf's
//  projector pipeline walks commit→tree→blob; passing a tree sha at
//  this seat would silently produce empty output (see
//  test/patch/26-deep-tree-take-theirs for the historical bug).
static ok64 get_resolve_chunk(get_tip *out, u8cs chunk) {
    sane(out);
    if ($empty(chunk)) return GETBAD;

    u8cs no_cur = {};
    ok64 rr = KEEPResolveRef(&out->sha, chunk, no_cur);
    if (rr != OK) return GETFAIL;

    //  Verify the resolved object is a COMMIT (not a tree / blob / tag).
    a_carve(u8, cbuf, 1UL << 20);
    u8 ct = 0;
    ok64 ko = KEEPGetExact(&out->sha, cbuf, &ct);
    if (ko != OK || ct != DOG_OBJ_COMMIT) return GETFAIL;

    out->h40 = WHIFFHashlet60(&out->sha);
    out->gen = 0;
    done;
}

// --- Drain a URI into (path, tips[]) ---
//
//  Accepts the two shapes in https://replicated.wiki/html/wiki/Verbs.html:
//      file.c?sha1&sha2          blob merge
//      dir/?sha1&sha2            tree merge  (is_tree = YES)
//  Also accepts the degenerate `path` (no `?`) as a single-tip lookup
//  resolved later by the caller.
static ok64 get_drain_uri(u8cs path_out,
                          get_tip *tips, u32 *ntips, u32 maxtips,
                          b8 *is_tree,
                          u8csc uri) {
    sane(path_out && tips && ntips && is_tree);
    *ntips = 0;
    *is_tree = NO;

    //  Split on `?`.  URIs in this surface don't carry scheme /
    //  authority / fragment — keep the parser trivial.
    u8cs query = {uri[0], uri[1]};
    ok64 has_q = u8csFind(query, '?');  // head advances to '?' or end

    path_out[0] = uri[0];
    path_out[1] = query[0];
    if ($len(path_out) > 0 && *u8csLast(path_out) == '/') *is_tree = YES;

    if (has_q != OK) done;  // path only; caller handles

    call(u8csUsed1, query);  // skip the '?'
    while (!$empty(query)) {
        if (*ntips >= maxtips) return GETBAD;
        u8cs chunk = {};
        DOGRefDrain(query, chunk);
        if ($empty(chunk)) continue;
        call(get_resolve_chunk, &tips[*ntips], chunk);
        (*ntips)++;
    }
    done;
}

// --- Byte-append a full blob fetched by (commit_h40, path) ---

static ok64 get_append_blob_at(u8b into, u64 commit_h40, u8cs path) {
    sane(into);
    a_carve(u8, blob, GET_BLOB_MAX);
    call(GRAFBlobAtCommit, blob, commit_h40, path);
    a_dup(u8c, bdata, u8bData(blob));
    return u8bFeed(into, bdata);
}

// --- LCA of two commits in the DAG -----------------------------------
//
//  Intersects each tip's ancestor set; returns the deepest member of
//  the intersection (the LCA closest to the tips).  Without gen we
//  topologically sort the intersection and pick the *last* commit —
//  in DFS post-order over parent edges, the last-emitted node is the
//  one farthest from the roots.
//
//  Sets `*out = 0` (still OK) when no shared ancestor is indexed:
//    * the DAG index is empty (graf hasn't indexed yet), or
//    * the two tips share no ancestors.
//  Real failures (BASS exhaustion, DAG read errors) propagate as ok64
//  rather than collapsing into the same 0 the empty case uses.
static ok64 get_lca(u64 *out, u64 a_h40, u64 b_h40) {
    sane(out);
    *out = 0;
    if (a_h40 == 0 || b_h40 == 0) done;

    //  Carved on BASS; rewound at the caller's call(get_lca, …) boundary.
    //  Hash-set buffers MUST be zero-filled — `HASHwh128Put/Get` use
    //  `is0(data, ndx)` to detect empty slots, and BASS acquires reuse
    //  arena memory that holds leftover content from prior carves.
    a_carve(wh128, set_a, GET_ANC_SIZE);
    a_carve(wh128, set_b, GET_ANC_SIZE);
    a_carve(wh128, set_c, GET_ANC_SIZE);
    zerob(set_a); zerob(set_b); zerob(set_c);

    wh128css runs = {NULL, NULL};
    GRAFRuns(runs);
    call(DAGAncestors, set_a, runs, a_h40);
    call(DAGAncestors, set_b, runs, b_h40);

    //  Build the intersection (common ancestors).
    wh128cp cells = wh128bHead(set_a);
    wh128cp cells_end = wh128bTerm(set_a);
    for (wh128cp c = cells; c < cells_end; c++) {
        u64 h = DAGHashlet(c->key);
        if (h == 0) continue;
        if (!DAGAncestorsHas(set_b, h)) continue;
        dag_anc_put(set_c, h);
    }

    //  Topo-sort the intersection; LCA = last (deepest) entry.
    size_t cap = (size_t)(wh128bTerm(set_c) - wh128bHead(set_c));
    if (cap > 0) {
        a_carve(u8, ord_buf, cap * sizeof(u64));
        u64 *ordered = (u64 *)u8bDataHead(ord_buf);
        u32 nord = DAGTopoSort(ordered, (u32)cap, set_c, runs);
        if (nord > 0) *out = ordered[nord - 1];
    }

    done;
}

// Public wrapper: `sha1 *` in/out for callers outside graf (sniff's
// PATCH uses this to classify modify/delete cases).  Returns OK with
// `*out` all-zero when no shared ancestor is indexed.
ok64 GRAFLca(sha1 *out, sha1cp a, sha1cp b) {
    sane(out && a && b);
    zero(out->data);

    u64 a_h40 = WHIFFHashlet60(a);
    u64 b_h40 = WHIFFHashlet60(b);
    u64 lca_h = 0;
    call(get_lca, &lca_h, a_h40, b_h40);
    if (lca_h == 0) done;   // unrelated histories — leave out zero

    //  Recover the full sha by fetching the commit body from keeper
    //  and rehashing (identical to the trick `get_resolve_chunk`
    //  uses — KEEPObjSha("commit <len>\0<body>") is canonical).
    a_carve(u8, cbuf, 1UL << 20);
    u8 ct = 0;
    ok64 o = KEEPGet(lca_h,
                     DAG_H60_HEXLEN, cbuf, &ct);
    if (o != OK || ct != DOG_OBJ_COMMIT) done;

    a_dup(u8c, body, u8bData(cbuf));
    KEEPObjSha(out, DOG_OBJ_COMMIT, body);
    done;
}

// --- 2-way blob merge via WEAVE -------------------------------------

//  Forward decls — implementations live further down with
//  `get_weave_union`'s helpers.
static ok64 build_tip_weave_tunable(weave *out, u8cs path, u8cs ext,
                                    u64 const *tip_hs, u32 ntips,
                                    u32 edges,
                                    u64 const *skip_hl, u32 nskip,
                                    Bu32 out_ids, b8 *out_no_history);
static ok64 emit_alive_bytes(u8b into, weave const *w);

//  Per-side membership predicate for `WEAVEEmitMerged`.  Backed by a
//  Bu32 of `sc` values used during `build_tip_weave_tunable` (and
//  optionally augmented with WEAVE_WT_SRC for the wt-folded side).
typedef struct {
    u32cp ids;     // u32 array of in-stamps reachable via this side
    u32   n;
} merge_id_set;

static b8 merge_id_set_has(u32 in, void *vctx) {
    merge_id_set *s = (merge_id_set *)vctx;
    if (!s) return NO;
    for (u32 i = 0; i < s->n; i++)
        if (s->ids[i] == in) return YES;
    return NO;
}

//  Sentinel seq for the tgt side, applied as ONE edit onto the base weave.
#define GET_TGT_SRC 0x5A5A5A5Au

//  Tgt-side membership: the sentinel, or any tgt-closure commit (so the
//  common ancestors a tgt token sits among also read as tgt).
static b8 get_tgt_pred(u32 in, void *vctx) {
    return in == GET_TGT_SRC || merge_id_set_has(in, vctx);
}

//  Fold the wt-on-disk bytes for `path` (relative to `reporoot`) into
//  the weave `cur` as a final WEAVE_WT_SRC layer, writing the result
//  into `next` and reporting via `*used_next` whether the fold ran
//  (NO when the file is missing or byte-identical to the prior layer
//  — caller keeps `cur`).  `nu_scratch` is used for the WEAVEFromBlob
//  intermediate.
static ok64 graf_fold_wt_layer(weave *next, b8 *used_next,
                               weave const *cur, weave *nu_scratch,
                               u8cs path, u8cs ext, u8cs reporoot) {
    sane(next && cur && nu_scratch && used_next);
    *used_next = NO;
    if (!$ok(reporoot)) done;

    a_path(wt_path, reporoot, path);
    u8bp wt_mapped = NULL;
    ok64 mo = FILEMapRO(&wt_mapped, $path(wt_path));
    if (mo != OK || !wt_mapped) done;       // missing-file → skip silently

    u8cs wt_data = {u8bDataHead(wt_mapped),
                    u8bDataHead(wt_mapped) + u8bDataLen(wt_mapped)};
    ok64 ret = WEAVEFromBlob(nu_scratch, wt_data, ext, WEAVE_WT_SRC);
    if (ret == OK) {
        ret = WEAVEDiff(next, cur, nu_scratch, WEAVE_WT_SRC);
        if (ret == OK) *used_next = YES;
    }
    FILEUnMap(wt_mapped);
    return ret;
}

//  Weave-merge a single file across two commits, treating the wt's
//  on-disk bytes for `path` as an implicit edit attached to `base`.
//  Builds the ancestor-closure weave for each tip, folds the wt
//  bytes as a final WEAVE_WT_SRC layer on the base side, runs
//  WEAVEMerge, and emits the alive-token bytes into `out` —
//  framing divergent regions with `<<<<` / `||||` / `>>>>` when the
//  two sides' inserts collide (see `WEAVEEmitMerged`).
//
//  Returns OK on success.  GRAFFAIL on history-empty-on-both-sides.
//  Caller writes `out` to disk and stamps the new mtime.
//  Index a commit tip's ancestry into graf's DAG (idempotent on
//  already-known tips).  When commits land in keeper packs via a path
//  that does NOT reindex graf (a wire push, `be patch`, a sub fetch,
//  or a fresh worktree clone), graf's DAG holds an INCOHERENT view:
//  the per-commit tree-witness edges may exist (so the ancestor walk
//  still finds the commits) while the COMMIT→COMMIT parent edges are
//  absent.  DAGTopoSortTunable then sees every commit as a parent-less
//  root, hashlet-sorts them, and the FF replay runs out of causal
//  order — dropping intermediate commits' edits NON-deterministically
//  (the order depends on the commit shas).  Indexing both merge
//  endpoints up front guarantees a coherent parent-edge DAG so the
//  topo sort and replay are correct and reproducible.  See DIS-041.
static ok64 graf_index_tip(sha1cp tip) {
    sane(tip);
    a_sha1hex(hex_bytes, tip);
    uri tip_uri = {};
    $mv(tip_uri.fragment, hex_bytes);
    $mv(tip_uri.data,     hex_bytes);
    call(GRAFIndexFromTips, &tip_uri);
    done;
}

ok64 GRAFMergeWtFileTunable(u8cs path, u8cs reporoot,
                            sha1cp base, sha1cp tgt,
                            u32 edges,
                            u64 const *skip_hl, u32 nskip,
                            u8b out) {
    //  Empty reporoot is allowed — callers that don't have a wt
    //  (keeper-side merges) skip the wt-fold layer entirely.
    sane($ok(path) && base && tgt && out);
    u8bReset(out);

    u64 base_h40 = WHIFFHashlet60(base);
    u64 tgt_h40  = WHIFFHashlet60(tgt);

    u8cs ext = {};
    PATHu8sExt(ext, path);

    weave wbase = {}, wbase_wt = {}, wnu = {};
    weave wtgt = {}, wmerge = {};
    Bu32 base_ids = {}, tgt_ids = {};
    Bu8  tgt_blob = {};
    ok64 ret = OK;
    if ((ret = WEAVEInit(&wbase))    != OK) return ret;
    if ((ret = WEAVEInit(&wbase_wt)) != OK) goto cleanup;
    if ((ret = WEAVEInit(&wnu))      != OK) goto cleanup;
    if ((ret = WEAVEInit(&wtgt))     != OK) goto cleanup;
    if ((ret = WEAVEInit(&wmerge))   != OK) goto cleanup;
    if ((ret = u32bMap(base_ids, 4096)) != OK) goto cleanup;
    if ((ret = u32bMap(tgt_ids,  4096)) != OK) goto cleanup;
    if ((ret = u8bMap(tgt_blob, 64UL << 20)) != OK) goto cleanup;

    weave const *wcur = &wbase;
    b8 wt_layered = NO;

    //  DIS-041: index both merge endpoints' ancestry into graf's DAG
    //  BEFORE building the weaves.  This is the single place GET (via
    //  GRAFMergeWtFile) and PATCH (direct) both funnel through, so both
    //  get a coherent parent-edge DAG and a correct, reproducible FF
    //  replay.  A genuine index failure PROPAGATES — never swallowed,
    //  never a half-merge.
    ret = graf_index_tip(base);
    if (ret == OK) ret = graf_index_tip(tgt);
    if (ret != OK) goto cleanup;

    //  Base side: ancestor closure of base commit, plus wt layer.  A
    //  no-history result here, AFTER a guaranteed index, means the
    //  commit object is genuinely unreachable — a hard error, not the
    //  lossy single-version fallback.
    b8 base_no_hist = NO, tgt_no_hist = NO;
    ret = build_tip_weave_tunable(&wbase, path, ext, &base_h40, 1,
                                  edges, skip_hl, nskip, base_ids,
                                  &base_no_hist);
    if (ret != OK) goto cleanup;

    ret = graf_fold_wt_layer(&wbase_wt, &wt_layered, &wbase, &wnu,
                             path, ext, reporoot);
    if (ret != OK) goto cleanup;
    if (wt_layered) {
        wcur = &wbase_wt;
        (void)u32bFeed1(base_ids, WEAVE_WT_SRC);
    }

    //  Target side: ancestor closure of tgt commit (same edges + skip).
    ret = build_tip_weave_tunable(&wtgt, path, ext, &tgt_h40, 1,
                                  edges, skip_hl, nskip, tgt_ids,
                                  &tgt_no_hist);
    if (ret != OK) goto cleanup;

    if (base_no_hist || tgt_no_hist) { ret = GRAFFAIL; goto cleanup; }

    //  Empty-side degeneracy.
    b8 base_empty = WEAVEEmpty(wcur);
    b8 tgt_empty  = WEAVEEmpty(&wtgt);
    if (base_empty && tgt_empty) { ret = GRAFFAIL; goto cleanup; }
    if (base_empty) { ret = emit_alive_bytes(out, &wtgt); goto cleanup; }
    if (tgt_empty)  { ret = emit_alive_bytes(out, wcur);  goto cleanup; }

    //  Single weave: apply tgt's tip content onto the base weave as ONE
    //  edit, diffed against the tgt-closure (common-ancestor) baseline.
    //  tgt's net change lands as GET_TGT_SRC tokens; base's divergent
    //  tokens pass through untouched.  No second weave to reconcile.
    merge_id_set base_set = {
        .ids = (u32cp)u32bDataHead(base_ids),
        .n   = (u32)u32bDataLen(base_ids),
    };
    merge_id_set tgt_set = {
        .ids = (u32cp)u32bDataHead(tgt_ids),
        .n   = (u32)u32bDataLen(tgt_ids),
    };
    {
        ret = emit_alive_bytes(tgt_blob, &wtgt);     // tgt tip content
        if (ret != OK) goto cleanup;
        a_dup(u8c, tgt_data, u8bData(tgt_blob));
        ret = WEAVEFromBlob(&wnu, tgt_data, ext, GET_TGT_SRC);
        if (ret == OK)
            ret = WEAVEApply(&wmerge, wcur, &wnu, GET_TGT_SRC,
                             merge_id_set_has, &tgt_set);
    }
    if (ret != OK) goto cleanup;

    //  Render with conflict markers: base side by closure membership, tgt
    //  side by the sentinel or any tgt-closure commit.
    WEAVEsetfn preds[2] = { merge_id_set_has, get_tgt_pred };
    void *ctxs[2] = { &base_set, &tgt_set };

    ret = WEAVEEmitMerged(&wmerge, preds, ctxs, 2, out);

cleanup:
    if (tgt_blob[0]) u8bUnMap(tgt_blob);
    if (tgt_ids[0])  u32bUnMap(tgt_ids);
    if (base_ids[0]) u32bUnMap(base_ids);
    WEAVEFree(&wmerge);
    WEAVEFree(&wtgt);
    WEAVEFree(&wnu);
    WEAVEFree(&wbase_wt);
    WEAVEFree(&wbase);
    return ret;
}

ok64 GRAFMergeWtFile(u8cs path, u8cs reporoot,
                     sha1cp base, sha1cp tgt,
                     u8b out) {
    //  Default to parent-only reachability — historic shape, kept so
    //  call sites that haven't migrated to the tunable variant get
    //  identical behaviour.  PATCH.c uses the tunable form directly
    //  with `parent | foster` to handle absorbed-via-foster history.
    return GRAFMergeWtFileTunable(path, reporoot, base, tgt,
                                  DAG_EDGE_PARENT, NULL, 0, out);
}

// --- Blob-only 3-way WEAVE merge ----------------------------------
//
//  Three-way merge from raw blob bytes (no keeper / no DAG walk).
//  Pipeline:  WEAVEFromBlob ×3 → WEAVEDiff ×2 → WEAVEMerge →
//  WEAVEEmitMerged.  Marker shape is the WEAVE convention
//  (`<<<<` / `||||` / `>>>>`), with the 1/4-line realignment pass
//  applied automatically by WEAVEEmitMerged.
//
//  Empty `base` is allowed (no common ancestor); empty `ours` or
//  `theirs` short-circuits to the other side's bytes.  `out` is
//  reset on entry.

#define MERGE3_BASE_SRC   0u           // spine (WEAVEEmitMerged: in==0)
#define MERGE3_OURS_SRC   0xA5A5A5A5u
#define MERGE3_THEIRS_SRC 0x5A5A5A5Au

static b8 merge3_pred(u32 in, void *vctx) {
    return in == *(u32 *)vctx;
}

//  Baseline predicate for ours/theirs: each diffs against the BASE alone.
static b8 merge3_base_pred(u32 seq, void *vctx) {
    (void)vctx;
    return seq == MERGE3_BASE_SRC;
}

//  Apply `content` (stamped `seq`) to `*W`, ping-ponging into `*Wn`.
//  `base` is the baseline predicate (NULL ⇒ empty baseline / root).
static ok64 merge3_apply(weave **W, weave **Wn, weave *nu,
                         u8cs content, u8cs ext, u32 seq, WEAVEsetfn base) {
    sane(W && Wn && nu);
    call(WEAVEFromBlob, nu, content, ext, seq);
    call(WEAVEApply, *Wn, *W, nu, seq, base, NULL);
    weave *t = *W; *W = *Wn; *Wn = t;
    done;
}

ok64 GRAFMerge3Bytes(u8cs base, u8cs ours, u8cs theirs,
                     u8cs ext, u8b out) {
    sane(out);
    u8bReset(out);

    //  Empty-side degeneracies — match get_merge_2way's prior shape.
    if ($empty(ours) && $empty(theirs)) return OK;
    if ($empty(ours))  return u8bFeed(out, theirs);
    if ($empty(theirs)) return u8bFeed(out, ours);

    //  Replay base → ours → theirs into ONE weave (insert/delete in
    //  place, never reconcile two weaves).  ours and theirs each diff
    //  against the BASE view; their tokens coexist in the one weave and
    //  WEAVEEmitMerged frames divergent regions by per-side membership.
    weave w0 = {}, w1 = {}, nu = {};
    ok64 ret = OK;
    if ((ret = WEAVEInit(&w0)) != OK) return ret;
    if ((ret = WEAVEInit(&w1)) != OK) goto cleanup;
    if ((ret = WEAVEInit(&nu)) != OK) goto cleanup;
    weave *W = &w0, *Wn = &w1;

    if ((ret = merge3_apply(&W, &Wn, &nu, base,   ext, MERGE3_BASE_SRC, NULL)) != OK)
        goto cleanup;
    if ((ret = merge3_apply(&W, &Wn, &nu, ours,   ext, MERGE3_OURS_SRC,
                            merge3_base_pred)) != OK) goto cleanup;
    if ((ret = merge3_apply(&W, &Wn, &nu, theirs, ext, MERGE3_THEIRS_SRC,
                            merge3_base_pred)) != OK) goto cleanup;

    u32 ours_src = MERGE3_OURS_SRC, theirs_src = MERGE3_THEIRS_SRC;
    WEAVEsetfn preds[2] = { merge3_pred, merge3_pred };
    void *ctxs[2]       = { &ours_src, &theirs_src };
    ret = WEAVEEmitMerged(W, preds, ctxs, 2, out);

cleanup:
    WEAVEFree(&nu);
    WEAVEFree(&w1);
    WEAVEFree(&w0);
    return ret;
}

// --- Weave-replay helpers: shared by N-tip union and 2-way merge ---

//  One replay step (GET-001): tokenize `new_data` into `wnu`, then diff
//  it onto the carried `*src_dec` (the accumulator's PERSISTENT decode)
//  to produce `*dst` (TLV) AND `*dst_dec` (its decode, captured as the
//  core emits).  Invoked under a `try()` boundary so the per-version
//  scratch — `wnu`'s decode plus the diff's NEIL/EDL buffers, all BACQ'd
//  off the version-sized TLVs — is rewound each iteration; that bounds
//  transient BASS and avoids the BNOROOM arena overflow.  Crucially we
//  do NOT re-decode the accumulator from its growing TLV each step: the
//  carried `src_dec`/`dst_dec` (heap-backed, survive the rewind) make
//  the replay's decode cost linear in total history instead of
//  quadratic.  `*dst` (TLV) stays the durable per-step output the caller
//  swaps into place; `wnu`'s own mmap also survives the rewind.
static ok64 build_weave_step(weave *dst, weavedec *dst_dec,
                            weavedec const *src_dec, weave *wnu,
                            u8cs new_data, u8cs ext, u32 sc) {
    sane(dst && dst_dec && src_dec && wnu);
    call(WEAVEFromBlob, wnu, new_data, ext, sc);
    call(WEAVEDiffCarry, dst, dst_dec, src_dec, wnu, sc);
    done;
}

//  Build a weave by replaying `path`'s blob versions across the
//  ancestor union of `tip_hs[0..ntips)` in topo order.  The result
//  weave's inrm carries provenance per token; alive tokens reproduce
//  the path's content at the most recent ancestor where it was last
//  written.  Caller owns `out` (must be inited and reset).
//
//  When `out_ids` is non-NULL, every 32-bit `sc` value passed to
//  `WEAVEDiffCarry` is appended to `*out_ids` in walk order.  Callers
//  driving `WEAVEEmitMerged` use these to build per-side membership
//  predicates over token `inrm.in` values.
static ok64 build_tip_weave_tunable(weave *out, u8cs path, u8cs ext,
                                    u64 const *tip_hs, u32 ntips,
                                    u32 edges,
                                    u64 const *skip_hl, u32 nskip,
                                    Bu32 out_ids, b8 *out_no_history) {
    sane(out && ntips > 0);
    if (out_no_history) *out_no_history = NO;

    //  Ancestor union across the supplied tips with the caller's
    //  edge-kind selector + skip set.  edges = DAG_EDGE_PARENT
    //  reproduces the legacy behaviour exactly.
    a_carve(wh128, anc, GET_ANC_SIZE);
    zerob(anc);  // hash set — must be zero-init
    wh128css runs = {NULL, NULL};
    GRAFRuns(runs);
    call(DAGAncestorsOfManyTunable, anc, runs, tip_hs, ntips,
         edges, skip_hl, nskip);

    //  Topo-sort using the SAME edge bitmask as the ancestor walk so
    //  foster-attached commits sit before the carrying commit in the
    //  replay order.  Without that, ours's WEAVEDiff stamps the
    //  attached commit's tokens with the carrying commit's sc, and
    //  WEAVEMerge can't align them with theirs's still-original-sc
    //  tokens.  Picked targets aren't given an ordering edge (they're
    //  reachability-set leaves with no replay step).
    u32 nvers = 0;
    u64 *vers = NULL;
    size_t anc_cap = (size_t)(wh128bTerm(anc) - wh128bHead(anc));
    Bu8 ord_buf = {};
    if (anc_cap > 0 && u8bAcquire(ABC_BASS, ord_buf, anc_cap * sizeof(u64)) == OK) {
        u64 *ordered = (u64 *)u8bDataHead(ord_buf);
        u32 topo_edges = edges & ~DAG_EDGE_PICKED;
        u32 nord = DAGTopoSortTunable(ordered, (u32)anc_cap, anc, runs,
                                      topo_edges);
        if (nord > GET_MAX_VERS) nord = GET_MAX_VERS;
        vers = ordered;
        nvers = nord;
    }

    //  No DAG entries: the tip's ancestry is not indexed in graf yet
    //  (commits landed in keeper packs via a wire push / `be patch` /
    //  sub fetch that didn't reindex graf, or a fresh import without
    //  GRAFIndex).  A FF-weave run here would lose every intermediate
    //  commit's edit — the multi-version replay below never happens —
    //  and silently emit only the tip's net bytes.  Signal the caller
    //  so it can INDEX the commits and RETRY (DIS-041); fall back to
    //  the tip's own blob bytes as a single-version weave only so an
    //  isolated-blob caller that can't index still gets something.
    if (nvers == 0) {
        if (out_no_history) *out_no_history = YES;
        a_carve(u8, fallback, GET_BLOB_MAX);
        ok64 fo = get_append_blob_at(fallback, tip_hs[0], path);
        if (fo == OK) {
            a_dup(u8c, fb, u8bData(fallback));
            fo = WEAVEFromBlob(out, fb, ext, (u32)tip_hs[0]);
        }
        return fo;
    }

    //  Carve blob scratch on BASS BEFORE WEAVEInit so an a_carve early-
    //  return can't leak weave mmaps.
    a_carve(u8, blob_a, GET_BLOB_MAX);
    a_carve(u8, blob_b, GET_BLOB_MAX);
    Bu8 *cur = &blob_a, *prev = &blob_b;

    //  Per-step state.  `wsrc`/`wdst` double-buffer the accumulator TLV
    //  (the last one becomes `out`); `dec_src`/`dec_dst` double-buffer
    //  the accumulator's PERSISTENT decode, carried across steps so it
    //  is never re-parsed from the growing TLV (GET-001 — the quadratic
    //  cost).  `wnu` holds each version's freshly tokenized blob.  After
    //  every step we swap BOTH pairs in lock-step.
    weave wA = {}, wB = {}, wnu = {};
    weavedec dA = {}, dB = {};
    ok64 r = OK;
    if ((r = WEAVEInit(&wA))     != OK) { goto out; }
    if ((r = WEAVEInit(&wB))     != OK) { WEAVEFree(&wA); goto out; }
    if ((r = WEAVEInit(&wnu))    != OK) { WEAVEFree(&wA); WEAVEFree(&wB); goto out; }
    if ((r = WEAVEDecInit(&dA))  != OK) { WEAVEFree(&wA); WEAVEFree(&wB); WEAVEFree(&wnu); goto out; }
    if ((r = WEAVEDecInit(&dB))  != OK) { WEAVEDecFree(&dA); WEAVEFree(&wA); WEAVEFree(&wB); WEAVEFree(&wnu); goto out; }
    weave    *wsrc = &wA, *wdst = &wB;
    weavedec *dec_src = &dA, *dec_dst = &dB;

    b8 have_prev = NO;
    for (u32 i = 0; i < nvers; i++) {
        u64 commit_h = vers[i];
        u8bReset(*cur);
        ok64 fo = GRAFBlobAtCommit(*cur, commit_h, path);
        if (fo != OK) continue;

        if (have_prev) {
            a_dup(u8c, cur_data,  u8bDataC(*cur));
            a_dup(u8c, prev_data, u8bDataC(*prev));
            if (u8csEq(cur_data, prev_data)) continue;
        }

        a_dup(u8c, new_data, u8bDataC(*cur));
        u32 sc = (u32)commit_h;
        //  Run the FromBlob+DiffCarry pair under a `try()` so the per-
        //  version BASS scratch (wnu decode + diff buffers) is rewound
        //  each iteration; the wnu/wsrc/wdst weaves and the heap-backed
        //  dec_src/dec_dst carried decodes persist.  See
        //  build_weave_step / GET-001.
        try(build_weave_step, wdst, dec_dst, dec_src, &wnu, new_data, ext, sc);
        if (__ == OK) {
            weave    *wtmp = wsrc; wsrc = wdst; wdst = wtmp;
            weavedec *dtmp = dec_src; dec_src = dec_dst; dec_dst = dtmp;
            if (out_ids[0]) (void)u32bFeed1(out_ids, sc);
        }

        Bu8 *tmp = cur; cur = prev; prev = tmp;
        have_prev = YES;
    }

    //  Move wsrc's contents into out (caller's buffer).  Cheapest
    //  route: copy the buffer header; wsrc's mappings now belong to
    //  out, and we zero wsrc before freeing so we don't double-free.
    //  Release any mapping `out` already holds first — the
    //  index-and-retry path (DIS-041) calls us twice on the same
    //  weave, and an unmapped-over header would leak the prior tlv.
    if (out->tlv[0]) u8bUnMap(out->tlv);
    if (wsrc == &wA) {
        memcpy(out, &wA, sizeof(weave));
        zero(wA);
    } else {
        memcpy(out, &wB, sizeof(weave));
        zero(wB);
    }

    WEAVEDecFree(&dA);
    WEAVEDecFree(&dB);
    WEAVEFree(&wA);
    WEAVEFree(&wB);
    WEAVEFree(&wnu);
out:
    return r;
}

//  Render `w`'s alive tokens into `into` in weave order.
static ok64 emit_alive_bytes(u8b into, weave const *w) {
    sane(into && w);
    call(WEAVEAliveBytes, w, into);
    done;
}


// --- Resolve commit_h40 + dir-path → tree object body ---
//
//  Mirrors GRAFBlobAtCommit but stops on the last path segment (or
//  the root tree if `path` is empty) instead of recursing into a
//  blob.  Emits the tree object's raw body into `into`.
static ok64 get_tree_at(u8b into, keeper *k, u64 commit_h40, u8cs path) {
    sane(into && k);

    Bu8 *cbuf = &GRAF.obj_buf;
    u8bReset(*cbuf);
    u8 ct = 0;
    ok64 o = KEEPGet(commit_h40, DAG_H60_HEXLEN, *cbuf, &ct);
    if (o != OK || ct != DOG_OBJ_COMMIT) return KEEPNONE;

    sha1 cur = {};
    o = GITu8sCommitTree(u8bDataC(*cbuf), cur.data);
    if (o != OK) return KEEPNONE;

    call(GRAFPathDescend, &cur, path);

    u8 ot = 0;
    call(KEEPGetExact, &cur, into, &ot);
    if (ot != DOG_OBJ_TREE) fail(KEEPFAIL);
    done;
}

// --- Public entry ---
//
//  Single-tip blob/tree read.  Multi-tip merge URIs (`path?A&B...`)
//  are no longer accepted — merge is PATCH territory.  Callers in
//  need of a 3-way merge use `GRAFMergeWtFileTunable` (DAG-aware,
//  takes commit shas) directly.

ok64 GRAFGet(u8b into, u8csc uri) {
    sane(into && uri);

    u8cs path = {};
    get_tip tips[GET_MAX_TIPS] = {};
    u32 ntips = 0;
    b8 is_tree = NO;
    a_dup(u8c, uri_in, uri);
    call(get_drain_uri, path, tips, &ntips, GET_MAX_TIPS,
         &is_tree, uri_in);

    if (ntips == 0)  return GETNOTIPS;
    if (ntips != 1)  return GETBAD;   // multi-tip merge URIs retired

    if (is_tree) {
        //  Strip the trailing '/' so the path reads as a dir name.
        if ($len(path) > 0 && path[1][-1] == '/') path[1]--;
        return get_tree_at(into, &KEEP, tips[0].h40, path);
    }

    return get_append_blob_at(into, tips[0].h40, path);
}
