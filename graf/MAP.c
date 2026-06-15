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
#include "dog/git/GIT.h"
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
    u8bp        arena;       //  borrowed: path bytes are interned here
} map_set;

static ok64 map_collect_cb(keep_tipcp t, void *ctx) {
    map_set *s = (map_set *)ctx;
    if (s->n >= s->cap) return OK;
    size_t pl = (size_t)$len(t->path);
    if (pl >= MAP_PATH_MAX) return OK;
    if ($len(t->sha) != 40) return OK;

    map_branch *b = &s->v[s->n++];
    zerop(b);
    if (PATHu8bAren(s->arena, b->path, t->path) != OK) { s->n--; return OK; }
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

//  Intern a slice into `arena`: one-shot bAren copies bytes and snapshots
//  the populated range as `out`.  Silent on overflow (out stays empty).
static void map_intern(u8bp arena, u8csp out, u8csc src) {
    if (u8csEmpty(src)) return;
    (void)u8bAren(arena, out, src);
}

// --- Main -----------------------------------------------------------

static ok64 grafmap_render(u8b strs_arena, map_branch *kept, u32 nk);

ok64 GRAFMap(uricp u) {
    sane(!BNULL(HOME.root));
    (void)u;

    //  Per-call string arena: path / summary / author bytes interned
    //  here.  Every u8cs in map_branch / map_commit points into it.
    a_carve(u8, strs_arena, 1UL << 20);

    //  Resolve current branch — sourced from `--at <root>?<branch>#<sha>`
    //  forwarded by `be` and parked in HOME.cur_branch by HOMEOpen.
    //  Empty branch == trunk; strip the canonical leading '?' and
    //  trailing '/' for prefix-matching against KEEPEachTip's paths.
    u8cs cur_path = {};
    {
        a_dup(u8c, cb, u8bData(HOME.cur_branch));
        if (!u8csEmpty(cb) && *cb[0] == '?') u8csUsed1(cb);
        if (!u8csEmpty(cb) && *u8csLast(cb) == '/') u8csShed1(cb);
        if (u8csLen(cb) > 0) {
            (void)PATHu8bAren(strs_arena, cur_path, cb);
        }
    }

    //  Stack arrays for the bounded per-branch tables (~3.5KB each).
    //  Path bytes inside map_branch.path point into strs_arena.
    map_branch all[MAP_MAX_BRANCHES]  = {};
    map_branch kept[MAP_MAX_BRANCHES] = {};
    map_set s = {.v = all, .n = 0, .cap = MAP_MAX_BRANCHES,
                 .arena = strs_arena};
    call(KEEPEachTip, map_collect_cb, &s);

    //  Filter window: current branch's ancestors-to-trunk ∪ descendants.
    u32 nk = 0;
    for (u32 i = 0; i < s.n && nk < MAP_MAX_BRANCHES; i++) {
        map_branch const *b = &all[i];
        u8csc bp = {b->path[0], b->path[1]};
        u8csc cp = {cur_path[0], cur_path[1]};
        b8 anc  = map_is_ancestor_of(bp, cp);
        b8 desc = map_is_ancestor_of(cp, bp);
        if (anc || desc) kept[nk++] = *b;
    }
    //  Column order: shallow-first (trunk left), then lex within depth.
    qsort(kept, nk, sizeof(*kept), map_branch_cmp_for_columns);

    //  GRAFOpen is idempotent — close only if we owned the open.
    //  MEM-040: every post-open error path leaks the ~18 MB owned graf
    //  open.  Funnel the post-open body through grafmap_render() so the
    //  `if (own_graf) GRAFClose()` epilogue runs on EVERY exit (the
    //  body's a_carve / call early-returns can't bypass it anymore).
    ok64 go = GRAFOpen(NO);
    b8 own_graf = (go == OK);
    if (go != OK && go != GRAFOPEN && go != GRAFOPENRO)
        return go;
    ok64 rr = grafmap_render(strs_arena, kept, nk);
    if (own_graf) GRAFClose();
    return rr;
}

//  Post-open render body for GRAFMap (MEM-040).  Split out so its
//  fallible a_carve / call sites return through GRAFMap's owned-open
//  GRAFClose epilogue instead of leaking the open.
static ok64 grafmap_render(u8b strs_arena, map_branch *kept, u32 nk) {
    sane(kept && strs_arena);
    //  Walk every kept branch's idx pups into graf's PAST/DATA so
    //  subsequent DAG queries span every branch.  Mirror on keeper
    //  so per-branch commit bodies (KEEPGet) resolve.  Map is
    //  read-only — the keeper switch is safe (no in-flight pack).
    for (u32 i = 0; i < nk; i++) {
        a_dup(u8c, br, kept[i].path);
        call(GRAFSwitchBranch, br);
        call(KEEPSwitchBranch, br);
    }
    for (u32 i = 0; i < nk; i++) {
        __ = wh128bAcquire(ABC_BASS, kept[i].ancestors, MAP_ANC_SIZE);
        if (__ != OK) return __;
        zerob(kept[i].ancestors);  // hash set — must be zero-init
        kept[i].anc_init = YES;
        wh128css runs = {NULL, NULL};
        GRAFRuns(runs);
        DAGAncestors(kept[i].ancestors, runs, kept[i].tip_h40);
    }

    //  Union all hashlets.  Deepest-claiming branch owns each commit
    //  (kept[] is shallow→deep; first match in kept[] order wins).
    a_carve(wh128, union_set, MAP_ANC_SIZE);
    zerob(union_set);
    if (nk > 0) {
        u64 tips[MAP_MAX_BRANCHES];
        for (u32 i = 0; i < nk; i++) tips[i] = kept[i].tip_h40;
        wh128css runs = {NULL, NULL};
        GRAFRuns(runs);
        DAGAncestorsOfMany(union_set, runs, tips, nk);
    }

    //  commits[] is 320KB — too large for the stack; carve on BASS.
    a_carve(u8, commits_buf, MAP_MAX_COMMITS * sizeof(map_commit));
    map_commit *commits = (map_commit *)u8bDataHead(commits_buf);
    u32 ncommits = 0;

    a_carve(u8, cbuf, MAP_OBJ_BUF);

    //  Walk the union: records live as hashtable slots (key == 0 ==
    //  empty), not packed.  Walk the full slot range, skip empties.
    //  Owner = SHALLOWEST branch whose ancestor set contains the
    //  commit; kept[] is shallow→deep so the first match wins.
    {
        wh128cp ub = (wh128cp)wh128bHead(union_set);
        wh128cp ue = (wh128cp)wh128bTerm(union_set);
        for (wh128cp r = ub; r < ue && ncommits < MAP_MAX_COMMITS; r++) {
            if (r->key == 0) continue;
            u64 h40 = DAGHashlet(r->key);
            if (h40 == 0) continue;

            u8bReset(cbuf);
            u8 ot = 0;
            if (KEEPGet(h40, DAG_H60_HEXLEN, cbuf, &ot) != OK
                || ot != DOG_OBJ_COMMIT) continue;
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
    }

    //  Topological order (parents → children), then reverse to render
    //  newest-first like `git log`.  Author-ts sorting is wrong here
    //  because timezone / clock skew can place a commit's author-ts
    //  AFTER its descendants (e.g. a +0200 commit at "08:57" looks
    //  newer than a UTC descendant at "08:30").  Topology is the only
    //  truth on a linear chain; for unrelated branches the topo-sort's
    //  internal tiebreak picks a stable order.
    a_carve(u8, topo_buf, MAP_MAX_COMMITS * sizeof(u64));
    u64 *topo = (u64 *)u8bDataHead(topo_buf);
    wh128css topo_runs = {NULL, NULL};
    GRAFRuns(topo_runs);
    u32 ntopo = DAGTopoSort(topo, MAP_MAX_COMMITS, union_set, topo_runs);

    //  Reorder commits[] in-place: for each topo entry (walked in
    //  reverse so newest is row 0), find its matching commit and swap
    //  to the next output slot.  O(ncommits²) worst case but ncommits
    //  ≤ MAP_MAX_COMMITS = 4096 so it stays sub-millisecond.
    u32 out_i = 0;
    for (u32 ti = ntopo; ti-- > 0 && out_i < ncommits; ) {
        u64 want_h = topo[ti];
        for (u32 ci = out_i; ci < ncommits; ci++) {
            if (commits[ci].commit_h40 == want_h) {
                if (ci != out_i) {
                    map_commit tmp = commits[out_i];
                    commits[out_i] = commits[ci];
                    commits[ci] = tmp;
                }
                out_i++;
                break;
            }
        }
    }
    //  Any commits the topo-sort didn't see (shouldn't happen — every
    //  commit lives in union_set) keep their tail position; sort the
    //  tail by author-ts as a fallback so the render stays deterministic.
    if (out_i < ncommits) {
        qsort(commits + out_i, ncommits - out_i,
              sizeof(*commits), map_commit_cmp_desc);
    }

    //  Render — one row per commit: glyph-per-branch + sha7 + 7-date
    //  + branch + summary.  TLV mode also emits a toks stream so bro
    //  can attach a `diff:?<sha>` U-token to each sha7 anchor;
    //  plain-text mode keeps toks empty.
    a_carve(u8, text, 1UL << 20);
    b8  want_toks = (HUNKMode == HUNKOutTLV);
    Bu32 toks_buf = {};
    if (want_toks) {
        __ = u32bAcquire(ABC_BASS, toks_buf, ncommits * 4);
        if (__ != OK) return __;
    }

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
        //  Short sha hashlet — anchor for the click-to-diff U-token.
        a_pad(u8, hashlet, SHA1_HASHLEN_LEN);
        (void)SHA1u8sFeedHashlet(hashlet_idle, &mc->csha);
        (void)u8bFeed(text, u8bDataC(hashlet));
        if (want_toks) {
            (void)u32bFeed1(toks_buf, tok32Pack('L', (u32)u8bDataLen(text)));
            GRAFPackUriDiffSha(toks_buf, text, &mc->csha);
        }
        (void)u8bFeed1(text, ' ');
        //  7-char date.
        a_pad(u8, date, 8);
        (void)DOGutf8sFeedDate(date_idle,
                               map_ron60_to_secs(mc->ts), now);
        (void)u8bFeed(text, u8bDataC(date));
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

    //  Emit one hunk via GRAFHunkEmit; HUNKu8sFeedOut dispatches off
    //  HUNKMode (TLV → bro, Color → ANSI, Plain → text).
    call(GRAFArenaInit);
    a_pad(u8, title, 16);
    a_cstr(prefix, "map:");
    (void)u8bFeed(title, prefix);
    hunk hk = {};
    hk.uri[0]  = u8bDataHead(title);
    hk.uri[1]  = u8bIdleHead(title);
    hk.text[0] = u8bDataHead(text);
    hk.text[1] = u8bIdleHead(text);
    if (want_toks) {
        hk.toks[0] = (tok32 const *)u32bDataHead(toks_buf);
        hk.toks[1] = (tok32 const *)u32bIdleHead(toks_buf);
    }
    (void)GRAFHunkEmit(&hk, NULL);
    done;
}
