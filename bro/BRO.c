#include "BRO.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "MAUS.h"
#include "abc/ANSI.h"
#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/TTY.h"
#include "abc/URI.h"
#include "abc/UTF8.h"
#include "dog/DOG.h"
#include "dog/tok/DEF.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"
#include "dog/THEME.h"
#include "dog/ULOG.h"
#include "dog/tok/TOK.h"

// --- Active bro instance ---
//
// The pager + cat-mode code uses the bro state's arena, hunks, toks
// and deferred-maps buffers heavily. Rather than thread `bro *b`
// through every static helper we keep storage in the caller-owned
// struct (per the DOG 4-fn API) and install a file-static pointer to
// it at BROOpen time. Macros below forward the long-established
// names to the active instance's typed buffers.

static bro *bro_state = NULL;

#define bro_arena  (bro_state->arena)
#define bro_hunks  hunkbDataHead(bro_state->hunks)   // hunk* into DATA
#define bro_toks   (bro_state->toks)                 // shared tok32b arena
#define BRO_COLOR  (bro_state->color)
#define bro_nhunks ((u32)hunkbDataLen(bro_state->hunks))

// Maximum length of the interactive search pattern.
#define BRO_SEARCH_MAX 256

// Maximum length of a one-shot status-bar flash message.
#define BRO_FLASH_MAX  128

// Tokens arena size — big enough for every hunk's tokens combined.
#define BRO_TOKS_SIZE (1UL << 22)   // 16M u32 entries = 64MB

// Per-walk scratch (bro_lineinfo[BRO_LINEINFO_CAP], ~192K) is carved
// from BASS inside bro_walk_hunk / bro_hunk_append — no process-wide
// buffer, no init.  The line-index API is callable without a bro
// instance (e.g. WRAP tests) as long as BASS is up (MAIN/TEST).

// --- DOG 4-fn: Open / Close / Update ---

ok64 BROOpen(bro *b, home *h, b8 rw) {
    sane(b && h);
    zerop(b);
    b->h = h;
    b->rw = rw;
    b->color = YES;
    b->pipe_fd = -1;
    b->worker_pid = -1;
    bro_state = b;
    call(u8bMap, b->arena, BRO_ARENA_SIZE);
    call(hunkbMap, b->hunks, BRO_MAX_HUNKS);
    call(tok32bMap,  b->toks,  BRO_TOKS_SIZE);
    call(i32bMap,  b->maps,  BRO_MAX_MAPS);
    done;
}

ok64 BROClose(bro *b) {
    sane(b);
    if (bro_state == b) {
        size_t n = i32bDataLen(b->maps);
        i32 const *fds = i32bDataHead(b->maps);
        for (size_t i = 0; i < n; i++) {
            int fd = (int)fds[i];
            if (fd < 0 || fd >= FILE_MAX_OPEN) continue;
            u8bp slot = FILE_WANT_BUFS[fd];
            if (slot && slot[0]) FILEUnMap(slot);
        }
        if (b->maps[0])  i32bUnMap(b->maps);
        if (b->toks[0])  tok32bUnMap(b->toks);
        if (b->hunks[0]) hunkbUnMap(b->hunks);
        if (b->arena[0]) u8bUnMap(b->arena);
        bro_state = NULL;
    }
    zerop(b);
    done;
}

// Bro does not index — stub to satisfy the DOG 4-fn contract.
ok64 BROUpdate(bro *b, u8 obj_type, u8cs blob, u8csc path) {
    sane(b);
    (void)obj_type; (void)blob; (void)path;
    done;
}

// Reset staging between subcommands — keeps the mappings alive.
ok64 BROArenaInit(void) {
    sane(bro_state);
    u8bShedAll(bro_state->arena);
    hunkbShedAll(bro_state->hunks);
    tok32bShedAll(bro_state->toks);
    // Deferred maps stay recorded across a reset so the owning
    // files remain valid for hunks already handed to BRORun.
    return OK;
}

void BROArenaCleanup(void) { /* cleanup lives in BROClose */ }

// Record a mmap'd file so BROClose can FILEUnMap it after the view
// that references it has been drained.  Returns OK when there is
// nothing to track (NULL / unbooked map) or the fd was recorded;
// NOROOM when a real booked map could NOT be recorded because the
// maps table is full — the caller MUST then FILEUnMap it itself, or
// the mapping leaks (MEM-020).
ok64 BRODefer(u8bp mapped) {
    if (!mapped) return OK;
    int fd = FILEBookedFD(mapped);
    if (fd < 0) return OK;  // not a booked map: nothing to unmap later
    if (i32bIdleLen(bro_state->maps) == 0) return NOROOM;
    i32bFeed1(bro_state->maps, (i32)fd);
    return OK;
}

// Copy one TLV-drained record's uri/text/toks into bro_arena and
// populate `hk`'s slices.  Returns NOROOM on any arena exhaustion —
// already-written bytes stay orphan in the arena, but `hk` is left
// unfinalized (caller skips hunkbFed1).
static ok64 bro_stage_hunk(hunk *hk, hunk *tlv_hk) {
    sane(1);
    *hk = (hunk){};
    hk->ts   = tlv_hk->ts;
    hk->verb = tlv_hk->verb;
    if (!$empty(tlv_hk->uri))
        call(u8bHost, bro_arena, hk->uri, tlv_hk->uri);
    if (!$empty(tlv_hk->text))
        call(u8bHost, bro_arena, hk->text, tlv_hk->text);
    if (!$empty(tlv_hk->toks)) {
        // tok32cs is u32 elements; arena is u8 — view the same
        // bytes through a u8 slice for the copy, then rebrand the
        // resulting borders as u32cp.
        u8cs tok_in = {(u8cp)tlv_hk->toks[0], (u8cp)tlv_hk->toks[1]};
        u8cs tok_out = {};
        call(u8bHost, bro_arena, tok_out, tok_in);
        hk->toks[0] = (u32cp)tok_out[0];
        hk->toks[1] = (u32cp)tok_out[1];
    }
    done;
}

// Drain as many complete TLV hunk records as fit in bro_hunks /
// bro_arena from `buf`'s DATA.  Each accepted record's uri/text/toks
// are copied into bro_arena and a hunk is appended.  Any consumed
// prefix is shifted out so a partial trailing record stays at the
// start for the next read.  Returns the status of the first
// HUNKu8sDrain / stage / append that didn't yield a record
// (typically NODATA for "more bytes needed"); OK if the loop stopped
// on empty buf or because BRO_MAX_HUNKS was reached.
static ok64 bro_drain_tlv(Bu8 buf) {
    a_dup(u8 const, from, u8bDataC(buf));
    ok64 stop = OK;
    while (!$empty(from) && bro_nhunks < BRO_MAX_HUNKS) {
        a_dup(u8 const, save, from);
        hunk tlv_hk = {};
        ok64 o = HUNKu8sDrain(from, &tlv_hk);
        if (o != OK) { $mv(from, save); stop = o; break; }
        o = bro_stage_hunk(&bro_hunks[bro_nhunks], &tlv_hk);
        if (o != OK) { $mv(from, save); stop = o; break; }
        o = hunkbFed1(bro_state->hunks);
        if (o != OK) { $mv(from, save); stop = o; break; }
    }
    size_t consumed = u8bDataLen(buf) - $len(from);
    if (consumed > 0) {
        u8bUsed(buf, consumed);
        u8bShift(buf, 0);
    }
    return stop;
}

// --- Line index ---
// Maps line number -> (hunk index, byte offset within hunk text).
// A "line" is range32: lo=hunk index, hi=byte offset within hunk text.
// A title separator is stored as hunk index with hi=UINT32_MAX.

// A hunk has a displayable title if it has a URI.
#define hunk_has_title(hk) (!$empty((hk)->uri))

// --- View stack for file navigation ---
#define BRO_MAX_VIEWS 32

// Saved state of the main view when a file is opened.
typedef struct {
    hunkcs hunks;
    Brange32 linesbuf;
    u32 scroll;
} BROsave;

// Resources owned by a file view (one opened file).
// Tokens live in the shared bro_state->toks arena; hunk.toks slices it.
typedef struct {
    u8bp mapped;    // mmap'd file
    hunk hunk;      // inline hunk (title + text + toks)
} BROfileview;

// --- Line index encoding ---
//
// Each `range32` row in lines[]:  lo = hunk index, hi = packed offset+pass.
// `hi == BRO_TITLE_LINE` (UINT32_MAX) tags a title sentinel.
// Otherwise: bits 23..0 = byte offset within hunk text, bits 25..24 = pass.
//
//   pass = 0: normal/inline   (eq lines, inrm-small, plain text)
//   pass = 1: rm-pass row     (rm + inrm-big rm-side, in-side bytes hidden)
//   pass = 2: in-pass row     (in + inrm-big in-side, rm-side bytes hidden)
#define BRO_LINE_OFF_BITS  24
#define BRO_LINE_OFF_MASK  ((1u << BRO_LINE_OFF_BITS) - 1)
#define BRO_PASS_NORMAL    0u
#define BRO_PASS_RM        1u
#define BRO_PASS_IN        2u

//  Resolve one cell's full SGR state from what tok32 already carries.
//  fg_tag is the dogenizer tag (`tok32Tag`); pass is bro's render
//  pass (normal / rm / in); side is the diff side (`tok32Side`).  Bg
//  letters match dog/THEME.h: I=ins, O=del, J=ins_emph, K=del_emph.
//  (pass, side) → bg:
//    NORMAL + IN → ins   ;  NORMAL + RM → del   ;  NORMAL + EQ → none
//    RM     + RM → del_emph ;  RM     + EQ → del   (eq context wears del wash)
//    IN     + IN → ins_emph ;  IN     + EQ → ins   (eq context wears ins wash)
//  (NORMAL+RM in PASS_RM and NORMAL+IN in PASS_IN are hidden bytes,
//   filtered out before this is called.)
fun ansi64 bro_cell_ansi(u8 fg_tag, u8 pass, u8 side, b8 in_search) {
    ansi64 want = THEMEAt(fg_tag);
    if (pass == BRO_PASS_NORMAL) {
        if (side == TOK_SIDE_IN)      want |= THEMEAt('I');
        else if (side == TOK_SIDE_RM) want |= THEMEAt('O');
    } else if (pass == BRO_PASS_RM) {
        want |= (side == TOK_SIDE_RM) ? THEMEAt('K') : THEMEAt('O');
    } else {  // BRO_PASS_IN
        want |= (side == TOK_SIDE_IN) ? THEMEAt('J') : THEMEAt('I');
    }
    if (in_search) want |= ANSI64_FLAG(ANSI_REVERSE);
    return want;
}

fun u32 bro_line_off (range32 const *ln) { return ln->hi & BRO_LINE_OFF_MASK; }
fun u8  bro_line_pass(range32 const *ln) {
    return (u8)((ln->hi >> BRO_LINE_OFF_BITS) & 0x3);
}
fun u32 bro_line_make(u32 off, u8 pass) {
    return (((u32)pass & 0x3) << BRO_LINE_OFF_BITS) | (off & BRO_LINE_OFF_MASK);
}

typedef struct {
    hunkcs hunks;
    Brange32 linesbuf; // line index buffer (one entry per display row)
    u32 scroll;        // first visible line
    u16 rows, cols;    // terminal dimensions
    Bu8 search;        // search pattern (u8bAlloc'd, BRO_SEARCH_MAX bytes)
    struct termios orig_termios;  // saved terminal state
    int tty_fd;        // /dev/tty for keyboard (stdin may be data pipe)
    b8 raw_mode;       // whether terminal is in raw mode
    b8 mouse_on;       // mouse tracking active (toggled with 'm')
    b8 wrap;           // soft-wrap long lines (toggled with 'w')
    b8 fixed_lines;    // linesbuf is fixed-capacity (pipe mode): rewrite
                       // in place instead of free+realloc on rebuild
    Bu8 flash;         // one-shot status bar message (cleared on render)
    // View stack
    BROsave saves[BRO_MAX_VIEWS];
    BROfileview files[BRO_MAX_VIEWS];
    int nsaves;
} BROstate;

// Line-buffer accessors: the buffer's data borders are the source of
// truth — `range32 *lines` and `u32 nlines` would just cache them and
// drift if a mid-flight realloc happens.
fun range32 *bro_lines (BROstate *st) { return range32bDataHead(st->linesbuf); }
fun u32      bro_nlines(BROstate const *st) { return (u32)range32bDataLen(st->linesbuf); }

// Global for signal handler
static volatile sig_atomic_t bro_resized = 0;

static void bro_winch_handler(int sig) {
    (void)sig;
    bro_resized = 1;
}

// Set the one-shot status-bar flash message (replaces prior).
// Truncates silently to BRO_FLASH_MAX.
static void bro_flash(BROstate *st, char const *fmt, ...)
    __attribute__((format(printf, 2, 3)));
static void bro_flash(BROstate *st, char const *fmt, ...) {
    u8bReset(st->flash);
    u8sp out = u8bIdle(st->flash);
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf((char *)out[0], $len(out), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if ((size_t)n > $len(out)) n = (int)$len(out);
    u8bFed(st->flash, (size_t)n);
}

// --- Terminal setup ---

static ok64 BRORawEnable(BROstate *st) {
    sane(st != NULL);
    // Open /dev/tty so keyboard input works even when stdin is a data pipe.
    if (st->tty_fd < 0) {
        st->tty_fd = open("/dev/tty", O_RDWR | O_NOCTTY);
        if (st->tty_fd < 0) fail(FAILSANITY);
    }
    if (tcgetattr(st->tty_fd, &st->orig_termios) < 0)
        fail(FAILSANITY);
    struct termios raw = st->orig_termios;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (tcflag_t)~(OPOST);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;  // 100ms timeout
    if (tcsetattr(st->tty_fd, TCSAFLUSH, &raw) < 0)
        fail(FAILSANITY);
    st->raw_mode = YES;
    done;
}

static void BRORawDisable(BROstate *st) {
    if (st->raw_mode) {
        tcsetattr(st->tty_fd, TCSAFLUSH, &st->orig_termios);
        st->raw_mode = NO;
    }
    if (st->tty_fd >= 0) {
        close(st->tty_fd);
        st->tty_fd = -1;
    }
}

static void BROGetSize(BROstate *st) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        st->rows = ws.ws_row;
        st->cols = ws.ws_col;
    } else {
        st->rows = 24;
        st->cols = 80;
    }
}

// Forward declarations for functions used before definition.
static void BROScrollCenter(BROstate *st, u32 target);
static b8 bro_is_source_start(hunkcsc hunks, range32cs lines, u32 i);
static void BROReadSpot(BROstate *st, char *buf, int bufsz,
                        char const *prompt);
static ok64 BROForkSpot(BROstate *st, char const *flag,
                        char const *token, char const *filepath,
                        char const *repo);
static ok64 BROForkBe(BROstate *st, char const *uri);
static u32 BROSearchNext(BROstate *st, u32 from, int direction);

// --- Build line index ---

// Walk one display row forward from `off` inside `text`: advance at most
// `cols` codepoints, stop on '\n' (not consumed), stop at end.  Returns
// the byte offset where the row ends (exclusive).
static u32 bro_row_end(u8csc text, u32 tlen, u32 off, u32 cols) {
    u32 cp = 0;
    while (off < tlen && cp < cols && text[0][off] != '\n') {
        u8 ch = text[0][off];
        u32 clen = UTF8_LEN[ch >> 4];
        if (clen == 0 || off + clen > tlen) clen = 1;
        off += clen;
        cp++;
    }
    return off;
}

// Pass-aware row end.  Skips bytes whose token side is hidden in this
// pass (in-side bytes for rm-pass, rm-side bytes for in-pass).  A
// '\n' only terminates the row when it's *visible* in this pass:
// an INS '\n' embedded mid-old-line (e.g. when the diff inserted whole
// new lines just before a modified line and shares a token prefix
// with them) must not break the rm-pass row, or the OLD line
// reconstruction looks fragmented.  Counts only visible codepoints
// toward `cols`.
static u32 bro_row_end_pass(hunkc const *hk, u32 tlen, u32 off,
                            u32 cols, u8 pass) {
    int ntoks = (int)$len(hk->toks);
    if (ntoks == 0 && pass == BRO_PASS_NORMAL) {
        u8csc text = {hk->text[0], hk->text[1]};
        return bro_row_end(text, tlen, off, cols);
    }
    int ti = 0;
    while (ti < ntoks && tok32Offset(hk->toks[0][ti]) <= off) ti++;
    u32 cp = 0;
    while (off < tlen && cp < cols) {
        while (ti < ntoks && tok32Offset(hk->toks[0][ti]) <= off) ti++;
        u8 side = (ti < ntoks) ? tok32Side(hk->toks[0][ti]) : TOK_SIDE_EQ;
        u8 tag  = (ti < ntoks) ? tok32Tag (hk->toks[0][ti]) : 'S';
        u8 ch = hk->text[0][off];
        b8 hidden = (tag == 'U') ||
                    (pass == BRO_PASS_RM && side == TOK_SIDE_IN) ||
                    (pass == BRO_PASS_IN && side == TOK_SIDE_RM);
        if (ch == '\n' && !hidden) break;
        u32 clen = UTF8_LEN[ch >> 4];
        if (clen == 0 || off + clen > tlen) clen = 1;
        off += clen;
        if (!hidden) cp++;
    }
    return off;
}

// Per-segment info.  A "segment" runs from the byte after the previous
// '\n' to (and including) the next '\n' in the merged text.  The
// boundary `\n` carries a side (eq/in/rm) — the side determines which
// pass(es) treat this `\n` as a real row break.  Token-level diff
// freely interleaves IN, RM, and EQ bytes across line boundaries, so
// a single OLD-side line can span multiple `\n`-bounded segments
// (the IN `\n`s inside it are hidden in rm-pass and don't break the
// OLD row).
typedef struct {
    u32 lo;       // segment start byte offset
    u32 hi;       // exclusive end (the '\n' or text end)
    u32 in_b;     // count of in-side bytes within [lo,hi)
    u32 rm_b;     // count of rm-side bytes within [lo,hi)
    u32 eq_b;     // count of eq-side bytes within [lo,hi)
    u8  bnd_side; // side of the byte at `hi` (TOK_SIDE_EQ/IN/RM); EQ
                  // for the trailing partial-line case (no '\n').
} bro_lineinfo;

