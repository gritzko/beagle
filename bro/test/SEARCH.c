// Property/repro tests for bro's interactive search step, BROSearchNext.
//
// These tests need access to file-static symbols in BRO.c
// (BROstate, BROSearchNext, bro_lines, the BRO_PASS_* encoding and the
// real per-hunk row walker).  Rather than re-export them, the test TU
// includes BRO.c directly so the statics are in scope.  It is compiled
// standalone (NOT linked against brolib) to avoid duplicate symbols.
//
// Regression target: MEM-006.  BROSearchNext read range32.hi raw as a
// byte offset, but for non-title rows hi is bro_line_make(off,pass) =
// (pass<<24)|(off&0xFFFFFF).  When a NORMAL-pass row is adjacent (same
// hunk index `lo`) to an RM/IN-pass row, the packed nhi (~0x1000000)
// passed the `nhi > off` test and indexed hk->text[0][nhi-1] ~16 MB
// past a few-byte hunk text — a heap OOB read under ASan.

#include "../BRO.c"

#include "abc/TEST.h"

// Build a tok32 (tag 'S', given diff side, end offset).  Mirrors the
// TOKSIDE helper in NAV.c; kept local so the file is self-contained.
#define TOKSIDE(side, off) tok32PackSide('S', (u8)(side), (u32)(off))

// Construct a BROstate over `hunks` whose line index is built by the
// real walker (BROAppendLines), set the search pattern, and run one
// search step from `from` in `dir`.  Returns the resulting line index.
static u32 search_step(hunkcs hunks, char const *pat, u32 from, int dir) {
    BROstate st = {};
    st.hunks[0] = hunks[0];
    st.hunks[1] = hunks[1];
    st.cols = 80;
    st.rows = 24;

    // Line index: carve a generous fixed Brange32 and let the real
    // walker fill it.  bro_wrap_cols(no-wrap) would be the 24-bit mask;
    // pass a concrete width so soft-wrap stays off for these tiny lines.
    aBpad(range32, lines, 256);
    if (BROAppendLines(lines, hunks, 0, 4096) != OK) return UINT32_MAX;
    // BROstate.linesbuf is read via bro_lines()/bro_nlines(): point it
    // at the freshly-built buffer.  Copy the 4 borders across.
    ((range32 **)st.linesbuf)[0] = lines[0];
    ((range32 **)st.linesbuf)[1] = lines[1];
    ((range32 **)st.linesbuf)[2] = lines[2];
    ((range32 **)st.linesbuf)[3] = lines[3];

    // Search pattern buffer.
    aBpad(u8, search, 256);
    for (char const *p = pat; *p; p++) u8bFeed1(search, (u8)*p);
    ((u8 **)st.search)[0] = search[0];
    ((u8 **)st.search)[1] = search[1];
    ((u8 **)st.search)[2] = search[2];
    ((u8 **)st.search)[3] = search[3];

    return BROSearchNext(&st, from, dir);
}

// Assert that the built line index actually contains a NORMAL-pass row
// immediately followed by an RM- or IN-pass row of the same hunk —
// i.e. the exact adjacency the MEM-006 trigger requires.  Without this
// the repro could silently stop reproducing if the walker changes.
static b8 has_normal_then_split(hunkcs hunks) {
    aBpad(range32, lines, 256);
    if (BROAppendLines(lines, hunks, 0, 4096) != OK) return NO;
    u32 n = (u32)range32bDataLen(lines);
    range32 *L = range32bDataHead(lines);
    for (u32 i = 0; i + 1 < n; i++) {
        if (L[i].hi == BRO_TITLE_LINE || L[i + 1].hi == BRO_TITLE_LINE)
            continue;
        if (L[i].lo != L[i + 1].lo) continue;
        if (bro_line_pass(&L[i]) == BRO_PASS_NORMAL &&
            bro_line_pass(&L[i + 1]) != BRO_PASS_NORMAL)
            return YES;
    }
    return NO;
}

// MEM-006 repro.  A hunk holding:
//   line 0: "needle\n"   — pure-eq context  -> NORMAL-pass row
//   line 1: "aaaabbbb"   — half rm, half in -> MOD_SPLIT (RM + IN rows)
// The NORMAL eq row precedes the RM-pass row (same hunk), so the search
// step at the eq row reads the next row's packed hi as a raw offset.
ok64 SEARCHtest_split_oob() {
    sane(1);

    // text offsets:  0..6 = "needle\n", 7..14 = "aaaabbbb"
    static u8 const text[] = "needle\naaaabbbb";
    static u8 const uri[] = "f.c";

    // toks tile the text exactly by *end* offset:
    //   eq up to 7 (covers "needle\n"), rm up to 11 ("aaaa"),
    //   in up to 15 ("bbbb").  Line 1 thus has rm_b=4 in_b=4 eq_b=0,
    //   changed==total -> MOD_SPLIT.  tok32PackSide is a fun (static
    //   inline), not a static-initializer constant, so fill at runtime.
    u32 toks_data[3];
    toks_data[0] = TOKSIDE(TOK_SIDE_EQ, 7);
    toks_data[1] = TOKSIDE(TOK_SIDE_RM, 11);
    toks_data[2] = TOKSIDE(TOK_SIDE_IN, 15);

    hunk hunks[1] = {};
    hunks[0].uri[0] = uri;
    hunks[0].uri[1] = uri + sizeof(uri) - 1;
    hunks[0].text[0] = text;
    hunks[0].text[1] = text + sizeof(text) - 1;
    hunks[0].toks[0] = toks_data;
    hunks[0].toks[1] = toks_data + 3;
    hunkcs H = {hunks, hunks + 1};

    // Confirm the layout actually has the triggering adjacency.
    want(has_normal_then_split(H));

    // Row layout the walker produces (verified):
    //   0: title sentinel
    //   1: NORMAL pass, off 0   -> "needle\n"
    //   2: RM pass,    off 7    -> "aaaa" (packed hi 0x1000007)
    //   3: IN pass,    off 7    -> "bbbb" (packed hi 0x2000007)
    // Row 1 (NORMAL) is adjacent to row 2 (RM-pass) of the same hunk —
    // the MEM-006 trigger.

    // Forward step from the title lands on the eq row, whose next-row
    // probe (row 2) carries the packed pass bit.  Before the fix this
    // over-read text[0][0x1000006] ~16 MB OOB; after, "needle" is found
    // on row 1.
    u32 hit = search_step(H, "needle", 0, +1);
    testeq(hit, 1u);

    // Backward step from past the block walks the same adjacency in
    // reverse and must also find the eq row without an OOB read.
    u32 back = search_step(H, "needle", 3, -1);
    testeq(back, 1u);

    // The RM-side content ("aaaa", row 2) is reachable too: searching it
    // exercises a row whose own `off` is packed in `hi`, so the decode
    // of the current row (not just the next) is covered.
    u32 rm = search_step(H, "aaaa", 0, +1);
    want(rm != UINT32_MAX);
    done;
}

ok64 SEARCHtest() {
    sane(1);
    call(SEARCHtest_split_oob);
    done;
}

TEST(SEARCHtest)
