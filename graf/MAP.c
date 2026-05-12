//  MAP: subway-map view of the branch tree for `be map:`.
//
//  Multi-column commit history.  Filters branches to the current
//  one's ancestors-to-trunk and descendants, walks each branch's
//  ancestor set via the DAG index, takes the union, time-sorts by
//  commit author-ts (newest first), and renders one commit per
//  line with a column-per-branch spine.
//
//  Spine glyphs vary by branch depth:
//    depth 0 (trunk)        → ║  (double)
//    depth 1 (child)        → ┃  (heavy)
//    depth 2+ (grandchild+) → │  (light)
//
//  Per row, every branch column whose ancestor set contains the
//  commit gets its spine glyph; others render as a single space.
//  The commit's owner column (the deepest branch claiming it) gets
//  the same spine — no separate marker today; fork glyphs are a
//  later layer.  Trailing fields: <sha7> <7-date> <branch> <summary>.
//
#include "GRAF.h"
#include "DAG.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/HUNK.h"
#include "dog/WHIFF.h"
#include "keeper/GIT.h"
#include "keeper/KEEP.h"

#define MAP_MAX_BRANCHES   32
#define MAP_MAX_COMMITS    4096
#define MAP_PATH_MAX       192
#define MAP_OBJ_BUF        (1UL << 20)
#define MAP_ANC_SIZE       (1u << 16)

//  Box-drawing glyph per branch depth (UTF-8 encoded).  Depth 0 is the
//  trunk, depth 1 the trunk's children, etc.  Caps at depth 2 — deeper
//  branches share the thin `│` glyph.  Each entry is a 3-byte UTF-8
//  sequence (BMP).  The trailing `' '` is for inactive columns.
static char const MAP_GLYPH_TRUNK[] = "\xe2\x95\x91";   // ║
static char const MAP_GLYPH_CHILD[] = "\xe2\x94\x83";   // ┃
static char const MAP_GLYPH_THIN[]  = "\xe2\x94\x82";   // │

static char const *map_glyph_for(u8 depth) {
    if (depth == 0) return MAP_GLYPH_TRUNK;
    if (depth == 1) return MAP_GLYPH_CHILD;
    return MAP_GLYPH_THIN;
}

// --- Branch metadata ------------------------------------------------

typedef struct {
    u8cs    path;            // slice into per-call strs_arena
    u8      depth;          // count of '/' in path; trunk = 0
    sha1hex sha;            // tip sha (40 hex)
    u64     tip_h40;        // 40-bit hashlet of tip
    Bwh128  ancestors;      // set populated via DAGAncestors
    b8      anc_init;
} map_branch;

// --- Per-commit row -------------------------------------------------

typedef struct {
    u64   commit_h40;       // 40-bit hashlet
    sha1  csha;             // full 20-byte sha
    ron60 ts;               // packed wall-clock time (RONOfTime)
    u32   owner;            // index into branches[] (deepest claimant)
    u8cs  summary;          // first line of commit message (slice into arena)
    u8cs  author;           // author name (slice into arena)
} map_commit;

// --- Helpers --------------------------------------------------------

static int map_branch_cmp_for_columns(void const *a_, void const *b_) {
    map_branch const *a = (map_branch const *)a_;
    map_branch const *b = (map_branch const *)b_;
    if (a->depth != b->depth) return (int)a->depth - (int)b->depth;
    size_t alen = u8csLen(a->path), blen = u8csLen(b->path);
    size_t ml = alen < blen ? alen : blen;
    int c = (ml == 0) ? 0 : memcmp(a->path[0], b->path[0], ml);
    if (c != 0) return c;
    return (int)alen - (int)blen;
}

static int map_commit_cmp_desc(void const *a_, void const *b_) {
    map_commit const *a = (map_commit const *)a_;
    map_commit const *b = (map_commit const *)b_;
    //  Newest first.  ron60 normalises so that lexicographic order
    //  matches chronological order; compare via ron60Z.
    if (ron60Z(&a->ts, &b->ts)) return 1;
    if (ron60Z(&b->ts, &a->ts)) return -1;
    if (a->commit_h40 != b->commit_h40)
        return (a->commit_h40 < b->commit_h40) ? 1 : -1;
    return 0;
}

//  ron60 (RON-packed wall-clock) → unix-epoch seconds for date
//  rendering.  Mirror of sniff/SNIFF.exe.c's status_ron60_to_secs.
static i64 map_ron60_to_secs(ron60 ts) {
    if (ts == 0) return 0;
    struct tm t = {};
    if (RONToTime(ts, &t, NULL) != OK) return 0;
    t.tm_isdst = -1;
    time_t s = mktime(&t);
    return s == (time_t)-1 ? 0 : (i64)s;
}