// Returns the side of the byte at offset `off` in the merged text.
static u8 bro_side_at(hunkc const *hk, u32 off) {
    int ntoks = (int)$len(hk->toks);
    int lo = 0, hi = ntoks;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (tok32Offset(hk->toks[0][mid]) <= off) lo = mid + 1;
        else hi = mid;
    }
    return (lo < ntoks) ? tok32Side(hk->toks[0][lo]) : TOK_SIDE_EQ;
}

static u32 bro_classify_lines(hunkc const *hk, bro_lineinfo *out, u32 cap) {
    u32 tlen = (u32)$len(hk->text);
    if (tlen == 0 || cap == 0) return 0;
    u32 nl = 0;
    u32 line_lo = 0;
    int ntoks = (int)$len(hk->toks);
    int ti = 0;
    u32 in_b = 0, rm_b = 0, eq_b = 0;
    for (u32 off = 0; off < tlen; off++) {
        while (ti < ntoks && tok32Offset(hk->toks[0][ti]) <= off) ti++;
        u8 side = (ti < ntoks) ? tok32Side(hk->toks[0][ti]) : TOK_SIDE_EQ;
        u8 tag  = (ti < ntoks) ? tok32Tag (hk->toks[0][ti]) : 'S';
        if (tag == 'U') continue;
        if (hk->text[0][off] == '\n') {
            if (nl < cap)
                out[nl] = (bro_lineinfo){line_lo, off, in_b, rm_b, eq_b,
                                         side};
            nl++;
            line_lo = off + 1;
            in_b = rm_b = eq_b = 0;
        } else {
            if (side == TOK_SIDE_IN) in_b++;
            else if (side == TOK_SIDE_RM) rm_b++;
            else eq_b++;
        }
    }
    if (line_lo < tlen) {
        if (nl < cap)
            out[nl] = (bro_lineinfo){line_lo, tlen, in_b, rm_b, eq_b,
                                     TOK_SIDE_EQ};
        nl++;
    }
    return nl < cap ? nl : cap;
}

// True if a segment's boundary `\n` is visible in this pass.
static b8 bro_pass_sees_nl(u8 pass, u8 bnd_side) {
    if (pass == BRO_PASS_NORMAL) return YES;
    if (pass == BRO_PASS_RM) return (bnd_side != TOK_SIDE_IN) ? YES : NO;
    return (bnd_side != TOK_SIDE_RM) ? YES : NO;  // BRO_PASS_IN
}

// Line type derived from byte counts.
//   EQ        : no in/rm bytes — context line.
//   PURE_IN   : no eq bytes; in only — classic INS line, only `+` shown.
//   PURE_RM   : no eq bytes; rm only — classic DEL line, only `-` shown.
//   MOD_INLINE: changed bytes < 25% of line — one row, inline highlights.
//   MOD_SPLIT : changed bytes ≥ 25% — two rows (rm-pass + in-pass).
// "Modified" covers eq+rm only, eq+in only, eq+in+rm, and (rare)
// no-eq+in+rm cases — the line exists in both old and new with
// different content, so both `-` and `+` views must be shown.
typedef enum {
    BRO_LINE_EQ,
    BRO_LINE_PURE_IN,
    BRO_LINE_PURE_RM,
    BRO_LINE_MOD_INLINE,
    BRO_LINE_MOD_SPLIT,
} bro_linekind;

static bro_linekind bro_classify(bro_lineinfo const *li) {
    u32 changed = li->in_b + li->rm_b;
    if (changed == 0) return BRO_LINE_EQ;
    if (li->eq_b == 0) {
        if (li->in_b > 0 && li->rm_b > 0) return BRO_LINE_MOD_SPLIT;
        return (li->in_b > 0) ? BRO_LINE_PURE_IN : BRO_LINE_PURE_RM;
    }
    // Small edits stay inline (single NORMAL-pass row, rm bytes
    // red-tinted and in bytes green-tinted in place).  Larger edits
    // split into rm-row + in-row, because the merged-text byte order
    // (INS-before-DEL by WEAVE canon) makes an inline render of
    // `1UL << 16 → 12` read as `<< 1216` once the change dominates
    // the line.
    u32 total = changed + li->eq_b;
    if (changed * 4 < total) return BRO_LINE_MOD_INLINE;
    return BRO_LINE_MOD_SPLIT;
}

static b8 bro_kind_in_rm_pass(bro_linekind k) {
    return (k == BRO_LINE_PURE_RM || k == BRO_LINE_MOD_SPLIT ||
            k == BRO_LINE_MOD_INLINE);
}
static b8 bro_kind_in_in_pass(bro_linekind k) {
    return (k == BRO_LINE_PURE_IN || k == BRO_LINE_MOD_SPLIT);
}
// MOD_INLINE renders once in the normal pass (eq plain, rm/in bg-tinted).
// All others use the side-aware split passes.
static u8 bro_kind_pass_for_rm(bro_linekind k) {
    return (k == BRO_LINE_MOD_INLINE) ? BRO_PASS_NORMAL : BRO_PASS_RM;
}
static u8 bro_kind_pass_for_in(bro_linekind k) {
    (void)k; return BRO_PASS_IN;
}

#define BRO_LINEINFO_CAP 8192  // generous; hunks are typically tiny

// Bytes of BASS scratch one walk carves for bro_lineinfo[BRO_LINEINFO_CAP],
// expressed in u32 cells (4-byte aligned, matching the struct's fields).
#define BRO_LINEINFO_CELLS \
    ((BRO_LINEINFO_CAP * sizeof(bro_lineinfo) + sizeof(u32) - 1) / sizeof(u32))

// Append soft-wrap rows for one logical line span in `pass`, between
// `lo` (inclusive) and `end_nl` (offset of the visible '\n' that
// terminates the row, or text length for trailing partials).  Pushes
// into `lines`'s idle; silently caps on SNOROOM.
static void bro_append_rows(range32b lines, hunkc const *hk, u32 h,
                            u32 lo, u32 end_nl, u32 cols, u8 pass) {
    u32 tlen = (u32)$len(hk->text);
    u32 off = lo;
    while (off <= end_nl) {
        range32bFeed1(lines, (range32){h, bro_line_make(off, pass)});
        u32 end = bro_row_end_pass(hk, tlen, off, cols, pass);
        if (end >= end_nl) break;
        off = end;
    }
}

static u32 bro_count_rows(hunkc const *hk, u32 lo, u32 end_nl,
                          u32 cols, u8 pass) {
    u32 tlen = (u32)$len(hk->text);
    u32 off = lo;
    u32 n = 0;
    while (off <= end_nl) {
        n++;
        u32 end = bro_row_end_pass(hk, tlen, off, cols, pass);
        if (end >= end_nl) break;
        off = end;
    }
    return n;
}

// Drive the per-hunk row-emission loop.  `emit` is the per-row sink:
// receives (row_start_offset, end_nl_offset, pass).  Used by both
// bro_hunk_count_rows (counts) and bro_hunk_append (writes lines[]).
typedef u32 (*bro_emit_fn)(void *ctx, u32 lo, u32 end_nl, u8 pass);

static ok64 bro_walk_hunk(hunkc const *hk, bro_emit_fn emit, void *ctx,
                          u32 *total) {
    sane(hk && emit && total);
    a_carve(u32, scratch, BRO_LINEINFO_CELLS);
    bro_lineinfo *info = (bro_lineinfo*)u32bIdleHead(scratch);
    u32 nl = bro_classify_lines(hk, info, BRO_LINEINFO_CAP);
    *total = 0;
    u32 i = 0;

    // Block detection.  An eq-only segment (in_b == 0 && rm_b == 0)
    // *whose boundary `\n` is also eq* is a context line — emit
    // directly.  Anything else is part of a "modified block".  Within
    // a block we collect rm-pass rows (visible RM/EQ `\n`s) and
    // in-pass rows (visible IN/EQ `\n`s).
    while (i < nl) {
        bro_linekind k = bro_classify(&info[i]);
        if (k == BRO_LINE_EQ && info[i].bnd_side == TOK_SIDE_EQ) {
            *total += emit(ctx, info[i].lo, info[i].hi, BRO_PASS_NORMAL);
            i++;
            continue;
        }
        if (k == BRO_LINE_MOD_INLINE && info[i].bnd_side == TOK_SIDE_EQ) {
            *total += emit(ctx, info[i].lo, info[i].hi, BRO_PASS_NORMAL);
            i++;
            continue;
        }
        // Block end: when we hit the next eq-context line OR an
        // inline-classifiable line (handled by the branch above on the
        // next iteration).
        u32 j = i;
        while (j < nl) {
            bro_linekind kj = bro_classify(&info[j]);
            if (info[j].bnd_side == TOK_SIDE_EQ &&
                (kj == BRO_LINE_EQ || kj == BRO_LINE_MOD_INLINE))
                break;
            j++;
        }

        //  Walk block segments for rm-pass; group across hidden IN `\n`s.
        //  `pend_*` accumulate visible/hidden byte counts across grouped
        //  segments (segments whose boundary `\n` is hidden in this
        //  pass) so the emit decision considers content carried in
        //  from earlier in the group, not just the current segment.
        //  Without this, an EQ `\n` matched onto a pure-RM segment
        //  fails the `info[m].in_b||eq_b` check and the IN bytes from
        //  earlier segments in the group never get a row.
        u32 row_start = info[i].lo;
        u32 pend_in = 0, pend_rm = 0, pend_eq = 0;
        for (u32 m = i; m < j; m++) {
            pend_in += info[m].in_b;
            pend_rm += info[m].rm_b;
            pend_eq += info[m].eq_b;
            if (bro_pass_sees_nl(BRO_PASS_RM, info[m].bnd_side)) {
                if (pend_rm > 0 || pend_eq > 0) {
                    *total += emit(ctx, row_start, info[m].hi, BRO_PASS_RM);
                }
                row_start = info[m].hi + 1;
                pend_in = pend_rm = pend_eq = 0;
            }
        }
        //  (Trailing range with no visible RM/EQ `\n` ⇒ no rm-row.)

        //  Walk block segments for in-pass — symmetric.
        row_start = info[i].lo;
        pend_in = pend_rm = pend_eq = 0;
        for (u32 m = i; m < j; m++) {
            pend_in += info[m].in_b;
            pend_rm += info[m].rm_b;
            pend_eq += info[m].eq_b;
            if (bro_pass_sees_nl(BRO_PASS_IN, info[m].bnd_side)) {
                if (pend_in > 0 || pend_eq > 0) {
                    *total += emit(ctx, row_start, info[m].hi, BRO_PASS_IN);
                }
                row_start = info[m].hi + 1;
                pend_in = pend_rm = pend_eq = 0;
            }
        }

        i = j;
    }
    done;
}

typedef struct {
    hunkc const *hk;
    u32 cols;
} bro_count_ctx;
static u32 bro_count_emit(void *vctx, u32 lo, u32 end_nl, u8 pass) {
    bro_count_ctx *c = vctx;
    return bro_count_rows(c->hk, lo, end_nl, c->cols, pass);
}

static ok64 bro_hunk_count_rows(hunkc const *hk, u32 cols, u32 *out) {
    sane(hk && out);
    bro_count_ctx c = {hk, cols};
    call(bro_walk_hunk, hk, bro_count_emit, &c, out);
    done;
}

typedef struct {
    range32 **lines;   // points into the caller's range32b slots
    hunkc const *hk;
    u32 h;
    u32 cols;
} bro_append_ctx;
static u32 bro_append_emit(void *vctx, u32 lo, u32 end_nl, u8 pass) {
    bro_append_ctx *c = vctx;
    size_t before = range32bDataLen(c->lines);
    bro_append_rows(c->lines, c->hk, c->h, lo, end_nl, c->cols, pass);
    return (u32)(range32bDataLen(c->lines) - before);
}

static ok64 bro_hunk_append(range32b lines, hunkc const *hk, u32 h,
                            u32 cols) {
    sane(hk);
    // Stamp custom bit on inrm-line tokens (per any-`\n` segment that
    // has both in and rm bytes).  Used by the renderer to distinguish
    // "this token is on a modified line" from a token in a pure-add
    // or pure-delete line, for any future styling that wants it.
    a_carve(u32, scratch, BRO_LINEINFO_CELLS);
    bro_lineinfo *info = (bro_lineinfo*)u32bIdleHead(scratch);
    u32 nl = bro_classify_lines(hk, info, BRO_LINEINFO_CAP);
    int ntoks = (int)$len(hk->toks);
    if (ntoks > 0) {
        u32p toks = (u32p)hk->toks[0];
        u32 prev = 0;
        u32 li_idx = 0;
        for (int ti = 0; ti < ntoks; ti++) {
            u32 end = tok32Offset(toks[ti]);
            while (li_idx < nl && info[li_idx].hi < prev) li_idx++;
            b8 in_inrm = NO;
            for (u32 m = li_idx; m < nl && info[m].lo < end; m++) {
                if (info[m].in_b > 0 && info[m].rm_b > 0) {
                    in_inrm = YES; break;
                }
            }
            toks[ti] = tok32SetCustom(toks[ti], in_inrm ? 1 : 0);
            prev = end;
        }
    }

    bro_append_ctx c = {(range32 **)lines, hk, h, cols};
    u32 nrows = 0;
    call(bro_walk_hunk, hk, bro_append_emit, &c, &nrows);
    done;
}

ok64 BROCountLines(hunkcs hunks, u32 cols, u32 *out) {
    sane(out);
    if (cols == 0) cols = 1;
    u32 total = 0;
    $for (hunk const, hk, hunks) {
        if (hunk_has_title(hk)) total++;
        if ($empty(hk->text)) continue;
        u32 sub = 0;
        call(bro_hunk_count_rows, hk, cols, &sub);
        total += sub;
    }
    *out = total;
    done;
}

// Effective wrap width for the line index.  Wrap mode wraps at the
// screen width; no-wrap mode passes the 24-bit offset ceiling so each
// source line becomes one index entry (renderer still clips at st->cols).
fun u32 bro_wrap_cols(BROstate const *st) {
    if (!st->wrap) return BRO_LINE_OFF_MASK;
    return st->cols > 0 ? st->cols : 80;
}

static ok64 BROBuildIndex(BROstate *st) {
    sane(st != NULL && !$empty(st->hunks));

    u32 cols = bro_wrap_cols(st);
    u32 total = 0;
    call(BROCountLines, st->hunks, cols, &total);
    u32 cap = total > 0 ? total : 1;
    call(range32bAlloc, st->linesbuf, cap);
    call(BROAppendLines, st->linesbuf, st->hunks, 0, cols);
    done;
}

// --- Directory listing ---
// Build a hunk for a directory listing. Each entry is a line tagged 'F'.
// Directories get a trailing '/' (FILEScan already yields dir paths with
// a trailing slash).

// Reference point passed to the FILEScan callback: where the text+toks
// blocks began for *this* listing pass, so each entry can write its
// offsets into bro_arena / bro_state->toks.
typedef struct {
    u8p  text_start;
} listdir_ctx;

static ok64 listdir_emit(void0p arg, path8p path) {
    listdir_ctx *ctx = (listdir_ctx *)arg;
    // FILEScan appends '/' to directory paths. Peel it before taking
    // the basename — PATHu8sBase of "bro/test/" would be empty.
    a_dup(u8c, full, u8bDataC(path));
    b8 is_dir = NO;
    if (!$empty(full) && *$last(full) == '/') {
        is_dir = YES;
        u8csShed1(full);
    }
    u8cs name = {};
    PATHu8sBase(name, full);
    if ($empty(name)) return OK;

    if (u8bFeed(bro_arena, name) != OK) return OK;
    u32 name_end = (u32)(u8bIdleHead(bro_arena) - ctx->text_start);
    tok32bFeed1(bro_state->toks, tok32Pack('F', name_end));
    if (is_dir) {
        a_cstr(slash, "/");
        if (u8bFeed(bro_arena, slash) != OK) return OK;
        u32 sl_end = (u32)(u8bIdleHead(bro_arena) - ctx->text_start);
        tok32bFeed1(bro_state->toks, tok32Pack('P', sl_end));
    }
    a_cstr(nl, "\n");
    if (u8bFeed(bro_arena, nl) != OK) return OK;
    u32 nl_end = (u32)(u8bIdleHead(bro_arena) - ctx->text_start);
    tok32bFeed1(bro_state->toks, tok32Pack('W', nl_end));
    return OK;
}

ok64 BROListDir(u8csc dirpath) {
    sane(!$empty(dirpath));
    if (hunkbIdleLen(bro_state->hunks) == 0) fail(NOROOM);

    a_path(dir);
    call(PATHu8bFeed, dir, dirpath);

    // Snapshot the arena head so the callback's text bytes can be
    // sliced into this hunk's text range; toks accumulate in DATA
    // and get captured via bDataC + bUsedAll below.
    listdir_ctx ctx = {.text_start = u8bIdleHead(bro_arena)};

    call(FILEScan, dir, FILE_SCAN_ALL, listdir_emit, &ctx);

    u8p text_end = u8bIdleHead(bro_arena);
    if (text_end == ctx.text_start) done;  // empty dir

    hunk *hk = hunkbIdleHead(bro_state->hunks);
    *hk = (hunk){};
    hk->verb = HUNK_VERB_HUNK;

    // URI = dirpath
    a_dup(u8 const, dp, dirpath);
    call(u8bHost, bro_arena, hk->uri, dp);

    hk->text[0] = ctx.text_start;
    hk->text[1] = text_end;
    tok32csMv(hk->toks, tok32bDataC(bro_state->toks));
    tok32bUsedAll(bro_state->toks);

    hunkbFed1(bro_state->hunks);
    done;
}

