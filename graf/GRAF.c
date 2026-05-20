#include "GRAF.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/DPATH.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"

// --- Producer-side staging state (legacy globals) ---
Bu8          graf_arena   = {};
int          graf_out_fd  = -1;
graf_emit_fn graf_emit    = NULL;

// --- Singleton ---

graf GRAF = {};

static b8 graf_is_open(void) { return GRAF.h != NULL; }
static b8 graf_is_rw = NO;

// --- GRAFOpenBranch / GRAFOpen / GRAFClose ---

#define GRAF_DIR_S       DOG_BE_NAME
#define GRAF_IDX_EXT     ".graf.idx"
#define GRAF_LOCK_S      ".lock.graf"
#define GRAF_LEAF_BRANCH_MAX 1024

//  Compose `<root>/.be[/<branch>]` (with `<branch>` empty for trunk).
//  `branch` is the canonical leaf-branch (trailing '/' if non-empty).
//  `out` is NUL-terminated.
static ok64 graf_branch_dir(path8b out, home *h, u8cs branch) {
    sane(h && out);
    u8bReset(out);
    a_dup(u8c, root_s, u8bDataC(h->root));
    call(PATHu8bFeed, out, root_s);
    a_cstr(rel, GRAF_DIR_S);
    call(PATHu8bAdd, out, rel);
    //  Project shard segment (project-sharded layout — see
    //  dog/DOG.h §"Canonical on-disk layout").  Empty `h->project`
    //  collapses to the legacy single-project shape.
    a_dup(u8c, proj, u8bDataC(h->project));
    if (!u8csEmpty(proj)) call(PATHu8bAdd, out, proj);
    if ($ok(branch) && !u8csEmpty(branch)) {
        a_dup(u8c, br, branch);
        //  Strip trailing '/' from canonical branch path before
        //  PATHu8bAdd (which inserts its own separator).
        if (!$empty(br) && *u8csLast(br) == '/') u8csShed1(br);
        if (!$empty(br)) call(PATHu8bAdd, out, br);
    }
    call(PATHu8bTerm, out);
    done;
}

//  YES iff `path` (NUL-terminated u8b) is an existing directory.
static b8 graf_dir_exists(path8s path) {
    filestat fs = {};
    if (FILEStat(&fs, path) != OK) return NO;
    return fs.kind == FILE_KIND_DIR;
}

//  Walk one branch path component at a time, calling `cb` per prefix
//  dir (trunk first → leaf last).  `cb` receives a freshly-built
//  NUL-terminated path slice for each prefix dir plus an `is_leaf`
//  flag (YES on the final call — trunk's call when `leaf` is empty,
//  otherwise the deepest segment's call).  Mirrors keeper's
//  `keep_walk_branch` so PAST/DATA boundary flipping happens in the
//  same place.  Stops at first non-OK return.
typedef ok64 (*graf_dir_cb)(graf *g, u8cs dir, b8 is_leaf, void0p ctx);

static ok64 graf_walk_branch(graf *g, u8cs leaf, graf_dir_cb cb,
                              void0p ctx) {
    sane(g && cb);
    a_pad(u8, gdir, FILE_PATH_MAX_LEN);
    a_dup(u8c, root_s, u8bDataC(g->h->root));
    call(PATHu8bFeed, gdir, root_s);
    a_cstr(rel, GRAF_DIR_S);
    call(PATHu8bAdd, gdir, rel);
    //  Project shard prefix — see graf_branch_dir's comment above.
    a_dup(u8c, proj, u8bDataC(g->h->project));
    if (!u8csEmpty(proj)) call(PATHu8bAdd, gdir, proj);
    call(PATHu8bTerm, gdir);

    //  Trunk first.  Empty leaf → trunk is the leaf.
    b8 trunk_is_leaf = u8csEmpty(leaf);
    {
        a_pad(u8, d, FILE_PATH_MAX_LEN);
        a_dup(u8c, gd, u8bDataC(gdir));
        call(PATHu8bFeed, d, gd);
        call(cb, g, $path(d), trunk_is_leaf, ctx);
    }
    if (trunk_is_leaf) done;

    //  Pre-scan: where does the last segment start?  We need the cb
    //  to know it's the leaf BEFORE the scan runs, so PAST/DATA can
    //  flip first.
    u8cp last_seg_start = leaf[0];
    for (u8cp p = leaf[0]; p < leaf[1]; p++)
        if (*p == '/' && p + 1 < leaf[1]) last_seg_start = p + 1;

    //  Each '/'-separated component, accumulating.
    a_pad(u8, d, FILE_PATH_MAX_LEN);
    a_dup(u8c, gd, u8bDataC(gdir));
    call(PATHu8bFeed, d, gd);
    u8cp p = leaf[0];
    u8cp seg_start = p;
    while (p <= leaf[1]) {
        b8 at_end = (p == leaf[1]);
        if (at_end || *p == '/') {
            if (p > seg_start) {
                u8cs seg = {seg_start, p};
                call(PATHu8bPush, d, seg);
                call(PATHu8bTerm, d);
                b8 is_leaf = (seg_start == last_seg_start);
                call(cb, g, $path(d), is_leaf, ctx);
                //  No `idle--`: PATHu8bPush writes its NUL via
                //  PATHu8bTerm WITHOUT advancing idle, so DATA is
                //  already at the correct end-of-segment.  Backing
                //  idle up would eat one byte of the segment (bug
                //  that surfaces on nested branches like
                //  `feature/fix`, see graf/GRAF.c history).
            }
            seg_start = p + 1;
        }
        p++;
    }
    done;
}