static b8 map_is_ancestor_of(u8csc anc, u8csc desc) {
    size_t al = u8csLen(anc), dl = u8csLen(desc);
    if (al == 0) return YES;
    if (al > dl) return NO;
    if (memcmp(anc[0], desc[0], al) != 0) return NO;
    return al == dl || desc[0][al] == '/';
}

// --- Branch enumeration --------------------------------------------

typedef struct {
    map_branch *v;
    u32         n;
    u32         cap;
    Bu8         arena;       //  borrowed: path bytes are interned here
} map_set;

static ok64 map_collect_cb(keep_tipcp t, void *ctx) {
    map_set *s = (map_set *)ctx;
    if (s->n >= s->cap) return OK;
    size_t pl = (size_t)$len(t->path);
    if (pl >= MAP_PATH_MAX) return OK;
    if ($len(t->sha) != 40) return OK;

    map_branch *b = &s->v[s->n++];
    zerop(b);
    if (u8bFeed(s->arena, t->path) != OK) { s->n--; return OK; }
    u8csMv(b->path, u8bDataC(s->arena));
    (void)u8csUsedAll(u8bDataC(s->arena));
    if (sha1hexFromHex(&b->sha, t->sha) != OK) {
        s->n--;
        return OK;
    }
    //  Depth = number of path segments.  trunk = "" (0 segments, depth 0);
    //  "feat" → 1 segment, depth 1; "feat/sub" → 2 segments, depth 2.
    u8 d = pl > 0 ? 1 : 0;
    $for(u8c, p, b->path) if (*p == '/') d++;
    b->depth = d;

    sha1 tip = {};
    if (sha1FromSha1hex(&tip, &b->sha) != OK) {
        s->n--;
        return OK;
    }
    b->tip_h40 = WHIFFHashlet60(&tip);
    return OK;
}

//  Intern a slice into `arena`: feed bytes, snapshot the new DATA
//  range as `out`, then UsedAll so subsequent feeds extend cleanly.
static void map_intern(Bu8 arena, u8csp out, u8csc src) {
    if (u8csEmpty(src)) return;
    if (u8bFeed(arena, src) != OK) return;
    u8csMv(out, u8bDataC(arena));
    (void)u8csUsedAll(u8bDataC(arena));
}

// --- Main -----------------------------------------------------------