// --- Tokenize helper ---
// Tokenize source into the active bro state's shared `toks` arena
// and set hk->toks to point at the freshly-written slice. Returns
// YES on success, NO otherwise (unknown ext, arena exhausted, …).
b8 BROTokenize(hunk *hk, u8csc pathslice) {
    if (bro_state == NULL) return NO;
    u8cs ext = {};
    a_dup(u8c, ps, pathslice);
    PATHu8sExt(ext, ps);
    if ($empty(ext) || !TOKKnownExt(ext)) return NO;

    u32 srclen = (u32)$len(hk->text);
    if (tok32bIdleLen(bro_state->toks) < (size_t)srclen + 1) return NO;

    u8cs source = {hk->text[0], hk->text[1]};
    if (HUNKu32bTokenize(bro_state->toks, source, ext) != OK) return NO;

    // DATA now holds the freshly-tokenized range; capture as both a
    // mutable view for DEFMark and a const slice for hk->toks, then
    // commit via bUsedAll (DATA → PAST) so the slice locks in.
    tok32s dts;
    tok32sMv(dts, tok32bData(bro_state->toks));
    DEFMark(dts, source, ext);
    tok32csMv(hk->toks, tok32bDataC(bro_state->toks));
    tok32bUsedAll(bro_state->toks);
    return YES;
}

// --- File view open / back ---

// Open a file by repo-relative path, push current view onto stack.
// `repo` is the NUL-terminated repo root path.  Returns OK if the
// file was opened (st is updated in place); non-OK on error (st unchanged).
static ok64 BROOpenFile(BROstate *st, u8csc relpath, char const *repo,
                        u32 target_line) {
    sane(st != NULL && !$empty(relpath) && repo != NULL);
    if (st->nsaves >= BRO_MAX_VIEWS) fail(NOROOM);

    // Build absolute path: repo/relpath.  PATHu8bPush rejects
    // multi-segment names, so PATHu8bAdd drains relpath by segments
    // and pushes each separately (handles `dir/sub/file.c`).
    a_path(fpbuf);
    {
        a_cstr(repo_s, repo);
        call(PATHu8bFeed, fpbuf, repo_s);
        call(PATHu8bAdd, fpbuf, relpath);
    }

    // Map file
    u8bp mapped = NULL;
    ok64 mo = FILEMapRO(&mapped, $path(fpbuf));
    if (mo != OK) fail(mo);

    int idx = st->nsaves;
    BROfileview *fv = &st->files[idx];
    *fv = (BROfileview){};
    fv->mapped = mapped;

    u8cp src_head = u8bDataHead(mapped);
    u8cp src_idle = u8bIdleHead(mapped);

    fv->hunk = (hunk){};
    fv->hunk.verb = HUNK_VERB_HUNK;
    fv->hunk.text[0] = src_head;
    fv->hunk.text[1] = src_idle;

    // Copy URI (= path) into arena.  The slot is not committed
    // (st->nsaves++) until the very end, and BROOpenFile records no
    // BRODefer, so on any error before commit nobody would unmap the
    // booked file map — free it here, on the error branch, before
    // returning (MEM-045).
    a_dup(u8 const, rp, relpath);
    try(u8bHost, bro_arena, fv->hunk.uri, rp);
    nedo {
        FILEUnMap(mapped);
        *fv = (BROfileview){};
        return __;
    }

    BROTokenize(&fv->hunk, relpath);

    // Save current view
    BROsave *sv = &st->saves[idx];
    $mv(sv->hunks, st->hunks);
    range32bHandOver(sv->linesbuf, st->linesbuf);
    sv->scroll = st->scroll;

    // Switch to file view
    st->hunks[0] = &fv->hunk;
    st->hunks[1] = &fv->hunk + 1;
    try(BROBuildIndex, st);
    nedo {
        //  Restore the view borders moved into sv above so st is left
        //  unchanged for the caller, then free the booked map (MEM-045).
        $mv(st->hunks, sv->hunks);
        range32bHandOver(st->linesbuf, sv->linesbuf);
        st->scroll = sv->scroll;
        FILEUnMap(mapped);
        *fv = (BROfileview){};
        return __;
    }
    // Scroll to target line (1-based file line number).
    // Count only source-line starts — wrap continuations share the
    // same source line number and must not bump the counter.
    if (target_line > 0 && bro_nlines(st) > 1) {
        u32 file_ln = 0;
        u32 best = 1;
        for (u32 i = 0; i < bro_nlines(st); i++) {
            if (!bro_is_source_start(st->hunks, range32bDataC(st->linesbuf), i))
                continue;
            file_ln++;
            if (file_ln == target_line) { best = i; break; }
            best = i;
        }
        BROScrollCenter(st, best);
    } else {
        st->scroll = (bro_nlines(st) > 1) ? 1 : 0;
    }
    st->nsaves = idx + 1;
    done;
}

// Go back to the previous view. Frees the current file view's resources.
static b8 BROBack(BROstate *st) {
    if (st->nsaves <= 0) return NO;
    st->nsaves--;
    int idx = st->nsaves;

    // Free file view resources (tokens live in shared arena).
    BROfileview *fv = &st->files[idx];
    if (fv->mapped != NULL) FILEUnMap(fv->mapped);
    *fv = (BROfileview){};

    // Free current line index
    range32bFree(st->linesbuf);

    // Restore saved view
    BROsave *sv = &st->saves[idx];
    $mv(st->hunks, sv->hunks);
    range32bHandOver(st->linesbuf, sv->linesbuf);
    st->scroll = sv->scroll;
    return YES;
}

// Map a 1-based screen (row, col) to the (hunk, byte-offset) of the
// visible character under that cell.  Walks the line the same way
// BRORender does, skipping pass-hidden bytes so `col` counts emitted
// codepoints (not raw bytes).  Returns NO for title rows, the status
// bar, blank tail rows, or clicks past end-of-line.
static b8 bro_screen_to_byte(BROstate *st, u32 row, u32 col,
                             hunk const **hk_out, u32 *off_out) {
    if (row == 0 || col == 0) return NO;
    u32 vi = st->scroll + row - 1;
    if (vi >= bro_nlines(st)) return NO;
    range32 const *ln = &bro_lines(st)[vi];
    if (ln->hi == BRO_TITLE_LINE) return NO;
    hunk const *hk = &st->hunks[0][ln->lo];
    u32 textlen = (u32)$len(hk->text);
    u32 off = bro_line_off(ln);
    u8 pass = bro_line_pass(ln);
    u32 cols = st->cols > 0 ? st->cols : 80;
    u32 line_end = bro_row_end_pass(hk, textlen, off, cols, pass);

    int ntoks = (int)$len(hk->toks);
    int ti = 0;
    while (ti < ntoks && tok32Offset(hk->toks[0][ti]) <= off) ti++;
    u32 cp = 1;
    u32 pos = off;
    while (pos < line_end) {
        while (ti < ntoks && tok32Offset(hk->toks[0][ti]) <= pos) ti++;
        u8 side = (ti < ntoks) ? tok32Side(hk->toks[0][ti]) : TOK_SIDE_EQ;
        u8 tag  = (ti < ntoks) ? tok32Tag (hk->toks[0][ti]) : 'S';
        b8 hidden = (tag == 'U') ||
                    (pass == BRO_PASS_RM && side == TOK_SIDE_IN) ||
                    (pass == BRO_PASS_IN && side == TOK_SIDE_RM);
        u8 ch = hk->text[0][pos];
        u32 clen = UTF8_LEN[ch >> 4];
        if (clen == 0 || pos + clen > line_end) clen = 1;
        if (!hidden) {
            if (cp == col) { *hk_out = hk; *off_out = pos; return YES; }
            cp++;
        }
        pos += clen;
    }
    return NO;
}

static b8 bro_is_word(u8 c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9') || c == '_';
}

// Find the token whose byte range contains `off` and copy its bytes
// to `out` (NUL-terminated).  Right-click greps for whatever the
// tokenizer fused — `TEST-123` lives in one F-tagged span, so the
// grep needle is `TEST-123` and not just `TEST`.  Returns 0 (no-op)
// when the token is pure whitespace / pure punctuation (nothing
// useful to grep for) or doesn't fit in `cap`.
static u32 bro_word_around(hunk const *hk, u32 off, char *out, u32 cap) {
    u32 textlen = (u32)$len(hk->text);
    if (off >= textlen) return 0;

    u32 ntok = (u32)$len(hk->toks);
    u32 tlo = 0, thi = 0;
    b8 found = NO;
    for (u32 i = 0; i < ntok; i++) {
        u32 end = tok32Offset(hk->toks[0][i]);
        if (off < end) { thi = end; found = YES; break; }
        tlo = end;
    }
    if (!found) return 0;

    b8 grepable = NO;
    for (u32 i = tlo; i < thi; i++) {
        u8 c = hk->text[0][i];
        if (bro_is_word(c) || c >= 0x80) { grepable = YES; break; }
    }
    if (!grepable) return 0;

    u32 n = thi - tlo;
    if (cap == 0 || n + 1 > cap) return 0;
    for (u32 i = 0; i < n; i++) out[i] = (char)hk->text[0][tlo + i];
    out[n] = 0;
    return n;
}

// Try to open the file/dir referenced by the hunk at the given line.
// For directory listings, the line text IS the filename; the hunk URI
// is the directory. Constructs dir/filename and opens.
static b8 BROTryOpen(BROstate *st, u32 line, char const *repo) {
    if (line >= bro_nlines(st)) return NO;
    range32 *ln = &bro_lines(st)[line];
    u32 hunk_idx = ln->lo;
    hunk const *hk = &st->hunks[0][hunk_idx];
    BROloc loc = {};
    BROHunkLoc(&loc, hk);

    // If this is a content line (not title), check if the hunk is a
    // dir listing. In that case the line text is the entry name.
    if (ln->hi != BRO_TITLE_LINE && !$empty(loc.path)) {
        u32 textlen = (u32)$len(hk->text);
        u32 off = ln->hi;
        u32 end = off;
        while (end < textlen && hk->text[0][end] != '\n') end++;
        if (end > off) {
            u32 elen = end - off;
            // Strip trailing '/' for dirs
            b8 is_dir = (hk->text[0][end - 1] == '/');
            if (is_dir) elen--;
            // Build full path: dir/entry
            a_path(fpbufA);
            a_rest(u8c, after, hk->text, off);
            a_head(u8c, entry, after, elen);
            if (PATHu8bFeed(fpbufA, loc.path) != OK) return NO;
            if (PATHu8bPush(fpbufA, entry) != OK) return NO;
            {
                if (repo == NULL || repo[0] == 0) {
                    bro_flash(st, "no repo root");
                    return NO;
                }
                if (is_dir) {
                    // Open directory listing
                    if (st->nsaves >= BRO_MAX_VIEWS) return NO;
                    int idx = st->nsaves;
                    BROsave *sv = &st->saves[idx];
                    *sv = (BROsave){};
                    $mv(sv->hunks, st->hunks);
                    sv->scroll = st->scroll;
                    range32bHandOver(sv->linesbuf, st->linesbuf);
                    st->files[idx] = (BROfileview){};
                    u32 save_nh = bro_nhunks;
                    if (BROListDir(u8bDataC(fpbufA)) == OK && bro_nhunks > save_nh) {
                        st->hunks[0] = bro_hunks + save_nh;
                        st->hunks[1] = bro_hunks + bro_nhunks;
                        BROBuildIndex(st);
                        st->scroll = (bro_nlines(st) > 1) ? 1 : 0;
                        st->nsaves = idx + 1;
                        return YES;
                    }
                    return NO;
                }
                ok64 o = BROOpenFile(st, u8bDataC(fpbufA), repo, 0);
                if (o != OK) {
                    bro_flash(st, "open: " U8SFMT ": %s",
                              u8sFmt(u8bDataC(fpbufA)), ok64str(o));
                    return NO;
                }
                return YES;
            }
        }
    }

    // Normal hunk: open path from URI
    if ($empty(loc.path)) {
        bro_flash(st, "no path in hunk");
        return NO;
    }
    if (repo == NULL || repo[0] == 0) {
        bro_flash(st, "no repo root found");
        return NO;
    }
    ok64 o = BROOpenFile(st, loc.path, repo, loc.line);
    if (o != OK) {
        bro_flash(st, "open: " U8SFMT ": %s",
                  u8sFmt(loc.path), ok64str(o));
        return NO;
    }
    return YES;
}

// --- Navigation primitives ---
// These walk the line index built by BROBuildIndex (or
// BROExtendIndex). They are exposed via BRO.h for tests, so they take
// raw arrays rather than BROstate.

// A "hili run" is a maximal sequence of consecutive tokens whose side
// is non-eq AND share the same side (in/rm).  Transition eq→non-eq or
// in↔rm starts a new run.
static b8 bro_side_real(u8 side) {
    return side != TOK_SIDE_EQ ? YES : NO;
}

// Display line `i` is the first row of its source line (i.e. not a
// wrap continuation of a longer source line).  Title rows return NO.
// A row is a source-start if it begins with offset 0 OR the previous
// row covers a different hunk OR pass changes OR the previous source
// byte is '\n'.
static b8 bro_is_source_start(hunkcsc hunks, range32cs lines, u32 i) {
    u32 nlines = (u32)$len(lines);
    if (i >= nlines) return NO;
    range32 const *ln = &lines[0][i];
    if (ln->hi == BRO_TITLE_LINE) return NO;
    u32 off = bro_line_off(ln);
    if (i == 0 || off == 0) return YES;
    range32 const *prev = &lines[0][i - 1];
    if (prev->lo != ln->lo) return YES;
    if (prev->hi == BRO_TITLE_LINE) return YES;
    if (bro_line_pass(prev) != bro_line_pass(ln)) return YES;
    return (hunks[0][ln->lo].text[0][off - 1] == '\n') ? YES : NO;
}

// Find the line that contains byte offset `off` in hunk `h`.
// Returns the largest line index i with lines[i].lo == h,
// lines[i].hi != BRO_TITLE_LINE, and bro_line_off(lines+i) <= off.
// BRO_NONE if no such line exists.  Used by hili nav — picks the first
// matching pass (rm) since that comes first in source order.
static u32 bro_line_for_off(range32cs lines, u32 h, u32 off) {
    u32 nlines = (u32)$len(lines);
    u32 best = BRO_NONE;
    for (u32 i = 0; i < nlines; i++) {
        if (lines[0][i].lo < h) continue;
        if (lines[0][i].lo > h) break;  // hunks are appended in order
        if (lines[0][i].hi == BRO_TITLE_LINE) continue;
        u32 lo_off = bro_line_off(&lines[0][i]);
        if (lo_off <= off) best = i;
        else break;
    }
    return best;
}

u32 BROHunkNextLine(range32cs lines, u32 from) {
    u32 nlines = (u32)$len(lines);
    if (nlines == 0) return BRO_NONE;
    u32 start = (from + 1 < nlines) ? (from + 1) : nlines;
    for (u32 i = start; i < nlines; i++) {
        if (lines[0][i].lo != lines[0][i - 1].lo) return i;
    }
    return BRO_NONE;
}

u32 BROHunkPrevLine(range32cs lines, u32 from) {
    u32 nlines = (u32)$len(lines);
    if (nlines == 0 || from == 0) return BRO_NONE;
    if (from >= nlines) from = nlines - 1;
    // Walk back to the start line of the current hunk.
    u32 i = from;
    while (i > 0 && lines[0][i - 1].lo == lines[0][i].lo) i--;
    if (i < from) return i;  // mid-hunk: jump to start of current
    // Already at the current hunk's start; step into the previous hunk.
    if (i == 0) return BRO_NONE;
    i--;
    while (i > 0 && lines[0][i - 1].lo == lines[0][i].lo) i--;
    return i;
}

u32 BROHunkCount(range32cs lines) {
    u32 nlines = (u32)$len(lines);
    if (nlines == 0) return 0;
    u32 n = 1;
    for (u32 i = 1; i < nlines; i++)
        if (lines[0][i].lo != lines[0][i - 1].lo) n++;
    return n;
}

u32 BROHunkIndexAt(range32cs lines, u32 at) {
    u32 nlines = (u32)$len(lines);
    if (nlines == 0) return 0;
    if (at >= nlines) at = nlines - 1;
    u32 n = 1;
    for (u32 i = 1; i <= at; i++)
        if (lines[0][i].lo != lines[0][i - 1].lo) n++;
    return n;
}

// A token starts a new hili run iff its side is non-eq AND either it
// is the first tok or the previous tok has a different (eq or other) side.
static b8 bro_is_run_start(hunk const *hk, u32 j) {
    u8 side = tok32Side(hk->toks[0][j]);
    if (!bro_side_real(side)) return NO;
    if (j == 0) return YES;
    u8 prev = tok32Side(hk->toks[0][j - 1]);
    return (prev != side) ? YES : NO;
}

u32 BROHiliCount(hunkcs hunks) {
    u32 n = 0;
    $for (hunk const, hk, hunks) {
        u32 nh = (u32)$len(hk->toks);
        for (u32 j = 0; j < nh; j++)
            if (bro_is_run_start(hk, j)) n++;
    }
    return n;
}

u32 BROHiliIndexAt(hunkcs hunks, range32cs lines, u32 at) {
    u32 n = 0;
    u32 nhunks = (u32)$len(hunks);
    for (u32 h = 0; h < nhunks; h++) {
        hunk const *hk = &hunks[0][h];
        u32 nh = (u32)$len(hk->toks);
        for (u32 j = 0; j < nh; j++) {
            if (!bro_is_run_start(hk, j)) continue;
            u32 start_off = (j == 0) ? 0
                          : tok32Offset(hk->toks[0][j - 1]);
            u32 ln = bro_line_for_off(lines, h, start_off);
            if (ln == BRO_NONE) continue;
            if (ln <= at) n++;
            else return n;
        }
    }
    return n;
}

