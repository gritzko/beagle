//  DAG: graf's commit-graph index, streaming ingest.
//
//  Fed via GRAFDagUpdate one COMMIT object at a time (TREE/BLOB
//  callbacks are accepted but ignored — only commit→parent and
//  commit→tree edges are recorded).  Finish flushes the pending
//  batch and triggers compaction.  No historical keeper lookups.
//
//  Layout:
//      .be/0000000001.idx   sorted wh128 runs (LSM)
//
#include "DAG.h"
#include "GRAF.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/git/SHA1.h"
#include "dog/git/GIT.h"
#include "abc/HEX.h"
#include "keeper/KEEP.h"   // DIS-038: commit-body fallback for the ancestor walk

// Resolve a 40-bit object hashlet.  Prefer the caller-supplied SHA
// (the UNPK hot path has it) — falls back to computing it from the
// object body for callers that don't (e.g. `graf index`'s manual
// reindex walk at graf/INDEX.c).
static u64 dag_obj_hashlet(u8 obj_type, sha1cp sha, u8cs body) {
    if (sha) return WHIFFHashlet60(sha);

    char hdr[32];
    char const *tn = "blob";
    switch (obj_type) {
        case DOG_OBJ_COMMIT: tn = "commit"; break;
        case DOG_OBJ_TREE:   tn = "tree";   break;
        case DOG_OBJ_BLOB:   tn = "blob";   break;
        case DOG_OBJ_TAG:    tn = "tag";    break;
    }
    int hlen = snprintf(hdr, sizeof(hdr), "%s %zu",
                        tn, (size_t)u8csLen(body));
    if (hlen < 0 || (size_t)hlen >= sizeof(hdr)) return 0;

    SHA1state st;
    SHA1Open(&st);
    u8cs hs = {(u8cp)hdr, (u8cp)hdr + hlen + 1};  // include trailing NUL
    SHA1Feed(&st, hs);
    SHA1Feed(&st, body);
    sha1 out = {};
    SHA1DCFinal(out.data, &st);
    return WHIFFHashlet60(&out);
}

// --- Template instantiations for wh128 (sort, merge, hash).
// Bx.h already instantiated via dog/WHIFF.h.
#define X(M, name) M##wh128##name
#include "abc/QSORTx.h"
#include "abc/HITx.h"
#include "abc/HASHx.h"
#undef X

// --- Constants ---

#define DAG_DIR         DOG_BE_NAME
#define GRAF_IDX_EXT    ".graf.idx"
#define DAG_SEQNO_W     10
#define DAG_BATCH       (1 << 22)   // 4M entries (64 MB) per flush

// --- Ingest state (opaque to callers) ---

struct dag_ingest {
    Bwh128  batch;          // emit buffer (typed); DataLen == queued, IdleLen == room
    u8      finished;
};

// --- LSM file I/O ---
//
//  Reads come from `GRAF.puppies` (set up by `GRAFOpenBranch` walking
//  trunk → … → leaf).  Writes go to the leaf branch dir via
//  `DOGPupCreate`; compaction uses `DOGPupThinTail` + `DOGPupCreate`
//  (matches keeper's `KEEPCompact`).  No more hand-rolled
//  `<seqno>.idx` scanning here — the puppy stack is the source of
//  truth, and `GRAFRefreshView` keeps the typed `wh128cs` view in
//  sync.

//  Compose the flat project shard dir `<root>/.be/<project>` and feed
//  it into `out` (NUL-terminated).  Flat store: one shard per project,
//  so the leaf branch never gets its own subdir.  Mirrors
//  `graf_branch_dir`, `keep_branch_dir` and `spot_branch_dir`.
static ok64 graf_leaf_dir(path8b out, home *h, u8cs leaf_branch) {
    sane(h && $ok(leaf_branch) && out);
    //  `<root>/.be/<project>` via the single store-dir composer (GET-004;
    //  honors *.be-is-store, drops a `.be` project).  Empty `h->project`
    //  collapses to the legacy single-project shape (bare `.be`).  Without
    //  the project segment graf would publish its idx into `<root>/.be/`
    //  while keeper wrote packs into `<root>/.be/<project>/` — out of sync.
    //  Flat store: one shard per project; the leaf branch never gets its
    //  own subdir — mirrors `graf_branch_dir` and `HOMEBranchDir`.
    a_dup(u8c, proj, u8bDataC(h->project));
    call(HOMEBeDir, h, proj, out);
    (void)leaf_branch;
    done;
}

