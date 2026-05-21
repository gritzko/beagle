// Property tests for the soft-wrap line-index builder:
//   BROCountLines / BROAppendLines
//
// Each case builds hunks, runs BROCountLines to size a buffer, runs
// BROAppendLines to fill it, and compares against an expected
// range32[] — no terminal, no rendering, no I/O.

#include "BRO.h"

#include "abc/B.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

// --- helpers ----------------------------------------------------------

// `name` is a range32cs slice over the data array.
#define LINES(name, ...)                                              \
    static range32 const name##_data[] = {__VA_ARGS__};               \
    range32cs name = {name##_data,                                    \
                      name##_data + sizeof(name##_data) / sizeof(range32)}

// Build one hunk with text t and optional uri u (NULL = no title).
// lits are string literals; length excludes the NUL terminator.
static void mk_hunk(hunk *hk, char const *uri, char const *t) {
    *hk = (hunk){};
    if (uri) {
        hk->uri[0] = (u8 const *)uri;
        hk->uri[1] = (u8 const *)uri + strlen(uri);
    }
    if (t) {
        hk->text[0] = (u8 const *)t;
        hk->text[1] = (u8 const *)t + strlen(t);
    }
}

static ok64 check_wrap(char const *label, hunkcs hunks, u32 cols,
                       range32cs want) {
    sane(label != NULL);
    u32 want_n = (u32)$len(want);
    u32 got_n = BROCountLines(hunks, cols);
    if (got_n != want_n) {
        fprintf(stderr, "%s: BROCountLines: want %u, got %u\n",
                label, want_n, got_n);
        fail(FAILSANITY);
    }
    a_pad(range32, got, 128);
    if (want_n > 128) fail(NOROOM);
    BROAppendLines(got, hunks, 0, cols);
    u32 n = (u32)range32bDataLen(got);
    if (n != want_n) {
        fprintf(stderr, "%s: BROAppendLines: want %u, got %u\n",
                label, want_n, n);
        fail(FAILSANITY);
    }
    range32 const *got_d = range32bDataHead(got);
    for (u32 i = 0; i < n; i++) {
        if (got_d[i].lo != want[0][i].lo || got_d[i].hi != want[0][i].hi) {
            fprintf(stderr,
                    "%s: entry[%u]: want {%u,%u}, got {%u,%u}\n",
                    label, i, want[0][i].lo, want[0][i].hi,
                    got_d[i].lo, got_d[i].hi);
            fail(FAILSANITY);
        }
    }
    done;
}

// --- cases ------------------------------------------------------------

// Short line (< cols) produces a single entry, no title when uri empty.
ok64 WRAPtest_short_no_title() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, NULL, "abc\n");
    LINES(W, {0, 0});
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("short_no_title", H, 80, W);
}

// Line exactly cols wide — still one entry, no wrap after it.
ok64 WRAPtest_exact_cols() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, NULL, "abcdef\n");  // 6 chars + \n, cols=6
    LINES(W, {0, 0});
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("exact_cols", H, 6, W);
}

// Long ASCII line wraps every cols codepoints.
ok64 WRAPtest_wrap_ascii() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, NULL, "abcdefghij");  // 10 chars, cols=3
    LINES(W,
          {0, 0},
          {0, 3},
          {0, 6},
          {0, 9});
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("wrap_ascii", H, 3, W);
}

// Wrap + newlines interleaved.
ok64 WRAPtest_wrap_then_newline() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, NULL, "abc\ndefghij");  // "abc\n" then 6-char wrap at 3
    LINES(W,
          {0, 0},   // "abc"
          {0, 4},   // "def"
          {0, 7},   // "ghi"
          {0, 10}); // "j"
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("wrap_then_newline", H, 3, W);
}

// Trailing newline must not yield a phantom empty row.
ok64 WRAPtest_trailing_newline() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, NULL, "abc\ndef\n");
    LINES(W,
          {0, 0},   // "abc"
          {0, 4});  // "def"
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("trailing_newline", H, 80, W);
}

// Empty text hunk contributes no rows (even with a title).
ok64 WRAPtest_empty_text_with_title() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, "foo.c", "");
    LINES(W, {0, BRO_TITLE_LINE});
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("empty_text_with_title", H, 80, W);
}

// Multi-byte UTF-8 (3 bytes per codepoint).  Three "日" bytes
// 0xE6 0x97 0xA5 * 3 = 9 bytes, 3 codepoints; cols=2 should wrap
// after 2 codepoints = 6 bytes.
ok64 WRAPtest_utf8_wrap() {
    sane(1);
    hunk hk;
    // "日本語" = 9 bytes; wrap at 2 cp -> two rows: offsets 0 and 6.
    mk_hunk(&hk, NULL, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
    LINES(W, {0, 0}, {0, 6});
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("utf8_wrap", H, 2, W);
}

// Title + multi-line + wrap — verifies title placement and hunk idx.
ok64 WRAPtest_title_multi() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, "a.c", "abcdef\nghij\n");
    // cols=4 ->  "abcd" | "ef" | "ghij"
    LINES(W,
          {0, BRO_TITLE_LINE},
          {0, 0},
          {0, 4},
          {0, 7});
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("title_multi", H, 4, W);
}

// Two hunks, second has no title, wrap only on second.
ok64 WRAPtest_two_hunks() {
    sane(1);
    hunk hks[2];
    mk_hunk(&hks[0], "a.c", "x\n");          // 1 title + 1 row
    mk_hunk(&hks[1], NULL, "abcdefg");       // cols=3 -> 3 rows
    LINES(W,
          {0, BRO_TITLE_LINE},
          {0, 0},
          {1, 0},
          {1, 3},
          {1, 6});
    hunkcs H = {hks, hks + 2};
    return check_wrap("two_hunks", H, 3, W);
}

// cols=1 is the pathological edge case: one codepoint per row.
ok64 WRAPtest_cols_one() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, NULL, "abc");
    LINES(W, {0, 0}, {0, 1}, {0, 2});
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("cols_one", H, 1, W);
}

// No-wrap mode: 'w'-toggle passes the 24-bit offset ceiling
// (BRO_LINE_OFF_MASK) so each source line is exactly one index entry
// regardless of its length.
ok64 WRAPtest_nowrap() {
    sane(1);
    hunk hk;
    mk_hunk(&hk, NULL, "abcdefghij\nkl\nmnopqrst");
    LINES(W,
          {0, 0},    // "abcdefghij" — no wrap inside this 10-char line
          {0, 11},   // "kl"
          {0, 14});  // "mnopqrst"
    hunkcs H = {&hk, &hk + 1};
    return check_wrap("nowrap", H, (1u << 24) - 1, W);
}

// --- runner ----------------------------------------------------------

ok64 WRAPtest() {
    sane(1);
    call(BROScratchInit);
    call(WRAPtest_short_no_title);
    call(WRAPtest_exact_cols);
    call(WRAPtest_wrap_ascii);
    call(WRAPtest_wrap_then_newline);
    call(WRAPtest_trailing_newline);
    call(WRAPtest_empty_text_with_title);
    call(WRAPtest_utf8_wrap);
    call(WRAPtest_title_multi);
    call(WRAPtest_two_hunks);
    call(WRAPtest_cols_one);
    call(WRAPtest_nowrap);
    done;
}

TEST(WRAPtest)