u32 BROHiliNextLine(hunkcs hunks, range32cs lines, u32 mid) {
    u32 nhunks = (u32)$len(hunks);
    for (u32 h = 0; h < nhunks; h++) {
        hunk const *hk = &hunks[0][h];
        u32 nh = (u32)$len(hk->toks);
        for (u32 j = 0; j < nh; j++) {
            if (!bro_is_run_start(hk, j)) continue;
            u32 start_off = (j == 0) ? 0
                          : tok32Offset(hk->toks[0][j - 1]);
            u32 ln = bro_line_for_off(lines, h, start_off);
            if (ln == BRO_NONE) continue;
            if (ln > mid) return ln;
        }
    }
    return BRO_NONE;
}

u32 BROHiliPrevLine(hunkcs hunks, range32cs lines, u32 mid) {
    u32 best = BRO_NONE;
    u32 nhunks = (u32)$len(hunks);
    for (u32 h = 0; h < nhunks; h++) {
        hunk const *hk = &hunks[0][h];
        u32 nh = (u32)$len(hk->toks);
        for (u32 j = 0; j < nh; j++) {
            if (!bro_is_run_start(hk, j)) continue;
            u32 start_off = (j == 0) ? 0
                          : tok32Offset(hk->toks[0][j - 1]);
            u32 ln = bro_line_for_off(lines, h, start_off);
            if (ln == BRO_NONE) continue;
            if (ln >= mid) return best;
            best = ln;
        }
    }
    return best;
}

// --- Rendering ---
// All screen output is built into the bro_scr buffer, flushed once.
// bro_scr is carved from BASS at the top of BRORun / BROPipeRun (the
// whole-session frame) and reached here via a file-static pointer —
// the same install-at-entry pattern as bro_state.  No dedicated mmap.

#define BRO_SCR_SIZE (1UL << 20)  // 1MB screen buffer
static Bu8 *bro_scr_p = NULL;
#define bro_scr (*bro_scr_p)

static ok64 BROScreenInit(void) {
    sane(bro_scr_p != NULL);
    u8bReset(bro_scr);
    done;
}

static void BROScreenFlush(void) {
    if (u8bDataLen(bro_scr) > 0)
        (void)write(STDOUT_FILENO, u8bDataHead(bro_scr), u8bDataLen(bro_scr));
    u8bReset(bro_scr);
}

// Feed string literal (use scr_puts("text") only with literals/known-len)
static void scr_puts(char const *s) {
    a_cstr(cs, s);
    u8sFeed(u8bIdle(bro_scr), cs);
}

// Emit the SGR for THEMEActive->fg_title.  Title color is a one-shot
// escape outside the scr_emit_char SGR-delta machinery, so we render
// the sequence inline here (caller follows with TTY_RESET).
static void scr_emit_title_color(void) {
    ANSIu8sFeedDelta(u8bIdle(bro_scr), THEMEAt('T'), ANSI_DEFAULT);
}

// Feed goto escape: \033[row;colH
static void scr_goto(int row, int col) {
    a_pad(u8, tmp, 32);
    u8sFeed1(tmp_idle, 033);
    u8sFeed1(tmp_idle, '[');
    utf8sFeed10(tmp_idle, (u64)row);
    u8sFeed1(tmp_idle, ';');
    utf8sFeed10(tmp_idle, (u64)col);
    u8sFeed1(tmp_idle, 'H');
    u8bFeed(bro_scr, u8bDataC(tmp));
}

// Direct write helpers (for prompts/setup, not screen rendering)
static void bro_puts(char const *s) {
    a_cstr(cs, s);
    (void)write(STDOUT_FILENO, cs[0], $len(cs));
}
static void bro_write(u8csc s) {
    (void)write(STDOUT_FILENO, s[0], $len(s));
}
static void bro_goto(int row, int col) {
    a_pad(u8, tmp, 32);
    u8sFeed1(tmp_idle, 033);
    u8sFeed1(tmp_idle, '[');
    utf8sFeed10(tmp_idle, (u64)row);
    u8sFeed1(tmp_idle, ';');
    utf8sFeed10(tmp_idle, (u64)col);
    u8sFeed1(tmp_idle, 'H');
    bro_write(u8bDataC(tmp));
}

//  Last SGR state emitted on the current screen row.  scr_emit_char
//  skips the SGR open when the desired state matches; the row driver
//  invokes scr_emit_reset() at row end to drop any open attrs in one
//  final `\033[0m`.  See abc/ANSI.h for the ansi64 layout and the
//  ANSIu8sFeedDelta / ANSIu8sFeedReset spellers.
static ansi64 bro_emit_cur;

static void scr_emit_reset(void) {
    ANSIu8sFeedReset(u8bIdle(bro_scr), bro_emit_cur);
    bro_emit_cur = ANSI_DEFAULT;
}

// Feed one codepoint with fg tag, bg tag, and search highlight.
// Emits an SGR transition only when the packed state differs from
// bro_emit_cur — runs of identical-style chars share one open-SGR.
// Callers MUST invoke scr_emit_reset() at row end (or before any raw
// escape sequence written via scr_puts) to flush trailing state.
static void scr_emit_char(u8cp p, u32 n, ansi64 want) {
    u8sp out = u8bIdle(bro_scr);
    if (want != bro_emit_cur) {
        ANSIu8sFeedDelta(out, want, bro_emit_cur);
        bro_emit_cur = want;
    }
    u8cs chars = {p, p + n};
    u8sFeed(out, chars);
}

// Check if search pattern matches at position pos
static b8 bro_search_at(BROstate *st, u8csc text, u32 pos) {
    u8cs ndl = {};
    $mv(ndl, u8bDataC(st->search));
    if ($empty(ndl)) return NO;
    if (pos + $len(ndl) > (size_t)$len(text)) return NO;
    a_part(u8c, hay, text, pos, $len(ndl));
    return $eq(hay, ndl);
}

static void BROStatusBar(BROstate *st);
fun b8 hunk_is_ulog(hunkc const *hk);

// Format display title "--- path :: func ---" into buf.
// Returns the number of bytes written (excl NUL).
//
// ULOG-shape hunks (ts or verb set) render as `<verb> <uri>` instead
// — same identity as the per-row ULOG header minus the date column.
static int bro_format_title(char *buf, size_t bufsz, hunkc const *hk) {
    if (hunk_is_ulog(hk)) {
        a_pad(u8, vbuf, 16);
        if (hk->verb) (void)RONutf8sFeed(vbuf_idle, hk->verb);
        a_dup(u8 const, vd, vbuf_datac);
        int n;
        if (!$empty(vd) && !$empty(hk->uri))
            n = snprintf(buf, bufsz, U8SFMT " " U8SFMT,
                         u8sFmt(vd), u8sFmt(hk->uri));
        else if (!$empty(hk->uri))
            n = snprintf(buf, bufsz, U8SFMT, u8sFmt(hk->uri));
        else if (!$empty(vd))
            n = snprintf(buf, bufsz, U8SFMT, u8sFmt(vd));
        else
            n = 0;
        if (n < 0) n = 0;
        if ((size_t)n >= bufsz) n = (int)(bufsz - 1);
        return n;
    }
    BROloc loc = {};
    BROHunkLoc(&loc, hk);
    b8 has_p = !$empty(loc.path);
    b8 has_f = !$empty(loc.symbol);
    int n = 0;
    if (has_p && has_f && loc.line > 0)
        n = snprintf(buf, bufsz, "--- " U8SFMT " :: " U8SFMT ":%u ---",
                     u8sFmt(loc.path), u8sFmt(loc.symbol), loc.line);
    else if (has_p && has_f)
        n = snprintf(buf, bufsz, "--- " U8SFMT " :: " U8SFMT " ---",
                     u8sFmt(loc.path), u8sFmt(loc.symbol));
    else if (has_p && loc.line > 0)
        n = snprintf(buf, bufsz, "--- " U8SFMT ":%u ---",
                     u8sFmt(loc.path), loc.line);
    else if (has_p)
        n = snprintf(buf, bufsz, "--- " U8SFMT " ---", u8sFmt(loc.path));
    else if (has_f)
        n = snprintf(buf, bufsz, "--- " U8SFMT " ---", u8sFmt(loc.symbol));
    if (n < 0) n = 0;
    if ((size_t)n >= bufsz) n = (int)(bufsz - 1);
    return n;
}

// A hunk carries ULOG-event shape when ts or verb is set.  Such titles
// render as the footer-style row `<date> <verb> <uri>`; click navigates
// via `be --tlv <uri>` so projector URIs work too.
fun b8 hunk_is_ulog(hunkc const *hk) { return hk->ts != 0 || hk->verb != 0; }

// Render a ULOG-shape title row into bro_scr (BRO-001): a banner that
// spans the full terminal width — black text on a pale-yellow band
// (THEME_BANNER) padded with spaces so the colour reaches the right
// edge.  7-cell date column, verb, then URI (elided to fit).  bro is
// the width-aware layer that knows `cols`, so it owns the full-width
// fill; the shared formatter (`HUNKu8sFeedColor`) frames the same
// content with the same THEME_BANNER SGR but width-agnostically.
// Caller has already moved the cursor to column 1 and emitted
// TTY_ERASE_LINE.
static void bro_render_ulog_title(BROstate *st, hunkc const *hk) {
    if (st->cols == 0) return;
    u32 cols = st->cols;
    u32 used = 0;

    ansi64 band = THEME_BANNER;
    ansi64 cur  = ANSI_DEFAULT;

    (void)ANSIu8sFeedDelta(u8bIdle(bro_scr), band, cur); cur = band;

    u8sFeed1(u8bIdle(bro_scr), ' ');
    used++;

    if (used < cols) {
        i64 now = (i64)time(NULL);
        i64 ts  = now;
        if (hk->ts) {
            struct tm tm = {};
            if (RONToTime(hk->ts, &tm, NULL) == OK) {
                time_t t = mktime(&tm);
                if (t != (time_t)-1) ts = (i64)t;
            }
        }
        (void)DOGutf8sFeedDate(u8bIdle(bro_scr), ts, now);
        used = used + 7 < cols ? used + 7 : cols;
    }

    if (used < cols) { u8sFeed1(u8bIdle(bro_scr), ' '); used++; }

    if (used < cols && hk->verb) {
        a_pad(u8, vbuf, 16);
        (void)RONutf8sFeed(vbuf_idle, hk->verb);
        a_dup(u8 const, vdata, vbuf_datac);
        size_t vlen  = u8csLen(vdata);
        size_t vshow = vlen < cols - used ? vlen : cols - used;
        if (vshow > 0) {
            a_head(u8c, vshow_sl, vdata, vshow);
            u8sFeed(u8bIdle(bro_scr), vshow_sl);
            used += (u32)vshow;
        }
    }

    if (used < cols) { u8sFeed1(u8bIdle(bro_scr), ' '); used++; }

    if (used < cols && !$empty(hk->uri)) {
        u32 avail = cols - used;
        a_dup(u8 const, uri, hk->uri);
        size_t ulen = u8csLen(uri);
        if (ulen <= avail) {
            u8sFeed(u8bIdle(bro_scr), uri);
            used += (u32)ulen;
        } else {
            a_dup(u8 const, scan, hk->uri);
            b8 has_slash = (u8csRevFind(scan, '/') == OK);
            size_t suff_off = has_slash ? u8csLen(scan) - 1 : 0;
            size_t suff_len = ulen - suff_off;
            if (has_slash && suff_len + 3 <= avail) {
                a_cstr(dots, "...");
                u8sFeed(u8bIdle(bro_scr), dots);
                a_rest(u8c, suffix, hk->uri, suff_off);
                u8sFeed(u8bIdle(bro_scr), suffix);
                used += 3 + (u32)suff_len;
            } else if (avail > 3) {
                a_head(u8c, head, uri, avail - 3);
                u8sFeed(u8bIdle(bro_scr), head);
                a_cstr(dots, "...");
                u8sFeed(u8bIdle(bro_scr), dots);
                used = cols;
            } else {
                a_head(u8c, head, uri, avail);
                u8sFeed(u8bIdle(bro_scr), head);
                used += avail;
            }
        }
    }

    // Pad to full screen width so the pale-yellow band reaches the edge.
    while (used < cols) {
        u8sFeed1(u8bIdle(bro_scr), ' ');
        used++;
    }

    (void)ANSIu8sFeedReset(u8bIdle(bro_scr), cur);
}

static void BRORender(BROstate *st) {
    u8bReset(bro_scr);
    scr_puts(TTY_CUR_HOME);

    u32 visible = (u32)(st->rows - 1);
    if (bro_nlines(st) > visible) {
        if (st->scroll > bro_nlines(st) - visible)
            st->scroll = bro_nlines(st) - visible;
    } else {
        st->scroll = 0;
    }
    u32 end = st->scroll + visible;
    if (end > bro_nlines(st)) end = bro_nlines(st);

    for (u32 vi = st->scroll; vi < end; vi++) {
        scr_goto((int)(vi - st->scroll + 1), 1);
        scr_puts(TTY_ERASE_LINE);

        range32 *ln = &bro_lines(st)[vi];
        hunk const *hk = &st->hunks[0][ln->lo];

        if (ln->hi == BRO_TITLE_LINE) {
            if (hunk_is_ulog(hk)) {
                bro_render_ulog_title(st, hk);
                continue;
            }
            scr_emit_title_color();
            char dtitle[HUNK_TITLE_MAX + 1];
            int dtlen = bro_format_title(dtitle, sizeof(dtitle), hk);
            u32 w = (u32)dtlen < st->cols ? (u32)dtlen : st->cols;
            u8cs ttl = {(u8cp)dtitle, (u8cp)dtitle + w};
            u8sFeed(u8bIdle(bro_scr), ttl);
            scr_puts(TTY_RESET);
            continue;
        }

        u32 textlen = (u32)$len(hk->text);
        u32 off = bro_line_off(ln);
        u8 pass = bro_line_pass(ln);
        u32 cols = st->cols > 0 ? st->cols : 80;
        u32 line_end = bro_row_end_pass(hk, textlen, off, cols, pass);
        u32 w = line_end - off;

        // Find tok cursor for this offset
        int ntoks = (int)$len(hk->toks);
        int tok_i = 0;
        while (tok_i < ntoks &&
               tok32Offset(hk->toks[0][tok_i]) <= off)
            tok_i++;

        // Carry-over byte counter so a multi-byte search match stays
        // highlighted across the whole run, not just its first char.
        // Seed from any match that started in the previous wrap segment
        // of the same source line (search can span wrap, not '\n').
        u32 search_left = 0;
        u32 slen = (u32)u8bDataLen(st->search);
        if (slen > 1 && off > 0) {
            u32 src_start = off;
            while (src_start > 0 && hk->text[0][src_start - 1] != '\n')
                src_start--;
            u32 back = slen - 1;
            if (off - src_start < back) back = off - src_start;
            for (u32 k = back; k >= 1; k--) {
                u32 bp = off - k;
                if (bro_search_at(st, hk->text, bp)) {
                    search_left = slen - k;
                    break;
                }
            }
        }
        for (u32 j = 0; j < w; ) {
            u32 pos = off + j;
            u8 ch = hk->text[0][pos];
            u32 clen = UTF8_LEN[ch >> 4];
            if (clen > w - j) clen = w - j;  // clamp to line end
            // Advance cursors past pos
            while (tok_i < ntoks &&
                   tok32Offset(hk->toks[0][tok_i]) <= pos)
                tok_i++;
            u8 fg_tag = (tok_i < ntoks)
                            ? tok32Tag(hk->toks[0][tok_i]) : 'S';
            u8 side = (tok_i < ntoks)
                          ? tok32Side(hk->toks[0][tok_i]) : TOK_SIDE_EQ;
            // Skip bytes hidden by this pass (the row width already
            // accounts for them, so just don't emit).  'U'-tagged
            // tokens carry click-target URI bytes — invisible too.
            if (fg_tag == 'U' ||
                (pass == BRO_PASS_RM && side == TOK_SIDE_IN) ||
                (pass == BRO_PASS_IN && side == TOK_SIDE_RM)) {
                j += clen;
                continue;
            }
            if (search_left == 0 && bro_search_at(st, hk->text, pos))
                search_left = slen;
            b8 in_search = (search_left > 0) ? YES : NO;
            scr_emit_char(hk->text[0] + pos, clen,
                          bro_cell_ansi(fg_tag, pass, side, in_search));
            if (search_left >= clen) search_left -= clen;
            else search_left = 0;
            j += clen;
        }
        scr_emit_reset();
    }

    for (u32 row = end - st->scroll + 1; row < st->rows; row++) {
        scr_goto((int)row, 1);
        scr_puts(TTY_ERASE_LINE);
    }

    BROStatusBar(st);
    BROScreenFlush();
}

// Set st->scroll so that line `target` lands on the middle row.
// Clamps to [0, nlines - page].
static void BROScrollCenter(BROstate *st, u32 target) {
    u32 page = (st->rows > 1) ? (u32)(st->rows - 1) : 1;
    u32 half = page / 2;
    u32 s = (target > half) ? (target - half) : 0;
    if (bro_nlines(st) > page && s > bro_nlines(st) - page)
        s = bro_nlines(st) - page;
    st->scroll = s;
}

//  YES iff `ch->uri`'s fragment is empty or a pure-line marker
//  (`L<digits>` / bare `<digits>`).  Pure-symbol or labelled-line
//  fragments (`#'sym'`, `#sym:L42`, `#abc1234`, …) return NO — those
//  hunks own their own URI and the status bar shows them verbatim.
static b8 bro_uri_is_line_or_empty(hunk const *ch) {
    if ($empty(ch->uri)) return YES;
    uri u = {};
    u8csc text = {ch->uri[0], ch->uri[1]};
    if (DOGParseURI(&u, text) != OK) return NO;
    if ($empty(u.fragment)) return YES;
    //  HUNKu8sFragSplit writes the symbol body slice into sym_s (or
    //  zeroes it when there's no symbol).  Pure-line iff a line was
    //  extracted AND the symbol slice ended up empty.
    u8cs sym_s = {};
    u8csc fr = {u.fragment[0], u.fragment[1]};
    u32 line = HUNKu8sFragSplit(fr, sym_s);
    return (line > 0 && $empty(sym_s));
}

