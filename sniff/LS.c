//  LS — `ls:` (one-level) / `lsr:` (recursive) projector.  See LS.h.
//
//  Emits ONE content hunk per invocation:
//    uri  = `ls:<prefix>` or `lsr:<prefix>` (the listing's own address)
//    text = per-row `<date>\t<verb>\t<path>[ -> <dst>]\n` + invisible
//           navigation-URI bytes (one per row)
//    toks = per-column syntax tags ('L' date, the verb's palette
//           letter, 'F' path) + a `'U'`-tagged span over each row's
//           hidden navigation URI (`cat:<path>`, `ls:<sub>/`, or
//           `cat:<dst>` for mov rows).
//  HUNKu8sFeedOut picks plain/color/TLV from the global HUNKMode; the
//  'U' bytes stay invisible in plain mode (hunk_feed_visible skips
//  them) and become click targets in TLV mode.

#include "LS.h"

#include <string.h>
#include <time.h>

#include "abc/B.h"
#include "abc/BUF.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/RON.h"

#include "dog/DOG.h"
#include "dog/HUNK.h"
#include "dog/ROWS.h"
#include "dog/ULOG.h"
#include "dog/tok/TOK.h"

#include "AT.h"
#include "CLASS.h"
#include "SNIFF.h"

// --- Local verb cache --------------------------------------------------
//
//  `v_dir` is the one-level collapsed-subdir verb (`dir`, palette slot
//  'D' via ULOG_VERB_TAGS in dog/ULOG.c).

typedef struct {
    ron60 v_put, v_new, v_mov, v_mod, v_del, v_mis, v_unk, v_eq, v_dir;
} ls_verbs;

// =====================================================================
//  Accumulator
// =====================================================================

typedef struct {
    u8cs     prefix;       // listing scope ("<path>/" or empty)
    u8cs     reporoot;     // for CLASSWtEqBase content-compare
    b8       recurse;      // YES = lsr:, NO = ls:
    Bu8      dir_seen;     // one-level dedup: last emitted subdir slice
    ls_verbs v;
    rows    *rows;         // shared row-table accumulator (dog/ROWS)
} ls_ctx;

//  Count '/' separators in `path` after stripping `prefix`.  Used by
//  `lsr:` to indent descendants by depth, building a visible tree.
//  Returns 0 if `path` is shorter than `prefix` (defensive — caller
//  has already passed the prefix-scope check).
static u32 ls_depth(u8cs prefix, u8cs path) {
    size_t plen = (size_t)$len(prefix);
    if ((size_t)$len(path) < plen) return 0;
    u32 d = 0;
    for (u8c *p = path[0] + plen; p < path[1]; p++)
        if (*p == '/') d++;
    return d;
}

//  Append one row via the shared `dog/ROWS` builder.  Caller has
//  already classified the step (verb chosen, ts resolved); this only
//  describes how to render it.  The ls layout is: path tag 'F', moves
//  joined with ` -> ` (`arrow=YES`), `lsr:` indents the path column by
//  depth*4 cols (a visible tree; one-level `ls:` never indents), and
//  the hidden nav URI is `ls:` for a collapsed subdir / `cat:` for a
//  file or move-dst.  ROWS handles the date/verb columns, the 'L'/verb
//  /'F'/'U' toks, and (mode-keyed) the live-stream-or-buffer decision.
static void ls_emit_row(ls_ctx *c, u8cs path, u8cs mov_dst, ron60 ts,
                        ron60 verb) {
    b8 is_dir = (verb == c->v.v_dir);
    rows_row row = {
        .ts = ts, .verb = verb,
        .path_tag = 'F', .arrow = YES,
        .indent = c->recurse ? ls_depth(c->prefix, path) * 4 : 0,
        .nav = is_dir ? ROWS_NAV_LS : ROWS_NAV_CAT,
    };
    u8csMv(row.path, path);
    u8csMv(row.mov_dst, mov_dst);
    //  dir rows nav into the subdir listing; move rows cat the dst;
    //  plain file rows cat the path.
    if (is_dir)                   u8csMv(row.nav_target, path);
    else if (!u8csEmpty(mov_dst)) u8csMv(row.nav_target, mov_dst);
    else                          u8csMv(row.nav_target, path);
    (void)ROWSu8bFeedRow(c->rows, &row);
}

//  One-level mode: collapse anything below the prefix dir's immediate
//  children into a single `dir` row per subdir.  Returns YES iff the
//  step was absorbed.
static b8 ls_one_level_dir(ls_ctx *c, u8cs path) {
    size_t plen = (size_t)$len(c->prefix);
    u8c *rel_lo = path[0] + plen;
    u8c *rel_hi = path[1];
    if (rel_lo >= rel_hi) return NO;
    u8c *slash = (u8c *)memchr(rel_lo, '/', (size_t)(rel_hi - rel_lo));
    if (slash == NULL) return NO;
    u8cs dir_full = {path[0], slash + 1};
    a_dup(u8c, last, u8bData(c->dir_seen));
    if (u8csEq(last, dir_full)) return YES;
    u8bReset(c->dir_seen);
    u8bFeed(c->dir_seen, dir_full);
    u8cs empty = {NULL, NULL};
    ls_emit_row(c, dir_full, empty, 0, c->v.v_dir);
    return YES;
}

