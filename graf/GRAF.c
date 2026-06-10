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
#include "keeper/KEEP.h"
#include "keeper/REFS.h"
#include "keeper/RESOLVE.h"

// --- Producer-side staging state ---
//
//  Bytes flow through `HUNKu8sFeedOut`, dispatched off the module-global
//  `HUNKMode` (TLV / Plain / Color) set once at CLI entry.  graf
//  contributes the byte sink (`graf_out_fd`) and a scratch arena.
Bu8          graf_arena   = {};
int          graf_out_fd  = -1;

// --- Singleton ---

graf GRAF = {};

static b8 graf_is_open(void) { return GRAF.h != NULL; }
static b8 graf_is_rw = NO;

// --- GRAFOpenBranch / GRAFOpen / GRAFClose ---

#define GRAF_DIR_S       DOG_BE_NAME
#define GRAF_IDX_EXT     ".graf.idx"
#define GRAF_LOCK_S      ".lock.graf"
#define GRAF_LEAF_BRANCH_MAX 1024

//  Compose `<root>/.be/<project>` — the single per-project graf shard.
//  Branch is no longer a directory: every `.graf.idx` run lives directly
//  under the project dir, with branch as pure ref context
//  (`h->cur_branch`).  Mirrors dog/HOME.c `HOMEBranchDir`'s flattened
//  shape.  `branch` is ignored; kept for call-site compatibility.
//  `out` is NUL-terminated.
static ok64 graf_branch_dir(path8b out, home *h, u8cs branch) {
    sane(h && out);
    (void)branch;
    //  <root>/.be/<project> via the single dog/HOME composer (honors the
    //  *.be-is-store rule).  Empty `h->project` collapses to `<root>/.be`.
    a_dup(u8c, proj, u8bDataC(h->project));
    call(HOMEBeDir, h, proj, out);
    call(PATHu8bTerm, out);
    done;
}

//  Filename → pup_key for `<10-RON64>.graf.idx`.  Returns 0 on parse
//  failure or non-graf-idx file.
static u64 graf_filename_seqno(u8cs name) {
    static char const EXT[] = GRAF_IDX_EXT;
    static const size_t SEQ_W = 10;
    size_t n = u8csLen(name);
    if (n != SEQ_W + sizeof(EXT) - 1) return 0;
    char const *tail = (char const *)name[0] + SEQ_W;
    if (memcmp(tail, EXT, sizeof(EXT) - 1) != 0) return 0;
    u8cs seq_s = {name[0], name[0] + SEQ_W};
    ok64 v = 0;
    if (RONutf8sDrain(&v, seq_s) != OK) return 0;
    return (u64)v;
}

static ok64 graf_max_seqno_cb(void0p arg, path8p path) {
    u64 *max = (u64 *)arg;
    u8cs base = {};
    PATHu8sBase(base, u8bDataC(path));
    u64 sq = graf_filename_seqno(base);
    if (sq > *max) *max = sq;
    return OK;
}

//  Recursive scan over `<root>/.be/` for every `.graf.idx` file;
//  returns the global max pup_key (default 0 when none found).  Used
//  by `graf_recompute_last_pup_key` to keep fresh pup_keys unique
//  across sibling branch dirs that aren't loaded into the registry.
static u64 graf_global_max_seqno(home *h) {
    u64 max = 0;
    a_path(bedir);
    (void)HOMEBranchDir(h, bedir, NULL);
    (void)FILEDeepScanFiles(bedir, graf_max_seqno_cb, &max);
    return max;
}

//  last_pup_key = max(disk pup_keys under `.be/`, loaded PastData).
//  Called by GRAFOpenBranch and GRAFSwitchBranch.  The next mint
//  picks `max(RONNow, last_pup_key + 1)`.
static void graf_recompute_last_pup_key(graf *g) {
    u64 max = graf_global_max_seqno(g->h);
    kv64s pups_all = {};
    kv64PastDataS(g->puppies, pups_all);
    for (kv64 const *p = pups_all[0]; p < pups_all[1]; p++)
        if (p->key > max) max = p->key;
    g->last_pup_key = max;
}

//  Mint a globally-unique ron60 pup_key, mirror of
//  `keep_next_pup_key`.
static u64 graf_next_pup_key(graf *g) {
    u64 now = RONNow();
    u64 next = now > g->last_pup_key ? now : g->last_pup_key + 1;
    g->last_pup_key = next;
    return next;
}

