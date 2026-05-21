// Property tests for bro navigation primitives:
//   BROHunkNextLine / BROHunkPrevLine / BROHunkCount / BROHunkIndexAt
//   BROHiliNextLine / BROHiliPrevLine / BROHiliCount / BROHiliIndexAt
//
// Tests synthesize a small hunk[] and a matching range32 line index
// (encoded by hand) and assert against expected results — no terminal,
// no rendering, no I/O.

#include "BRO.h"
#include "MAUS.h"

#include "abc/B.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/tok/TOK.h"

// --- helpers ----------------------------------------------------------

// Build a tok32 from (tag, end_offset). Inlined as a constant expression
// (tok32Pack is a static inline, not usable as a static initializer).
#define TOK(tag, off)                                          \
    (((u32)((u8)(tag) - 'A') << 27) | ((u32)(off) & ((1u << 24) - 1)))
#define TOKSIDE(tag, side, off)                                \
    (((u32)((u8)(tag) - 'A') << 27)                            \
     | (((u32)(side) & 0x3u) << 24)                            \
     | ((u32)(off) & ((1u << 24) - 1)))

// Convenience: declare a tok32 array as a local static and yield a u32cs.
#define TOKS(name, ...)                                          \
    static u32 const name##_data[] = {__VA_ARGS__};              \
    u32cs name = {(u32 const *)name##_data,                      \
                  (u32 const *)name##_data +                     \
                      sizeof(name##_data) / sizeof(u32)}

// Same for a line index range32 array.  Yields `name` as a range32cs
// slice over the data array.
#define LINES(name, ...)                                              \
    static range32 const name##_data[] = {__VA_ARGS__};               \
    range32cs name = {name##_data,                                    \
                      name##_data + sizeof(name##_data) / sizeof(range32)}

// --- Hunk navigation tests --------------------------------------------

// Layout: three hunks, each with a title line and content lines.
//
//  line | hunk | role
//  -----+------+------------
//   0   |  0   | title
//   1   |  0   | text line 0
//   2   |  0   | text line 1
//   3   |  1   | title
//   4   |  1   | text line 0
//   5   |  2   | title
//   6   |  2   | text line 0
//   7   |  2   | text line 1
//   8   |  2   | text line 2
ok64 NAVtest_hunk_layout() {
    sane(1);
    LINES(L,
          {0, BRO_TITLE_LINE},
          {0, 0},
          {0, 8},
          {1, BRO_TITLE_LINE},
          {1, 0},
          {2, BRO_TITLE_LINE},
          {2, 0},
          {2, 6},
          {2, 12});

    testeq(BROHunkCount(L), 3);

    // BROHunkIndexAt: 1-based, line 0 in hunk 1, etc.
    testeq(BROHunkIndexAt(L, 0), 1);
    testeq(BROHunkIndexAt(L, 2), 1);
    testeq(BROHunkIndexAt(L, 3), 2);
    testeq(BROHunkIndexAt(L, 4), 2);
    testeq(BROHunkIndexAt(L, 5), 3);
    testeq(BROHunkIndexAt(L, 8), 3);

    // BROHunkNextLine: from any line in hunk i, lands on first line of hunk i+1.
    testeq(BROHunkNextLine(L, 0), 3);
    testeq(BROHunkNextLine(L, 1), 3);
    testeq(BROHunkNextLine(L, 2), 3);
    testeq(BROHunkNextLine(L, 3), 5);
    testeq(BROHunkNextLine(L, 4), 5);
    testeq(BROHunkNextLine(L, 5), BRO_NONE);
    testeq(BROHunkNextLine(L, 8), BRO_NONE);

    // BROHunkPrevLine: vim [[ semantics — mid-hunk goes to start of current.
    testeq(BROHunkPrevLine(L, 2), 0);  // mid-hunk-0 -> start of hunk 0
    testeq(BROHunkPrevLine(L, 1), 0);  // mid-hunk-0 -> start of hunk 0
    testeq(BROHunkPrevLine(L, 0), BRO_NONE);  // already at top
    testeq(BROHunkPrevLine(L, 4), 3);  // mid-hunk-1 -> start of hunk 1
    testeq(BROHunkPrevLine(L, 3), 0);  // at hunk 1 start -> hunk 0 start
    testeq(BROHunkPrevLine(L, 8), 5);  // mid-hunk-2 -> start of hunk 2
    testeq(BROHunkPrevLine(L, 5), 3);  // at hunk 2 start -> hunk 1 start
    done;
}

// A hunk with no title (only content) should still be navigable.
ok64 NAVtest_hunk_no_title() {
    sane(1);
    LINES(L,
          {0, 0},  // hunk 0, no title, two text lines
          {0, 5},
          {1, 0},  // hunk 1, no title, one text line
          {2, BRO_TITLE_LINE},  // hunk 2, with title
          {2, 0});

    testeq(BROHunkCount(L), 3);
    testeq(BROHunkNextLine(L, 0), 2);
    testeq(BROHunkNextLine(L, 1), 2);
    testeq(BROHunkNextLine(L, 2), 3);  // lands on title of hunk 2
    testeq(BROHunkPrevLine(L, 1), 0);
    testeq(BROHunkPrevLine(L, 4), 3);
    testeq(BROHunkPrevLine(L, 3), 2);
    done;
}

// Empty index: all queries return BRO_NONE / 0.
ok64 NAVtest_hunk_empty() {
    sane(1);
    range32cs empty = {NULL, NULL};
    testeq(BROHunkCount(empty), 0);
    testeq(BROHunkIndexAt(empty, 0), 0);
    testeq(BROHunkNextLine(empty, 0), BRO_NONE);
    testeq(BROHunkPrevLine(empty, 0), BRO_NONE);
    done;
}

// --- Hili navigation tests --------------------------------------------

// Two hunks, each with non-trivial hili spans.
//
// Hunk 0 text (offsets):
//   "abcdef\nghijkl\nmnopqr"
//    0      7       14
//   line indices in the global index:
//     1: offset 0  (hunk-0 line 0)
//     2: offset 7  (hunk-0 line 1)
//     3: offset 14 (hunk-0 line 2)
//   hili toks: A(end=2) D(end=4) A(end=10) I(end=12) A(end=20)
//     -> three real ranges:
//        D at start_off=2  -> on line 1 (offset 7 > 2 > offset 0)
//        I at start_off=10 -> on line 2 (offset 7 <= 10 < offset 14)
//        (last A is neutral)
//
// Hunk 1 text:
//   "uvw\nxyz"
//    0   4
//   line indices: 5 (offset 0), 6 (offset 4)
//   hili toks: A(end=4) D(end=7)
//     -> one real range:
//        D at start_off=4 -> on line 6
//
// Layout in the line index (line 0 = title of hunk 0):
//   0 -> {0, BRO_TITLE_LINE}
//   1 -> {0, 0}
//   2 -> {0, 7}
//   3 -> {0, 14}
//   4 -> {1, BRO_TITLE_LINE}
//   5 -> {1, 0}
//   6 -> {1, 4}
ok64 NAVtest_hili_layout() {
    sane(1);
    LINES(L,
          {0, BRO_TITLE_LINE},
          {0, 0},
          {0, 7},
          {0, 14},
          {1, BRO_TITLE_LINE},
          {1, 0},
          {1, 4});

    // Toks tile the text exactly; sides eq/rm/eq/in/eq for h0, eq/rm for h1.
    TOKS(h0,
         TOKSIDE('S', 0, 2),    // eq
         TOKSIDE('S', 2, 4),    // rm
         TOKSIDE('S', 0, 10),   // eq
         TOKSIDE('S', 1, 12),   // in
         TOKSIDE('S', 0, 20));  // eq
    TOKS(h1,
         TOKSIDE('S', 0, 4),    // eq
         TOKSIDE('S', 2, 7));   // rm

    static u8 const h0_text[] = "abcdef\nghijkl\nmnopqr";
    static u8 const h1_text[] = "uvw\nxyz";
    static u8 const t0[] = "h0";
    static u8 const t1[] = "h1";

    hunk hunks[2] = {};
    hunks[0].uri[0] = (u8 const *)t0;
    hunks[0].uri[1] = (u8 const *)t0 + 2;
    hunks[0].text[0]  = (u8 const *)h0_text;
    hunks[0].text[1]  = (u8 const *)h0_text + sizeof(h0_text) - 1;
    hunks[0].toks[0]  = h0[0];
    hunks[0].toks[1]  = h0[1];
    hunks[1].uri[0] = (u8 const *)t1;
    hunks[1].uri[1] = (u8 const *)t1 + 2;
    hunks[1].text[0]  = (u8 const *)h1_text;
    hunks[1].text[1]  = (u8 const *)h1_text + sizeof(h1_text) - 1;
    hunks[1].toks[0]  = h1[0];
    hunks[1].toks[1]  = h1[1];

    hunkcs H = {hunks, hunks + 2};

    testeq(BROHiliCount(H), 3);

    // Range start lines (in the global line index):
    //   range 0 (D in hunk 0): line 1
    //   range 1 (I in hunk 0): line 2
    //   range 2 (D in hunk 1): line 6

    // BROHiliNextLine: strict > mid.
    testeq(BROHiliNextLine(H, L, 0), 1);
    testeq(BROHiliNextLine(H, L, 1), 2);
    testeq(BROHiliNextLine(H, L, 2), 6);
    testeq(BROHiliNextLine(H, L, 5), 6);
    testeq(BROHiliNextLine(H, L, 6), BRO_NONE);

    // BROHiliPrevLine: strict < mid.
    testeq(BROHiliPrevLine(H, L, 0), BRO_NONE);
    testeq(BROHiliPrevLine(H, L, 1), BRO_NONE);
    testeq(BROHiliPrevLine(H, L, 2), 1);
    testeq(BROHiliPrevLine(H, L, 3), 2);
    testeq(BROHiliPrevLine(H, L, 6), 2);
    testeq(BROHiliPrevLine(H, L, 7), 6);

    // BROHiliIndexAt: largest range whose first line <= at.
    testeq(BROHiliIndexAt(H, L, 0), 0);
    testeq(BROHiliIndexAt(H, L, 1), 1);
    testeq(BROHiliIndexAt(H, L, 2), 2);
    testeq(BROHiliIndexAt(H, L, 5), 2);
    testeq(BROHiliIndexAt(H, L, 6), 3);
    done;
}

// Adjacent INS then DEL toks count as two separate ranges (property #6).
// They share line 0 here, so navigation can't distinguish them — that
// is by design (line-granular centering). The relevant assertion is
// the count.
ok64 NAVtest_hili_adjacent_kinds() {
    sane(1);

    // toks: in(end=4) rm(end=8) eq(end=20) — two real runs, then eq.
    TOKS(h,
         TOKSIDE('S', 1, 4),    // in
         TOKSIDE('S', 2, 8),    // rm
         TOKSIDE('S', 0, 20));  // eq

    static u8 const text[] = "0123456789xxxxxxxxxx";
    hunk hunks[1] = {};
    hunks[0].text[0] = (u8 const *)text;
    hunks[0].text[1] = (u8 const *)text + sizeof(text) - 1;
    hunks[0].toks[0] = h[0];
    hunks[0].toks[1] = h[1];

    hunkcs H = {hunks, hunks + 1};
    testeq(BROHiliCount(H), 2);
    done;
}

// No hili at all -> count zero, all queries return BRO_NONE.
ok64 NAVtest_hili_none() {
    sane(1);
    LINES(L, {0, 0});
    hunk hunks[1] = {};
    static u8 const text[] = "x";
    hunks[0].text[0] = (u8 const *)text;
    hunks[0].text[1] = (u8 const *)text + 1;

    hunkcs H = {hunks, hunks + 1};
    testeq(BROHiliCount(H), 0);
    testeq(BROHiliNextLine(H, L, 0), BRO_NONE);
    testeq(BROHiliPrevLine(H, L, 0), BRO_NONE);
    testeq(BROHiliIndexAt(H, L, 0), 0);
    done;
}

// --- Mouse parsing tests -----------------------------------------------

ok64 NAVtest_maus_press() {
    sane(1);
    // Left press at col=35, row=12: \033[<0;35;12M
    u8 buf[] = "\033[<0;35;12M";
    MAUSevent ev = {};
    int n = MAUSParse(&ev, buf, (int)(sizeof(buf) - 1));
    want(n > 0);
    testeq(ev.type, MAUS_PRESS);
    testeq(ev.button, MAUS_LEFT);
    testeq(ev.col, 35);
    testeq(ev.row, 12);
    done;
}

ok64 NAVtest_maus_release() {
    sane(1);
    // Right release at col=1, row=100: \033[<2;1;100m
    u8 buf[] = "\033[<2;1;100m";
    MAUSevent ev = {};
    int n = MAUSParse(&ev, buf, (int)(sizeof(buf) - 1));
    want(n > 0);
    testeq(ev.type, MAUS_RELEASE);
    testeq(ev.button, MAUS_RIGHT);
    testeq(ev.col, 1);
    testeq(ev.row, 100);
    done;
}

ok64 NAVtest_maus_wheel() {
    sane(1);
    // Wheel up at col=10, row=5: \033[<64;10;5M
    u8 buf[] = "\033[<64;10;5M";
    MAUSevent ev = {};
    int n = MAUSParse(&ev, buf, (int)(sizeof(buf) - 1));
    want(n > 0);
    testeq(ev.type, MAUS_WHEEL);
    testeq(ev.button, MAUS_UP);
    // Wheel down: \033[<65;10;5M
    u8 buf2[] = "\033[<65;10;5M";
    n = MAUSParse(&ev, buf2, (int)(sizeof(buf2) - 1));
    want(n > 0);
    testeq(ev.type, MAUS_WHEEL);
    testeq(ev.button, MAUS_DOWN);
    done;
}

ok64 NAVtest_maus_incomplete() {
    sane(1);
    // Incomplete sequence should return 0
    u8 buf[] = "\033[<0;35";
    MAUSevent ev = {};
    int n = MAUSParse(&ev, buf, (int)(sizeof(buf) - 1));
    testeq(n, 0);
    done;
}

ok64 NAVtest() {
    sane(1);
    call(NAVtest_hunk_layout);
    call(NAVtest_hunk_no_title);
    call(NAVtest_hunk_empty);
    call(NAVtest_hili_layout);
    call(NAVtest_hili_adjacent_kinds);
    call(NAVtest_hili_none);
    call(NAVtest_maus_press);
    call(NAVtest_maus_release);
    call(NAVtest_maus_wheel);
    call(NAVtest_maus_incomplete);
    done;
}

TEST(NAVtest)
