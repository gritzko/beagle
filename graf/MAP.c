//  MAP: subway-map view of the branch tree for `be map:`.
//
//  Phase 1 (this file): enumerate every local branch via KEEPEachTip,
//  pick out the current branch (via sniff/at.log), filter to the
//  union of (a) the current branch's ancestor chain back to trunk
//  and (b) every descendant of current, sort the result by depth
//  then path, and emit one line per branch:
//
//      <depth-indent> ?<path>  <sha7>  <5-date>  <summary>
//
//  Phase 2 (TODO): replace the indent with proper subway glyphs —
//  ║ for trunk, ┃ for child-of-trunk, │ for grandchild+, with
//  ╠═━┓ / ┣━━┐ fork rows.  Phase 3 weaves in commit-timeline rows
//  (one row per commit, globally time-sorted) instead of one row
//  per branch.
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
#include "abc/URI.h"
#include "dog/AT.h"
#include "dog/DOG.h"
#include "dog/HUNK.h"
#include "keeper/GIT.h"
#include "keeper/KEEP.h"

#define MAP_MAX_BRANCHES  256
#define MAP_PATH_MAX      192
#define MAP_OBJ_BUF       (1UL << 20)

typedef struct {
    char path[MAP_PATH_MAX];   //  branch path (no leading `?`); "" = trunk
    u16  path_len;
    u8   sha[40];              //  40-hex tip sha
    u8   depth;                //  count of '/' in path; trunk = 0
    b8   keep;                 //  passed the ancestors+descendants filter
} map_branch;

typedef struct {
    map_branch *v;
    u32         n;
    u32         cap;
} map_set;

// --- Collection ------------------------------------------------------

static ok64 map_collect_cb(keep_tipcp t, void *ctx) {
    map_set *s = (map_set *)ctx;
    if (s->n >= s->cap) return OK;
    size_t pl = (size_t)$len(t->path);
    if (pl >= MAP_PATH_MAX) return OK;
    if ($len(t->sha) != 40) return OK;

    map_branch *b = &s->v[s->n++];
    memcpy(b->path, t->path[0], pl);
    b->path_len = (u16)pl;
    memcpy(b->sha, t->sha[0], 40);

    u8 d = 0;
    for (size_t i = 0; i < pl; i++) if (b->path[i] == '/') d++;
    b->depth = d;
    return OK;
}

// --- Filter: keep ancestors + self + descendants of `cur` -----------
//
//  ancestor: cur's path starts with branch's path + (`/` or end).
//  descendant: branch's path starts with cur's path + (`/` or end).
//  Trunk ("") is an ancestor of everything.

static b8 map_is_ancestor_of(char const *anc, u16 anc_len,
                             char const *desc, u16 desc_len) {
    if (anc_len == 0) return YES;          // trunk ancestors all
    if (anc_len > desc_len) return NO;
    if (memcmp(anc, desc, anc_len) != 0) return NO;
    return anc_len == desc_len || desc[anc_len] == '/';
}

static void map_apply_filter(map_set *s,
                             char const *cur, u16 cur_len) {
    for (u32 i = 0; i < s->n; i++) {
        map_branch *b = &s->v[i];
        b8 anc  = map_is_ancestor_of(b->path, b->path_len, cur, cur_len);
        b8 desc = map_is_ancestor_of(cur, cur_len, b->path, b->path_len);
        b->keep = anc || desc;
    }
}

// --- Ordering: depth ascending, path lexicographic ------------------

static int map_branch_cmp(void const *a_, void const *b_) {
    map_branch const *a = (map_branch const *)a_;
    map_branch const *b = (map_branch const *)b_;
    if (a->depth != b->depth) return (int)a->depth - (int)b->depth;
    size_t ml = a->path_len < b->path_len ? a->path_len : b->path_len;
    int c = memcmp(a->path, b->path, ml);
    if (c != 0) return c;
    return (int)a->path_len - (int)b->path_len;
}

// --- Per-branch tip-commit summary ---------------------------------

//  Pull the tip commit's body from the keeper singleton, extract
//  first line of message + author ts (for the 5-char date column).
//  Empty `summary` / ts=0 on any failure — caller renders "?".
static void map_tip_summary(u8 const *sha40,
                            u8 *summary, size_t summary_cap,
                            i64 *ts_out) {
    summary[0] = 0;
    *ts_out = 0;

    sha1 csha = {};
    u8s sb = {csha.data, csha.data + 20};
    u8cs hx = {sha40, sha40 + 40};
    if (HEXu8sDrainSome(sb, hx) != OK) return;

    Bu8 cbuf = {};
    if (u8bMap(cbuf, MAP_OBJ_BUF) != OK) return;
    u8 ot = 0;
    if (KEEPGetExact(&KEEP, &csha, cbuf, &ot) != OK || ot != DOG_OBJ_COMMIT) {
        u8bUnMap(cbuf);
        return;
    }

    a_dup(u8c, body, u8bData(cbuf));
    u8cs field = {}, value = {};
    u8cs message = {};
    while (GITu8sDrainCommit(body, field, value) == OK) {
        if (u8csEmpty(field)) { $mv(message, value); break; }
        a_cstr(fa, "author");
        if ($eq(field, fa)) {
            //  Trailing "ts tz" — pick out ts.
            u8cp gt = value[0];
            while (gt < value[1] && *gt != '>') gt++;
            if (gt < value[1]) gt++;
            while (gt < value[1] && *gt == ' ') gt++;
            i64 ts = 0;
            while (gt < value[1] && *gt >= '0' && *gt <= '9') {
                ts = ts * 10 + (*gt - '0');
                gt++;
            }
            *ts_out = ts;
        }
    }

    //  First non-empty line of the message → summary.
    u8cp ms = message[0];
    while (ms < message[1] && (*ms == '\n' || *ms == '\r')) ms++;
    u8cp me = ms;
    while (me < message[1] && *me != '\n' && *me != '\r') me++;
    size_t n = (size_t)(me - ms);
    if (n >= summary_cap) n = summary_cap - 1;
    if (n > 0) memcpy(summary, ms, n);
    summary[n] = 0;

    u8bUnMap(cbuf);
}