//  Append `run` as a new puppy under the leaf branch dir.  Refreshes
//  GRAF's typed view so subsequent lookups see the new run.
static ok64 dag_index_write_leaf(graf *g, wh128cs run) {
    sane(g);
    if ($empty(run)) done;
    a_pad(u8, leafdir, FILE_PATH_MAX_LEN);
    a_dup(u8c, leaf, u8bDataC(g->h->cur_branch));
    call(graf_leaf_dir, leafdir, g->h, leaf);
    call(FILEMakeDirP, $path(leafdir));
    a_cstr(ext, GRAF_IDX_EXT);
    size_t bytes = $len(run) * sizeof(wh128);
    u8cs data = {(u8cp)run[0], (u8cp)run[0] + bytes};
    call(GRAFPupCreateNext, $path(leafdir), ext, data);
    GRAFRefreshView();
    done;
}

// --- Graph-navigation primitives ---

ok64 DAGRange(wh128css hits, wh128css runs, wh64 key) {
    sane(hits);
    a_dup(wh128cs, scan, runs);
    $for(wh128cs, run, scan) {
        size_t len = wh128csLen(*run);
        size_t lo  = 0, hi = len;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (wh128csAtP(*run, mid)->key < key) lo = mid + 1;
            else hi = mid;
        }
        size_t end = lo;
        while (end < len && wh128csAtP(*run, end)->key == key) end++;
        if (end > lo) {
            a_rest(wh128c, from_lo, *run, lo);
            a_head(wh128c, hit, from_lo, end - lo);
            if (wh128cssFeed1(hits, hit) != OK) return DAGNOROOM;
        }
    }
    done;
}

u64 DAGCommitTree(wh128css runs, u64 commit_h) {
    wh128cs slots[MSET_MAX_LEVELS] = {};
    wh128css hits = {slots, slots + MSET_MAX_LEVELS};
    wh128cs *base = hits[0];
    if (DAGRange(hits, runs, DAGPack(DAG_T_COMMIT, commit_h)) != OK) return 0;
    for (wh128cs *r = base; r < hits[0]; r++) {
        for (wh128cp e = (*r)[0]; e < (*r)[1]; e++) {
            if (DAGType(e->val) == DAG_T_TREE) return DAGHashlet(e->val);
        }
    }
    return 0;
}

ok64 DAGParents(wh128css index, wh64s parents, wh64 commit_h) {
    sane(parents);
    wh128cs slots[MSET_MAX_LEVELS] = {};
    wh128css hits = {slots, slots + MSET_MAX_LEVELS};
    wh128cs *base = hits[0];
    call(DAGRange, hits, index, commit_h);
    for (wh128cs *r = base; r < hits[0]; r++) {
        for (wh128cp e = (*r)[0]; e < (*r)[1]; e++) {
            if (DAGType(e->val) != DAG_T_COMMIT) continue;
            if (wh64sFeed1(parents, e->val) != OK) return DAGNOROOM;
        }
    }
    done;
}

ok64 dag_anc_put(Bwh128 set, u64 commit_h) {
    wh128 rec = {.key = DAGPack(0, commit_h), .val = 0};
    wh128s tab = {wh128bHead(set), wh128bTerm(set)};
    return HASHwh128Put(tab, &rec);
}

b8 DAGAncestorsHas(wh128b set, u64 commit_h) {
    wh128 probe = {.key = DAGPack(0, commit_h), .val = 0};
    wh128s tab = {wh128bHead(set), wh128bTerm(set)};
    return HASHwh128Get(&probe, tab) == OK;
}

ok64 DAGAncestors(Bwh128 set, wh128css runs, u64 tip) {
    return DAGAncestorsTunable(set, runs, tip, DAG_EDGE_PARENT, NULL, 0);
}

ok64 DAGEdgesOf(wh128css runs, u64 commit_h, u8 kind,
                u64 *out, u32 cap, u32 *nout) {
    sane(out && nout);
    *nout = 0;
    if (commit_h == 0) done;
    wh128cs slots[MSET_MAX_LEVELS] = {};
    wh128css hits = {slots, slots + MSET_MAX_LEVELS};
    wh128cs *base = hits[0];
    call(DAGRange, hits, runs, DAGPack(DAG_T_COMMIT, commit_h));
    for (wh128cs *r = base; r < hits[0]; r++) {
        for (wh128cp e = (*r)[0]; e < (*r)[1]; e++) {
            if (DAGType(e->val) != kind) continue;
            if (*nout >= cap) return DAGNOROOM;
            out[(*nout)++] = DAGHashlet(e->val);
        }
    }
    done;
}

//  Linear-scan membership in a (small) skip array.
static b8 dag_in_skip(u64 const *skip_hl, u32 nskip, u64 h) {
    for (u32 i = 0; i < nskip; i++) {
        if (skip_hl[i] == h) return YES;
    }
    return NO;
}