//  Create a new `.graf.idx` in `dir` with a globally-unique ron60
//  pup_key; drop-in replacement for
//  `DOGPupCreate(g->puppies, dir, ext, data)`.  Mirrors keeper/KEEP.c
//  §keep_pup_create_next.
ok64 GRAFPupCreateNext(path8s dir, u8cs ext, u8cs data) {
    sane($ok(dir));
    graf *g = &GRAF;
    u64 pup_key = graf_next_pup_key(g);
    return DOGPupCreateAt(g->puppies, dir, ext, data, pup_key);
}

void GRAFRefreshView(void) {
    graf *g = &GRAF;
    g->runs_n = 0;
    //  Span PastData — single-shard layout keeps PAST empty, so
    //  PastData == DATA: every `.graf.idx` run in the one project dir.
    //  DAG reads (LCA, WEAVE history walks) see the whole set.
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

//  Worker: acquires every resource (puppies kv, project-dir lock fd, the
//  three u8bMap arenas).  The wrapper has already zero-inited the
//  singleton and set g->h / g->lock_fd, so a failing `call(...)` is
//  recoverable: the wrapper runs GRAFClose() on any non-OK return, which
//  idempotently releases whatever was acquired so far (DOGPupClose on a
//  zero/partial kv, lock_fd<0 skip, u8bUnMap no-op on never-mapped bufs).
static ok64 graf_open_branch_w(graf *g, home *h, u8cs norm, b8 rw) {
    sane(g != NULL && h != NULL);

    call(kv64bAllocate, g->puppies, FILE_MAX_OPEN);

    //  Canonical leaf-branch bytes live in `h->cur_branch` (claimed by
    //  HOMEOpenBranch in the wrapper).
    if (u8csLen(norm) >= HOME_BRANCH_MAX) return GRAFFAIL;

    //  Single per-project shard dir: `<root>/.be/<project>`.  Branch is
    //  pure ref context now (`h->cur_branch`); every `.graf.idx` run
    //  lives directly here, no per-branch subdirs.  First writer creates
    //  it.  Mirrors keeper's flattened layout.
    a_pad(u8, projdir, FILE_PATH_MAX_LEN);
    //  Compose + create the project shard via dog/HOME (mkdir only in rw).
    {
        a_dup(u8c, proj, u8bDataC(h->project));
        call(HOMEMakeBeDir, h, proj, projdir);
        call(PATHu8bTerm, projdir);
    }

    //  Scan the project dir for `<seqno>.graf.idx` runs.  Single shard —
    //  no trunk→leaf walk, no PAST/DATA flip (PAST stays empty).
    {
        a_cstr(ext, GRAF_IDX_EXT);
        call(DOGPupOpenAll, g->puppies, $path(projdir), ext);
    }

    GRAFRefreshView();
    graf_recompute_last_pup_key(g);

    //  Worktree sharing: lock the project shard dir (the only dir writes
    //  ever land in).  Readers open lockless — runs are immutable
    //  (tmp+rename publication) and DOGPupOpenAll retries on ENOENT.
    if (rw) {
        a_pad(u8, lockpath, FILE_PATH_MAX_LEN);
        a_dup(u8c, lds, u8bDataC(projdir));
        call(PATHu8bFeed, lockpath, lds);
        a_cstr(lockrel, GRAF_LOCK_S);
        call(PATHu8bAdd, lockpath, lockrel);
        call(PATHu8bTerm, lockpath);
        call(FILECreate, &g->lock_fd, $path(lockpath));
        call(FILELock,   &g->lock_fd, rw);
    }

    call(u8bMap, g->arena,    GRAF_ARENA_SIZE);
    call(u8bMap, g->obj_buf,  GRAF_OBJ_BUF_SIZE);
    call(u8bMap, g->tree_buf, GRAF_OBJ_BUF_SIZE);

    //  graf only ever READS keeper (commits/trees/blobs; it writes only
    //  its own `.graf.idx`).  Open it RO here — at the top of the call
    //  chain — so GRAFExec is a pure read against an already-open store
    //  (CLAUDE.md §5; resource open/close never belongs in Exec).  The
    //  flat single-pool store means this open serves every branch: the
    //  `?ref` slot is resolved per-Exec via REFS, no per-branch reopen.
    //  If keeper is already open (a caller or sibling dog opened it),
    //  KEEPOPEN says so and we leave the close to whoever owns it.
    {
        ok64 ko = KEEPOpenBranch(h, norm, NO);
        if (ko == OK)            g->keep_owned = YES;
        else if (ko == KEEPOPEN) g->keep_owned = NO;
        else                     return ko;
    }

    done;
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
        if (o != OK && o != HOMEOPEN && o != HOMEROBR)
            return o;
    }

    graf *g = &GRAF;
    zerop(g);
    g->h = h;               //  arms graf_is_open() so GRAFClose cleans up
    g->lock_fd = -1;
    g->out_fd = -1;
    graf_is_rw = rw;

    //  All resource acquisition is in the worker; on any non-OK return
    //  GRAFClose() releases whatever was acquired so far (idempotent on a
    //  partially-initialised singleton).  Avoids the per-failure cleanup
    //  duplication that previously leaked the mapped bufs / lock fd.
    ok64 r = graf_open_branch_w(g, h, norm, rw);
    if (r != OK) {
        GRAFClose();
        graf_is_rw = NO;
    }
    return r;
}