// --- Render ----------------------------------------------------------

ok64 GRAFMap(uricp u) {
    sane(KEEP.h != NULL);
    (void)u;   // Phase 1 ignores the URI's path/query/frag.

    //  Resolve the wt's current branch (path only — sha not needed
    //  here; we'll use it in Phase 3 for the timeline anchor).
    a_pad(u8, cur_branch_buf, 256);
    a_pad(u8, cur_sha_buf, 64);
    a_dup(u8c, root_s, u8bDataC(KEEP.h->root));
    char cur_path[MAP_PATH_MAX] = {0};
    u16  cur_len = 0;
    if (DOGAtTail(cur_branch_buf, cur_sha_buf, root_s) == OK) {
        a_dup(u8c, cb, u8bData(cur_branch_buf));
        size_t cl = (size_t)$len(cb);
        if (cl > 0 && cb[0][0] == '?') { cb[0]++; cl--; }
        if (cl >= MAP_PATH_MAX) cl = MAP_PATH_MAX - 1;
        memcpy(cur_path, cb[0], cl);
        cur_len = (u16)cl;
    }

    //  Pull the branch list.
    map_branch *vec = (map_branch *)calloc(MAP_MAX_BRANCHES, sizeof(*vec));
    if (!vec) fail(GRAFFAIL);
    map_set s = {.v = vec, .n = 0, .cap = MAP_MAX_BRANCHES};
    call(KEEPEachTip, &KEEP, map_collect_cb, &s);

    map_apply_filter(&s, cur_path, cur_len);
    qsort(s.v, s.n, sizeof(*s.v), map_branch_cmp);

    //  TLV mode: build a single hunk's text + emit.  Plain mode:
    //  write directly.  No syntax-tag toks yet — Phase 2 wires those.
    b8 tlv = (graf_emit == HUNKu8sFeed);
    if (tlv) call(GRAFArenaInit);

    Bu8 text = {};
    call(u8bAllocate, text, 1UL << 16);

    for (u32 i = 0; i < s.n; i++) {
        map_branch const *b = &s.v[i];
        if (!b->keep) continue;

        //  depth-indent: 2 spaces per level.  (Subway glyphs replace
        //  this in Phase 2.)
        for (u8 d = 0; d < b->depth; d++) (void)u8bFeed(text,
            ((u8cs){(u8 const *)"  ", (u8 const *)"  " + 2}));

        //  Marker for current branch.
        b8 is_cur = (b->path_len == cur_len) &&
                    (cur_len == 0 ||
                     memcmp(b->path, cur_path, cur_len) == 0);
        u8 marker = is_cur ? '*' : '?';
        (void)u8bFeed1(text, marker);
        u8cs path_s = {(u8 const *)b->path,
                       (u8 const *)b->path + b->path_len};
        if (b->path_len) (void)u8bFeed(text, path_s);
        (void)u8bFeed1(text, '\t');

        //  sha7 + 5-char date + summary
        u8cs sha7 = {b->sha, b->sha + 7};
        (void)u8bFeed(text, sha7);
        (void)u8bFeed1(text, ' ');

        i64 ts = 0;
        u8 summary[128] = {0};
        map_tip_summary(b->sha, summary, sizeof(summary), &ts);

        u8 date_buf[8];
        u8s date_into = {date_buf, date_buf + sizeof(date_buf)};
        u8cp date_start = date_into[0];
        i64 now = (i64)time(NULL);
        (void)DOGutf8sFeedDate(date_into, ts, now);
        u8cs date_slice = {date_start, date_into[0]};
        (void)u8bFeed(text, date_slice);
        (void)u8bFeed1(text, ' ');

        if (summary[0]) {
            u8cs sum_s = {summary, summary + strlen((char *)summary)};
            (void)u8bFeed(text, sum_s);
        }
        (void)u8bFeed1(text, '\n');
    }

    if (!tlv) {
        a_dup(u8c, bytes, u8bData(text));
        int fd = (graf_out_fd >= 0) ? graf_out_fd : STDOUT_FILENO;
        (void)FILEFeedAll(fd, bytes);
    } else {
        a_pad(u8, title, 32);
        a_cstr(prefix, "map:");
        (void)u8bFeed(title, prefix);
        hunk hk = {};
        hk.uri[0]  = u8bDataHead(title);
        hk.uri[1]  = u8bIdleHead(title);
        hk.text[0] = u8bDataHead(text);
        hk.text[1] = u8bIdleHead(text);
        (void)GRAFHunkEmit(&hk, NULL);
    }

    u8bFree(text);
    free(vec);
    done;
}