ok64 GRAFMap(uricp u) {
    sane(KEEP.h != NULL);
    (void)u;

    //  Resolve current branch (path) so we can compute the include
    //  filter (current ∪ ancestors-to-trunk ∪ descendants).  Sourced
    //  from `--at <root>?<branch>#<sha>` forwarded by `be` and parked
    //  Per-call arena for path / summary / author bytes — every slice
    //  in map_branch / map_commit points into here.
    Bu8 strs_arena = {};
    if (u8bAllocate(strs_arena, 1UL << 20) != OK) fail(GRAFFAIL);

    //  in `KEEP.h->cur_branch` by HOMEOpen.  Empty branch == trunk.
    u8cs cur_path = {};
    {
        a_dup(u8c, cb, u8bData(KEEP.h->cur_branch));
        if (!u8csEmpty(cb) && *cb[0] == '?') u8csUsed1(cb);
        if (u8csLen(cb) > 0) {
            if (u8bFeed(strs_arena, cb) != OK) {
                u8bFree(strs_arena);
                fail(GRAFFAIL);
            }
            u8csMv(cur_path, u8bDataC(strs_arena));
            (void)u8csUsedAll(u8bDataC(strs_arena));
        }
    }

    //  Pull every local-branch tip, then keep only those in the
    //  ancestors-or-descendants window of the current branch.
    map_branch *all = (map_branch *)calloc(MAP_MAX_BRANCHES, sizeof(*all));
    if (!all) { u8bFree(strs_arena); fail(GRAFFAIL); }
    map_set s = {.v = all, .n = 0, .cap = MAP_MAX_BRANCHES,
                 .arena = {strs_arena[0], strs_arena[1],
                           strs_arena[2], strs_arena[3]}};
    call(KEEPEachTip, &KEEP, map_collect_cb, &s);
    //  KEEPEachTip mutates s.arena's data/idle pointers in place.  Sync
    //  the outer arena so subsequent feeds continue from where it left.
    strs_arena[1] = s.arena[1];
    strs_arena[2] = s.arena[2];

    map_branch *kept = (map_branch *)calloc(MAP_MAX_BRANCHES, sizeof(*kept));
    u32 nk = 0;
    for (u32 i = 0; i < s.n && nk < MAP_MAX_BRANCHES; i++) {
        map_branch const *b = &s.v[i];
        u8csc bp = {b->path[0], b->path[1]};
        u8csc cp = {cur_path[0], cur_path[1]};
        b8 anc  = map_is_ancestor_of(bp, cp);
        b8 desc = map_is_ancestor_of(cp, bp);
        if (anc || desc) kept[nk++] = *b;
    }
    free(all);

    //  Column order: shallow-first (trunk left), then lex within depth.
    qsort(kept, nk, sizeof(*kept), map_branch_cmp_for_columns);

    //  Build per-branch ancestor sets via the DAG index.  GRAFOpen is
    //  idempotent — close only if we owned the open.
    ok64 go = GRAFOpen(KEEP.h, NO);
    b8 own_graf = (go == OK);
    if (go != OK && go != GRAFOPEN && go != GRAFOPENRO) {
        free(kept); return go;
    }
    //  Load every branch's idx pups into graf's PAST/DATA before
    //  walking ancestors.  Each `GRAFSwitchBranch` collapses the
    //  prior DATA into PAST and opens the next branch's dir into
    //  DATA (DOGPupOpenAside semantics).  After the loop, runs span
    //  every kept branch.
    for (u32 i = 0; i < nk; i++) {
        a_dup(u8c, br, kept[i].path);
        (void)GRAFSwitchBranch(KEEP.h, br);
        //  Mirror on keeper so per-branch commit bodies (KEEPGet) and
        //  per-branch tree/blob fetches downstream resolve.  Map is
        //  read-only so the keeper switch is safe (no in-flight pack).
        (void)KEEPSwitchBranch(KEEP.h, br);
    }
    for (u32 i = 0; i < nk; i++) {
        ok64 ao = wh128bAllocate(kept[i].ancestors, MAP_ANC_SIZE);
        if (ao != OK) {
            for (u32 j = 0; j < i; j++)
                if (kept[j].anc_init) wh128bFree(kept[j].ancestors);
            if (own_graf) GRAFClose();
            free(kept);
            return ao;
        }
        kept[i].anc_init = YES;
        wh128css runs = {NULL, NULL};
        GRAFRuns(runs);
        DAGAncestors(kept[i].ancestors, runs, kept[i].tip_h40);
    }

    //  Union all hashlets.  The deepest-claiming branch owns each.
    Bwh128 union_set = {};
    ok64 uo = wh128bAllocate(union_set, MAP_ANC_SIZE);
    if (uo != OK) goto cleanup_branches;
    if (nk > 0) {
        u64 *tips = (u64 *)calloc(nk, sizeof(u64));
        for (u32 i = 0; i < nk; i++) tips[i] = kept[i].tip_h40;
        wh128css runs = {NULL, NULL};
        GRAFRuns(runs);
        DAGAncestorsOfMany(union_set, runs, tips, nk);
        free(tips);
    }

    //  Walk the union: for each commit, find the deepest claimant
    //  (iterate branches in deep-to-shallow order, first match wins),
    //  fetch its body via keeper for ts + summary, accumulate.
    map_commit *commits = (map_commit *)calloc(MAP_MAX_COMMITS,
                                               sizeof(*commits));
    if (!commits) goto cleanup_union;
    u32 ncommits = 0;

    Bu8 cbuf = {};
    if (u8bMap(cbuf, MAP_OBJ_BUF) != OK) goto cleanup_commits;

    //  Owner = SHALLOWEST branch whose ancestor set contains the
    //  commit (the branch where it was first made — descendants
    //  inherit older commits through their fork-commit chain).
    //  kept[] is already sorted shallow→deep by column-cmp, so the
    //  first match in kept[] order is the answer.

    //  Iterate the wh128 union — records are scattered across the
    //  set's full slot range as a hash table (DAGAncestors uses
    //  HASHwh128Put), not packed into the data segment.  Walk every
    //  slot, skip empties (key == 0).
    wh128cp ub = (wh128cp)wh128bHead(union_set);
    wh128cp ue = (wh128cp)wh128bTerm(union_set);
    for (wh128cp r = ub; r < ue && ncommits < MAP_MAX_COMMITS; r++) {
        if (r->key == 0) continue;
        u64 h40 = DAGHashlet(r->key);
        if (h40 == 0) continue;

        u8bReset(cbuf);
        u8 ot = 0;
        if (KEEPGet(&KEEP, h40, DAG_H60_HEXLEN,
                    cbuf, &ot) != OK || ot != DOG_OBJ_COMMIT) continue;
        a_dup(u8c, body, u8bData(cbuf));
        sha1 csha = {};
        KEEPObjSha(&csha, DOG_OBJ_COMMIT, body);

        u32 owner = nk;
        for (u32 i = 0; i < nk; i++) {
            if (DAGAncestorsHas(kept[i].ancestors, h40)) {
                owner = i; break;
            }
        }
        if (owner == nk) continue;

        git_commit gc = {};
        GITu8sParseCommit(body, &gc);
        map_commit *mc = &commits[ncommits++];
        mc->commit_h40 = h40;
        mc->csha = csha;
        mc->owner = owner;
        mc->ts = gc.author_ts;
        u8csc gc_subject_c   = {gc.subject[0],   gc.subject[1]};
        u8csc gc_author_id_c = {gc.author_id[0], gc.author_id[1]};
        map_intern(strs_arena, mc->summary, gc_subject_c);
        map_intern(strs_arena, mc->author,  gc_author_id_c);
    }
    u8bUnMap(cbuf);

    qsort(commits, ncommits, sizeof(*commits), map_commit_cmp_desc);

    //  Render.  Each row: per-column glyph (spine if commit ∈ branch's
    //  ancestor set, space otherwise) + sha7 + 7-date + branch + summary.
    //  Plain text only for now — bro pager hookup follows once the
    //  rendering is settled.
    Bu8 text = {};
    ok64 ta = u8bAllocate(text, 1UL << 20);
    if (ta != OK) goto cleanup_commits;

    i64 now = (i64)time(NULL);
    for (u32 i = 0; i < ncommits; i++) {
        map_commit const *mc = &commits[i];
        //  Glyph row.
        for (u32 c = 0; c < nk; c++) {
            map_branch const *b = &kept[c];
            b8 active = DAGAncestorsHas(b->ancestors, mc->commit_h40);
            if (active) {
                char const *g = map_glyph_for(b->depth);
                a_cstr(g_s, g);
                (void)u8bFeed(text, g_s);
            } else {
                (void)u8bFeed1(text, ' ');
            }
        }
        (void)u8bFeed1(text, ' ');
        //  sha7
        u8 hex[40];
        u8s hs = {hex, hex + 40};
        u8cs ss = {mc->csha.data, mc->csha.data + 20};
        HEXu8sFeedSome(hs, ss);
        u8cs sha7 = {hex, hex + 7};
        (void)u8bFeed(text, sha7);
        (void)u8bFeed1(text, ' ');
        //  7-char date
        u8 date_buf[8];
        u8s date_into = {date_buf, date_buf + sizeof(date_buf)};
        u8cp date_start = date_into[0];
        (void)DOGutf8sFeedDate(date_into,
                               map_ron60_to_secs(mc->ts), now);
        u8cs date_slice = {date_start, date_into[0]};
        (void)u8bFeed(text, date_slice);
        (void)u8bFeed1(text, ' ');
        //  branch + summary
        map_branch const *ob = &kept[mc->owner];
        if (u8csEmpty(ob->path)) {
            a_cstr(trunk_s, "trunk");
            (void)u8bFeed(text, trunk_s);
        } else {
            (void)u8bFeed1(text, '?');
            (void)u8bFeed(text, ob->path);
        }
        (void)u8bFeed1(text, ' ');
        if (!u8csEmpty(mc->summary)) (void)u8bFeed(text, mc->summary);
        (void)u8bFeed1(text, '\n');
    }

    //  Always emit one hunk via GRAFHunkEmit; the formatter in
    //  `graf_emit` (set by graf_start_pager) picks bytes —
    //  HUNKu8sFeed (TLV → bro) or HUNKu8sFeedText (plain → terminal).
    call(GRAFArenaInit);
    a_pad(u8, title, 16);
    a_cstr(prefix, "map:");
    (void)u8bFeed(title, prefix);
    hunk hk = {};
    hk.uri[0]  = u8bDataHead(title);
    hk.uri[1]  = u8bIdleHead(title);
    hk.text[0] = u8bDataHead(text);
    hk.text[1] = u8bIdleHead(text);
    (void)GRAFHunkEmit(&hk, NULL);
    u8bFree(text);
    free(commits);
    if (wh128bHead(union_set) != wh128bTerm(union_set)) wh128bFree(union_set);
    for (u32 i = 0; i < nk; i++)
        if (kept[i].anc_init) wh128bFree(kept[i].ancestors);
    if (own_graf) GRAFClose();
    free(kept);
    u8bFree(strs_arena);
    done;

cleanup_commits:
    free(commits);
cleanup_union:
    if (wh128bHead(union_set) != wh128bTerm(union_set)) wh128bFree(union_set);
cleanup_branches:
    for (u32 i = 0; i < nk; i++)
        if (kept[i].anc_init) wh128bFree(kept[i].ancestors);
    if (own_graf) GRAFClose();
    free(kept);
    u8bFree(strs_arena);
    fail(GRAFFAIL);
}