ok64 DAGAncestorsTunable(Bwh128 set, wh128css runs, u64 tip,
                         u32 edges,
                         u64 const *skip_hl, u32 nskip) {
    sane(set);
    if (tip == 0) done;
    if (edges == 0) {
        //  No edges to traverse — still seed the tip if not skipped.
        if (!dag_in_skip(skip_hl, nskip, tip)) dag_anc_put(set, tip);
        done;
    }
    if (dag_in_skip(skip_hl, nskip, tip)) done;

    size_t cap = (size_t)(wh128bTerm(set) - wh128bHead(set));
    if (cap == 0) return DAGFAIL;

    Bwh128 queue = {};
    call(wh128bMap, queue, cap);

    //  DIS-038: scratch for the commit-body parent fallback below —
    //  used only when a commit has no parent edges in the DAG index.
    //  MEM-040: acquire via *bAcquire (not a_carve) so a BNOROOM here
    //  unmaps `queue` before returning instead of leaking the mmap —
    //  a_carve would `return __` straight past the wh128bUnMap below.
    Bu8 dis038_body = {};
    {
        ok64 ba = u8bAcquire(ABC_BASS, dis038_body, 1UL << 16);
        if (ba != OK) { wh128bUnMap(queue); return ba; }
    }

    dag_anc_put(set, tip);
    wh128 q0 = { .key = DAGPack(0, tip), .val = 0 };
    wh128bFeed1(queue, q0);

    size_t head = 0;
    u64 nbuf[16];
    //  Set when the ancestor set's probe line / BFS queue overflows.  A
    //  truncated walk yields a SILENTLY-WRONG ancestor set (bad ahead/
    //  behind, blame, dedup), so we surface DAGNOROOM to the caller
    //  instead of swallowing the overflow.  No stdio here (see file head).
    b8 overflow = NO;
    while (head < wh128bDataLen(queue)) {
        wh128cp cur = wh128bDataHead(queue) + head;
        u64 c = DAGHashlet(cur->key);
        head++;

        //  Helper closure: try to add `nh` to the set.  When `traverse`
        //  is YES, also enqueue for further BFS expansion.  Skip-set
        //  entries are dropped silently and no traversal happens
        //  through them.  On a set/queue overflow set `overflow` and
        //  stop expanding this node — the loop exits and we return
        //  DAGNOROOM rather than a silently-partial set.
        #define DAG_TUN_VISIT(nh, traverse) do {                    \
            u64 _h = (nh);                                          \
            if (dag_in_skip(skip_hl, nskip, _h)) break;             \
            if (DAGAncestorsHas(set, _h)) break;                    \
            if (dag_anc_put(set, _h) != OK) { overflow = YES; break; } \
            if (traverse) {                                         \
                wh128 _qr = { .key = DAGPack(0, _h), .val = 0 };    \
                if (wh128bFeed1(queue, _qr) != OK) { overflow = YES; break; } \
            }                                                       \
        } while (0)

        if (edges & DAG_EDGE_PARENT) {
            u32 nn = 0;
            if (DAGEdgesOf(runs, c, DAG_T_COMMIT, nbuf, 16, &nn) == OK
                && nn > 0) {
                for (u32 i = 0; i < nn; i++) DAG_TUN_VISIT(nbuf[i], YES);
            } else {
                //  DIS-038: no parent edges indexed for this commit — a
                //  freshly-fetched, not-yet-graf-indexed tip (or a true
                //  root).  Fall back to the commit body in keeper and
                //  parse its `parent <40hex>` lines, traversing each, so
                //  reachability is self-sufficient on fetched history
                //  (mirrors graf log's DAG-miss fallback, graf/LOG.c).  A
                //  real root simply yields no `parent` line; a KEEPGet
                //  miss (no keeper / absent object) leaves the node a
                //  leaf — no worse than the pre-fix bottom-out.
                u8bReset(dis038_body);
                u8 ot = 0;
                if (KEEPGet(c, DAG_H60_HEXLEN, dis038_body, &ot) == OK
                    && ot == DOG_OBJ_COMMIT) {
                    a_dup(u8c, scan, u8bData(dis038_body));
                    u8cs field = {}, value = {};
                    while (GITu8sDrainCommit(scan, field, value) == OK) {
                        if ($empty(field)) break;
                        a_cstr(par_kw, "parent");
                        if (u8csEq(field, par_kw) && u8csLen(value) >= 40) {
                            sha1 par_sha = {};
                            u8s  bin = {par_sha.data, par_sha.data + 20};
                            u8cs hx  = {value[0], value[0] + 40};
                            if (HEXu8sDrainSome(bin, hx) == OK) {
                                u64 ph = WHIFFHashlet60(&par_sha);
                                DAG_TUN_VISIT(ph, YES);
                                if (overflow) break;
                            }
                        }
                    }
                }
            }
        }
        if (edges & DAG_EDGE_FOSTER) {
            u32 nn = 0;
            if (DAGEdgesOf(runs, c, DAG_T_FOSTER, nbuf, 16, &nn) == OK) {
                //  Foster targets traverse fully — they're real
                //  ancestor commits absorbed into cur's history,
                //  just under a non-standard header name.
                for (u32 i = 0; i < nn; i++) DAG_TUN_VISIT(nbuf[i], YES);
            }
        }
        if (edges & DAG_EDGE_PICKED) {
            u32 nn = 0;
            if (DAGEdgesOf(runs, c, DAG_T_PICKED, nbuf, 16, &nn) == OK) {
                //  picked targets are leaves — added to the set but
                //  NOT enqueued.  Per spec picked is dedup-only and
                //  doesn't transitively pull in the picked commit's
                //  own ancestors.
                for (u32 i = 0; i < nn; i++) DAG_TUN_VISIT(nbuf[i], NO);
            }
        }

        #undef DAG_TUN_VISIT
        if (overflow) break;
    }

    wh128bUnMap(queue);
    if (overflow) return DAGNOROOM;
    done;
}