ok64 GRAFOpen(home *h, b8 rw) {
    static u8c const _zero = 0;
    u8cs trunk = {(u8cp)&_zero, (u8cp)&_zero};
    return GRAFOpenBranch(h, trunk, rw);
}

//  --- GRAFRefIsName (branch-first disambiguation probe) -----------
//
//  URI-001 §"The one rule": branch-vs-hash disambiguation is
//  BRANCH-FIRST.  Returns OK iff `ref` resolves as a REFS NAME
//  (branch / tag / wire-prefixed ref) in the open keeper store — so a
//  ref whose NAME is all-hex (`dead`, `c0ffee`) is recognised as a
//  name and NOT misrouted into the hashlet path.  A bare commit sha
//  (not stored as a ref) returns GRAFNONE.  The diff / blame ref-URI
//  composers call this to pick `?<name>` (REFS) vs `#<sha>` (hashlet)
//  instead of the old syntactic `DOGIsHashlet` guess.  Best-effort:
//  any miss / no-keeper / lookup error reads as "not a name", and the
//  caller falls back to the syntactic hashlet test.
ok64 GRAFRefIsName(u8cs ref) {
    sane(1);
    if (u8csEmpty(ref)) return GRAFNONE;
    keeper *k = &KEEP;
    if (k->h == NULL || u8bDataLen(k->h->root) == 0) return GRAFNONE;

    a_path(keepdir);
    if (HOMEBranchDir(k->h, keepdir, NULL) != OK) return GRAFNONE;

    //  Try the raw name plus the wire-prefix peels REFS may have
    //  canonicalised away (`refs/heads/X` → `X`) — mirrors
    //  resolve_branch_path / graf_log_resolve_target.
    char const *strips[] = {"", "refs/heads/", "refs/", "heads/", NULL};
    for (u32 si = 0; strips[si] != NULL; si++) {
        a_dup(u8c, q, ref);
        a_cstr(strip, strips[si]);
        if (!u8csEmpty(strip)) {
            if (u8csLen(q) <= u8csLen(strip)) continue;
            if (!u8csHasPrefix(q, strip)) continue;
            u8csUsed(q, u8csLen(strip));
        }
        a_pad(u8, arena, 512);
        uri resolved = {};
        a_uri(qkey, 0, 0, 0, q, 0);
        if (REFSResolve(&resolved, arena, $path(keepdir), qkey) == OK &&
            u8csLen(resolved.query) >= 40)
            done;
    }
    return GRAFNONE;
}

