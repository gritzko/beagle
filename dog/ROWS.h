#ifndef DOG_ROWS_H
#define DOG_ROWS_H

//  ROWS — the shared status/action row-table builder.
//
//  Beagle's status- and action-reporters (`be status`, `ls:`, and the
//  GET / PUT / DELETE / PATCH / POST / keeper-banner producers) all
//  speak ONE output model (BRO-002): every (sub)module emits ONE
//  content hunk whose
//      uri  = the module's address (`status:`, `ls:<prefix>`, the wt
//             root, …)
//      text = a per-row table `<7-date> <3-verb> <path>[<mov><dst>]\n`
//             plus an invisible navigation URI after each row's '\n'
//      toks = per-column tok32 tags ('L' date, the verb's palette slot,
//             the path tag) + a trailing 'U'-tag over the hidden nav URI
//  rendered through `HUNKu8sFeedOut` (which picks Text/Color/Html/TLV
//  from the global `HUNKMode`).  The per-module hunk header IS the
//  BRO-001 banner (abbrev date + verb + uri).
//
//  Flush is mode-keyed (BRO-002 RULED):
//    * TLV / relay (machine): BUFFER every row and flush ONE table hunk
//      per module at `ROWSClose` — clean for the relay + per-module
//      grouping + parsing.  Producers MUST flush before recursing into
//      submodules so the relay never interleaves a parent's late rows
//      after a child's hunk.
//    * direct-tty (human, stderr/stdout): STREAM each row LIVE as its
//      event occurs (preserve line-by-line progress on a long clone) —
//      no batching.  `ROWSClose` is then a no-op flush.
//  The producer always appends via `ROWSu8bFeedRow`; the builder decides
//  (from `HUNKMode`, captured at `ROWSOpen`) whether to also emit the row
//  immediately (tty) or hold it for the single end-of-module hunk (TLV).
//
//  Whole-table consumers (`be status`, `ls:`) always accumulate and
//  flush one hunk regardless of mode — they pass `ROWS_BATCH`.  The
//  4 MiB mmap `text` + 256K `toks` caps mirror `sniff/LS.c` (BASS-backed,
//  paged on demand).

#include "abc/B.h"
#include "abc/BUF.h"
#include "abc/RON.h"
#include "abc/URI.h"
#include "dog/HUNK.h"
#include "dog/ULOG.h"
#include "dog/tok/TOK.h"

#define ROWS_TEXT_CAP   (1UL << 22)   // 4 MiB body, mmap-backed
#define ROWS_TOKS_CAP   (1UL << 18)   // 256 K tok32 entries

//  Flush discipline.
typedef enum {
    //  Mode-keyed: tty streams each row live, TLV buffers + flushes one
    //  hunk at Close.  Used by the action producers (get/put/del/…).
    ROWS_MODE_KEYED = 0,
    //  Always buffer + flush one hunk at Close (be status / ls:).
    ROWS_BATCH      = 1,
} ROWSdiscipline;

//  Per-row navigation target.  Each table row carries an invisible
//  click-URI (covered by a 'U' tok) that bro turns into a link.
typedef enum {
    ROWS_NAV_NONE = 0,   // no nav URI emitted
    ROWS_NAV_CAT,        // `cat:<path>`  (open the file)
    ROWS_NAV_DIFF,       // `diff:<path>` (show what changed)
    ROWS_NAV_LS,         // `ls:<path>`   (descend into a subdir)
    ROWS_NAV_COMMIT,     // `commit:?<query>` (open the commit; COMMIT-001)
} ROWSnav;

//  One row to append.  Caller has already classified the step (verb,
//  ts, path resolved); this struct only describes how to RENDER it.
typedef struct {
    u8cs    path;        // the path column body
    u8cs    mov_dst;     // move destination (empty = not a move)
    ron60   ts;          // 0 → 7 blank date cols
    ron60   verb;        // status verb (drives the palette tag)
    u8      path_tag;    // tok tag for the path column ('F' ls / 'S' status)
    b8      arrow;       // YES → `<src> -> <dst>` ; NO → `<src>#<dst>`
    u32     indent;      // path-column left pad in cols (lsr: tree depth)
    ROWSnav nav;         // navigation scheme for the hidden URI
    u8cs    nav_target;  // bytes after the scheme (path / dst / query)
} rows_row;

//  Accumulator.  Owns the mmap'd text/toks; module uri/verb head the
//  flushed hunk (the BRO-001 banner).
typedef struct {
    Bu8           text;     // accumulating table body
    Bu32          toks;     // accumulating column / 'U' tags
    i64           now;      // for DOGutf8sFeedDate (relative form)
    u8cs          uri;      // module address (borrowed; outlives Close)
    ron60         verb;     // module action verb (0 = absent)
    ron60         ts;       // module banner ts (0 = absent)
    ROWSdiscipline disc;    // flush discipline
    int           fd;       // output sink (0/STDOUT_FILENO = stdout;
                            // mutators set STDERR_FILENO for tty streaming)
    b8            stream;   // YES → tty live-stream each row
    b8            open;     // YES between Open and Close
    b8            banner_done; // streaming: module banner already emitted
} rows;

//  Acquire the mmap'd text/toks and arm the accumulator.  `uri` is the
//  module address (borrowed — must outlive Close); `verb`/`ts` head the
//  flushed hunk's banner (0 = omit).  `disc` picks the flush discipline:
//  `ROWS_MODE_KEYED` streams per-row on a tty, `ROWS_BATCH` always holds
//  one hunk.  Pair every Open with exactly one Close.
ok64 ROWSOpen(rows *r, u8cs uri, ron60 verb, ron60 ts, ROWSdiscipline disc);

//  Append one row to the table (text + toks).  In a tty-streaming
//  accumulator this also emits the row immediately as a one-row content
//  hunk on stdout; in a buffering accumulator it only grows the table.
ok64 ROWSu8bFeedRow(rows *r, rows_row const *row);

//  Convenience: append a row straight from a ULOG record (path in
//  `rec->uri.path`, optional move-dst in `rec->uri.fragment`).  Renders
//  with the status-column layout (path tag 'S', `#`-joined moves) — the
//  shape the action producers want.  `nav` selects the click target.
ok64 ROWSu8bFeedRec(rows *r, ulogreccp rec, ROWSnav nav);

//  Flush + release.  In a buffering accumulator (TLV, or ROWS_BATCH)
//  this emits ONE table hunk (banner header = uri/verb/ts) via
//  `HUNKu8sFeedOut` and FILEout; in a tty-streaming accumulator the rows
//  already went out, so this just frees the buffers.  `text`/`toks` are
//  unmapped/freed; the accumulator is reset.
ok64 ROWSClose(rows *r);

//  The canonical per-row status emitter (replaces the old
//  `ULOGPrintStatusLine`): open a one-row mode-keyed accumulator headed
//  by the row's own uri, feed the row, close.  Used where a producer has
//  no module-level accumulator open (legacy single-row sites).  `nav`
//  selects the click target.
ok64 ROWSPrintRow(ulogreccp rec, ROWSnav nav);

#endif