ok64 DAGAncestorsOfManyTunable(Bwh128 set, wh128css runs,
                               u64 const *tips, u32 ntips,
                               u32 edges,
                               u64 const *skip_hl, u32 nskip) {
    sane(set);
    for (u32 i = 0; i < ntips; i++) {
        if (tips[i] == 0) continue;
        call(DAGAncestorsTunable, set, runs, tips[i],
             edges, skip_hl, nskip);
    }
    done;
}

ok64 DAGAncestorsOfMany(Bwh128 set, wh128css runs,
                        u64 const *tips, u32 n) {
    sane(set);
    for (u32 i = 0; i < n; i++) {
        if (tips[i] == 0) continue;
        call(DAGAncestors, set, runs, tips[i]);
    }
    done;
}

ok64 DAGAllCommits(Bwh128 set, wh128css runs) {
    sane(set);
    //  Each commit has exactly one (COMMIT, TREE) edge — use it as a
    //  unique-per-commit witness while skipping (COMMIT, COMMIT)
    //  parent edges and the new (TREE, *) child edges.
    a_dup(wh128cs, scan, runs);
    $for(wh128cs, run, scan) {
        wh128cp base = (*run)[0];
        wh128cp end  = (*run)[1];
        for (wh128cp p = base; p < end; p++) {
            if (DAGType(p->key) != DAG_T_COMMIT) continue;
            if (DAGType(p->val) != DAG_T_TREE)   continue;
            dag_anc_put(set, DAGHashlet(p->key));
        }
    }
    done;
}

// --- Topological sort over a hashlet set ---
//
//  Iterative DFS post-order: descend into parents that are inside
//  `set`; emit a commit when all its in-set parents have been emitted.
//  Result: parents-before-children for arbitrary topology, no gen field
//  required.

#define DAG_TOPO_MAX_PARENTS 16

typedef struct {
    u64 c;
    u32 par_i;       // next parent slot to explore
    u32 npar;
    u64 pars[DAG_TOPO_MAX_PARENTS];
} topo_frame;

//  Thin wrapper: DAGParents (wh64s feed) → u64[] of hashlets, the
//  shape topo_frame stores.  Capped at `cap` slots.
static u32 topo_parents_of(wh128css runs, u64 commit_h,
                           u64 *out, u32 cap) {
    wh64 buf[DAG_TOPO_MAX_PARENTS];
    wh64s parents = {buf, buf + DAG_TOPO_MAX_PARENTS};
    wh64 *base = parents[0];
    DAGParents(runs, parents, DAGPack(DAG_T_COMMIT, commit_h));
    u32 n = (u32)(parents[0] - base);
    if (n > cap) n = cap;
    for (u32 i = 0; i < n; i++) out[i] = DAGHashlet(base[i]);
    return n;
}

//  Edge-set-aware variant of `topo_parents_of`.  Concatenates targets
//  of each edge kind in `edges` into `out` (capped at `cap`): PARENT
//  edges first, then FOSTER.  Within each kind the targets are sorted
//  by hashlet so the DFS descent is reproducible regardless of the
//  LSM-scan order DAGEdgesOf returns.  The PARENT/FOSTER boundary is
//  preserved across the two segments — that ordering is causal (a
//  merge's first-parent spine is replayed before its foster subtree)
//  and must NOT be collapsed into a single by-hashlet sort, which is
//  what made the ancestor-closure weave replay non-deterministic
//  (see DAGTopoSortTunable + graf/GET.c::build_tip_weave_tunable).
static void topo_sort_u64(u64 *a, u32 n);   // fwd decl
static u32 topo_links_of(wh128css runs, u64 commit_h,
                         u32 edges,
                         u64 *out, u32 cap) {
    u32 n = 0;
    if (edges & DAG_EDGE_PARENT) {
        u32 nn = 0;
        if (DAGEdgesOf(runs, commit_h, DAG_T_COMMIT,
                       out + n, cap - n, &nn) == OK) {
            topo_sort_u64(out + n, nn);
            n += nn;
        }
    }
    if ((edges & DAG_EDGE_FOSTER) && n < cap) {
        u32 nn = 0;
        if (DAGEdgesOf(runs, commit_h, DAG_T_FOSTER,
                       out + n, cap - n, &nn) == OK) {
            topo_sort_u64(out + n, nn);
            n += nn;
        }
    }
    //  Picked targets are not topo-ordered: see DAGTopoSortTunable
    //  comment.  Even if DAG_EDGE_PICKED is in the bitmask, no edge
    //  is followed here.
    return n;
}

