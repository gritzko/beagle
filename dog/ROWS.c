//  ROWS — the shared status/action row-table builder.  See ROWS.h.
//
//  Extracted from `sniff/LS.c`'s row accumulator (BRO-002) so every
//  status/action reporter feeds one `(text, toks)` table and flushes
//  ONE content hunk per (sub)module.

#include "ROWS.h"

#include <time.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/BUF.h"
#include "abc/FILE.h"
#include "abc/PRO.h"
#include "abc/RON.h"

#include "dog/DOG.h"      // DOGutf8sFeedDate
#include "dog/HUNK.h"
#include "dog/ULOG.h"
#include "dog/tok/TOK.h"

//  Module-global "active table": the accumulator a producer's per-event
//  emitters append to.  Set between ROWSOpen and ROWSClose so the
//  scattered emit sites (get_write_one, force_orphan_cb, …) keep calling
//  one append function with no ctx threading.  NULL → ROWSPrintRow runs
//  its own transient one-row accumulator (legacy single-row sites).
//  Sniff/keeper are single-threaded per process, so one global is safe.
static rows *ROWS_ACTIVE = NULL;

//  Pack a tok32 covering [last_end, current_end) with tag `tag`.
//  Mirror of `sniff/LS.c::ls_pack`.  Buffer-end offsets are u32; the
//  4 MiB ROWS_TEXT_CAP stays under the 2^28 tok32 offset budget.
static void rows_pack(rows *r, u8 tag) {
    (void)u32bFeed1(r->toks, tok32Pack(tag, (u32)u8bDataLen(r->text)));
}

//  Resolve a ron60 wall-clock stamp to unix-epoch seconds for
//  DOGutf8sFeedDate.  isdst=-1 lets mktime resolve DST (SUBS-003 — a 0
//  would shift a summer stamp +1h).  0 ts → 0 (the "?" placeholder).
static i64 rows_ron60_to_secs(ron60 ts) {
    if (ts == 0) return 0;
    struct tm t = {};
    if (RONToTime(ts, &t, NULL) != OK) return 0;
    t.tm_isdst = -1;
    time_t s = mktime(&t);
    return s == (time_t)-1 ? 0 : (i64)s;
}

//  Append one row's text + toks to the accumulator.  Layout (one row):
//    <7-date> <3-verb> [<indent>]<path>[<mov><dst>]\n<nav-uri>
//      └tag 'L'└verb-slot └path_tag                  └tag 'U' (invisible)
//  Column spaces inherit the neutral 'S' tag so click→byte mapping in
//  bro counts each visible char as one cp.
static void rows_emit(rows *r, rows_row const *row) {
    a_cstr(SP, "                                ");   // 32 spaces

    //  Date column: 7 cols (DOGutf8sFeedDate centre-pads to 7); empty
    //  ts → 7 spaces.
    if (row->ts) {
        a_pad(u8, date, 8);
        i64 secs = rows_ron60_to_secs(row->ts);
        if (secs > 0) (void)DOGutf8sFeedDate(date_idle, secs, r->now);
        (void)u8bFeed(r->text, u8bDataC(date));
    } else {
        u8cs sp7 = {SP[0], SP[0] + 7};
        (void)u8bFeed(r->text, sp7);
    }
    rows_pack(r, 'L');
    (void)u8bFeed1(r->text, ' ');
    rows_pack(r, 'S');

    //  Verb column: 3 cols, left-justified, space-padded.  Tag = the
    //  verb's palette slot (ULOGVerbTag).
    {
        a_pad(u8, vbuf, 16);
        (void)RONutf8sFeed(vbuf_idle, row->verb);
        a_dup(u8c, vs, u8bDataC(vbuf));
        (void)u8bFeed(r->text, vs);
        size_t need = ($len(vs) < 3) ? 3 - $len(vs) : 0;
        u8cs pad = {SP[0], SP[0] + need};
        (void)u8bFeed(r->text, pad);
    }
    rows_pack(r, ULOGVerbTag(row->verb));
    (void)u8bFeed1(r->text, ' ');
    rows_pack(r, 'S');

    //  Path column.  `lsr:` prepends depth*4 spaces (a visible tree);
    //  the indent stays inside the path span.  Moves render inline as
    //  `<src> -> <dst>` (ls:) or `<src>#<dst>` (status).
    {
        u32 ind = row->indent;
        if (ind > 32) ind = 32;
        u8cs ip = {SP[0], SP[0] + ind};
        (void)u8bFeed(r->text, ip);
    }
    (void)u8bFeed(r->text, row->path);
    if (!u8csEmpty(row->mov_dst)) {
        if (row->arrow) { a_cstr(arrow, " -> "); (void)u8bFeed(r->text, arrow); }
        else            (void)u8bFeed1(r->text, '#');
        (void)u8bFeed(r->text, row->mov_dst);
    }
    (void)u8bFeed1(r->text, '\n');
    rows_pack(r, row->path_tag);

    //  Invisible navigation URI — covered by a 'U' tok so plain/color
    //  renderers skip the bytes and TLV consumers get a click target.
    if (row->nav != ROWS_NAV_NONE) {
        switch (row->nav) {
            case ROWS_NAV_CAT:    { a_cstr(s, "cat:");    (void)u8bFeed(r->text, s); break; }
            case ROWS_NAV_DIFF:   { a_cstr(s, "diff:");   (void)u8bFeed(r->text, s); break; }
            case ROWS_NAV_LS:     { a_cstr(s, "ls:");     (void)u8bFeed(r->text, s); break; }
            case ROWS_NAV_COMMIT: { a_cstr(s, "commit:?");(void)u8bFeed(r->text, s); break; }
            default: break;
        }
        if (!u8csEmpty(row->nav_target)) (void)u8bFeed(r->text, row->nav_target);
        rows_pack(r, 'U');
    }
}