//  Filename → seqno for `<10-RON64>.graf.idx`.  Returns 0 on parse
//  failure or non-graf-idx file.
static u32 graf_filename_seqno(u8cs name) {
    static char const EXT[] = GRAF_IDX_EXT;
    static const size_t SEQ_W = 10;
    size_t n = u8csLen(name);
    if (n != SEQ_W + sizeof(EXT) - 1) return 0;
    char const *tail = (char const *)name[0] + SEQ_W;
    if (memcmp(tail, EXT, sizeof(EXT) - 1) != 0) return 0;
    u8cs seq_s = {name[0], name[0] + SEQ_W};
    ok64 v = 0;
    if (RONutf8sDrain(&v, seq_s) != OK) return 0;
    return (u32)v;
}

static ok64 graf_max_seqno_cb(void0p arg, path8p path) {
    u32 *max = (u32 *)arg;
    u8cs base = {};
    PATHu8sBase(base, u8bDataC(path));
    u32 sq = graf_filename_seqno(base);
    if (sq > *max) *max = sq;
    return OK;
}

//  Recursive scan over `<root>/.be/` for every `.graf.idx` file;
//  returns the global max seqno (default 0 when none found).  Used
//  by `graf_recompute_next_seqno` to keep fresh seqnos unique across
//  sibling branch dirs that aren't loaded into the registry.
static u32 graf_global_max_seqno(home *h) {
    u32 max = 0;
    a_path(bedir, u8bDataC(h->root), KEEP_DIR_S, u8bDataC(h->project));
    (void)FILEDeepScanFiles(bedir, graf_max_seqno_cb, &max);
    return max;
}

//  next_seqno = max(disk seqnos under `.be/`) + 1, then bumped above
//  any loaded run as a defensive max.  Called by GRAFOpenBranch and
//  GRAFSwitchBranch.
static void graf_recompute_next_seqno(graf *g) {
    u32 max = graf_global_max_seqno(g->h);
    kv32s pups_all = {};
    kv32PastDataS(g->puppies, pups_all);
    for (kv32 const *p = pups_all[0]; p < pups_all[1]; p++)
        if (p->key > max) max = p->key;
    g->next_seqno = max + 1;
}

//  Create a new `.graf.idx` in `dir` with a globally-unique seqno;
//  drop-in replacement for `DOGPupCreate(g->puppies, dir, ext, data)`
//  that picks `seqno = g->next_seqno` and bumps the counter on
//  success.  Mirrors keeper/KEEP.c §keep_pup_create_next.
ok64 GRAFPupCreateNext(path8s dir, u8cs ext, u8cs data) {
    sane($ok(dir));
    graf *g = &GRAF;
    u32 seqno = g->next_seqno;
    call(DOGPupCreateAt, g->puppies, dir, ext, data, seqno);
    if (seqno >= g->next_seqno) g->next_seqno = seqno + 1;
    done;
}