//  Stable u64 sort (insertion — set sizes are small in practice).
static void topo_sort_u64(u64 *a, u32 n) {
    for (u32 i = 1; i < n; i++) {
        u64 v = a[i];
        u32 j = i;
        while (j > 0 && a[j - 1] > v) { a[j] = a[j - 1]; j--; }
        a[j] = v;
    }
}

u32 DAGTopoSortTunable(u64 *out, u32 cap,
                       Bwh128 set, wh128css runs,
                       u32 edges) {
    if (cap == 0 || !out) return 0;
    //  No edges to follow → no ordering, just emit set members in
    //  array order (caller will see arbitrary order, fine for
    //  parent-less degenerate cases).
    if (edges == 0) edges = DAG_EDGE_PARENT;

    size_t set_cap = (size_t)(wh128bTerm(set) - wh128bHead(set));
    if (set_cap == 0) return 0;

    //  Non-sane'd helper (returns u32 count); called from inside the
    //  caller's call() frame, so BASS auto-rewinds at that boundary.
    //  `visited` is a hash set — zero-init mandatory (see graf/GET.c
    //  ::get_lca for the why).
    Bwh128 visited = {};
    if (wh128bAcquire(ABC_BASS, visited, set_cap) != OK) return 0;
    zerob(visited);

    Bu8 stk_buf = {};
    if (u8bAcquire(ABC_BASS, stk_buf, set_cap * sizeof(topo_frame)) != OK) return 0;
    topo_frame *stack = (topo_frame *)u8bDataHead(stk_buf);
    u32 stack_max = (u32)set_cap;

    //  Determinism: collect set members into a flat array, sort by
    //  hashlet, then iterate in sorted order.  Without this the outer
    //  DFS loop visits commits in hash-position order, which depends
    //  on the commit hashlets — different runs of the same scenario
    //  (which assign different ts → different commit shas) produce
    //  different topo orders → different WEAVE replays → different
    //  output bytes.  Stable hashlet order makes the merge reproducible.
    //  Mark every set member that is an edge-target (parent/foster) of
    //  another set member — those are NOT tips.  The DFS must start at
    //  the tips (heads), so the first-parent spine of a merge tip
    //  linearizes contiguously; starting at an interior commit would
    //  fragment the order.  `pointed` is a hash set (zero-init).
    Bwh128 pointed = {};
    if (wh128bAcquire(ABC_BASS, pointed, set_cap) != OK) return 0;
    zerob(pointed);
    {
        wh128cp set_head = wh128bHead(set);
        wh128cp set_term = wh128bTerm(set);
        for (wh128cp p = set_head; p < set_term; p++) {
            if (p->key == 0) continue;
            u64 c = DAGHashlet(p->key);
            u64 links[DAG_TOPO_MAX_PARENTS];
            u32 nl = topo_links_of(runs, c, edges, links, DAG_TOPO_MAX_PARENTS);
            for (u32 li = 0; li < nl; li++) {
                if (links[li] == 0) continue;
                if (DAGAncestorsHas(set, links[li]))
                    dag_anc_put(pointed, links[li]);
            }
        }
    }

    //  Roots in two strata: tips (not pointed-to) first, interior
    //  members after.  Each stratum is hashlet-sorted for a stable
    //  tie-break among genuinely independent heads.  Starting at tips
    //  makes the post-order replay follow real causal order rather
    //  than commit-sha order.
    Bu8 roots_buf = {};
    if (u8bAcquire(ABC_BASS, roots_buf, set_cap * sizeof(u64)) != OK) return 0;
    u64 *roots = (u64 *)u8bDataHead(roots_buf);
    u32 nroots = 0;
    {
        wh128cp set_head = wh128bHead(set);
        wh128cp set_term = wh128bTerm(set);
        u32 ntips = 0;
        for (wh128cp p = set_head; p < set_term; p++) {
            if (p->key == 0) continue;
            u64 c = DAGHashlet(p->key);
            if (!DAGAncestorsHas(pointed, c)) roots[nroots++] = c;
        }
        ntips = nroots;
        topo_sort_u64(roots, ntips);
        for (wh128cp p = set_head; p < set_term; p++) {
            if (p->key == 0) continue;
            u64 c = DAGHashlet(p->key);
            if (DAGAncestorsHas(pointed, c)) roots[nroots++] = c;
        }
        topo_sort_u64(roots + ntips, nroots - ntips);
    }

    u32 written = 0;

    for (u32 ri = 0; ri < nroots && written < cap; ri++) {
        u64 root = roots[ri];
        if (DAGAncestorsHas(visited, root)) continue;
        if (1 > stack_max) goto outta_room;

        u32 sp = 0;
        stack[sp].c = root;
        stack[sp].par_i = 0;
        stack[sp].npar = topo_links_of(runs, root, edges,
                                       stack[sp].pars,
                                       DAG_TOPO_MAX_PARENTS);
        if (stack[sp].npar > DAG_TOPO_MAX_PARENTS)
            stack[sp].npar = DAG_TOPO_MAX_PARENTS;
        //  Parent/foster targets are already kind-segmented and
        //  per-segment hashlet-sorted by topo_links_of; do NOT
        //  re-sort here (that would mix PARENT and FOSTER targets by
        //  hashlet and destroy the first-parent-before-foster order).
        sp++;
        dag_anc_put(visited, root);

        while (sp > 0) {
            topo_frame *t = &stack[sp - 1];
            b8 descended = NO;
            while (t->par_i < t->npar) {
                u64 par = t->pars[t->par_i++];
                if (par == 0) continue;
                if (!DAGAncestorsHas(set, par)) continue;
                if (DAGAncestorsHas(visited, par)) continue;
                if (sp >= stack_max) goto outta_room;

                stack[sp].c = par;
                stack[sp].par_i = 0;
                stack[sp].npar = topo_links_of(runs, par, edges,
                                               stack[sp].pars,
                                               DAG_TOPO_MAX_PARENTS);
                if (stack[sp].npar > DAG_TOPO_MAX_PARENTS)
                    stack[sp].npar = DAG_TOPO_MAX_PARENTS;
                //  Already kind-segmented + per-segment sorted by
                //  topo_links_of; no re-sort (preserves parent-first).
                sp++;
                dag_anc_put(visited, par);
                descended = YES;
                break;
            }
            if (!descended) {
                if (written < cap) out[written++] = t->c;
                sp--;
            }
        }
    }

outta_room:
    return written;
}