//  Trim trailing blank lines from a rendered hunk (plain/color).  The
//  content-hunk renderer can emit 2–3 trailing newlines (the U-tagged
//  invisible nav URI is the last raw byte, so the "ensure final \n"
//  guard fires, then the inter-hunk separator adds another).  Each
//  module emits ONE hunk — peel back to a single terminating \n.  TLV
//  mode is binary, leave it alone.  Mirror of `sniff/LS.c::ls_run`.
static void rows_trim(Bu8 big) {
    if (HUNKMode == HUNKOutTLV) return;
    for (;;) {
        if (u8bDataLen(big) < 2) break;
        u8cs view = {};
        u8csTailS(u8bDataC(big), view, 2);
        if (view[0][0] != '\n' || view[0][1] != '\n') break;
        u8bShed1(big);
    }
}

//  Render the accumulator's (text, toks) as ONE content hunk headed by
//  the module banner (uri / verb / ts) and push it to stdout.
static ok64 rows_flush_hunk(rows *r) {
    sane(r);
    if (u8bDataLen(r->text) == 0 && u8csEmpty(r->uri)) done;

    hunk hk = {.ts = r->ts, .verb = r->verb};
    u8csMv(hk.uri, r->uri);
    u8csMv(hk.text, u8bDataC(r->text));
    {
        tok32cs kv = {};
        kv[0] = (tok32c *)u32bDataHead(r->toks);
        kv[1] = (tok32c *)u32bDataHead(r->toks) + u32bDataLen(r->toks);
        u32csMv(hk.toks, kv);
    }
    a_carve(u8, big, ROWS_TEXT_CAP + (1UL << 16));
    ok64 fo = HUNKu8sFeedOut(u8bIdle(big), &hk);
    if (fo == OK) {
        rows_trim(big);
        (void)FILEFeedAll(r->fd, u8bDataC(big));
    }
    return fo;
}