static ok64 graf_open_dir_cb(graf *g, u8cs dir, b8 is_leaf, void0p ctx) {
    (void)ctx;
    //  Branch shard dirs are materialised lazily — mirror
    //  keep_open_dir_cb's "missing dir = no inherited shards"
    //  policy.  REFS (sniff) is the source of truth on branch
    //  existence; here we just accumulate what's on disk.
    b8 exists = graf_dir_exists(dir);
    //  Mirror keeper's leaf-flip: freeze inherited (trunk + ancestor)
    //  graf-idx entries into PAST before scanning the leaf dir.  The
    //  active leaf's entries then accumulate in DATA, where writes
    //  (DOGPupCreate / DAGCompact) target.  Reads scan PastData via
    //  GRAFRefreshView so cross-branch DAG walks see every loaded
    //  run.  See KEEP.h §"Branch-aware object store".
    if (is_leaf && kv32bDataLen(g->puppies) > 0)
        ((kv32 **)g->puppies)[1] = (kv32 *)g->puppies[2];
    if (!exists) return OK;
    a_cstr(ext, GRAF_IDX_EXT);
    return DOGPupOpenAll(g->puppies, dir, ext);
}

void GRAFRefreshView(void) {
    graf *g = &GRAF;
    g->runs_n = 0;
    //  Span PastData — inherited (parent / sibling-after-switch) runs
    //  in PAST plus leaf-owned runs in DATA — so cross-branch DAG
    //  reads (LCA, WEAVE history walks) see every loaded run.
    //  Compaction (DAGCompact) operates on DATA only via DOGPupCount.
    u32 n = DOGPupCountAll(g->puppies);
    for (u32 i = 0; i < n && g->runs_n < MSET_MAX_LEVELS; i++) {
        u8cs raw = {};
        DOGPupDataAll(raw, g->puppies, i);
        if (raw[0] == NULL) continue;
        wh128cp base = (wh128cp)raw[0];
        size_t bytes = (size_t)(raw[1] - raw[0]);
        g->runs[g->runs_n][0] = base;
        g->runs[g->runs_n][1] = base + bytes / sizeof(wh128);
        g->runs_n++;
    }
}

void GRAFRuns(wh128cssp out) {
    out[0] = GRAF.runs;
    out[1] = GRAF.runs + GRAF.runs_n;
}

ok64 GRAFOpenBranch(home *h, u8cs branch, b8 rw) {
    sane(h != NULL && $ok(branch));

    //  Already open?  Compatible if the existing mode is at least as
    //  strong as the request.  Same conflict rule as KEEPOpenBranch.
    if (graf_is_open()) {
        if (rw && !graf_is_rw) return GRAFOPENRO;
        return GRAFOPEN;
    }

    //  Normalize the branch: trunk aliases (empty / main / master /
    //  trunk + `heads/` variants) → empty; non-trunk gains trailing
    //  '/'.  Mirrors KEEPOpenBranch.
    a_pad(u8, nb, GRAF_LEAF_BRANCH_MAX);
    call(DPATHBranchNormFeed, nb, branch);
    a_dup(u8c, norm, u8bDataC(nb));

    //  Register on the home singleton (idempotent re-opens absorbed).
    {
        ok64 o = HOMEOpenBranch(h, branch, rw);
        if (o != OK && o != HOMEOPEN && o != HOMEROBR && o != HOMEMAX)
            return o;
    }

    graf *g = &GRAF;
    zerop(g);
    g->h = h;
    g->lock_fd = -1;
    g->out_fd = -1;
    graf_is_rw = rw;

    call(kv32bAllocate, g->puppies, FILE_MAX_OPEN);

    //  Stash the canonical leaf-branch bytes in graf-owned storage.
    if (u8csLen(norm) >= GRAF_LEAF_BRANCH_MAX) return GRAFFAIL;
    call(u8bAllocate, g->leaf_branch, GRAF_LEAF_BRANCH_MAX);
    call(PATHu8bTerm, g->leaf_branch);
    if (!u8csEmpty(norm)) call(PATHu8bFeed, g->leaf_branch, norm);

    //  Trunk dir always exists after this — first writer creates it.
    a_pad(u8, trunkdir, FILE_PATH_MAX_LEN);
    {
        u8cs empty = {};
        ok64 to = graf_branch_dir(trunkdir, h, empty);
        if (to != OK) {
            DOGPupClose(g->puppies);
            u8bFree(g->leaf_branch);
            zerop(g); graf_is_rw = NO;
            return to;
        }
    }
    call(FILEMakeDirP, $path(trunkdir));

    //  Walk trunk → leaf, scanning each branch dir for `<seqno>.graf.idx`.
    {
        a_dup(u8c, leaf, u8bDataC(g->leaf_branch));
        ok64 wo = graf_walk_branch(g, leaf, graf_open_dir_cb, NULL);
        if (wo != OK) {
            DOGPupClose(g->puppies);
            u8bFree(g->leaf_branch);
            zerop(g); graf_is_rw = NO;
            return wo;
        }
    }

    GRAFRefreshView();
    graf_recompute_next_seqno(g);

    //  Worktree sharing: lock the LEAF dir (writes only land in the
    //  deepest dir).  For trunk leaf this is `<.be>/.lock`.
    //  Readers open lockless — runs are immutable (tmp+rename
    //  publication) and DOGPupOpenAll retries on ENOENT.
    if (rw) {
        a_pad(u8, leafdir, FILE_PATH_MAX_LEN);
        a_dup(u8c, leaf, u8bDataC(g->leaf_branch));
        call(graf_branch_dir, leafdir, h, leaf);
        //  Lazy materialisation, see keeper/KEEP.c §KEEPOpenBranch.
        call(FILEMakeDirP, $path(leafdir));
        a_pad(u8, lockpath, FILE_PATH_MAX_LEN);
        a_dup(u8c, lds, u8bDataC(leafdir));
        call(PATHu8bFeed, lockpath, lds);
        a_cstr(lockrel, GRAF_LOCK_S);
        call(PATHu8bAdd, lockpath, lockrel);
        call(PATHu8bTerm, lockpath);
        call(FILECreate, &g->lock_fd, $path(lockpath));
        call(FILELock,   &g->lock_fd, rw);
    }

    call(u8bMap, g->arena, GRAF_ARENA_SIZE);

    done;
}