//  Source line at the screen-centre row.  Walks source-line starts
//  from the hunk's first display row to the centre row.  Returns 0
//  when the view is empty.  Centre tracks `st->scroll + visible/2`
//  clamped to the active hunk's last row.
//
//  Two URI shapes for the hunk's fragment land here:
//    * Pure-line (`#L<n>` / `#<n>` / no fragment) — `<n>` was a
//      *navigation marker* (cursor parked there on open), not a
//      hunk-text anchor.  Source line = count from hunk top.
//    * Labelled line (`#'sym':L<n>`, `#sym:L<n>`) — `<n>` *is* the
//      hunk-text anchor (snippet from grep/log/blame starts at <n>).
//      Source line = `<n> + count - 1`.
static u32 bro_center_src_line(BROstate const *st) {
    if (bro_nlines(st) == 0) return 0;
    u32 visible = (st->rows > 1) ? (u32)(st->rows - 1) : 1;
    u32 cur = st->scroll + visible / 2;
    if (cur >= bro_nlines(st)) cur = bro_nlines(st) - 1;
    range32 const *lines = bro_lines((BROstate *)st);
    u32 cur_hunk = lines[cur].lo;
    while (cur > st->scroll && lines[cur].lo != cur_hunk) cur--;
    u32 first = cur;
    while (first > 0 && lines[first - 1].lo == cur_hunk) first--;
    u32 count = 0;
    for (u32 i = first; i <= cur; i++) {
        if (bro_is_source_start(st->hunks,
                                range32bDataC(st->linesbuf), i))
            count++;
    }
    //  Snippet anchor only when the fragment carries a symbol body
    //  (e.g., `#'sym':L42`, `#sym:L42`).  Pure-line / no fragment →
    //  the URI's L<n> is a navigation marker, not a hunk-anchor;
    //  source line is just `count` from the hunk's top.
    hunk const *ch = &st->hunks[0][cur_hunk];
    b8 use_loc_anchor = NO;
    if (!$empty(ch->uri)) {
        uri u = {};
        u8csc text = {ch->uri[0], ch->uri[1]};
        if (DOGParseURI(&u, text) == OK && !$empty(u.fragment)) {
            u8cs sym_s = {};
            u8csc fr = {u.fragment[0], u.fragment[1]};
            (void)HUNKu8sFragSplit(fr, sym_s);
            use_loc_anchor = !$empty(sym_s);
        }
    }
    BROloc loc = {};
    BROHunkLoc(&loc, ch);
    return (use_loc_anchor && loc.line > 0) ? loc.line + count - 1 : count;
}

//  Compose the URI shown in the status bar.  Rule:
//    * Hunk URI carries a non-line fragment (`#'snippet'`, `#abc…`,
//      `#sym:L42`) → output `ch->uri` verbatim — the producer owns it.
//    * Otherwise (no fragment, or pure-line `#L<n>` / `#<n>`) →
//      `<path>#L<centre-source-line>` so the centre of the screen
//      becomes a stable, reopenable address.
//  NUL-terminated; caller-provided buffer.
static void BROStatusURI(BROstate const *st, char *out, size_t cap) {
    if (cap == 0) return;
    out[0] = 0;
    if (bro_nlines(st) == 0) return;
    u32 cur_hunk = bro_lines((BROstate *)st)[st->scroll].lo;
    hunk const *ch = &st->hunks[0][cur_hunk];

    if (!bro_uri_is_line_or_empty(ch)) {
        if (!$empty(ch->uri)) snprintf(out, cap, U8SFMT, u8sFmt(ch->uri));
        return;
    }

    BROloc loc = {};
    BROHunkLoc(&loc, ch);
    u32 center = bro_center_src_line(st);
    if (!$empty(loc.path) && center > 0)
        snprintf(out, cap, U8SFMT "#L%u", u8sFmt(loc.path), center);
    else if (!$empty(ch->uri))
        snprintf(out, cap, U8SFMT, u8sFmt(ch->uri));
}

static void BROStatusBar(BROstate *st) {
    scr_goto(st->rows, 1);
    scr_puts(TTY_UNDERLINE TTY_ERASE_LINE);

    // Flash message: show it and clear for next render.
    if (u8bDataLen(st->flash) > 0) {
        u8sFeed1(u8bIdle(bro_scr), ' ');
        u8cs body = {};
        $mv(body, u8bDataC(st->flash));
        if ((u32)$len(body) + 1 > st->cols && st->cols > 1) {
            a$head(u8c, clipped, body, (size_t)(st->cols - 1));
            u8bFeed(bro_scr, clipped);
        } else {
            u8bFeed(bro_scr, body);
        }
        scr_puts(TTY_RESET);
        u8bReset(st->flash);
        return;
    }

    u32 cur_hunk = 0;
    if (st->scroll < bro_nlines(st))
        cur_hunk = bro_lines(st)[st->scroll].lo;

    u32 hunk_tot = BROHunkCount(range32bDataC(st->linesbuf));
    u32 hunk_idx = BROHunkIndexAt(range32bDataC(st->linesbuf), st->scroll);
    u32 hili_tot = BROHiliCount(st->hunks);
    u32 hili_idx = BROHiliIndexAt(st->hunks,
                                  range32bDataC(st->linesbuf), st->scroll);

    hunk const *ch = &st->hunks[0][cur_hunk];

    // Scroll position as percentage (source line numbers are in the
    // URI, not the local index — see project docs).  ALL means the
    // whole view fits on screen; TOP / BOT are the endpoints.
    char posbuf[8];
    u32 visible = (st->rows > 1) ? (u32)(st->rows - 1) : 1;
    if (bro_nlines(st) <= visible)
        snprintf(posbuf, sizeof(posbuf), "ALL");
    else if (st->scroll == 0)
        snprintf(posbuf, sizeof(posbuf), "TOP");
    else if (st->scroll + visible >= bro_nlines(st))
        snprintf(posbuf, sizeof(posbuf), "BOT");
    else {
        u32 max_scroll = bro_nlines(st) - visible;
        u32 pct = (u32)((u64)st->scroll * 100 / max_scroll);
        if (pct > 99) pct = 99;  // reserve 100% for BOT
        snprintf(posbuf, sizeof(posbuf), "%u%%", pct);
    }

    // Build the right-side stats first so we know how much room the
    // title gets.  Use a small local buffer (stats are short).
    char stats[128];
    int sn;
    size_t plen = u8bDataLen(st->search);
    char const *spat = (char const *)u8bDataHead(st->search);
    if (hili_tot > 0 && plen > 0)
        sn = snprintf(stats, sizeof(stats),
                      "  %s  H%u/%u  C%u/%u  [/%.*s]",
                      posbuf,
                      hunk_idx, hunk_tot, hili_idx, hili_tot,
                      (int)plen, spat);
    else if (hili_tot > 0)
        sn = snprintf(stats, sizeof(stats),
                      "  %s  H%u/%u  C%u/%u",
                      posbuf,
                      hunk_idx, hunk_tot, hili_idx, hili_tot);
    else if (plen > 0)
        sn = snprintf(stats, sizeof(stats),
                      "  %s  H%u/%u  [/%.*s]",
                      posbuf,
                      hunk_idx, hunk_tot,
                      (int)plen, spat);
    else
        sn = snprintf(stats, sizeof(stats),
                      "  %s  H%u/%u",
                      posbuf,
                      hunk_idx, hunk_tot);
    if (sn < 0) sn = 0;
    if ((size_t)sn >= sizeof(stats)) sn = (int)(sizeof(stats) - 1);

    // Status URI tracks the current view position.  The shared
    // BROStatusURI helper applies the same logic the `r` reload uses
    // — centre line for cat-style hunks (no fragment or pure-line),
    // verbatim for snippet hunks (`#'sym'`, `#abc1234`, …).
    char dtitle[HUNK_TITLE_MAX + 1];
    BROStatusURI(st, dtitle, sizeof(dtitle));
    int dtlen = (int)strlen(dtitle);
    if (dtlen == 0 && !$empty(ch->uri)) {
        dtlen = snprintf(dtitle, sizeof(dtitle), U8SFMT, u8sFmt(ch->uri));
    }
    if (dtlen < 0) dtlen = 0;
    if ((size_t)dtlen >= sizeof(dtitle)) dtlen = (int)(sizeof(dtitle) - 1);

    // Truncate title to fit: " <title><stats>" within cols.
    u32 cols = st->cols;
    u32 stats_w = (u32)sn;
    u32 title_max = (cols > stats_w + 2) ? (cols - stats_w - 1) : 0;
    u32 title_len = (u32)dtlen;
    if (title_len > title_max) title_len = title_max;

    // Layout: " <title><padding><stats>" exactly cols wide.
    u32 left_len = 1 + title_len;
    u32 pad = (cols > left_len + stats_w) ? (cols - left_len - stats_w) : 0;

    u8sp out = u8bIdle(bro_scr);
    int slen = snprintf((char *)out[0], $len(out),
                        " %.*s%*s%s",
                        (int)title_len, dtitle,
                        (int)pad, "",
                        stats);
    if (slen > 0) {
        if ((u32)slen > cols) slen = (int)cols;
        u8sFed(out, (size_t)slen);
    }
    scr_puts(TTY_RESET);
}

// --- Search ---

// Find next occurrence of search pattern starting from line `from` (direction=+1)
// or backwards from `from` (direction=-1). Returns line index or UINT32_MAX.
//
// A match is reported on the display row whose first byte contains the
// match's start.  The match body may extend past the row's end into a
// wrap continuation (same source line) but not across a '\n'.
static u32 BROSearchNext(BROstate *st, u32 from, int direction) {
    u8cs ndl = {};
    $mv(ndl, u8bDataC(st->search));
    if ($empty(ndl)) return UINT32_MAX;
    u32 slen = (u32)$len(ndl);
    u32 i = from;
    for (;;) {
        if (direction > 0) {
            i++;
            if (i >= bro_nlines(st)) return UINT32_MAX;
        } else {
            if (i == 0) return UINT32_MAX;
            i--;
        }
        range32 *ln = &bro_lines(st)[i];
        if (ln->hi == BRO_TITLE_LINE) continue;
        hunk const *hk = &st->hunks[0][ln->lo];
        u32 textlen = (u32)$len(hk->text);
        // `hi` packs (pass<<24)|offset — decode like every other reader.
        // Using the raw word as a byte index reads ~16 MB OOB for any
        // non-NORMAL-pass row (MEM-006).
        u32 off = bro_line_off(ln);
        u8 pass = bro_line_pass(ln);
        // Byte span of this display row (bounded by next row in same
        // source line, else source-line end at '\n').
        u32 row_end;
        range32 const *nln =
            (i + 1 < bro_nlines(st)) ? &bro_lines(st)[i + 1] : NULL;
        // The next row continues this one only when it is the same hunk,
        // the same render pass (a different pass = a separate rm/in view
        // of the same source line, not a wrap continuation), and not a
        // title sentinel.
        if (nln && nln->lo == ln->lo && nln->hi != BRO_TITLE_LINE &&
            bro_line_pass(nln) == pass) {
            u32 nhi = bro_line_off(nln);
            // Continuation if no '\n' between; otherwise nhi sits past
            // the '\n'.  Either way, the row's first-byte limit is nhi
            // (continuations) or the '\n' (new source line).
            row_end = (nhi > off && hk->text[0][nhi - 1] == '\n')
                      ? nhi - 1 : nhi;
        } else {
            row_end = off;
            while (row_end < textlen && hk->text[0][row_end] != '\n')
                row_end++;
        }
        // Match body may extend up to the source-line end.
        u32 src_end = row_end;
        while (src_end < textlen && hk->text[0][src_end] != '\n')
            src_end++;
        if (row_end > off && src_end - off >= slen) {
            u32 first_byte_limit = row_end;
            u32 max_start = src_end - slen;
            if (first_byte_limit > max_start + 1) first_byte_limit = max_start + 1;
            for (u32 j = 0; off + j < first_byte_limit; j++) {
                a_part(u8c, hay, hk->text, off + j, slen);
                if ($eq(hay, ndl)) return i;
            }
        }
    }
}

// Read search pattern from user (displayed in status bar)
static void BROReadSearch(BROstate *st) {
    u8bReset(st->search);

    for (;;) {
        // Render prompt on status bar
        bro_goto(st->rows, 1);
        bro_puts(TTY_UNDERLINE TTY_ERASE_LINE);
        bro_puts(" /");
        if (u8bDataLen(st->search) > 0)
            bro_write(u8bDataC(st->search));
        bro_puts(TTY_RESET);

        u8 ch = 0;
        ssize_t n = read(st->tty_fd, &ch, 1);
        if (n <= 0) continue;
        if (ch == '\n' || ch == '\r') break;  // confirm
        if (ch == 27) {  // escape: cancel
            u8bReset(st->search);
            break;
        }
        if (ch == 127 || ch == 8) {  // backspace
            if (u8bDataLen(st->search) > 0) u8bShed1(st->search);
            continue;
        }
        if (ch >= 32) u8bFeed1(st->search, ch);
    }
}

// Read line number from user (displayed in status bar)
// Strip trailing .ext from fragment text. Writes ext to ext_out
// (incl dot, e.g. ".c"), trims from frag in place. Returns YES if found.
static b8 bro_strip_ext(char *frag, char *ext_out, int ext_sz) {
    int fl = (int)strlen(frag);
    int di = fl;
    while (di > 0 && isalnum((u8)frag[di - 1])) di--;
    if (di <= 0 || frag[di - 1] != '.') return NO;
    int es = di - 1;
    if (es > 0 && frag[es - 1] != ' ' &&
        frag[es - 1] != '\'' && frag[es - 1] != '/') return NO;
    int elen = fl - es;
    if (elen <= 1 || elen >= ext_sz) return NO;
    memcpy(ext_out, frag + es, (size_t)elen);
    ext_out[elen] = 0;
    int trim = es;
    while (trim > 0 && frag[trim - 1] == ' ') trim--;
    frag[trim] = 0;
    return YES;
}

// Dispatch a GURI fragment (line/grep/snippet/regex) via spot.
// Handles ext stripping and default ext from current hunk.
static void BRODispatchFragment(BROstate *st, char *frag,
                                char const *repo) {
    if (frag[0] == 0) return;

    // Strip trailing .ext
    char ext_arg[16] = {};
    bro_strip_ext(frag, ext_arg, sizeof(ext_arg));
    if (frag[0] == 0) return;

    // Line number — `42` or `L42`.
    {
        char const *digits = frag;
        if (digits[0] == 'L' && digits[1] >= '0' && digits[1] <= '9')
            digits++;
        if (digits[0] >= '0' && digits[0] <= '9') {
            u32 target = (u32)atoi(digits);
            if (target > 0) target--;
            if (target >= bro_nlines(st))
                target = bro_nlines(st) > 0 ? bro_nlines(st) - 1 : 0;
            st->scroll = target;
            return;
        }
    }

    // Classify search type
    int fl = (int)strlen(frag);
    char const *flag = "-g";
    char const *pattern = frag;
    char unquoted[256] = {};
    if (fl >= 2 && frag[0] == '\'' && frag[fl - 1] == '\'') {
        flag = "-s";
        memcpy(unquoted, frag + 1, (size_t)(fl - 2));
        pattern = unquoted;
    } else if (fl >= 2 && frag[0] == '/' && frag[fl - 1] == '/') {
        flag = "-p";
        memcpy(unquoted, frag + 1, (size_t)(fl - 2));
        pattern = unquoted;
    }

    // Resolve ext: explicit > current hunk's ext
    char const *arg = NULL;
    char argz[16] = {};
    if (ext_arg[0]) {
        arg = ext_arg;
    } else if (st->scroll < bro_nlines(st)) {
        u32 hi = bro_lines(st)[st->scroll].lo;
        hunkc const *hk = &st->hunks[0][hi];
        BROloc loc2 = {};
        BROHunkLoc(&loc2, hk);
        if (!$empty(loc2.path)) {
            u8cs ext = {};
            a_dup(u8c, lp, loc2.path);
            PATHu8sExt(ext, lp);
            if (!$empty(ext)) {
                size_t el = (size_t)$len(ext);
                if (el + 1 < sizeof(argz)) {
                    argz[0] = '.';
                    memcpy(argz + 1, ext[0], el);
                    arg = argz;
                }
            }
        }
    }

    ok64 o = BROForkSpot(st, flag, pattern, arg, repo);
    if (o != OK && u8bDataLen(st->flash) == 0)
        bro_flash(st, "spot: %s", ok64str(o));
}