//  --- GRAFResolveVersion (universal version resolver) --------------
//
//  Phase 1: project-relative branch arm.  `?<branch>` (optional
//  trailing `/`) gets resolved against trunk REFS via REFSResolve;
//  on hit, the query slot is rewritten to canonic form
//      ?/<project>/<branch>/<40-hex-tip>
//  (hashlet suffix deferred — see https://replicated.wiki/html/wiki/Store.html §"Project identity").
//
//  Anything else (absolute `/`, relative `./`, hashlet, magic,
//  search) currently passes through verbatim so downstream dogs
//  keep their existing resolution behavior — those arms land in
//  Phase 1.x.  GRAFNONE is reserved for the future full-project
//  fallback scan (test/TRIANGLE.todo.md); the pass-through case
//  returns OK so be's top-of-chain loop doesn't surface false
//  negatives.
//
//  Requires keeper opened on the same home (REFSResolve reads
//  `<store>/<project>/refs`).
ok64 GRAFResolveVersion(u8s canonic, u8csc given) {
    sane(1);
    test(!u8csEmpty(given), GRAFNONE);
    test($len(given) <= $len(canonic), GRAFFULL);

    //  Parse `given` via URILexer (no manual parsing — ABC.md §"Never
    //  ever do manual parsing in C").  URILexer consumes its `u.data`
    //  slice; `given` itself stays untouched because `u.data` is its
    //  own member-array storage.
    uri u = {};
    u8csMv(u.data, given);
    ok64 lo = URILexer(&u);
    if (lo != OK) return u8sFeed(canonic, given);

    //  No query slot to resolve — pass through.
    if (u8csEmpty(u.query)) return u8sFeed(canonic, given);

    u8cs q = {};
    u8csMv(q, u.query);

    //  Already-canonic input (`?/<project>/<branch>/<sha>`):
    //  pass through.  The full-project fallback / future phases
    //  may revisit this gate but today re-resolving an already-
    //  resolved URI is a no-op.
    if (*q[0] == '/') return u8sFeed(canonic, given);

    //  TODO (no test coverage yet): magic ?null / ?back, pure-hex
    //  hashlet prefix expansion via KEEPResolveRef, commit-msg
    //  search by whitespace shape.  Leave as pass-through so the
    //  downstream legacy resolvers continue handling them.
    {
        a_cstr(s_null, "null");
        a_cstr(s_back, "back");
        if (u8csEq(q, s_null) || u8csEq(q, s_back))
            return u8sFeed(canonic, given);
    }
    if (HEXu8sValid(q) && u8csLen(q) >= 4 && u8csLen(q) <= 40)
        return u8sFeed(canonic, given);
    {
        u8cs scan = {};
        u8csMv(scan, q);
        if (u8csFind(scan, ' ') == OK) return u8sFeed(canonic, given);
    }

    //  Resolve branch-relative anchors (`?./<sub>`, `?../<sib>`,
    //  `?..`) via dog/DPATH's path arithmetic, producing the
    //  absolute branch path under cur's project.  `was_rel == NO`
    //  for non-relative shapes leaves `branch_abs` empty and we
    //  fall through to the project-relative path below.
    keeper *k = &KEEP;
    if (k->h == NULL)                return u8sFeed(canonic, given);
    if (!u8bHasData(k->h->project))  return u8sFeed(canonic, given);

    a_path(branch_abs);
    b8 was_rel = NO;
    {
        u8cs cur_branch = {};
        u8csMv(cur_branch, u8bDataC(k->h->cur_branch));
        call(DPATHBranchResolveRel, branch_abs, cur_branch, q, &was_rel);
    }
    //  Relative resolved to trunk (`?..` from a top-level branch
    //  pops past root) — defer to the downstream legacy resolver
    //  for now; empty-branch canonic emit needs a dedicated trunk
    //  arm we haven't designed yet.
    if (was_rel && u8bDataLen(branch_abs) == 0)
        return u8sFeed(canonic, given);

    //  Branch slice for canonic emit + REFS lookup: relative
    //  resolution wrote the absolute path into `branch_abs`; for
    //  non-relative inputs, fall back to the user-typed query with
    //  any trailing '/' stripped (branch-form hint, not part of
    //  the name).
    u8cs branch = {};
    if (was_rel) {
        u8csMv(branch, u8bDataC(branch_abs));
    } else {
        u8csMv(branch, q);
        if (!u8csEmpty(branch) && *u8csLast(branch) == '/')
            u8csShed1(branch);
    }
    if (u8csEmpty(branch)) return u8sFeed(canonic, given);

    //  Ref lookup via keeper/RESOLVE.c's two-axis resolver:
    //  KEEPResolveRef walks trunk REFS, then falls back to the
    //  branch's own leaf-shard REFS (where peer-form rows from
    //  `keeper get //h?<branch>` actually land — gap #1 fix
    //  2026-05-27).  This catches the sub-mount `?master` case
    //  REFSResolve-on-trunk-alone misses.
    sha1 sha = {};
    {
        u8cs cur_branch = {};
        u8csMv(cur_branch, u8bDataC(k->h->cur_branch));
        ok64 rr = KEEPResolveRef(&sha, branch, cur_branch);
        if (rr != OK) return u8sFeed(canonic, given);
    }
    (void)sha;   //  resolved only to VALIDATE the branch exists (above)

    //  URI-001 Stage 3: canonicalise the SCOPE only — emit
    //      /<project>/<branch>
    //  NOT a pin-in-query `/<project>/<branch>/<sha>`.  In argv URIs the
    //  fragment is verb payload (PATCH squash/merge/rebase mode, GET
    //  `#~N`/`#sha`), so pinning the tip into the query would read back
    //  as a located-cherry / wrong shape downstream; the resolved sha
    //  pin lives in `--at` (wtlog) and REFS storage.  KEEPResolveRef
    //  above already validated the branch (passed through on miss).
    //  Leading '/' is mandatory (URI.mkd "Ref shapes").
    a_abspath(canon_q);
    call(PATHu8bPush, canon_q, u8bDataC(k->h->project));
    call(PATHu8bPush, canon_q, branch);

    //  Splice the new query back into the URI shape via URIutf8Feed
    //  (no manual serialize).  Mutate `u` in place — function-local.
    //  `u.query` is `u8cs` (const); take the const data view so the
    //  move target / source qualifiers match (no discards-qualifiers).
    u8csMv(u.query, u8bDataC(canon_q));
    return URIutf8Feed(canonic, &u);
}