ok64 GRAFOpen(home *h, b8 rw) {
    static u8c const _zero = 0;
    u8cs trunk = {(u8cp)&_zero, (u8cp)&_zero};
    return GRAFOpenBranch(h, trunk, rw);
}

// --- GRAFSwitchBranch ------------------------------------------------
//
// Re-target graf from current leaf to `new_branch` without closing.
// Slides current DATA (the active leaf's `.graf.idx` runs) into PAST
// on `g->puppies`, walks segments past LCA(old, new) scanning each
// new dir, refreshes `g->runs[]`, swaps the leaf flock.  Mirrors
// keeper's `KEEPSwitchBranch`.  Use case: cross-branch ops (POST
// promote-to-sibling, located-cherry PATCH from another branch)
// need both branches' DAG runs visible — `SNIFFMaybeSwitchGraf`
// pairs this with `KEEPSwitchBranch` at every cross-branch call site.

static size_t graf_branch_lca_prefix(u8cs a, u8cs b) {
    size_t na = u8csLen(a), nb = u8csLen(b);
    size_t n = na < nb ? na : nb;
    size_t matched = 0;
    size_t last_slash = 0;
    for (; matched < n; matched++) {
        if (a[0][matched] != b[0][matched]) break;
        if (a[0][matched] == '/') last_slash = matched + 1;
    }
    if (matched == n) {
        if (na == nb) return na;
        u8cp longer_head = (na > nb) ? a[0] : b[0];
        if (longer_head[n] == '/') return n;
    }
    return last_slash;
}