// Unified URI prompt. Accepts: line number, path, path#fragment,
// #fragment, or bare fragment. Uses abc/URI to parse.
static void BROReadURI(BROstate *st, char const *repo) {
    char buf[512] = {};
    BROReadSpot(st, buf, sizeof(buf), " : ");
    if (buf[0] == 0) return;

    // Parse with abc/URI
    a_cstr(input, buf);
    uri u = {};
    $mv(u.data, input);
    ok64 po = URILexer(&u);

    // If URI parsing fails, treat as plain goto-line or search
    if (po != OK) {
        if (buf[0] >= '0' && buf[0] <= '9') {
            u32 target = (u32)atoi(buf);
            if (target > 0) target--;
            if (target >= bro_nlines(st))
                target = bro_nlines(st) > 0 ? bro_nlines(st) - 1 : 0;
            st->scroll = target;
        }
        return;
    }

    //  Schemed URI (`spot:`, `grep:`, `sha1:`, `tree:`, …) → hand off to
    //  `be --tlv <uri>`; the dispatcher routes to the right projector
    //  dog and emits TLV to our pipe.
    if (!$empty(u.scheme)) {
        ok64 fo = BROForkBe(st, buf);
        if (fo != OK && u8bDataLen(st->flash) == 0)
            bro_flash(st, "be: %s", ok64str(fo));
        return;
    }

    b8 has_path = !$empty(u.path);
    b8 has_frag = !$empty(u.fragment);

    //  Parse a leading-line marker from `frag`: bare digits or `L<digits>`.
    //  Returns the 1-based line, or 0 if not a line marker.  FRAG/QURY
    //  ragel grammars are stale; sscanf is sufficient here.
    u32 frag_target_line = 0;
    if (has_frag) {
        char fbuf[64] = {};
        size_t fl = (size_t)$len(u.fragment);
        if (fl > 0 && fl < sizeof(fbuf)) {
            memcpy(fbuf, u.fragment[0], fl);
            unsigned ln = 0;
            if (sscanf(fbuf, "L%u", &ln) == 1 ||
                sscanf(fbuf, "%u", &ln) == 1)
                frag_target_line = ln;
        }
    }

    //  Path component → open file.  BROOpenFile takes a u8csc directly,
    //  so we hand `u.path` through with no intermediate C-string.
    if (has_path) {
        ok64 o = BROOpenFile(st, u.path, repo, frag_target_line);
        if (o != OK)
            bro_flash(st, "open: " U8SFMT ": %s",
                      u8sFmt(u.path), ok64str(o));

        //  Non-line fragment → search pattern after opening.
        if (o == OK && has_frag && frag_target_line == 0) {
            u8bReset(st->search);
            if (u8bFeed(st->search, u.fragment) == OK) {
                u32 f = BROSearchNext(st, st->scroll, +1);
                if (f != UINT32_MAX) st->scroll = f;
            }
        }
        return;
    }

    //  Bare `#L123` (or `#123`) with no path/scheme → goto-line in cur view.
    if (has_frag && frag_target_line > 0) {
        u32 target = frag_target_line - 1;
        if (target >= bro_nlines(st))
            target = bro_nlines(st) > 0 ? bro_nlines(st) - 1 : 0;
        st->scroll = target;
        return;
    }

    //  Fragment only → dispatch as GURI search.  BRODispatchFragment
    //  still takes a NUL-terminated `char *frag` (it passes it on to
    //  spot via execlp + manipulates it in place); materialise once
    //  here using a stack u8 buffer with the path-style NUL invariant.
    if (has_frag) {
        a_pad(u8, frag, 256);
        u8bFeed(frag, u.fragment);
        u8sFeed1(frag_idle, 0);
        BRODispatchFragment(st, (char *)u8bDataHead(frag), repo);
        return;
    }

    // Bare text without # → treat as goto line or local search
    if (buf[0] >= '0' && buf[0] <= '9') {
        u32 target = (u32)atoi(buf);
        if (target > 0) target--;
        if (target >= bro_nlines(st))
            target = bro_nlines(st) > 0 ? bro_nlines(st) - 1 : 0;
        st->scroll = target;
    }
}

// --- Fallback: one-shot ANSI dump (when piped) ---
//
// Builds the same line index BRORender uses (so MOD_SPLIT lines emit
// as paired rm-row + in-row, not as merged-byte goo) and walks each
// row, applying side-aware bg colours.  Cursor positioning is omitted
// — output is plain stream-of-bytes suitable for a pipe.

static ok64 BROPlain(hunkcs hunks) {
    sane(!$empty(hunks));
    call(BROScreenInit);
    u32 nhunks = (u32)$len(hunks);

    //  --tlv: parent process expects HUNK TLV records on stdout (e.g.
    //  a parent bro that forked `be --tlv` to render this URI as a
    //  new view).  Serialize each hunk via HUNKu8sFeed and flush.
    if (HUNKMode == HUNKOutTLV) {
        for (u32 h = 0; h < nhunks; h++) {
            u8bReset(bro_scr);
            (void)HUNKu8sFeed(u8bIdle(bro_scr), &hunks[0][h]);
            BROScreenFlush();
        }
        done;
    }

    if (!BRO_COLOR) {
        // No colours: emit hunk text verbatim with a `--- uri ---` title
        // per hunk.  Same as before — plain --no-color dump.
        for (u32 h = 0; h < nhunks; h++) {
            u8bReset(bro_scr);
            if (hunk_has_title(&hunks[0][h])) {
                char dtitle[HUNK_TITLE_MAX + 1];
                int dtlen = bro_format_title(dtitle, sizeof(dtitle), &hunks[0][h]);
                u8cs dts = {(u8cp)dtitle, (u8cp)dtitle + dtlen};
                u8bFeed(bro_scr, dts);
                u8sFeed1(u8bIdle(bro_scr), '\n');
            }
            if (!$empty(hunks[0][h].text)) {
                u8bFeed(bro_scr, hunks[0][h].text);
                u32 tlen = (u32)$len(hunks[0][h].text);
                if (tlen > 0 && hunks[0][h].text[0][tlen - 1] != '\n')
                    u8sFeed1(u8bIdle(bro_scr), '\n');
            }
            BROScreenFlush();
        }
        done;
    }

    // Colour mode: build line index (which classifies inrm-modified
    // lines and emits two passes) and render each row with the same
    // side+pass→bg mapping the interactive pager uses.
    u32 cols = 200;  // generous; soft-wrap rarely needed in pipe mode
    u32 nlines_max = 0;
    call(BROCountLines, hunks, cols, &nlines_max);
    if (nlines_max == 0) done;

    Brange32 linesbuf = {};
    call(range32bAlloc, linesbuf, nlines_max);
    range32 *lines = linesbuf[0];
    call(BROAppendLines, linesbuf, hunks, 0, cols);
    u32 nlines = (u32)range32bDataLen(linesbuf);

    for (u32 vi = 0; vi < nlines; vi++) {
        u8bReset(bro_scr);
        range32 const *ln = &lines[vi];
        hunk const *hk = &hunks[0][ln->lo];

        if (ln->hi == BRO_TITLE_LINE) {
            if (hunk_is_ulog(hk)) {
                // <date>\t<verb>\t<uri>\n  — same shape as the pipe ULOG
                // line so plain/TUI stay byte-compatible aside from bg.
                (void)HUNKu8sFeedColor(u8bIdle(bro_scr), hk);
                BROScreenFlush();
                continue;
            }
            char dtitle[HUNK_TITLE_MAX + 1];
            int dtlen = bro_format_title(dtitle, sizeof(dtitle), hk);
            scr_emit_title_color();
            u8cs dts = {(u8cp)dtitle, (u8cp)dtitle + dtlen};
            u8bFeed(bro_scr, dts);
            scr_puts(TTY_RESET);
            u8sFeed1(u8bIdle(bro_scr), '\n');
            BROScreenFlush();
            continue;
        }

        u32 textlen = (u32)$len(hk->text);
        u32 off = bro_line_off(ln);
        u8 pass = bro_line_pass(ln);
        u32 line_end = bro_row_end_pass(hk, textlen, off, cols, pass);
        u32 w = line_end - off;

        int ntoks = (int)$len(hk->toks);
        int tok_i = 0;
        while (tok_i < ntoks &&
               tok32Offset(hk->toks[0][tok_i]) <= off)
            tok_i++;

        for (u32 j = 0; j < w; ) {
            u32 pos = off + j;
            u8 ch = hk->text[0][pos];
            u32 clen = UTF8_LEN[ch >> 4];
            if (clen > w - j) clen = w - j;
            while (tok_i < ntoks &&
                   tok32Offset(hk->toks[0][tok_i]) <= pos)
                tok_i++;
            u8 fg_tag = (tok_i < ntoks)
                            ? tok32Tag(hk->toks[0][tok_i]) : 'S';
            u8 side = (tok_i < ntoks)
                          ? tok32Side(hk->toks[0][tok_i]) : TOK_SIDE_EQ;
            if (fg_tag == 'U' ||
                (pass == BRO_PASS_RM && side == TOK_SIDE_IN) ||
                (pass == BRO_PASS_IN && side == TOK_SIDE_RM)) {
                j += clen;
                continue;
            }
            scr_emit_char(hk->text[0] + pos, clen,
                          bro_cell_ansi(fg_tag, pass, side, NO));
            j += clen;
        }
        scr_emit_reset();
        u8sFeed1(u8bIdle(bro_scr), '\n');
        BROScreenFlush();
    }

    range32bFree(linesbuf);
    done;
}

// --- Spot invocation ---

//  Resolve spot binary path (lazy, cached).  The buffer is allocated
//  on first call and lives until process exit; PATHu8b's NUL-past-DATA
//  invariant lets us hand `(char *)u8bDataHead(...)` straight to execlp
//  without a manual `buf[len] = 0` step.
static path8b bro_spot_path;
static ok64 bro_resolve_spot(void) {
    sane(1);
    if (bro_spot_path[1] != NULL) done;
    a_path(p);
    a$rg(a0, 0);
    a_cstr(spot_name, "spot");
    call(HOMEResolveSibling, NULL, p, spot_name, a0);
    call(PATHu8bAlloc, bro_spot_path);
    try(PATHu8bFeed, bro_spot_path, $path(p));
    nedo {
        u8bFree(bro_spot_path);
        fail(__);
    }
    done;
}


// Collect unique words from hunk text matching a prefix.
// Words = runs of [a-zA-Z0-9_] starting with [a-zA-Z_].
// Returns count of matches written to out[0..maxout).
// Scan one hunk for words matching prefix, append unique to out.
// Check if word contains needle as a substring (case-sensitive).
static b8 bro_has_substr(u8csc word, u8csc ndl) {
    if ($empty(ndl)) return YES;
    if ($len(ndl) > $len(word)) return NO;
    u64 limit = $len(word) - $len(ndl) + 1;
    for (u64 i = 0; i < limit; i++) {
        a_part(u8c, hay, word, i, $len(ndl));
        if ($eq(hay, ndl)) return YES;
    }
    return NO;
}

static int bro_scan_hunk(hunkc const *hk, u8csc ndl,
                         b8 substr, char out[][64], int n, int maxout) {
    u32 tlen = (u32)$len(hk->text);
    if (tlen == 0) return n;
    u8cp txt = hk->text[0];
    u32 i = 0;
    while (i < tlen && n < maxout) {
        if (!isalpha(txt[i]) && txt[i] != '_') { i++; continue; }
        u32 ws = i;
        while (i < tlen && (isalnum(txt[i]) || txt[i] == '_')) i++;
        u32 wlen = i - ws;
        if (wlen < 2 || wlen >= 64) continue;
        if ((u32)$len(ndl) > wlen) continue;
        a_part(u8c, word_s, hk->text, ws, wlen);
        if (!$empty(ndl)) {
            if (substr) {
                if (!bro_has_substr(word_s, ndl)) continue;
            } else {
                a_head(u8c, pfx, word_s, $len(ndl));
                if (!$eq(pfx, ndl)) continue;
            }
        }
        a_pad(u8, wbuf, 64);
        u8bFeed(wbuf, word_s);
        u8sFeed1(wbuf_idle, 0);  // NUL for strcmp
        char *wp = (char *)u8bDataHead(wbuf);
        b8 dup = NO;
        for (int j = 0; j < n; j++)
            if (strcmp(out[j], wp) == 0) { dup = YES; break; }
        if (!dup) { memcpy(out[n], wp, wlen + 1); n++; }
    }
    return n;
}

// Collect unique words matching needle. Tries prefix first; if no
// matches, retries as substring.
static int bro_collect_words(hunkc const *hunks, u32 nhunks,
                             u32 start_hunk, u8csc ndl,
                             char out[][64], int maxout) {
    // Pass 1: prefix match
    int n = 0;
    for (u32 k = 0; k < nhunks && n < maxout; k++) {
        u32 h = (start_hunk + k) % nhunks;
        n = bro_scan_hunk(&hunks[h], ndl, NO, out, n, maxout);
    }
    if (n > 0 || $empty(ndl)) return n;
    // Pass 2: substring match
    for (u32 k = 0; k < nhunks && n < maxout; k++) {
        u32 h = (start_hunk + k) % nhunks;
        n = bro_scan_hunk(&hunks[h], ndl, YES, out, n, maxout);
    }
    return n;
}

// Interactive spot prompt with Tab completion.
// Returns the accepted token in buf (NUL-terminated), or buf[0]==0 on cancel.
static void BROReadSpot(BROstate *st, char *buf, int bufsz,
                        char const *prompt) {
    int len = 0;
    buf[0] = 0;

    // Completion state
    char matches[256][64];
    int nmatch = 0, match_idx = -1;
    b8 matches_valid = NO;

    for (;;) {
        // Render prompt
        bro_goto(st->rows, 1);
        bro_puts(TTY_UNDERLINE TTY_ERASE_LINE);
        bro_puts(prompt);
        if (len > 0) {
            u8cs bs = {(u8cp)buf, (u8cp)buf + len};
            bro_write(bs);
        }
        // Show match count after Tab
        if (matches_valid) {
            char info[32];
            int il = snprintf(info, sizeof(info), " [%d/%d]",
                              nmatch > 0 ? match_idx + 1 : 0, nmatch);
            if (il > 0) (void)write(STDOUT_FILENO, info, (size_t)il);
        }
        bro_puts(TTY_RESET);

        u8 ch = 0;
        ssize_t nr = read(st->tty_fd, &ch, 1);
        if (nr <= 0) continue;
        if (ch == '\n' || ch == '\r') break;
        if (ch == 27) { len = 0; buf[0] = 0; break; }
        if (ch == 127 || ch == 8) {
            if (len > 0) { len--; buf[len] = 0; matches_valid = NO; }
            continue;
        }
        if (ch == '\t') {
            // Tab: complete the last word (separated by non-alpha).
            // Find where the last word starts.
            int wstart = len;
            while (wstart > 0 && (isalnum((u8)buf[wstart - 1]) ||
                                  buf[wstart - 1] == '_'))
                wstart--;
            int pfxlen = len - wstart;
            if (!matches_valid) {
                u32 sh = (st->scroll < bro_nlines(st))
                       ? bro_lines(st)[st->scroll].lo : 0;
                u8cs wndl = {(u8cp)buf + wstart,
                             (u8cp)buf + wstart + pfxlen};
                nmatch = bro_collect_words(st->hunks[0], (u32)$len(st->hunks),
                                           sh, wndl, matches, 256);
                match_idx = -1;
                matches_valid = YES;
            }
            if (nmatch > 0) {
                match_idx = (match_idx + 1) % nmatch;
                int mlen = (int)strlen(matches[match_idx]);
                int newlen = wstart + mlen;
                if (newlen >= bufsz) newlen = bufsz - 1;
                memcpy(buf + wstart, matches[match_idx],
                       (size_t)(newlen - wstart));
                buf[newlen] = 0;
                len = newlen;
            }
            continue;
        }
        if (ch >= 32 && len < bufsz - 1) {
            buf[len++] = (char)ch;
            buf[len] = 0;
            matches_valid = NO;
        }
    }
}

// Fork spot, drain all TLV hunks, push as new view.
// Returns OK if hunks were produced and view pushed.
static ok64 BROForkSpot(BROstate *st, char const *flag,
                        char const *token, char const *filepath,
                        char const *repo) {
    sane(st != NULL && token != NULL && token[0] != 0);
    if (st->nsaves >= BRO_MAX_VIEWS) fail(NOROOM);

    try(bro_resolve_spot);
    nedo { bro_flash(st, "spot: %s", ok64str(__)); fail(__); }

    int pfd[2];
    if (pipe(pfd) != 0) fail(FAILSANITY);

    pid_t pid = fork();
    if (pid < 0) { close(pfd[0]); close(pfd[1]); fail(FAILSANITY); }

    if (pid == 0) {
        // Child: run spot -g <token> [filepath]
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);
        char const *spot_cstr = (char const *)u8bDataHead(bro_spot_path);
        if (filepath)
            execlp(spot_cstr, "spot", "--tlv", flag, token,
                   filepath, (char *)NULL);
        else
            execlp(spot_cstr, "spot", "--tlv", flag, token,
                   (char *)NULL);
        _exit(127);
    }

    // Parent: read TLV hunks from pipe
    close(pfd[1]);

    // Use a temporary arena-like approach: drain into bro_arena at current pos.
    // Save the arena position to restore if we fail.
    u8p arena_save = u8bIdleHead(bro_arena);
    u32 hunks_save = bro_nhunks;

    // Read buffer — heap allocated, not 128KB on stack
    Bu8 pdbuf = {};
    ok64 mo2 = u8bMap(pdbuf, 1UL << 16);
    if (mo2 != OK) { close(pfd[0]); waitpid(pid, NULL, 0); fail(mo2); }

    for (;;) {
        size_t space = u8bIdleLen(pdbuf);
        if (space == 0) {
            if (u8bReMap(pdbuf, u8bSize(pdbuf) * 2) != OK) break;
            space = u8bIdleLen(pdbuf);
        }
        ssize_t nr = read(pfd[0], u8bIdleHead(pdbuf), space);
        if (nr <= 0) break;
        u8bFed(pdbuf, (size_t)nr);
        bro_drain_tlv(pdbuf);
    }
    u8bUnMap(pdbuf);

    close(pfd[0]);
    int status = 0;
    waitpid(pid, &status, 0);

    u32 new_nhunks = bro_nhunks - hunks_save;
    if (new_nhunks == 0) {
        // No results — restore hunks buffer + arena, flash message
        hunkbShed(bro_state->hunks,
                  (size_t)hunkbDataLen(bro_state->hunks) - hunks_save);
        // Roll IDLE back to the snapshot taken before this fork
        size_t added = (size_t)(u8bIdleHead(bro_arena) - arena_save);
        if (added > 0) u8bShed(bro_arena, added);
        bro_flash(st, "spot: no results");
        fail(FAILSANITY);
    }

    // Save current view and switch to spot results
    int idx = st->nsaves;
    BROsave *sv = &st->saves[idx];
    $mv(sv->hunks, st->hunks);
    range32bHandOver(sv->linesbuf, st->linesbuf);
    sv->scroll = st->scroll;

    st->hunks[0] = bro_hunks + hunks_save;
    st->hunks[1] = bro_hunks + hunks_save + new_nhunks;
    call(BROBuildIndex, st);
    st->scroll = (bro_nlines(st) > 1) ? 1 : 0;
    st->nsaves = idx + 1;
    done;
}