u32 DAGTopoSort(u64 *out, u32 cap,
                Bwh128 set, wh128css runs) {
    if (cap == 0 || !out) return 0;

    size_t set_cap = (size_t)(wh128bTerm(set) - wh128bHead(set));
    if (set_cap == 0) return 0;

    //  Non-sane'd helper; BASS auto-rewinds at caller's call() boundary.
    //  `visited` is a hash set — zero-init mandatory.
    Bwh128 visited = {};
    if (wh128bAcquire(ABC_BASS, visited, set_cap) != OK) return 0;
    zerob(visited);

    //  Stack capacity = set capacity is overkill but safe (a DFS stack
    //  is bounded by the longest chain in the subgraph, which never
    //  exceeds the number of nodes).
    Bu8 stk_buf = {};
    if (u8bAcquire(ABC_BASS, stk_buf, set_cap * sizeof(topo_frame)) != OK) return 0;
    topo_frame *stack = (topo_frame *)u8bDataHead(stk_buf);
    u32 stack_max = (u32)set_cap;

    u32 written = 0;
    wh128cp set_head = wh128bHead(set);
    wh128cp set_term = wh128bTerm(set);

    for (wh128cp p = set_head; p < set_term && written < cap; p++) {
        if (p->key == 0) continue;            // empty hash slot
        u64 root = DAGHashlet(p->key);
        if (DAGAncestorsHas(visited, root)) continue;
        if (1 > stack_max) goto outta_room;

        u32 sp = 0;
        stack[sp].c = root;
        stack[sp].par_i = 0;
        stack[sp].npar = topo_parents_of(runs, root, stack[sp].pars,
                                       DAG_TOPO_MAX_PARENTS);
        if (stack[sp].npar > DAG_TOPO_MAX_PARENTS)
            stack[sp].npar = DAG_TOPO_MAX_PARENTS;
        sp++;
        dag_anc_put(visited, root);

        while (sp > 0) {
            topo_frame *t = &stack[sp - 1];
            b8 descended = NO;
            while (t->par_i < t->npar) {
                u64 par = t->pars[t->par_i++];
                if (par == 0) continue;
                if (!DAGAncestorsHas(set, par)) continue;
                if (DAGAncestorsHas(visited, par)) continue;
                if (sp >= stack_max) goto outta_room;

                stack[sp].c = par;
                stack[sp].par_i = 0;
                stack[sp].npar = topo_parents_of(runs, par, stack[sp].pars,
                                                 DAG_TOPO_MAX_PARENTS);
                if (stack[sp].npar > DAG_TOPO_MAX_PARENTS)
                    stack[sp].npar = DAG_TOPO_MAX_PARENTS;
                sp++;
                dag_anc_put(visited, par);
                descended = YES;
                break;
            }
            if (!descended) {
                if (written < cap) out[written++] = t->c;
                sp--;
            }
        }
    }

outta_room:
    return written;
}