//  CLASS callback — same bucket decisions as sniff/SNIFF.exe.c::status_step,
//  but appends to the accumulator instead of pushing per-row hunks.
static ok64 ls_step(class_step const *step, void *ctx_) {
    ls_ctx *c = (ls_ctx *)ctx_;
    u8cs path = {step->path[0], step->path[1]};
    if (!u8csHasPrefix(path, c->prefix)) return OK;
    if (!c->recurse && ls_one_level_dir(c, path)) return OK;

    u8cs empty = {NULL, NULL};
    if (step->del_rec != NULL) {
        ls_emit_row(c, path, empty, step->del_rec->ts, c->v.v_del);
        return OK;
    }
    if (step->put_rec != NULL) {
        u8cs frag = {step->put_rec->uri.fragment[0],
                     step->put_rec->uri.fragment[1]};
        ron60 ts = step->put_rec->ts;
        b8 is_bump = DOGIsFullSha(frag);
        if (!u8csEmpty(frag) && !is_bump) {
            ls_emit_row(c, path, frag, ts, c->v.v_mov);
            return OK;
        }
        ron60 verb = (step->kind == CLASS_BOTH ||
                      step->kind == CLASS_BASE_ONLY)
                     ? c->v.v_put : c->v.v_new;
        ls_emit_row(c, path, empty, ts, verb);
        return OK;
    }
    switch (step->kind) {
        case CLASS_WT_ONLY:
            if (step->wt_rec && SNIFFAtKnown(step->wt_rec->ts))
                ls_emit_row(c, path, empty, step->wt_rec->ts, c->v.v_new);
            else
                ls_emit_row(c, path, empty,
                            step->wt_rec ? step->wt_rec->ts : 0,
                            c->v.v_unk);
            break;
        case CLASS_BASE_ONLY:
            ls_emit_row(c, path, empty, 0, c->v.v_mis);
            break;
        case CLASS_BOTH:
            //  CLASSWtState is the one body of truth shared with bare
            //  `be` status (sniff/SNIFF.exe.c).  CLEAN / PATCHED bytes
            //  list as `eq`; a content-confirmed change lists as `mod`.
            //  Content-confirmed, never mtime-only — a restored-stamp
            //  mtime over edited bytes still lists as `mod` (DIS-023).
            if (CLASSWtState(c->reporoot, step) == CLASS_WT_MODIFIED)
                ls_emit_row(c, path, empty, step->wt_rec->ts, c->v.v_mod);
            else
                ls_emit_row(c, path, empty, step->wt_rec->ts, c->v.v_eq);
            break;
    }
    return OK;
}

// =====================================================================
//  Entry point
// =====================================================================

//  Acquire the `ls:` one-level dedup buffer (`dir_seen`).  `lsr:`
//  doesn't collapse subdirs, so it needs nothing here.  The row table's
//  text/toks now live in `dog/ROWS` (ROWSOpen owns + unwinds them);
//  this is the sole ls-local scratch.  Exposed for the leak-repro test.
ok64 SNIFFLsBufsAcquire(Bu8 dir_seen, b8 recurse) {
    sane(dir_seen != NULL);
    if (!recurse) call(u8bAllocate, dir_seen, 4096);
    done;
}

static ok64 ls_run(u8cs reporoot, uri const *u, b8 recurse) {
    sane(u);

    ls_ctx c = {.recurse = recurse};
    u8csMv(c.reporoot, reporoot);
    #define LSV(field, lit) do {                       \
        a_cstr(_s, lit); a_dup(u8c, _d, _s);           \
        c.v.field = SNIFFAtVerbOf(_d);                 \
    } while (0)
    LSV(v_put, "put");
    LSV(v_new, "new");
    LSV(v_mov, "mov");
    LSV(v_mod, "mod");
    LSV(v_del, "del");
    LSV(v_mis, "mis");
    LSV(v_unk, "unk");
    LSV(v_eq,  "eq");
    LSV(v_dir, "dir");
    #undef LSV

    u8csMv(c.prefix, u->path);

    //  The listing's own URI (`ls:<prefix>` / `lsr:<prefix>`) heads the
    //  one module hunk; whole-table consumers always batch one hunk.
    a_pad(u8, uri_buf, MAX_URI_LEN);
    if (recurse) { a_cstr(s, "lsr:"); (void)u8bFeed(uri_buf, s); }
    else         { a_cstr(s, "ls:");  (void)u8bFeed(uri_buf, s); }
    if (!u8csEmpty(c.prefix)) (void)u8bFeed(uri_buf, c.prefix);

    rows r = {};
    call(ROWSOpen, &r, u8bDataC(uri_buf), 0, 0, ROWS_BATCH);
    c.rows = &r;

    ok64 ba = SNIFFLsBufsAcquire(c.dir_seen, recurse);
    if (ba != OK) { (void)ROWSClose(&r); return ba; }

    //  TODO: `?ref` baseline override.  CLASS today resolves baseline
    //  via SNIFFAtCurTip; for `ls:?ref` we'd want SNIFFClassifyAt
    //  (sha1cp base_tree, class_cb, void *ctx).
    ok64 cr = SNIFFClassify(ls_step, &c);

    //  Flush the one accumulated module hunk (ROWS_BATCH).
    ok64 fo = ROWSClose(&r);

    if (!recurse) u8bFree(c.dir_seen);
    return (cr == OK) ? fo : cr;
}

ok64 SNIFFLs (u8cs reporoot, uri const *u) { return ls_run(reporoot, u, NO);  }
ok64 SNIFFLsr(u8cs reporoot, uri const *u) { return ls_run(reporoot, u, YES); }