//  Fork `be --tlv <uri>` and drain TLV hunks as a new view.  Mirrors
//  BROForkSpot — same arena/hunks bookkeeping, same nsaves push.  Used
//  by BROReadURI when the typed URI carries a scheme (projector or
//  transport).
static ok64 BROForkBe(BROstate *st, char const *uri) {
    sane(st != NULL && uri != NULL && uri[0] != 0);
    if (st->nsaves >= BRO_MAX_VIEWS) { bro_flash(st, "be: views full"); fail(NOROOM); }

    //  Resolve sibling `be` into a stack path buffer.  HOMEResolveSibling
    //  always populates `bepath` — with a real sibling path when argv0
    //  has a dirname / PATH entry holding bro, else with the bare name
    //  "be" for execvp fallback.
    a_path(bepath);
    a$rg(a0, 0);
    a_cstr(be_name, "be");
    (void)HOMEResolveSibling(NULL, bepath, be_name, a0);
    if (u8bDataLen(bepath) == 0) {
        bro_flash(st, "be: binary not resolved");
        fail(FAILSANITY);
    }

    int pfd[2];
    if (pipe(pfd) != 0) { bro_flash(st, "be: pipe: %s", strerror(errno)); fail(FAILSANITY); }

    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]); close(pfd[1]);
        bro_flash(st, "be: fork: %s", strerror(errno));
        fail(FAILSANITY);
    }

    if (pid == 0) {
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);
        char const *be_cstr = (char const *)u8bDataHead(bepath);
        execlp(be_cstr, "be", "--tlv", uri, (char *)NULL);
        _exit(127);
    }

    close(pfd[1]);

    u8p arena_save = u8bIdleHead(bro_arena);
    u32 hunks_save = bro_nhunks;

    Bu8 pdbuf = {};
    ok64 mo2 = u8bMap(pdbuf, 1UL << 16);
    if (mo2 != OK) { close(pfd[0]); waitpid(pid, NULL, 0); fail(mo2); }

    for (;;) {
        size_t space = u8bIdleLen(pdbuf);
        if (space == 0) {
            if (u8bReMap(pdbuf, u8bSize(pdbuf) * 2) != OK) break;
            space = u8bIdleLen(pdbuf);
        }
        ssize_t nr = read(pfd[0], u8bIdleHead(pdbuf), space);
        if (nr <= 0) break;
        u8bFed(pdbuf, (size_t)nr);
        bro_drain_tlv(pdbuf);
    }
    u8bUnMap(pdbuf);

    close(pfd[0]);
    int status = 0;
    waitpid(pid, &status, 0);

    u32 new_nhunks = bro_nhunks - hunks_save;
    if (new_nhunks == 0) {
        hunkbShed(bro_state->hunks,
                  (size_t)hunkbDataLen(bro_state->hunks) - hunks_save);
        size_t added = (size_t)(u8bIdleHead(bro_arena) - arena_save);
        if (added > 0) u8bShed(bro_arena, added);
        bro_flash(st, "be: no results");
        fail(FAILSANITY);
    }

    int idx = st->nsaves;
    BROsave *sv = &st->saves[idx];
    $mv(sv->hunks, st->hunks);
    range32bHandOver(sv->linesbuf, st->linesbuf);
    sv->scroll = st->scroll;

    st->hunks[0] = bro_hunks + hunks_save;
    st->hunks[1] = bro_hunks + hunks_save + new_nhunks;
    call(BROBuildIndex, st);
    st->scroll = (bro_nlines(st) > 1) ? 1 : 0;
    st->nsaves = idx + 1;
    done;
}

// --- Unified key handler ---
// Returns: -1 = quit, 0 = no change, 1 = changed (needs render).

#define BRO_KEY_QUIT    (-1)
#define BRO_KEY_NONE    0
#define BRO_KEY_CHANGED 1

static int BROHandleKey(BROstate *st, u8 ch, char const *repo) {
    u32 page = (st->rows > 1) ? (u32)(st->rows - 1) : 1;

    if (ch == 'q' || ch == 'Q') {
        return BRO_KEY_QUIT;
    }
    if (ch == 'h' || ch == 127 || ch == 8)
        return BROBack(st) ? BRO_KEY_CHANGED : BRO_KEY_NONE;
    if (ch == 'l' || ch == '\r' || ch == '\n') {
        BROTryOpen(st, st->scroll, repo);
        return BRO_KEY_CHANGED;
    }
    if (ch == ' ' || ch == 'f') {
        if (st->scroll + page < bro_nlines(st)) st->scroll += page;
        else if (bro_nlines(st) > page) st->scroll = bro_nlines(st) - page;
        return BRO_KEY_CHANGED;
    }
    if (ch == 'b') {
        if (st->scroll >= page) st->scroll -= page;
        else st->scroll = 0;
        return BRO_KEY_CHANGED;
    }
    if (ch == 'j') {
        if (st->scroll + 1 < bro_nlines(st)) { st->scroll++; return BRO_KEY_CHANGED; }
        return BRO_KEY_NONE;
    }
    if (ch == 'k') {
        if (st->scroll > 0) { st->scroll--; return BRO_KEY_CHANGED; }
        return BRO_KEY_NONE;
    }
    if (ch == 'g') { st->scroll = 0; return BRO_KEY_CHANGED; }
    if (ch == 'G') {
        st->scroll = (bro_nlines(st) > page) ? bro_nlines(st) - page : 0;
        return BRO_KEY_CHANGED;
    }
    if (ch == ':' || ch == '#') { BROReadURI(st, repo); return BRO_KEY_CHANGED; }
    if (ch == 'r' || ch == 'R') {
        //  Reload: re-fork `be --tlv <uri>` for the URI currently
        //  shown in the status bar.  Reads screen state — no cached
        //  copy — so any view (initial argv, sub-view, search result)
        //  reloads the bytes the user actually sees.  Pop first so
        //  the new view replaces the current one.  After the fresh
        //  view is on top, honour the URI's `#L<line>` fragment
        //  (BROForkBe drains TLV but doesn't itself jump-to-line —
        //  bro's BROExec ignores the fragment for argv-passed URIs).
        char uri[512] = {};
        BROStatusURI(st, uri, sizeof(uri));
        if (uri[0] == 0) {
            bro_flash(st, "reload: no URI on this view");
            return BRO_KEY_NONE;
        }
        if (st->nsaves > 0) (void)BROBack(st);
        ok64 fo = BROForkBe(st, uri);
        if (fo != OK && u8bDataLen(st->flash) == 0)
            bro_flash(st, "reload: %s", ok64str(fo));
        if (fo == OK) {
            //  Match BROOpenFile's source-line walk: count only
            //  source-line starts (wrap continuations share the same
            //  source line and must not bump the counter), then
            //  centre the view on the matching row.
            char const *hash = strchr(uri, '#');
            unsigned ln = 0;
            if (hash && (sscanf(hash + 1, "L%u", &ln) == 1
                      || sscanf(hash + 1, "%u",  &ln) == 1)
                && ln > 0 && bro_nlines(st) > 1) {
                u32 file_ln = 0, best = 1;
                for (u32 i = 0; i < bro_nlines(st); i++) {
                    if (!bro_is_source_start(st->hunks,
                                             range32bDataC(st->linesbuf),
                                             i))
                        continue;
                    file_ln++;
                    if (file_ln == ln) { best = i; break; }
                    best = i;
                }
                BROScrollCenter(st, best);
            }
        }
        return BRO_KEY_CHANGED;
    }
    if (ch == '/') {
        BROReadSearch(st);
        if (u8bDataLen(st->search) > 0) {
            u32 f = BROSearchNext(st, st->scroll, +1);
            if (f != UINT32_MAX) st->scroll = f;
        }
        return BRO_KEY_CHANGED;
    }
    if (ch == 'n') {
        u32 f = BROSearchNext(st, st->scroll, +1);
        if (f != UINT32_MAX) { st->scroll = f; return BRO_KEY_CHANGED; }
        return BRO_KEY_NONE;
    }
    if (ch == 'N') {
        u32 f = BROSearchNext(st, st->scroll, -1);
        if (f != UINT32_MAX) { st->scroll = f; return BRO_KEY_CHANGED; }
        return BRO_KEY_NONE;
    }
    if (ch == 'd') {
        u32 half = page / 2;
        if (st->scroll + half < bro_nlines(st)) st->scroll += half;
        else if (bro_nlines(st) > page) st->scroll = bro_nlines(st) - page;
        return BRO_KEY_CHANGED;
    }
    if (ch == 'u') {
        u32 half = page / 2;
        if (st->scroll >= half) st->scroll -= half;
        else st->scroll = 0;
        return BRO_KEY_CHANGED;
    }
    if (ch == ']' || ch == '}') {
        u32 nx = BROHunkNextLine(range32bDataC(st->linesbuf), st->scroll);
        if (nx != BRO_NONE) { st->scroll = nx; return BRO_KEY_CHANGED; }
        return BRO_KEY_NONE;
    }
    if (ch == '[' || ch == '{') {
        u32 pv = BROHunkPrevLine(range32bDataC(st->linesbuf), st->scroll);
        if (pv != BRO_NONE) { st->scroll = pv; return BRO_KEY_CHANGED; }
        return BRO_KEY_NONE;
    }
    if (ch == ')' || ch == '(') {
        u32 mid = st->scroll + (page > 0 ? (page - 1) / 2 : 0);
        u32 hl = (ch == ')')
            ? BROHiliNextLine(st->hunks, range32bDataC(st->linesbuf), mid)
            : BROHiliPrevLine(st->hunks, range32bDataC(st->linesbuf), mid);
        if (hl != BRO_NONE) { BROScrollCenter(st, hl); return BRO_KEY_CHANGED; }
        return BRO_KEY_NONE;
    }
    if (ch == '.') {
        // List the containing directory of the current hunk's file.
        // Falls back to cwd when the current hunk has no URI or its
        // path is already a bare name (no '/').
        u8cs dir = {};
        u8cs loc_path = {};
        if (st->scroll < bro_nlines(st)) {
            u32 hi = bro_lines(st)[st->scroll].lo;
            BROloc loc = {};
            BROHunkLoc(&loc, &st->hunks[0][hi]);
            if (!$empty(loc.path)) $mv(loc_path, loc.path);
        }
        if (!$empty(loc_path)) {
            // Peel a trailing '/' (dir URI like "bro/") so we look at
            // the parent of the dir, not the dir itself.
            if (*$last(loc_path) == '/') u8csShed1(loc_path);
            PATHu8sDir(dir, loc_path);
        }
        if ($empty(dir)) {
            u8cs dot = u8slit(".");
            $mv(dir, dot);
        }
        if (st->nsaves >= BRO_MAX_VIEWS) {
            bro_flash(st, "view stack full");
            return BRO_KEY_NONE;
        }
        int idx = st->nsaves;
        BROsave *sv = &st->saves[idx];
        $mv(sv->hunks, st->hunks);
        range32bHandOver(sv->linesbuf, st->linesbuf);
        sv->scroll = st->scroll;
        st->files[idx] = (BROfileview){};

        u32 save_nh = bro_nhunks;
        ok64 lo = BROListDir(dir);
        if (lo != OK) {
            bro_flash(st, "list dir " U8SFMT ": %s",
                      u8sFmt(dir), ok64str(lo));
            return BRO_KEY_NONE;
        }
        if (bro_nhunks <= save_nh) {
            bro_flash(st, "empty: " U8SFMT, u8sFmt(dir));
            return BRO_KEY_NONE;
        }
        st->hunks[0] = bro_hunks + save_nh;
        st->hunks[1] = bro_hunks + bro_nhunks;
        BROBuildIndex(st);
        st->scroll = (bro_nlines(st) > 1) ? 1 : 0;
        st->nsaves = idx + 1;
        return BRO_KEY_CHANGED;
    }
    if (ch == 'm') {
        st->mouse_on = !st->mouse_on;
        if (st->mouse_on) MAUSEnable(STDOUT_FILENO);
        else MAUSDisable(STDOUT_FILENO);
        return BRO_KEY_NONE;
    }
    if (ch == 'w') {
        // Save source-line anchor before rebuilding the index.
        u32 anchor_h = 0, anchor_off = 0;
        b8 have_anchor = NO;
        if (st->scroll < bro_nlines(st) &&
            bro_lines(st)[st->scroll].hi != BRO_TITLE_LINE) {
            anchor_h = bro_lines(st)[st->scroll].lo;
            anchor_off = bro_line_off(&bro_lines(st)[st->scroll]);
            have_anchor = YES;
        }
        st->wrap = !st->wrap;
        u32 cols = bro_wrap_cols(st);
        if (st->fixed_lines) {
            // Rewrite in place; the fixed capacity was set at open.
            range32bReset(st->linesbuf);
            if (BROAppendLines(st->linesbuf, st->hunks, 0, cols) != OK) {
                bro_flash(st, "wrap: rebuild failed");
                return BRO_KEY_CHANGED;
            }
        } else {
            range32bFree(st->linesbuf);
            if (BROBuildIndex(st) != OK) {
                bro_flash(st, "wrap: rebuild failed");
                return BRO_KEY_CHANGED;
            }
        }
        if (have_anchor) {
            u32 ln = bro_line_for_off(range32bDataC(st->linesbuf),
                                      anchor_h, anchor_off);
            if (ln != BRO_NONE) st->scroll = ln;
        }
        bro_flash(st, "wrap: %s", st->wrap ? "on" : "off");
        return BRO_KEY_CHANGED;
    }
    if (ch == '\'') {
        // Local token search (GURI-consistent alias for /)
        BROReadSearch(st);
        if (u8bDataLen(st->search) > 0) {
            u32 f = BROSearchNext(st, st->scroll, +1);
            if (f != UINT32_MAX) st->scroll = f;
        }
        return BRO_KEY_CHANGED;
    }
    if (ch == 033) {
        u8 seq[2] = {};
        ssize_t n1 = read(st->tty_fd, &seq[0], 1);
        if (n1 <= 0) {
            // Bare Esc (no sequence followed): disable mouse if on
            if (st->mouse_on) {
                st->mouse_on = NO;
                MAUSDisable(STDOUT_FILENO);
            }
            return BRO_KEY_NONE;
        }
        if (seq[0] != '[') return BRO_KEY_NONE;
        ssize_t n2 = read(st->tty_fd, &seq[1], 1);
        if (n2 <= 0) return BRO_KEY_NONE;
        if (seq[1] == '<') {
            // SGR mouse
            u8 mbuf[32];
            mbuf[0] = 033; mbuf[1] = '['; mbuf[2] = '<';
            int mi = 3;
            for (;;) {
                if (mi >= (int)sizeof(mbuf)) break;
                ssize_t r = read(st->tty_fd, &mbuf[mi], 1);
                if (r <= 0) break;
                if (mbuf[mi] == 'M' || mbuf[mi] == 'm') { mi++; break; }
                mi++;
            }
            MAUSevent mev = {};
            if (MAUSParse(&mev, mbuf, mi)) {
                if (mev.type == MAUS_WHEEL) {
                    u32 step = 3;
                    if (mev.button == MAUS_UP) {
                        if (st->scroll >= step) st->scroll -= step;
                        else st->scroll = 0;
                    } else if (mev.button == MAUS_DOWN) {
                        if (st->scroll + step < bro_nlines(st)) st->scroll += step;
                        else if (bro_nlines(st) > page) st->scroll = bro_nlines(st) - page;
                    }
                    return BRO_KEY_CHANGED;
                }
                if (mev.type == MAUS_PRESS && mev.button == MAUS_LEFT) {
                    u32 line = st->scroll + mev.row - 1;
                    if (line < bro_nlines(st)) {
                        range32 const *ln = &bro_lines(st)[line];
                        b8 is_title = (ln->hi == BRO_TITLE_LINE);
                        hunk const *thk = &st->hunks[0][ln->lo];

                        // Content row: clicking a token followed by a
                        // 'U'-tagged URI token navigates to that URI
                        // explicitly (see TOK.h).
                        if (!is_title) {
                            hunk const *hk = NULL;
                            u32 byte_off = 0;
                            if (bro_screen_to_byte(st, mev.row, mev.col,
                                                   &hk, &byte_off)) {
                                int ntoks = (int)$len(hk->toks);
                                int ti = 0;
                                while (ti < ntoks &&
                                       tok32Offset(hk->toks[0][ti]) <= byte_off)
                                    ti++;
                                int nxt = ti + 1;
                                if (nxt < ntoks &&
                                    tok32Tag(hk->toks[0][nxt]) == 'U') {
                                    u8cs uri_slice = {};
                                    tok32Val(uri_slice, hk->toks,
                                             hk->text[0], nxt);
                                    if (!$empty(uri_slice)) {
                                        a_pad(u8, ubuf, 1024);
                                        (void)u8sFeed(ubuf_idle, uri_slice);
                                        (void)u8sFeed1(ubuf_idle, '\0');
                                        (void)BROForkBe(st,
                                            (char const *)u8bDataHead(ubuf));
                                        return BRO_KEY_CHANGED;
                                    }
                                }
                            }
                        }

                        // In-process open of the URI's path covers
                        // code-hunk titles, dir-listing entries, and
                        // status rows whose URI is a file path.
                        if (BROTryOpen(st, line, repo))
                            return BRO_KEY_CHANGED;
                        // Fallback: fork `be --tlv <uri>` for things
                        // that aren't readable files — projector URIs
                        // (`log:?…`, `commit:?…`), or directories /
                        // submodules (URI ends in `/`), which we route
                        // through `ls:` so the new view keeps the
                        // status-listing format instead of bro's plain
                        // dir listing.
                        if (is_title && !$empty(thk->uri)) {
                            a_pad(u8, ubuf, 1024);
                            b8 is_dir = (*u8csLast(thk->uri) == '/');
                            if (is_dir) {
                                a_cstr(ls_pfx, "ls:");
                                (void)u8sFeed(ubuf_idle, ls_pfx);
                            }
                            (void)u8sFeed(ubuf_idle, thk->uri);
                            (void)u8sFeed1(ubuf_idle, '\0');
                            (void)BROForkBe(st,
                                (char const *)u8bDataHead(ubuf));
                        }
                        return BRO_KEY_CHANGED;
                    }
                }
                if (mev.type == MAUS_PRESS && mev.button == MAUS_RIGHT) {
                    hunk const *hk = NULL;
                    u32 byte_off = 0;
                    char word[128];
                    if (bro_screen_to_byte(st, mev.row, mev.col,
                                           &hk, &byte_off)
                        && bro_word_around(hk, byte_off,
                                           word, sizeof(word)) > 0) {
                        char uri[160];
                        int m = snprintf(uri, sizeof(uri),
                                         "grep:#%s", word);
                        if (m > 0 && m < (int)sizeof(uri))
                            (void)BROForkBe(st, uri);
                        return BRO_KEY_CHANGED;
                    }
                }
            }
            return BRO_KEY_NONE;
        }
        switch (seq[1]) {
        case 'A': if (st->scroll > 0) st->scroll--; return BRO_KEY_CHANGED;
        case 'B': if (st->scroll + 1 < bro_nlines(st)) st->scroll++; return BRO_KEY_CHANGED;
        case '5':
            (void)read(st->tty_fd, &seq[0], 1);
            if (st->scroll >= page) st->scroll -= page;
            else st->scroll = 0;
            return BRO_KEY_CHANGED;
        case '6':
            (void)read(st->tty_fd, &seq[0], 1);
            if (st->scroll + page < bro_nlines(st)) st->scroll += page;
            else if (bro_nlines(st) > page) st->scroll = bro_nlines(st) - page;
            return BRO_KEY_CHANGED;
        case 'H': st->scroll = 0; return BRO_KEY_CHANGED;
        case 'F':
            st->scroll = (bro_nlines(st) > page) ? bro_nlines(st) - page : 0;
            return BRO_KEY_CHANGED;
        }
    }
    return BRO_KEY_NONE;
}

