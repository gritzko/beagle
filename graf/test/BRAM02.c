//
//  BRAM01 — property tests for BRAMu64s (Cohen patience diff) plus a
//  BASS arena high-water bound check (MEM-018).
//
//  Property: the returned EDL, walked over the OLD/NEW token-hash
//  arrays, reconstructs each side exactly (EQ/DEL consume OLD,
//  EQ/INS consume NEW; the totals match the input lengths).
//
//  MEM-018 regression: BRAMu64s carves ABC_BASS scratch at EVERY
//  recursion level (bram_region → BRAMu64s → bram_region …).  Before
//  the fix those carves were never rewound per level, so a deeply
//  recursive diff accumulated `depth × region_size` of scratch on
//  BASS until `a_carve` returned NOROOM and the diff silently
//  truncated.  After the fix each recursion descent marks/rewinds, so
//  the scratch left on BASS by ONE raw (un-call()-wrapped) BRAMu64s
//  invocation is bounded by a single level's footprint, independent
//  of recursion depth.
//
#include "BRAM.h"

#include <stdio.h>
#include <string.h>

#include "abc/DIFF.h"
#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/TEST.h"

// Instantiate the u64 DIFF specialization (DIFFu64s, DIFFu64AddEntry…).
#define X(M, name) M##u64##name
#include "abc/DIFFx.h"
#undef X

// --- Run BRAMu64s over two hash arrays, return EDL + count ---

static ok64 bram_run(e32 *out_edl, u32 cap, u32 *out_n, i64 *out_leak,
                     u64cp old_h, u32 na, u64cp new_h, u32 nb) {
    sane(out_edl && out_n && out_leak);
    u64 emax = DIFFEdlMaxEntries(na, nb);
    if (emax == 0 || emax > cap) fail(TESTFAIL);
    u64 ws_size = DIFFWorkSize(na, nb);

    //  `work` comes from a real allocation (not BASS) so it does not
    //  perturb the BASS dispense point we are about to probe.
    Bi32 work = {};
    call(i32bAllocate, work, ws_size);

    e32g edl = {out_edl, out_edl + emax, out_edl};
    i32s ws  = {i32bHead(work), i32bTerm(work)};
    u64cs oh = {old_h, old_h + na};
    u64cs nh = {new_h, new_h + nb};

    //  Probe the BASS dispense point (idle head) immediately around a
    //  RAW BRAMu64s call (no call() between probe and the diff), so any
    //  per-recursion-level scratch BRAMu64s fails to rewind stays
    //  visible as a delta.  With per-level mark/rewind the delta is
    //  bounded by a single level; without it grows with recursion depth.
    u8 *bass_before = ABC_BASS[2];
    ok64 r = BRAMu64s(edl, ws, oh, nh);
    *out_leak = (i64)(ABC_BASS[2] - bass_before);
    *out_n = (u32)(edl[0] - edl[2]);
    i32bFree(work);
    return r;
}

// --- Property: EDL reconstructs both sides ---

static ok64 bram_check_edl(e32 const *edl, u32 n, u32 na, u32 nb) {
    sane(1);
    u32 oi = 0, ni = 0;
    for (u32 k = 0; k < n; k++) {
        u32 op  = DIFF_OP(edl[k]);
        u32 len = DIFF_LEN(edl[k]);
        if (len == 0) { fprintf(stderr, "zero-len entry\n"); fail(TESTFAIL); }
        if      (op == DIFF_EQ)  { oi += len; ni += len; }
        else if (op == DIFF_DEL) { oi += len; }
        else if (op == DIFF_INS) { ni += len; }
        else { fprintf(stderr, "bad op %u\n", op); fail(TESTFAIL); }
    }
    if (oi != na) { fprintf(stderr, "old %u != %u\n", oi, na); fail(TESTFAIL); }
    if (ni != nb) { fprintf(stderr, "new %u != %u\n", ni, nb); fail(TESTFAIL); }
    done;
}

// --- Recursive-input generator -------------------------------------
//
//  Build OLD/NEW token-hash arrays that force bram_region recursion:
//  a sequence of `nblocks` blocks, each block = one unique anchor
//  "line" (hash NL-terminated) bracketing a nested sub-region that is
//  ITSELF made of within-region-unique lines.  The top-level anchors
//  match on both sides, so every inter-anchor region recurses into a
//  fresh BRAMu64s — driving the per-level carve.  NEW perturbs each
//  block's interior so the region is non-trivial (real DEL+INS work).

static u64 g_nl_hash = 0;

//  Emit one "line": `count` distinctive token hashes then the NL hash.
static void bram_emit_line(u64 *arr, u32 *pos, u64 seed, u32 count) {
    for (u32 i = 0; i < count; i++)
        arr[(*pos)++] = (seed * 0x9e3779b97f4a7c15ULL) + i + 1;
    arr[(*pos)++] = g_nl_hash;
}

// --- Cases ----------------------------------------------------------

