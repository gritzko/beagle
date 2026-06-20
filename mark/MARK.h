#ifndef MARK_MARK_H
#define MARK_MARK_H

//  mark — StrictMark → HTML renderer (the wiki dog).
//
//  Drives MKDT for block structure (the MKDTB grammar via the MKDT* line
//  classifiers), inline tokens (MKDTInlineLexer), and span decomposition
//  (MKDTDecomposeSpan), generates HTML through u8b feed helpers, and
//  sanitizes literal text through ragel (the MARKE machine — mark's only one).
//  Enforces the WikiWeb page structure and size budgets — see README.mkd.

#include "abc/INT.h"
#include "abc/S.h"
#include "abc/B.h"

con ok64 MARKBAD = 0x1629b50b28d;        // malformed input
con ok64 MARKFAIL = 0x58a6d43ca495;      // internal failure
con ok64 MARKLIMIT = 0x1629b51549649d;   // WikiWeb structure/limit violation
con ok64 MARKARG = 0x1629b50a6d0;        // bad CLI argument

//  WikiWeb size budgets, counted in 64-char "lines".
#define MARK_LINE 64
#define MARK_HEAD_MAX (1 * MARK_LINE)    // header  <= 64
#define MARK_BULLET_MAX (2 * MARK_LINE)  // bullet  <= 128
#define MARK_SUMM_MAX (4 * MARK_LINE)    // summary <= 256
#define MARK_OPEN_MAX (8 * MARK_LINE)    // opener  <= 512

typedef struct {
    b8   strict;  // YES: a structure/limit violation aborts with MARKLIMIT
    u8cs head;    // raw HTML injected before </head> (from --head=FILE); empty = none
    u8cs body;    // raw HTML injected after <body> (from --body=FILE); empty = none
    u8cs root;    // filesystem path of the site root (the `/` anchor); empty = no tree probe.
                  // An extensionless `[/x]` resolves to x.html iff root/x.mkd or root/x.md exists.
} markopts;

//  Render StrictMark `src` to a standalone HTML document in `out`.
//  `title` seeds <title> (may be empty).  Validates the WikiWeb structure
//  and budgets: strict mode returns MARKLIMIT on the first violation;
//  otherwise each is warned to stderr and rendering continues.  `out`
//  must have room (the caller sizes it generously).
ok64 MARKRenderDoc(u8bp out, u8csc src, u8csc title, markopts opts);

//  --- generation helpers (shared with the ragel units) ---

//  Append the C string `s` verbatim to `out`.
ok64 MARKu8bLit(u8bp out, const char *s);

//  HTML-escape `text` into `out` (& < > " -> entities).  ragel: MARKE.
//  Inline span decomposition lives in the StrictMark inline grammar now:
//  see mkdtspan / MKDTDecomposeSpan in dog/tok/MKDT.h.
ok64 MARKu8bFeedEsc(u8bp out, u8csc text);

#endif