// --- Compaction (merges multiple runs when newer is large vs older) ---
//
//  Mirrors keeper's `KEEPCompact`: builds a typed `wh128cs[]` view
//  over GRAF.puppies, runs `HITwh128Compact`, then `DOGPupThinTail`
//  + `DOGPupCreate` against the leaf branch dir.  Refreshes the view
//  so subsequent lookups see the merged run.  No-op when the stack
//  already satisfies the 1/8 size-tiered invariant.
static ok64 dag_compact(graf *g) {
    sane(g);

    u32 nfiles = DOGPupCount(g->puppies);
    if (nfiles < 2) done;

    //  Build typed view from puppy data slices.
    wh128cs runs[MSET_MAX_LEVELS] = {};
    u32 nview = 0;
    for (u32 i = 0; i < nfiles && nview < MSET_MAX_LEVELS; i++) {
        u8cs raw = {};
        DOGPupData(raw, g->puppies, i);
        if (raw[0] == NULL) continue;
        runs[nview][0] = (wh128cp)raw[0];
        runs[nview][1] = (wh128cp)raw[1];
        nview++;
    }
    wh128css stack = {runs, runs + nview};

    if (HITwh128IsCompact(stack)) done;

    size_t total = 0;
    for (u32 i = 0; i < nview; i++)
        total += (size_t)(runs[i][1] - runs[i][0]);

    a_carve(wh128, cbuf, total);
    wh128 *base = cbuf[0];
    wh128s into = {cbuf[0], cbuf[3]};
    size_t before_len = $len(stack);
    call(HITwh128Compact, stack, into);
    size_t m = before_len - $len(stack) + 1;
    if (m < 2) done;

    a_pad(u8, leafdir, FILE_PATH_MAX_LEN);
    a_dup(u8c, leaf, u8bDataC(g->h->cur_branch));
    call(graf_leaf_dir, leafdir, g->h, leaf);
    a_cstr(ext, GRAF_IDX_EXT);
    u8cs merged = {(u8cp)base, (u8cp)(into[0])};
    call(DOGPupThinTail, g->puppies, $path(leafdir), ext, (u32)m);
    call(GRAFPupCreateNext, $path(leafdir), ext, merged);

    GRAFRefreshView();
    done;
}

// --- Ingest state management ---
//
//  Seqno is owned by the puppy stack — DOGPupCreate picks
//  max(seqno)+1 internally — so the ingest struct no longer carries
//  one.  `dagdir`/`dagdir_buf` are also gone: writes always land in
//  the leaf branch dir resolved on-demand from `GRAF.leaf_branch`.

static ok64 dag_ingest_alloc(dag_ingest **out) {
    sane(out);
    *out = NULL;

    dag_ingest *g = calloc(1, sizeof(*g));
    if (!g) return DAGFAIL;

    ok64 ao = wh128bAllocate(g->batch, DAG_BATCH);
    if (ao != OK) { free(g); return ao; }

    *out = g;
    done;
}

static void dag_ingest_free(dag_ingest *g) {
    if (!g) return;
    if (g->batch[0]) wh128bFree(g->batch);
    free(g);
}

// --- Emit helpers ---

static void dag_emit(dag_ingest *g,
                     u8 ktype, u64 khash,
                     u8 vtype, u64 vhash) {
    if (!wh128bHasRoom(g->batch)) return;  // overflow; handled by flush
    (void)wh128bFeed1(g->batch, DAGEntry(ktype, khash, vtype, vhash));
}

static ok64 dag_flush_batch(dag_ingest *g) {
    sane(g);
    if (!wh128bHasData(g->batch)) done;
    wh128bSort(g->batch);
    wh128bDedup(g->batch);
    a_dup(wh128c, run, wh128bDataC(g->batch));
    call(dag_index_write_leaf, &GRAF, run);
    wh128bReset(g->batch);
    //  Maintain the 1/8 LSM ladder right here, every flush.  Without
    //  this the puppy stack grows unboundedly during a long ingest,
    //  exceeds the runs[MSET_MAX_LEVELS] view cap, and older runs go
    //  silently invisible to both reads and the finish-time compact.
    call(dag_compact, &GRAF);
    done;
}

static ok64 dag_batch_maybe_flush(dag_ingest *g) {
    sane(g);
    if (wh128bIdleLen(g->batch) < 64) call(dag_flush_batch, g);
    done;
}