ok64 GRAFSwitchBranch(home *h, u8cs new_branch) {
    sane(h != NULL && $ok(new_branch));
    graf *g = &GRAF;
    if (!graf_is_open()) return GRAFFAIL;

    //  Normalize the new branch.
    a_pad(u8, nb, GRAF_LEAF_BRANCH_MAX);
    call(DPATHBranchNormFeed, nb, new_branch);
    a_dup(u8c, norm, u8bDataC(nb));

    //  No-op when already on the requested branch.  `g->leaf_branch`
    //  is normalized (trailing '/' for non-trunk) so direct compare
    //  is sufficient if both sides match the convention.  Strip a
    //  trailing slash on either side before compare.
    a_dup(u8c, cur, u8bDataC(g->leaf_branch));
    {
        u8cs a = {}, b = {};
        u8csMv(a, cur);
        u8csMv(b, norm);
        if (!u8csEmpty(a) && *(a[1] - 1) == '/') u8csShed1(a);
        if (!u8csEmpty(b) && *(b[1] - 1) == '/') u8csShed1(b);
        if (u8csLen(a) == u8csLen(b) &&
            (u8csLen(a) == 0 ||
             memcmp(a[0], b[0], u8csLen(a)) == 0))
            done;
    }

    //  Strip trailing slash on cur for LCA comparison.
    u8cs cur_for_lca = {};
    u8csMv(cur_for_lca, cur);
    if (!u8csEmpty(cur_for_lca) && *(cur_for_lca[1] - 1) == '/')
        u8csShed1(cur_for_lca);
    size_t lca = graf_branch_lca_prefix(cur_for_lca, norm);

    //  1. Collapse old leaf's DATA into PAST.
    if (kv32bDataLen(g->puppies) > 0)
        ((kv32 **)g->puppies)[1] = (kv32 *)g->puppies[2];

    //  2. Walk the new branch past LCA, scanning each new dir.
    a_path(d);
    {
        a_dup(u8c, root_s, u8bDataC(h->root));
        call(PATHu8bFeed, d, root_s);
        a_cstr(rel, GRAF_DIR_S);
        call(PATHu8bAdd, d, rel);
        //  Project shard segment (project-sharded layout — see
        //  dog/DOG.h §"Canonical on-disk layout").  Empty `h->project`
        //  collapses to the legacy single-project shape.
        a_dup(u8c, proj, u8bDataC(h->project));
        if (!u8csEmpty(proj)) call(PATHu8bAdd, d, proj);
        call(PATHu8bTerm, d);
    }
    if (u8csLen(norm) > 0) {
        u8cp p = norm[0];
        u8cp seg_start = p;
        size_t off = 0;
        while (p <= norm[1]) {
            b8 at_end = (p == norm[1]);
            if (at_end || *p == '/') {
                if (p > seg_start) {
                    u8cs seg = {seg_start, p};
                    call(PATHu8bPush, d, seg);
                    if (off >= lca) {
                        if (!graf_dir_exists($path(d)))
                            return GRAFNOPATH;
                        a_cstr(ext, GRAF_IDX_EXT);
                        call(DOGPupOpenAll, g->puppies, $path(d), ext);
                    }
                }
                seg_start = p + 1;
                off = (size_t)(p - norm[0]) + 1;
            }
            p++;
        }
    }

    //  3. Refresh the typed runs[] view + global next_seqno.
    GRAFRefreshView();
    graf_recompute_next_seqno(g);

    //  Lock stays on the original leaf: cross-branch graf access is
    //  read-only context (writes still target the originally-opened
    //  leaf).  Swapping the flock would create stray `.lock.graf`
    //  files in sibling shard dirs that `be delete ?branch` can't
    //  clean up via REFS tombstone alone.

    //  5. Update leaf_branch.
    u8bReset(g->leaf_branch);
    call(PATHu8bTerm, g->leaf_branch);
    if (!u8csEmpty(norm)) call(PATHu8bFeed, g->leaf_branch, norm);
    done;
}

ok64 GRAFClose(void) {
    sane(1);
    if (!graf_is_open()) return OK;
    graf *g = &GRAF;
    // Flush any pending ingest (runs the finish walk + compaction).
    if (g->ing) GRAFDagFinish();
    if (!BNULL(g->puppies))     DOGPupClose(g->puppies);
    if (!BNULL(g->leaf_branch)) u8bFree(g->leaf_branch);
    if (g->arena[0]) u8bUnMap(g->arena);
    if (g->lock_fd >= 0) FILEClose(&g->lock_fd);
    g->runs_n = 0;
    g->out_fd = -1;
    g->emit = NULL;
    g->h = NULL;
    graf_is_rw = NO;
    done;
}

ok64 GRAFArenaInit(void) {
    if (graf_arena[0] != NULL) {
        ((u8 **)graf_arena)[2] = graf_arena[1];  // reset idle to start
        return OK;
    }
    return u8bMap(graf_arena, GRAF_ARENA_SIZE);
}

void GRAFArenaCleanup(void) {
    if (graf_arena[0] != NULL)
        ((u8 **)graf_arena)[2] = graf_arena[1];
}

ok64 GRAFHunkEmit(hunk const *hk, void *ctx) {
    sane(hk != NULL);
    (void)ctx;
    if (graf_emit == NULL || graf_out_fd < 0) return OK;

    // Reuse the trailing portion of graf_arena as TLV scratch.
    range64 mark;
    Bu8mark(graf_arena, &mark);
    u8cp start = u8bIdleHead(graf_arena);
    if (graf_emit(u8bIdle(graf_arena), hk) != OK) {
        Bu8rewind(graf_arena, mark);
        return OK;
    }

    u8cs ser = {start, u8bIdleHead(graf_arena)};
    while (!$empty(ser)) {
        ssize_t w = write(graf_out_fd, ser[0], $len(ser));
        if (w < 0) {
            if (errno == EINTR) continue;
            //  Pager exited — close our end so subsequent emits early-
            //  return at the top guard.  Streaming producers (LOG.c)
            //  poll graf_out_fd to break their walk.
            if (errno == EPIPE) graf_out_fd = -1;
            break;
        }
        if (w == 0) break;
        u8csFed(ser, (size_t)w);
    }

    Bu8rewind(graf_arena, mark);
    return OK;
}