// --- GRAFSwitchBranch ------------------------------------------------
//
// Re-target graf from current branch to `new_branch` without closing.
// With the single-shard layout, branch is pure ref context: every
// `.graf.idx` run already lives in the one project dir loaded at open,
// so a switch is a label-only operation — re-point `h->cur_branch`.
// No dir walk, no PAST/DATA flip, no rescans, no lock swap.  Mirrors
// keeper's flattened `KEEPSwitchBranch`.  Use case: cross-branch ops
// (POST promote-to-sibling, located-cherry PATCH from another branch)
// — `SNIFFMaybeSwitchGraf` pairs this with `KEEPSwitchBranch`.

ok64 GRAFSwitchBranch(home *h, u8cs new_branch) {
    sane(h != NULL && $ok(new_branch));
    graf *g = &GRAF;
    if (!graf_is_open()) return GRAFFAIL;

    //  Inner APIs receive canonic URIs from beagle (https://replicated.wiki/html/wiki/Store.html
    //  §"URI structure": `/<project>/<branch-path>/<pin>`).  Pull
    //  the branch slice out of any canonic query form before handing
    //  it to HOMESetCurBranch (which normalises).
    u8cs branch_in = {};
    u8csMv(branch_in, new_branch);
    {
        u8cs c_proj = {}, c_branch = {}, c_pin = {};
        if (DOGCanonQueryParse(branch_in, c_proj, c_branch, c_pin))
            u8csMv(branch_in, c_branch);
    }

    //  Re-target the worktree current branch.  No-op handling and
    //  normalisation live in HOMESetCurBranch.  All runs already
    //  visible from the single project shard, so nothing to rescan.
    call(HOMESetCurBranch, h, branch_in);
    done;
}

ok64 GRAFClose(void) {
    sane(1);
    if (!graf_is_open()) return OK;
    graf *g = &GRAF;
    // Flush any pending ingest (runs the finish walk + compaction).
    if (g->ing) GRAFDagFinish();
    if (!BNULL(g->puppies))     DOGPupClose(g->puppies);
    if (g->arena[0])    u8bUnMap(g->arena);
    if (g->obj_buf[0])  u8bUnMap(g->obj_buf);
    if (g->tree_buf[0]) u8bUnMap(g->tree_buf);
    if (g->lock_fd >= 0) FILEClose(&g->lock_fd);
    //  Release the keeper we opened in graf_open_branch_w (skip when a
    //  caller/sibling owns it — KEEPOPEN at open time set keep_owned=NO).
    if (g->keep_owned) { KEEPClose(); g->keep_owned = NO; }
    g->runs_n = 0;
    g->out_fd = -1;
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

void GRAFEmitDiffUri(u32b toks, u8b out, u8cs hex) {
    if (!$ok(toks)) return;
    a_cstr(prefix, "diff:?");
    (void)u8bFeed(out, prefix);
    (void)u8bFeed(out, hex);
    (void)u32bFeed1(toks, tok32Pack('U', (u32)u8bDataLen(out)));
}

ok64 GRAFHunkEmit(hunk const *hk, void *ctx) {
    sane(hk != NULL);
    (void)ctx;
    if (graf_out_fd < 0) return OK;

    // Reuse the trailing portion of graf_arena as TLV scratch.
    range64 mark;
    Bu8mark(graf_arena, &mark);
    u8cp start = u8bIdleHead(graf_arena);
    if (HUNKu8sFeedOut(u8bIdle(graf_arena), hk) != OK) {
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
