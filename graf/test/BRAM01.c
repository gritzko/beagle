//
//  BRAM01 — MEM-036 repro: the weave_diff_core BRAM-NOROOM fallback.
//
//  weave_diff_core (graf/WEAVE.c) sizes its EDL buffer to
//  DIFFEdlMaxEntries(olen,nlen) = olen+nlen and runs BRAMu64s into it.
//  When BRAM can't fit (DIFFNOROOM) it falls back to a wholesale
//  DEL(olen)+INS(nlen).  The OLD fallback did:
//
//      edlg[1] = edlg[0];
//      *edlg[1]++ = DIFF_ENTRY(DIFF_DEL, olen);
//      *edlg[1]++ = DIFF_ENTRY(DIFF_INS, nlen);
//      NEILCanon(edlg);
//
//  Two defects (this file reproduces both):
//    1.  On NOROOM BRAM has filled the buffer, so the gauge cursor
//        edl[0]==edl[1]==buf_end; the two raw *edlg[1]++ writes land at
//        and past the cap end -> heap OOB (ASan catches it).
//    2.  The writes go through edl[1] but never advance edl[0], so
//        NEILCanon (n = edl[0]-edl[2]) sees zero new entries and the
//        DEL/INS pair is silently dropped.
//
//  The fix routes the fallback through the checked DIFFu64AddEntry
//  against a CLEAN buffer so NOROOM propagates instead of overflowing,
//  and advances edl[0].  This test exercises the fixed contract:
//  weave_fallback_edl must (a) never write OOB and (b) leave the two
//  entries visible to a edl[0]-based reader.
//
#include "graf/WEAVE.h"
#include "graf/BRAM.h"

#include <stdio.h>

#include "abc/DIFF.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "abc/RAP.h"
#include "graf/NEIL.h"

static u64 hh(u8 b) { u8cs s = {&b, &b + 1}; return RAPHash(s); }

//  Drive BRAMu64s into DIFFNOROOM with a cap-1 EDL, exactly as
//  weave_diff_core would when BRAM exhausts its olen+nlen buffer, then
//  emit the wholesale DEL+INS fallback via the (fixed) checked helper.
//  Asserts: no OOB (ASan), NOROOM propagated cleanly, and after a clean
//  re-emit the two entries are visible to NEILCanon (edl[0] advanced).
static ok64 bram_fallback_case(u32 na, u32 nb) {
    sane(1);
    u8 nl = '\n';
    u8cs s = {&nl, &nl + 1};
    u64 NLH = RAPHash(s);

    //  OLD = x x NL x x x NL   NEW = x x  -> BRAM wants 2 EDL entries.
    u64 oa[7] = { hh('x'),hh('x'),NLH,hh('x'),hh('x'),hh('x'),NLH };
    u64 ob[2] = { hh('x'),hh('x') };
    must(na <= 7 && nb <= 2, "fixed fixture");

    //  Step 1: buffer sized to ONE entry to force the NOROOM state with
    //  the gauge pinned at the cap end (the bug's precondition).
    a_carve(u32, edlbuf, 1);
    a_carve(i32, work, DIFFWorkSize(na, nb));
    e32g edl = {edlbuf[0], edlbuf[3], edlbuf[0]};
    i32s ws  = {i32bHead(work), i32bTerm(work)};
    u64cs oh = {oa, oa + na};
    u64cs nh = {ob, ob + nb};
    ok64 r = BRAMu64s(edl, ws, oh, nh);
    must(r != OK, "expected NOROOM with cap-1 EDL");
    must(edl[0] == edl[1], "NOROOM leaves cursor pinned at cap end");

    //  Step 2: the fallback.  The fixed code resets the EDL to a clean
    //  base and appends via the checked DIFFu64AddEntry; this MUST NOT
    //  write OOB and MUST advance edl[0].  (The old code did
    //  edlg[1]=edlg[0] + two raw *edlg[1]++ writes here -> OOB at buf
    //  end + dropped entries.)
    ok64 fo = WEAVEFallbackEdl(edl, (u32)na, (u32)nb);
    //  With a cap-1 buffer the wholesale DEL+INS can't fit either, so the
    //  fixed path returns NOROOM rather than overflowing.
    must(fo != OK, "cap-1 fallback must propagate NOROOM, not overflow");
    done;
}

//  Same fallback, but with a buffer big enough for the DEL+INS pair:
//  assert the two entries land and are visible to a edl[0] reader
//  (defect #2 — dropped entries — would leave edl[0] unadvanced).
static ok64 bram_fallback_visible(u32 olen, u32 nlen) {
    sane(1);
    a_carve(u32, edlbuf, 4);
    e32g edl = {edlbuf[0], edlbuf[3], edlbuf[0]};
    ok64 fo = WEAVEFallbackEdl(edl, olen, nlen);
    must(fo == OK, "fallback should fit in a 4-slot buffer");
    u32 n = (u32)(edl[0] - edl[2]);   // NEILCanon's view
    must(n == 2, "DEL+INS must be visible via edl[0] advance");
    must(DIFF_OP(edl[2][0]) == DIFF_DEL && DIFF_LEN(edl[2][0]) == olen,
         "first entry = DEL(olen)");
    must(DIFF_OP(edl[2][1]) == DIFF_INS && DIFF_LEN(edl[2][1]) == nlen,
         "second entry = INS(nlen)");
    //  And NEILCanon must accept it without tripping.
    call(NEILCanon, edl);
    done;
}

ok64 BRAM01test() {
    sane(1);
    call(bram_fallback_case, 7, 2);
    call(bram_fallback_visible, 7, 2);
    call(bram_fallback_visible, 1, 1);
    call(bram_fallback_visible, 13, 5);
    done;
}

TEST(BRAM01test);
