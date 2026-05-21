#ifndef BRO_BRO_H
#define BRO_BRO_H

#include "abc/B.h"
#include "abc/BUF.h"
#include "abc/INT.h"
#include "abc/URI.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "dog/FRAG.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"

// `u32b` (from abc/INT.h) and `u8bb` (from abc/BUF.h) are used below.

// --- bro control struct (per DOG.md rule 8) ---
//
// bro persists nothing on disk — state exists only for one
// invocation. BROOpen allocates the arena + typed buffers; BROClose
// unmaps everything including any deferred mmap'd files.

#define BRO_ARENA_SIZE (1UL << 27)   // 128MB
#define BRO_MAX_HUNKS  4096
#define BRO_MAX_MAPS   1024

typedef struct {
    home *h;            // borrowed
    b8    rw;
    b8    color;        // stdout is a color tty
    int   pipe_fd;      // TLV hunk input pipe; -1 when not piped
    int   worker_pid;   // child PID feeding the pipe; -1 when none

    Bu8   arena;        // hunk staging arena (URI/text/toks bytes)
    hunkb hunks;        // typed buffer of hunks (DATA length = count)
    u32b  toks;         // flat tokens arena for all hunks
    u8bb  maps;         // buffer of mmap'd files awaiting cleanup
} bro;

#define BRO_NONE UINT32_MAX

// Parsed hunk URI: path, symbol, line — extracted once.
typedef struct {
    u8cs path;    // repo-relative path (no leading /)
    u8cs symbol;  // function/identifier name, or empty
    u32  line;    // 1-based line number, or 0
} BROloc;

// Parse a hunk's URI into path + symbol + line. Single URI+FRAG parse.
fun void BROHunkLoc(BROloc *loc, hunkc const *hk) {
    *loc = (BROloc){};
    if ($empty(hk->uri)) return;
    uri u = {};
    u8csc text = {hk->uri[0], hk->uri[1]};
    if (DOGParseURI(&u, text) != OK) return;
    if (!$empty(u.path)) {
        $mv(loc->path, u.path);
        if (!$empty(loc->path) && *loc->path[0] == '/')
            u8csFed(loc->path, 1);
    }
    if (!$empty(u.fragment)) {
        frag fr = {};
        if (FRAGu8sDrain(u.fragment, &fr) == OK) {
            //  `#42`/`#42-50` → line.  Anything non-line non-empty is the
            //  symbol body — strip a single pair of surrounding quotes
            //  written by HUNKu8sMakeURI for non-ident symbols.
            loc->line = fr.line;
            if (fr.type != FRAG_LINE && !$empty(u.fragment)) {
                u8cs sym = {u.fragment[0], u.fragment[1]};
                if ($len(sym) >= 2 && *sym[0] == '\'' &&
                    *$last(sym) == '\'') {
                    u8csUsed1(sym);
                    u8csShed1(sym);
                }
                $mv(loc->symbol, sym);
            }
        }
    }
}

#define BRO_TITLE_LINE UINT32_MAX

// Reset the arena + hunk/map/toks buffers between passes (kept for
// subcommands that restart collection mid-session). BROOpen/Close
// own the actual mmap lifecycle.
ok64 BROArenaInit(void);
void BROArenaCleanup(void);
// Copy `orig`'s bytes into the bro arena and, if `in_arena` is
// non-NULL, populate it with the resulting slice (borders point into
// the arena).  Returns NOROOM if the arena lacks room; the arena is
// not mutated in that case.
ok64 BROArenaWrite(u8csp in_arena, u8cs orig);

// Map the process-wide scratch buffer used by BROCountLines /
// BROAppendLines. Idempotent. BROOpen calls this; tests that exercise
// the line-index APIs without a bro instance must call it explicitly.
ok64 BROScratchInit(void);

// Record a mmap'd file for cleanup at BROClose time.
void BRODefer(u8bp mapped);

// List a directory; one hunk per entry tagged 'F'.
ok64 BROListDir(u8csc dirpath);

// Tokenize source in hk->text using the extension from pathslice.
// Appends tok32 words into the active bro state's `toks` arena and
// sets hk->toks to the freshly-written slice. Returns YES if
// tokenized, NO otherwise (unknown ext, no room, etc).
b8 BROTokenize(hunk *hk, u8csc pathslice);

// Interactive pager: displays hunks with syntax colors, diff highlighting,
// status bar, and search. Falls back to plain output when !isatty.
ok64 BRORun(hunkcs hunks);

// Pager event loop: reads TLV hunks from pipefd, displays incrementally.
ok64 BROPipeRun(int pipefd);

// --- Line index builder (exposed for testing) ---
//
// Build the display-line index for hunks[from..nhunks).  One `range32`
// entry per display row: `lo` = hunk index, `hi` = byte offset into
// that hunk's text, or BRO_TITLE_LINE for a title separator.  Long
// source lines get multiple entries — one per `cols` codepoints —
// so soft-wrap is baked into the index.  Callers must pre-allocate
// `lines[0..maxlines)`; returns the new total line count (<=maxlines).
void BROAppendLines(range32b lines, hunkcs hunks, u32 from, u32 cols);

// Total display-line count that BROAppendLines would produce for
// `hunks` at the given `cols`.  Used to size allocations.
u32 BROCountLines(hunkcs hunks, u32 cols);

// --- Navigation primitives (exposed for testing) ---

// Next hunk start strictly after line `from`.
u32 BROHunkNextLine(range32cs lines, u32 from);

// Start of current hunk if `from` is not already on it; else previous
// hunk start. (vim's [[ semantics)
u32 BROHunkPrevLine(range32cs lines, u32 from);

// Total number of hunks represented in `lines`.
u32 BROHunkCount(range32cs lines);

// 1-based index of the hunk that contains line `at`; 0 if lines is empty.
u32 BROHunkIndexAt(range32cs lines, u32 at);

// Total non-eq side runs (in/rm) across `hunks`.
u32 BROHiliCount(hunkcs hunks);

// 1-based index of the latest hili range whose first line <= `at`.
u32 BROHiliIndexAt(hunkcs hunks, range32cs lines, u32 at);

// First line containing a hili range whose first line > `mid`.
u32 BROHiliNextLine(hunkcs hunks, range32cs lines, u32 mid);

// Last line containing a hili range whose first line < `mid`.
u32 BROHiliPrevLine(hunkcs hunks, range32cs lines, u32 mid);

// --- Public API (DOG 4-fn) ---

ok64 BROOpen(bro *b, home *h, b8 rw);
ok64 BROExec(bro *b, cli *c);
ok64 BROUpdate(bro *b, u8 obj_type, u8cs blob, u8csc path);
ok64 BROClose(bro *b);

#endif
