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

    Bu8   arena;        // hunk staging arena (URI/text/hili bytes)
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
                if ($len(sym) >= 2 && sym[0][0] == '\'' &&
                    sym[1][-1] == '\'') {
                    sym[0]++; sym[1]--;
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
u8p  BROArenaWrite(void const *data, size_t len);

// Record a mmap'd file for cleanup at BROClose time.
void BRODefer(u8bp mapped);

// Finalize the hunk that was filled at hunkbIdleHead.
void BROHunkAdd(void);

// List a directory; one hunk per entry tagged 'F'.
ok64 BROListDir(u8csc dirpath);

// Tokenize source in hk->text using the extension from pathslice.
// Appends tok32 words into the active bro state's `toks` arena and
// sets hk->toks to the freshly-written slice. Returns YES if
// tokenized, NO otherwise (unknown ext, no room, etc).
b8 BROTokenize(hunk *hk, u8csc pathslice);

// Interactive pager: displays hunks with syntax colors, diff highlighting,
// status bar, and search. Falls back to plain output when !isatty.
ok64 BRORun(hunkc const *hunks, u32 nhunks);

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
u32 BROAppendLines(range32 *lines, u32 nlines, u32 maxlines,
                   hunkc const *hunks, u32 from, u32 nhunks, u32 cols);

// Total display-line count that BROAppendLines would produce for
// hunks[0..nhunks) at the given `cols`.  Used to size allocations.
u32 BROCountLines(hunkc const *hunks, u32 nhunks, u32 cols);

// --- Navigation primitives (exposed for testing) ---

// Next hunk start strictly after line `from`.
u32 BROHunkNextLine(range32 const *lines, u32 nlines, u32 from);

// Start of current hunk if `from` is not already on it; else previous
// hunk start. (vim's [[ semantics)
u32 BROHunkPrevLine(range32 const *lines, u32 nlines, u32 from);

// Total number of hunks represented in lines[0..nlines).
u32 BROHunkCount(range32 const *lines, u32 nlines);

// 1-based index of the hunk that contains line `at`; 0 if nlines==0.
u32 BROHunkIndexAt(range32 const *lines, u32 nlines, u32 at);

// Total non-neutral hili ranges across hunks[0..nhunks).
u32 BROHiliCount(hunkc const *hunks, u32 nhunks);

// 1-based index of the latest hili range whose first line <= `at`.
u32 BROHiliIndexAt(hunkc const *hunks, u32 nhunks,
                   range32 const *lines, u32 nlines, u32 at);

// First line containing a hili range whose first line > `mid`.
u32 BROHiliNextLine(hunkc const *hunks, u32 nhunks,
                    range32 const *lines, u32 nlines, u32 mid);

// Last line containing a hili range whose first line < `mid`.
u32 BROHiliPrevLine(hunkc const *hunks, u32 nhunks,
                    range32 const *lines, u32 nlines, u32 mid);

// --- Public API (DOG 4-fn) ---

ok64 BROOpen(bro *b, home *h, b8 rw);
ok64 BROExec(bro *b, cli *c);
ok64 BROUpdate(bro *b, u8 obj_type, u8cs blob, u8csc path);
ok64 BROClose(bro *b);

#endif