//  Emit the module banner ONCE for a streaming table that carries a
//  state uri/verb/ts (BE-005 / GET-026).  The banner IS the hunk header,
//  single-sourced through HUNKu8sFeedBanner keyed on uri/verb/ts and the
//  global HUNKMode — so a streaming `be get` prints the same pale-yellow
//  state band a batching `be status` always does, before the file rows.
//  A table with no state (empty uri, no ts/verb) stays a flat progress
//  log (long-clone shape).  No-op once `banner_done`.
static ok64 rows_stream_banner(rows *r) {
    sane(r);
    if (r->banner_done) done;
    r->banner_done = YES;
    if (u8csEmpty(r->uri) && !r->ts && !r->verb) done;
    hunk hk = {.ts = r->ts, .verb = r->verb};
    u8csMv(hk.uri, r->uri);
    a_carve(u8, big, (1UL << 16));
    call(HUNKu8sFeedBanner, u8bIdle(big), &hk, HUNKMode, 0);
    (void)FILEFeedAll(r->fd, u8bDataC(big));
    done;
}

//  Emit a single row LIVE as a one-row content hunk (tty streaming).
//  The module banner (if any) is emitted once before the first row;
//  the row itself carries no header (ts/verb 0, uri empty) so a long
//  clone reads as a flat line-by-line progress log — the same shape the
//  old `ULOGPrintStatusLine` produced per event.
static ok64 rows_stream_one(rows *r, rows_row const *row) {
    sane(r);
    call(rows_stream_banner, r);
    u8bReset(r->text);
    u32bReset(r->toks);
    rows_emit(r, row);

    hunk hk = {};
    u8csMv(hk.text, u8bDataC(r->text));
    {
        tok32cs kv = {};
        kv[0] = (tok32c *)u32bDataHead(r->toks);
        kv[1] = (tok32c *)u32bDataHead(r->toks) + u32bDataLen(r->toks);
        u32csMv(hk.toks, kv);
    }
    a_carve(u8, big, (1UL << 16));
    ok64 fo = HUNKu8sFeedOut(u8bIdle(big), &hk);
    if (fo == OK) {
        rows_trim(big);
        (void)FILEFeedAll(r->fd, u8bDataC(big));
    }
    return fo;
}

ok64 ROWSOpen(rows *r, u8cs uri, ron60 verb, ron60 ts, ROWSdiscipline disc) {
    sane(r);
    *r = (rows){.now = (i64)time(NULL), .verb = verb, .ts = ts, .disc = disc,
                .fd = STDOUT_FILENO};
    u8csMv(r->uri, uri);
    call(u8bMap, r->text, ROWS_TEXT_CAP);
    ok64 to = u32bAllocate(r->toks, ROWS_TOKS_CAP);
    if (to != OK) { u8bUnMap(r->text); return to; }
    //  Mode-keyed: stream live on a tty (any non-TLV human mode); batch
    //  for the relay.  ROWS_BATCH always buffers (status / ls:).
    r->stream = (disc == ROWS_MODE_KEYED) && (HUNKMode != HUNKOutTLV);
    r->open = YES;
    ROWS_ACTIVE = r;
    done;
}

ok64 ROWSu8bFeedRow(rows *r, rows_row const *row) {
    sane(r && row);
    if (!r->open) fail(BNOROOM);
    if (r->stream) return rows_stream_one(r, row);
    rows_emit(r, row);
    done;
}

ok64 ROWSu8bFeedRec(rows *r, ulogreccp rec, ROWSnav nav) {
    sane(r && rec);
    u8cs path    = {rec->uri.path[0],     rec->uri.path[1]};
    u8cs mov_dst = {rec->uri.fragment[0], rec->uri.fragment[1]};
    u8cs query   = {rec->uri.query[0],    rec->uri.query[1]};
    rows_row row = {
        .ts = rec->ts, .verb = rec->verb,
        .path_tag = 'S', .arrow = NO, .indent = 0,
        .nav = nav,
    };
    if (nav == ROWS_NAV_COMMIT) {
        //  Commit-range row (COMMIT-001): the producer carries the sha
        //  in `uri.query` and the subject in `uri.fragment`.  Render the
        //  visible `?<sha>#<subject>` column (the tested banner grammar —
        //  `post  ?<hashlet>#<subject>`) by putting `?<sha>` in the path
        //  and the subject in mov_dst joined with `#` (arrow=NO), and add
        //  a hidden `commit:?<sha>` nav so the row is clickable in bro.
        a_carve(u8, qbuf, 256);
        (void)u8bFeed1(qbuf, '?');
        (void)u8bFeed(qbuf, query);
        u8csMv(row.path, u8bDataC(qbuf));
        u8csMv(row.mov_dst, mov_dst);          // subject (joined with '#')
        u8csMv(row.nav_target, query);         // sha → commit:?<sha>
    } else {
        u8csMv(row.path, path);
        u8csMv(row.mov_dst, mov_dst);
        //  Nav target: moves cat: the dst; everything else cat:/diff:
        //  the path.
        if (!u8csEmpty(mov_dst)) u8csMv(row.nav_target, mov_dst);
        else                     u8csMv(row.nav_target, path);
    }
    return ROWSu8bFeedRow(r, &row);
}

