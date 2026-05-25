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

#include "dog/HUNK.h"
#include "dog/ULOG.h"
#include "dog/tok/TOK.h"

#include "AT.h"
#include "CLASS.h"
#include "SNIFF.h"

#define LS_TEXT_CAP   (1UL << 22)   // 4 MiB body, mmap-backed
#define LS_TOKS_CAP   (1UL << 18)   // 256 K tok32 entries

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
    Bu8      text;         // accumulating hunk body
    Bu32     toks;         // accumulating column / 'U' tags
    i64      now;          // for DOGutf8sFeedDate
} ls_ctx;

//  Pack a tok32 covering [last_end, current_end) with tag `tag`.
//  Buffer-end offsets are u32; the LSM-style cap (LS_TEXT_CAP, 4 MiB)
//  stays under the 2^28 tok32 offset budget.
static void ls_pack(ls_ctx *c, u8 tag) {
    (void)u32bFeed1(c->toks, tok32Pack(tag, (u32)u8bDataLen(c->text)));
}

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

//  Append one row's text + toks.  Caller has already classified the
//  step (verb chosen, ts resolved); this just renders.  Columns are
//  space-padded — bro counts each visible char as one cp for
//  click→byte mapping, so tabs would drift visual vs byte (tabs
//  expand on the terminal) and clicks on the path would miss the
//  F-span entirely.  `lsr:` additionally indents the path column by
//  depth*4 so the listing reads as a tree.
//
//  Layout (one row):
//    <7-date> <3-verb> [<indent>]<path>[ -> <dst>]\n<nav-uri>
//      └tag 'L'└tag verb's-slot └tag 'F'                 └tag 'U' (invisible)
static void ls_emit_row(ls_ctx *c, u8cs path, u8cs mov_dst, ron60 ts,
                        ron60 verb) {
    //  Static space slice — reused for column-pad and tree-indent.
    //  32 cols covers 8 levels of tree indent (lsr: depth 0..8) plus
    //  column-pad headroom.
    a_cstr(LS_SP, "                                ");

    //  Date column: 7 cols.  DOGutf8sFeedDate centre-pads to 7; empty
    //  ts → 7 spaces.
    if (ts) {
        a_pad(u8, date, 8);
        struct tm tm = {};
        if (RONToTime(ts, &tm, NULL) == OK) {
            time_t t = mktime(&tm);
            (void)DOGutf8sFeedDate(date_idle, (i64)t, c->now);
        }
        (void)u8bFeed(c->text, u8bDataC(date));
    } else {
        u8cs sp7 = {LS_SP[0], LS_SP[0] + 7};
        (void)u8bFeed(c->text, sp7);
    }
    ls_pack(c, 'L');
    (void)u8bFeed1(c->text, ' ');
    ls_pack(c, 'S');

    //  Verb column: 3 cols, left-justified.  Every verb in our set is
    //  2–3 chars; pad short ones ("eq") with trailing spaces so the
    //  path column starts at a fixed byte offset.  Tag = palette slot.
    {
        a_pad(u8, vbuf, 16);
        (void)RONutf8sFeed(vbuf_idle, verb);
        a_dup(u8c, vs, u8bDataC(vbuf));
        (void)u8bFeed(c->text, vs);
        size_t need = ($len(vs) < 3) ? 3 - $len(vs) : 0;
        u8cs pad = {LS_SP[0], LS_SP[0] + need};
        (void)u8bFeed(c->text, pad);
    }
    ls_pack(c, ULOGVerbTag(verb));
    (void)u8bFeed1(c->text, ' ');
    ls_pack(c, 'S');

    //  Path column.  In recursive (lsr:) mode, prepend `depth*4` spaces
    //  so descendants form a visible tree.  The indent stays inside
    //  the F span — clicks anywhere on the row's path column still
    //  resolve to the U-tagged navigation URI immediately after.
    //  `mov` rows render `<src> -> <dst>` inline.
    if (c->recurse) {
        u32 ind = ls_depth(c->prefix, path) * 4;
        if (ind > 32) ind = 32;
        u8cs ip = {LS_SP[0], LS_SP[0] + ind};
        (void)u8bFeed(c->text, ip);
    }
    (void)u8bFeed(c->text, path);
    if (!u8csEmpty(mov_dst)) {
        a_cstr(arrow, " -> ");
        (void)u8bFeed(c->text, arrow);
        (void)u8bFeed(c->text, mov_dst);
    }
    (void)u8bFeed1(c->text, '\n');
    ls_pack(c, 'F');

    //  Invisible navigation URI — covered by a 'U' tok so plain/color
    //  renderers skip the bytes and TLV consumers get a click target.
    if (verb == c->v.v_dir) {
        a_cstr(s, "ls:"); (void)u8bFeed(c->text, s);
        (void)u8bFeed(c->text, path);
    } else if (!u8csEmpty(mov_dst)) {
        a_cstr(s, "cat:"); (void)u8bFeed(c->text, s);
        (void)u8bFeed(c->text, mov_dst);
    } else {
        a_cstr(s, "cat:"); (void)u8bFeed(c->text, s);
        (void)u8bFeed(c->text, path);
    }
    ls_pack(c, 'U');
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
        b8 is_bump = (u8csLen(frag) == 40 && HEXu8sValid(frag));
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
            //  mtime stamp-set hit → baseline content, render `eq`.
            //  Stamp-miss but bytes hash equal to the baseline blob is
            //  the "touched-unchanged" case (mtime drift only); also
            //  `eq`.  Anything else is a real `mod`.  Same three-way
            //  classification bare `be` uses (sniff/SNIFF.exe.c).
            if (step->wt_rec && SNIFFAtKnown(step->wt_rec->ts))
                ls_emit_row(c, path, empty, step->wt_rec->ts, c->v.v_eq);
            else if (CLASSWtEqBase(c->reporoot, step->base_rec, path))
                ls_emit_row(c, path, empty, step->wt_rec->ts, c->v.v_eq);
            else
                ls_emit_row(c, path, empty, step->wt_rec->ts, c->v.v_mod);
            break;
    }
    return OK;
}