// --- Main entry ---

ok64 BRORun(hunkcs hunks) {
    sane(!$empty(hunks));

    //  Screen buffer for the whole session — carve before the non-tty
    //  branch so BROPlain sees it too; rewound when this frame returns.
    a_carve(u8, scr, BRO_SCR_SIZE);
    bro_scr_p = &scr;

    // Fallback: plain output when stdout is not a terminal
    if (!isatty(STDOUT_FILENO))
        return BROPlain(hunks);

    BROstate st = {};
    st.tty_fd = -1;
    st.wrap = YES;
    $mv(st.hunks, hunks);
    //  search + flash are whole-session scratch; carve from BASS so they
    //  need no free path (rewound when BROExec's frame returns).
    a_carve(u8, search, BRO_SEARCH_MAX);
    a_carve(u8, flash,  BRO_FLASH_MAX);
    memcpy(st.search, search, sizeof(Bu8));
    memcpy(st.flash,  flash,  sizeof(Bu8));

    //  Resolve repo root for file navigation — reuse bro's home if set,
    //  otherwise walk up from cwd.  a_path materialises a stack buffer
    //  with the path-NUL invariant, so `(char *)u8bDataHead(repo)` is
    //  a valid C-string for the legacy `char const *repo` interfaces
    //  downstream (BROHandleKey, BROReadURI, …).
    a_path(repo);
    home scratch_h = {};
    home *rh = bro_state && bro_state->h ? bro_state->h : &scratch_h;
    if (rh == &scratch_h) {
        uri none = {};
        if (HOMEOpen(rh, &none, NO) != OK) rh = NULL;
    }
    if (rh != NULL) PATHu8bFeed(repo, $path(rh->root));
    char const *repo_cstr = (char const *)u8bDataHead(repo);

    BROGetSize(&st);
    try(BROBuildIndex, &st);
    then try(BROScreenInit);
    then try(BRORawEnable, &st);
    nedo {
        range32bFree(st.linesbuf);
        done;
    }

    // Install SIGWINCH handler
    struct sigaction sa = {}, old_sa = {};
    sa.sa_handler = bro_winch_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, &old_sa);

    // Hide cursor
    bro_puts("\033[?25l");

    // Alternate screen buffer
    bro_puts("\033[?1049h");
    // Mouse tracking starts enabled (toggle with 'm', or bare Esc disables).
    MAUSEnable(STDOUT_FILENO);
    st.mouse_on = YES;

    // Initial scroll: if first hunk has a line number in its URI, center
    // on that line. Otherwise skip the title.  Once the navigation
    // marker has been consumed, truncate the URI at the `#` so the
    // status bar's line math doesn't read the fragment as a hunk-anchor
    // (snippet shape) and double-count.
    {
        BROloc loc0 = {};
        if (!$empty(hunks)) BROHunkLoc(&loc0, hunks[0]);
        if (loc0.line > 0 && bro_nlines(&st) > 1) {
            u32 file_ln = 0, best = 1;
            for (u32 i = 0; i < bro_nlines(&st); i++) {
                if (!bro_is_source_start(st.hunks, range32bDataC(st.linesbuf), i))
                    continue;
                file_ln++;
                if (file_ln == loc0.line) { best = i; break; }
                best = i;
            }
            BROScrollCenter(&st, best);
        } else if (bro_nlines(&st) > 1) {
            st.scroll = 1;
        }
    }

    BRORender(&st);

    b8 quit = NO;
    while (!quit) {
        if (bro_resized) {
            bro_resized = 0;
            // Save source-line anchor before the index is rebuilt.
            u32 anchor_h = 0, anchor_off = 0;
            b8 have_anchor = NO;
            if (st.scroll < bro_nlines(&st) &&
                bro_lines(&st)[st.scroll].hi != BRO_TITLE_LINE) {
                anchor_h = bro_lines(&st)[st.scroll].lo;
                anchor_off = bro_lines(&st)[st.scroll].hi;
                have_anchor = YES;
            }
            BROGetSize(&st);
            range32bFree(st.linesbuf);
            if (BROBuildIndex(&st) == OK && have_anchor) {
                u32 ln = bro_line_for_off(range32bDataC(st.linesbuf),
                                          anchor_h, anchor_off);
                if (ln != BRO_NONE) st.scroll = ln;
            }
            BRORender(&st);
        }
        u8 ch = 0;
        ssize_t nr = read(st.tty_fd, &ch, 1);
        if (nr <= 0) continue;
        int r = BROHandleKey(&st, ch, repo_cstr);
        if (r == BRO_KEY_QUIT) quit = YES;
        else if (r == BRO_KEY_CHANGED) BRORender(&st);
    }

    // Restore: disable mouse, leave alternate screen, show cursor, reset
    if (st.mouse_on) MAUSDisable(STDOUT_FILENO);
    bro_puts("\033[?1049l");
    bro_puts("\033[?25h");
    BRORawDisable(&st);
    sigaction(SIGWINCH, &old_sa, NULL);
    // Free any stacked file views
    while (st.nsaves > 0) BROBack(&st);
    range32bFree(st.linesbuf);
    //  st.search / st.flash are BASS carves — no free.

    done;
}

// --- Pipe pager: incremental display of TLV hunks from a pipe ---

#define PIPE_RDBUF_INIT (1UL << 22)  // 4MB initial read buffer
#define PIPE_MAX_LINES  (1UL << 20)  // 1M lines max

// Append lines for `hunks` into `lines`'s idle.  One entry per
// display row: title separator, source-line start, or a wrap
// continuation of a long source line (`cols` codepoints each).
// `from` is the absolute hunk index assigned to the first entry of
// `hunks`; subsequent entries get from+1, from+2, ...  Silently caps
// on idle exhaustion.  Caller reads new count via range32bDataLen().
ok64 BROAppendLines(range32b lines, hunkcs hunks, u32 from, u32 cols) {
    sane(1);
    if (cols == 0) cols = 1;
    u32 nhunks = (u32)$len(hunks);
    for (u32 i = 0; i < nhunks; i++) {
        u32 h = from + i;
        hunkc const *hk = &hunks[0][i];
        if (hunk_has_title(hk))
            range32bFeed1(lines, (range32){h, BRO_TITLE_LINE});
        if ($empty(hk->text)) continue;
        call(bro_hunk_append, lines, hk, h, cols);
    }
    done;
}

// Drain all TLV records from `pipefd` into `bro_hunks` then return.
// Used by the non-interactive (non-TTY stdout) pipe path: callers
// invoke this and then dispatch to BROPlain for a one-shot ANSI dump.
static ok64 bro_pipe_drain_all(int pipefd) {
    sane(pipefd >= 0);
    call(BROArenaInit);
    Bu8 rdbuf = {};
    call(u8bMap, rdbuf, PIPE_RDBUF_INIT);

    for (;;) {
        size_t space = u8bIdleLen(rdbuf);
        if (space == 0) {
            if (u8bReMap(rdbuf, u8bSize(rdbuf) * 2) != OK) break;
            space = u8bIdleLen(rdbuf);
        }
        ssize_t nr = read(pipefd, u8bIdleHead(rdbuf), space);
        if (nr <= 0) break;
        u8bFed(rdbuf, (size_t)nr);
    }

    bro_drain_tlv(rdbuf);
    u8bUnMap(rdbuf);
    done;
}

ok64 BROPipeRun(int pipefd) {
    sane(pipefd >= 0);

    //  Screen buffer for the whole session — carve before the non-tty
    //  branch so BROPlain sees it too; rewound when this frame returns.
    a_carve(u8, scr, BRO_SCR_SIZE);
    bro_scr_p = &scr;

    // Non-TTY stdout: drain all TLV and emit a one-shot ANSI dump via
    // BROPlain.  This is the path `be --color diff:foo | cat` takes —
    // no interactive pager, but the renderer's colours are preserved.
    if (!isatty(STDOUT_FILENO)) {
        call(bro_pipe_drain_all, pipefd);
        if (bro_nhunks > 0) call(BROPlain, hunkbDataC(bro_state->hunks));
        BROArenaCleanup();
        done;
    }

    // If pipefd is a TTY (bro invoked with no data source), ignore
    // it — otherwise the pipe-drain would swallow keystrokes that
    // should reach the keyboard handler (e.g. 'q' to quit).
    b8 pipe_eof = isatty(pipefd) ? YES : NO;

    // Non-blocking so the read loop can drain what's ready and exit
    // on EAGAIN without a separate poll probe.
    if (!pipe_eof) {
        int fl = fcntl(pipefd, F_GETFL, 0);
        if (fl >= 0) (void)fcntl(pipefd, F_SETFL, fl | O_NONBLOCK);
    }

    call(BROArenaInit);

    // Allocate growable read buffer
    Bu8 rdbuf = {};
    call(u8bMap, rdbuf, PIPE_RDBUF_INIT);

    //  Resolve repo root.  Click navigation opens FILES from the
    //  worktree, so `h->wt` is what `BROOpenFile` needs to prepend —
    //  NOT `h->root` (which is the store dir for secondary wts and
    //  doesn't carry the user's source tree).  Primary wts have
    //  wt == root, so this only matters for colocated/sub-mount.
    a_path(repo);
    {
        home rh = {};
        uri none = {};
        if (HOMEOpen(&rh, &none, NO) == OK) {
            PATHu8bFeed(repo, $path(rh.wt));
            HOMEClose(&rh);
        }
    }
    char const *repo_cstr = (char const *)u8bDataHead(repo);

    // Allocate line index
    BROstate st = {};
    st.tty_fd = -1;
    st.wrap = YES;
    st.fixed_lines = YES;
    //  search + flash carve from BASS (no free path); only linesbuf and
    //  the growable rdbuf still need explicit cleanup.
    a_carve(u8, search, BRO_SEARCH_MAX);
    a_carve(u8, flash,  BRO_FLASH_MAX);
    memcpy(st.search, search, sizeof(Bu8));
    memcpy(st.flash,  flash,  sizeof(Bu8));
    try(range32bAlloc, st.linesbuf, PIPE_MAX_LINES);
    nedo { u8bUnMap(rdbuf); done; }
    st.hunks[0] = st.hunks[1] = bro_hunks;

    BROGetSize(&st);

    try(BROScreenInit);
    then try(BRORawEnable, &st);
    nedo {
        range32bFree(st.linesbuf);
        u8bUnMap(rdbuf);
        done;
    }

    struct sigaction sa = {}, old_sa = {};
    sa.sa_handler = bro_winch_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, &old_sa);

    bro_puts("\033[?25l");    // hide cursor
    bro_puts("\033[?1049h");  // alt screen
    // Mouse tracking starts enabled (toggle with 'm', or bare Esc disables).
    MAUSEnable(STDOUT_FILENO);
    st.mouse_on = YES;

    // Show initial "nothing..." while waiting for data
    u8bReset(bro_scr);
    scr_goto(st.rows, 1);
    scr_puts(TTY_UNDERLINE TTY_ERASE_LINE " nothing..." TTY_RESET);
    BROScreenFlush();

    b8 quit = NO;
    u32 indexed_nhunks = 0;
    u32 rendered_nhunks = 0;

    while (!quit) {
        if (bro_resized) {
            bro_resized = 0;
            // Preserve source-line anchor across reindex.
            u32 anchor_h = 0, anchor_off = 0;
            b8 have_anchor = NO;
            if (st.scroll < bro_nlines(&st) &&
                bro_lines(&st)[st.scroll].hi != BRO_TITLE_LINE) {
                anchor_h = bro_lines(&st)[st.scroll].lo;
                anchor_off = bro_lines(&st)[st.scroll].hi;
                have_anchor = YES;
            }
            BROGetSize(&st);
            range32bReset(st.linesbuf);
            (void)BROAppendLines(st.linesbuf, st.hunks, 0, bro_wrap_cols(&st));
            indexed_nhunks = bro_nhunks;
            if (have_anchor) {
                u32 ln = bro_line_for_off(range32bDataC(st.linesbuf),
                                          anchor_h, anchor_off);
                if (ln != BRO_NONE) st.scroll = ln;
            }
            if (bro_nlines(&st) > 0) BRORender(&st);
        }

        struct pollfd fds[2];
        int nfds = 0;
        fds[0].fd = st.tty_fd;
        fds[0].events = POLLIN;
        nfds = 1;
        if (!pipe_eof) {
            fds[1].fd = pipefd;
            fds[1].events = POLLIN;
            nfds = 2;
        }

        int pr = poll(fds, (nfds_t)nfds, 16);

        b8 changed = NO;
        b8 key_pressed = NO;

        // Drain everything ready before rendering so a fast producer
        // doesn't trigger a burst of partial-frame repaints (visible
        // as screen blinking during load).
        if (!pipe_eof && (fds[1].revents & (POLLIN | POLLHUP))) {
            for (;;) {
                size_t space = u8bIdleLen(rdbuf);
                if (space == 0) break;
                ssize_t nr = read(pipefd, u8bIdleHead(rdbuf), space);
                if (nr > 0) {
                    u8bFed(rdbuf, (size_t)nr);
                    continue;
                }
                if (nr == 0) pipe_eof = YES;
                break;  // EAGAIN or EOF
            }

            bro_drain_tlv(rdbuf);
            // If a single TLV record fills rdbuf entirely without
            // yielding, grow so the next read can complete it.
            if (u8bIdleLen(rdbuf) == 0 && u8bDataLen(rdbuf) > 0)
                (void)u8bReMap(rdbuf, u8bSize(rdbuf) * 2);
        }

        // Handle keyboard input
        if (fds[0].revents & POLLIN) {
            u8 ch = 0;
            ssize_t nr = read(st.tty_fd, &ch, 1);
            if (nr > 0) {
                int r = BROHandleKey(&st, ch, repo_cstr);
                if (r == BRO_KEY_QUIT) quit = YES;
                else if (r == BRO_KEY_CHANGED) changed = YES;
                key_pressed = YES;
            }
        }

        // Extend line index for any new hunks
        if (bro_nhunks > indexed_nhunks) {
            hunkcs new_hunks = {bro_hunks + indexed_nhunks,
                                bro_hunks + bro_nhunks};
            (void)BROAppendLines(st.linesbuf, new_hunks, indexed_nhunks,
                            bro_wrap_cols(&st));
            st.hunks[1] = bro_hunks + bro_nhunks;
            indexed_nhunks = bro_nhunks;
        }
        // Render whenever new hunks have been indexed.  The 16ms poll
        // timeout caps refresh at ~60 fps even when the producer
        // streams quickly, so explicit debouncing isn't needed.
        if (indexed_nhunks > rendered_nhunks) {
            // Skip the first title line on the first render so the
            // user lands on actual content, not the title separator.
            if (rendered_nhunks == 0 && bro_nlines(&st) > 1) st.scroll = 1;
            changed = YES;
            rendered_nhunks = indexed_nhunks;
        }
        if ((changed || key_pressed) && bro_nlines(&st) > 0)
            BRORender(&st);

        // Pipe done, no results — show "nothing!" and wait for quit
        if (pipe_eof && bro_nlines(&st) == 0 && !quit) {
            u8bReset(bro_scr);
            scr_goto(st.rows, 1);
            scr_puts(TTY_UNDERLINE TTY_ERASE_LINE " nothing!" TTY_RESET);
            BROScreenFlush();
        }
    }

    // Teardown
    if (st.mouse_on) MAUSDisable(STDOUT_FILENO);
    bro_puts("\033[?1049l");
    bro_puts("\033[?25h");
    BRORawDisable(&st);
    sigaction(SIGWINCH, &old_sa, NULL);

    while (st.nsaves > 0) BROBack(&st);
    range32bFree(st.linesbuf);
    //  st.search / st.flash are BASS carves — no free.
    u8bUnMap(rdbuf);
    BROArenaCleanup();

    done;
}