ok64 ROWSClose(rows *r) {
    sane(r);
    if (!r->open) done;
    ok64 fo = OK;
    //  Buffering accumulator → emit the one module hunk now.  Streaming
    //  accumulator already pushed each row; an EMPTY streaming result
    //  still owes its state banner (an up-to-date `be get` prints the
    //  band like `be status` always does — BE-005 / GET-026).
    if (!r->stream) fo = rows_flush_hunk(r);
    else            fo = rows_stream_banner(r);
    if (ROWS_ACTIVE == r) ROWS_ACTIVE = NULL;
    u32bFree(r->toks);
    u8bUnMap(r->text);
    r->open = NO;
    return fo;
}

//  POST-018: append a trailing summary line (`staged N put row(s)`,
//  `N change(s)`, …) to the active table — the data-bearing count rides
//  the module hunk instead of a bare stderr line (BE-005).  No date /
//  verb / nav columns; the whole line wears the neutral 'S' tag like
//  the `be status` summary tail.  Streaming tables emit the banner once
//  (if still owed) then this single line as a one-line hunk; buffering
//  tables fold it into the one module hunk at Close.  No-op with no
//  active table.
ok64 ROWSu8bFeedSummary(u8cs text) {
    sane(1);
    rows *r = ROWS_ACTIVE;
    if (r == NULL || !r->open) done;
    if (r->stream) {
        //  Live: emit the banner once (if still owed), then this single
        //  summary line as its own content hunk on the sink.
        call(rows_stream_banner, r);
        u8bReset(r->text);
        u32bReset(r->toks);
    }
    (void)u8bFeed(r->text, text);
    (void)u8bFeed1(r->text, '\n');
    rows_pack(r, 'S');
    if (!r->stream) done;
    hunk hk = {};
    u8csMv(hk.text, u8bDataC(r->text));
    {
        tok32cs kv = {};
        kv[0] = (tok32c *)u32bDataHead(r->toks);
        kv[1] = (tok32c *)u32bDataHead(r->toks) + u32bDataLen(r->toks);
        u32csMv(hk.toks, kv);
    }
    a_carve(u8, big, (1UL << 16));
    ok64 fo = HUNKu8sFeedOut(u8bIdle(big), &hk);
    if (fo == OK) { rows_trim(big); (void)FILEFeedAll(r->fd, u8bDataC(big)); }
    return fo;
}

ok64 ROWSPrintRow(ulogreccp rec, ROWSnav nav) {
    sane(rec);
    //  A module accumulator is already open → append to it (the common
    //  case once producers arm a per-module table).
    if (ROWS_ACTIVE != NULL && ROWS_ACTIVE->open)
        return ROWSu8bFeedRec(ROWS_ACTIVE, rec, nav);

    //  No active table — run a transient one-row mode-keyed accumulator
    //  headed by the row's own uri.  Equivalent to the old
    //  ULOGPrintStatusLine for the few sites reached without an Open.
    rows r = {};
    u8cs empty = {};
    call(ROWSOpen, &r, empty, 0, 0, ROWS_MODE_KEYED);
    ok64 fr = ROWSu8bFeedRec(&r, rec, nav);
    ok64 cr = ROWSClose(&r);
    return fr != OK ? fr : cr;
}