// --- Finish: flush pending records, compact runs. ---

static ok64 dag_finish(dag_ingest *g) {
    sane(g);
    if (g->finished) done;
    call(dag_flush_batch, g);
    dag_compact(&GRAF);
    g->finished = 1;
    done;
}

// ============================================================
// Public entry: GRAFDagUpdate
// ============================================================

// `state` is graf's own state (struct graf from GRAF.h).  We reach
// into state->ing to lazily allocate the ingest context.  Forward-
// decl of struct graf comes from GRAF.h include above.

ok64 GRAFDagUpdate(u8 obj_type, sha1cp sha, u8cs blob) {
    sane(1);
    graf *state = &GRAF;

    // Lazy allocate ingest state on first call.  Writes target the
    // leaf branch dir (resolved via GRAF.leaf_branch); no longer
    // carry a dagdir copy — `dag_index_write_leaf` re-derives it.
    if (!state->ing) {
        call(dag_ingest_alloc, &state->ing);
    }

    dag_ingest *g = state->ing;

    switch (obj_type) {
    case DOG_OBJ_COMMIT: {
        //  Parse headers: tree (mandatory), parents[], fosters[],
        //  pickeds[].  GITu8sDrainCommit walks line-by-line; an empty
        //  `field` row marks the header/body separator.  `picked` is a
        //  beagle-only header riding next to `foster` (after committer,
        //  before the blank line) — same wire shape, read identically.
        a_dup(u8c, scan, blob);
        u8cs field = {}, value = {};
        sha1 tree_sha = {};
        sha1 parents[16] = {};
        sha1 fosters[16] = {};
        sha1 pickeds[16] = {};
        u32 npar = 0, nfost = 0, npick = 0;
        b8 got_tree = NO;
        while (GITu8sDrainCommit(scan, field, value) == OK) {
            if (u8csEmpty(field)) break;
            if (u8csEq(field, GIT_FIELD_TREE) && u8csLen(value) >= 40) {
                DAGsha1FromHex(&tree_sha, (char const *)value[0]);
                got_tree = YES;
            } else if (u8csEq(field, GIT_FIELD_PARENT) && u8csLen(value) >= 40
                       && npar < 16) {
                DAGsha1FromHex(&parents[npar], (char const *)value[0]);
                npar++;
            } else if (u8csEq(field, GIT_FIELD_FOSTER) && u8csLen(value) >= 40
                       && nfost < 16) {
                DAGsha1FromHex(&fosters[nfost], (char const *)value[0]);
                nfost++;
            } else if (u8csEq(field, GIT_FIELD_PICKED) && u8csLen(value) >= 40
                       && npick < 16) {
                DAGsha1FromHex(&pickeds[npick], (char const *)value[0]);
                npick++;
            }
        }
        if (!got_tree) return DAGFAIL;

        u64 commit_h = dag_obj_hashlet(DOG_OBJ_COMMIT, sha, blob);

        u64 tree_h = WHIFFHashlet60(&tree_sha);

        //  (COMMIT, commit_h) → (TREE,   tree_h)    root-tree edge
        //  (COMMIT, commit_h) → (COMMIT, parent_h)  one per parent
        //  (COMMIT, commit_h) → (FOSTER, foster_h)  one per foster
        //  (COMMIT, commit_h) → (PICKED, picked_h)  one per picked header
        dag_emit(g, DAG_T_COMMIT, commit_h,
                    DAG_T_TREE,   tree_h);
        for (u32 i = 0; i < npar; i++) {
            u64 parent_h = WHIFFHashlet60(&parents[i]);
            dag_emit(g, DAG_T_COMMIT, commit_h,
                        DAG_T_COMMIT, parent_h);
        }
        for (u32 i = 0; i < nfost; i++) {
            u64 foster_h = WHIFFHashlet60(&fosters[i]);
            dag_emit(g, DAG_T_COMMIT, commit_h,
                        DAG_T_FOSTER, foster_h);
        }
        for (u32 i = 0; i < npick; i++) {
            u64 picked_h = WHIFFHashlet60(&pickeds[i]);
            dag_emit(g, DAG_T_COMMIT, commit_h,
                        DAG_T_PICKED, picked_h);
        }

        call(dag_batch_maybe_flush, g);
        done;
    }

    case DOG_OBJ_TREE:
    case DOG_OBJ_BLOB:
    default:
        done;  // tree/blob payloads carry no graph edges; commit→tree
               // is the only tree-side edge and it lives on the COMMIT
               // record.  Path resolution at query time goes through
               // keeper, not the LSM.
    }
}

ok64 GRAFDagFinish(void) {
    sane(1);
    graf *state = &GRAF;
    if (!state->ing) done;
    ok64 r = dag_finish(state->ing);
    dag_ingest_free(state->ing);
    state->ing = NULL;
    return r;
}