typedef struct {
    char const *name;
    u32         nblocks;     // recursion-driving block count
    u32         interior;    // tokens per interior line
} BRAMcase;

static BRAMcase cases[] = {
    {"tiny",        4,   2},
    {"shallow",    16,   3},
    {"deep",      256,   4},
    {"deeper",   1024,   4},
};

//  Build OLD and NEW arrays for a case.  Returns token counts via
//  *na/*nb.  Caller provides `old_h`/`new_h` with ample capacity.
static void bram_build(BRAMcase const *c, u64 *old_h, u32 *na,
                       u64 *new_h, u32 *nb) {
    u32 po = 0, pn = 0;
    for (u32 b = 0; b < c->nblocks; b++) {
        //  Unique anchor line shared by both sides (matches on both →
        //  top-level patience anchor → forces a between-anchor recurse).
        bram_emit_line(old_h, &po, 0xA0000000ULL + b, 1);
        bram_emit_line(new_h, &pn, 0xA0000000ULL + b, 1);
        //  Interior region: OLD has one variant, NEW another — a real
        //  diff that itself contains within-region-unique lines so the
        //  recursion re-anchors.
        bram_emit_line(old_h, &po, 0xB0000000ULL + b, c->interior);
        bram_emit_line(old_h, &po, 0xC0000000ULL + b, c->interior);
        bram_emit_line(new_h, &pn, 0xB0000000ULL + b, c->interior);
        bram_emit_line(new_h, &pn, 0xD0000000ULL + b, c->interior);  // changed
        bram_emit_line(new_h, &pn, 0xC0000000ULL + b, c->interior);
    }
    *na = po; *nb = pn;
}

#define BRAM_MAX_TOK (1u << 16)
#define BRAM_MAX_EDL (BRAM_MAX_TOK * 2)

static ok64 bram_run_case(BRAMcase const *c) {
    sane(1);
    fprintf(stderr, "  %s (nblocks=%u) ...", c->name, c->nblocks);

    u8 *omem[4] = {}, *nmem[4] = {}, *emem[4] = {};
    call(u8bAlloc, omem, (u64)BRAM_MAX_TOK * sizeof(u64));
    call(u8bAlloc, nmem, (u64)BRAM_MAX_TOK * sizeof(u64));
    call(u8bAlloc, emem, (u64)BRAM_MAX_EDL * sizeof(e32));
    u64 *old_h = (u64 *)omem[1];
    u64 *new_h = (u64 *)nmem[1];
    e32 *edl   = (e32 *)emem[1];

    u32 na = 0, nb = 0;
    bram_build(c, old_h, &na, new_h, &nb);
    if (na >= BRAM_MAX_TOK || nb >= BRAM_MAX_TOK) {
        u8bFree(omem); u8bFree(nmem); u8bFree(emem);
        fail(TESTFAIL);
    }

    u32 n = 0;
    i64 leaked = 0;
    call(bram_run, edl, BRAM_MAX_EDL, &n, &leaked, old_h, na, new_h, nb);

    call(bram_check_edl, edl, n, na, nb);

    //  MEM-018 bound: BRAMu64s only writes its result into the caller's
    //  `edl` buffer; every line/pair/anchor/inner-edl/work buffer it
    //  carves on BASS is internal scratch that must NOT outlive the call.
    //  With per-recursion-level mark/rewind the BASS dispense point on
    //  return equals its value on entry (leak == 0), independent of input
    //  size and recursion depth.  The buggy (no-rewind) build instead
    //  retains every level's carves, so the leak scales with the
    //  recursion tree (observed 2.5 KB → 800 KB across these cases).  Cap
    //  at a small fixed constant the fixed build meets and the buggy one
    //  blows past on the larger cases.
    u64 const BASS_LEAK_CAP = 64u * 1024u;   // fixed; must not scale with input
    fprintf(stderr, " bass_leak=%lld cap=%llu n=%u",
            (long long)leaked, (unsigned long long)BASS_LEAK_CAP, n);
    if (leaked < 0 || (u64)leaked > BASS_LEAK_CAP) {
        fprintf(stderr, "  BASS HIGH-WATER UNBOUNDED (MEM-018)\n");
        u8bFree(omem); u8bFree(nmem); u8bFree(emem);
        fail(TESTFAIL);
    }

    u8bFree(omem); u8bFree(nmem); u8bFree(emem);
    fprintf(stderr, " ok\n");
    done;
}

ok64 BRAMtest() {
    sane(1);
    //  NL hash: BRAM uses RAPHash("\n") internally; mirror it so our
    //  generated "lines" split on the same boundary.
    {
        u8 nl = '\n';
        u8csc s = {&nl, &nl + 1};
        g_nl_hash = RAPHash(s);
    }
    u32 n = (u32)(sizeof(cases) / sizeof(cases[0]));
    for (u32 i = 0; i < n; i++)
        call(bram_run_case, &cases[i]);
    done;
}

TEST(BRAMtest);