// =====================================================================
//  Entry point
// =====================================================================

static ok64 ls_run(u8cs reporoot, uri const *u, b8 recurse) {
    sane(u);

    ls_ctx c = {.recurse = recurse, .now = (i64)time(NULL)};
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

    call(u8bMap,       c.text, LS_TEXT_CAP);
    call(u32bAllocate, c.toks, LS_TOKS_CAP);
    if (!recurse) call(u8bAllocate, c.dir_seen, 4096);

    //  TODO: `?ref` baseline override.  CLASS today resolves baseline
    //  via SNIFFAtCurTip; for `ls:?ref` we'd want SNIFFClassifyAt
    //  (sha1cp base_tree, class_cb, void *ctx).
    ok64 cr = SNIFFClassify(ls_step, &c);

    //  Build the listing's own URI (`ls:<prefix>` / `lsr:<prefix>`)
    //  and emit the one accumulated hunk.
    a_pad(u8, uri_buf, MAX_URI_LEN);
    if (recurse) { a_cstr(s, "lsr:"); (void)u8bFeed(uri_buf, s); }
    else         { a_cstr(s, "ls:");  (void)u8bFeed(uri_buf, s); }
    if (!u8csEmpty(c.prefix)) (void)u8bFeed(uri_buf, c.prefix);

    hunk hk = {};
    u8csMv (hk.uri,  u8bDataC(uri_buf));
    u8csMv (hk.text, u8bDataC(c.text));
    u32csMv(hk.toks, u32bDataC(c.toks));

    a_pad(u8, line, 4096);
    Bu8 big = {};
    ok64 mo = u8bMap(big, LS_TEXT_CAP + (1UL << 16));
    ok64 fo = (mo == OK)
            ? HUNKu8sFeedOut(u8bIdle(big), &hk)
            : HUNKu8sFeedOut(u8bIdle(line), &hk);
    //  Trim trailing blank lines.  HUNK's plain/color content-hunk
    //  renderer can emit 2–3 trailing newlines (the U-tagged invisible
    //  nav URI is the last raw byte, so the "ensure final \n" guard
    //  fires, then the unconditional inter-hunk separator adds
    //  another).  `ls:` emits ONE hunk per call — peel back to a
    //  single terminating \n.  TLV mode is binary, leave it alone.
    if (fo == OK && HUNKMode != HUNKOutTLV) {
        for (;;) {
            size_t dn = (mo == OK) ? u8bDataLen(big) : u8bDataLen(line);
            if (dn < 2) break;
            u8cs view = {};
            u8csTailS((mo == OK) ? u8bDataC(big) : u8bDataC(line),
                      view, 2);
            if (view[0][0] != '\n' || view[0][1] != '\n') break;
            if (mo == OK) u8bShed1(big);
            else          u8bShed1(line);
        }
    }
    if (fo == OK) (void)FILEout(mo == OK ? u8bDataC(big) : u8bDataC(line));
    if (mo == OK) u8bUnMap(big);

    if (!recurse) u8bFree(c.dir_seen);
    u32bFree(c.toks);
    u8bUnMap(c.text);
    return (cr == OK) ? fo : cr;
}

ok64 SNIFFLs (u8cs reporoot, uri const *u) { return ls_run(reporoot, u, NO);  }
ok64 SNIFFLsr(u8cs reporoot, uri const *u) { return ls_run(reporoot, u, YES); }
