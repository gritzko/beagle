//  DELT test: encode a delta, apply it, verify the result equals the
//  target.  Table of (base, target) pairs covering a few shapes:
//  identical inputs, head-rewrite, tail-append, full-replace, single
//  edit in the middle, and a larger random-ish blob for the extend
//  paths (forward + backward).

#include "dog/git/DELT.h"

#include <string.h>

#include "abc/B.h"
#include "abc/PRO.h"
#include "abc/S.h"
#include "abc/TEST.h"

typedef struct {
    const char *name;
    const char *base;
    const char *target;
} delt_case;

static const delt_case CASES[] = {
    {"identical",
     "the quick brown fox jumps over the lazy dog",
     "the quick brown fox jumps over the lazy dog"},

    {"head-rewrite",
     "the quick brown fox jumps over the lazy dog",
     "that quick brown fox jumps over the lazy dog"},

    {"tail-append",
     "the quick brown fox jumps over the lazy dog",
     "the quick brown fox jumps over the lazy dog — twice"},

    {"middle-edit",
     "the quick brown fox jumps over the lazy dog",
     "the quick RED fox jumps over the lazy dog"},

    {"full-replace",
     "the quick brown fox jumps over the lazy dog",
     "some entirely unrelated content here"},

    {"tiny-base",
     "abc",
     "xyzabc"},

    {"empty-target",
     "the quick brown fox",
     ""},

    {"empty-base",
     "",
     "unrelated"},
};

static ok64 round_trip(delt_case const *c) {
    sane(c);
    u8cs base   = {(u8cp)c->base,   (u8cp)c->base   + strlen(c->base)};
    u8cs target = {(u8cp)c->target, (u8cp)c->target + strlen(c->target)};

    Bu8 delta_buf = {};
    call(u8bMap, delta_buf, 1u << 16);

    a_dup(u8c, bcs, base);
    a_dup(u8c, tcs, target);
    ok64 eo = DELTEncode(bcs, tcs, delta_buf);
    //  DELTFAIL is legitimate (delta >= target size); skip apply in
    //  that case — raw would be used instead.
    if (eo != OK && eo != DELTFAIL) {
        fprintf(stderr, "DELT %s: encode failed %s\n", c->name, ok64str(eo));
        u8bUnMap(delta_buf);
        fail(TESTFAIL);
    }

    if (eo == OK) {
        Bu8 out = {};
        call(u8bMap, out, 1u << 16);

        a_dup(u8c, dcs, u8bDataC(delta_buf));
        a_dup(u8c, bcs2, base);
        u8g og = {u8bIdleHead(out), u8bIdleHead(out), u8bTerm(out)};
        ok64 ao = DELTApply(dcs, bcs2, og);
        if (ao != OK) {
            fprintf(stderr, "DELT %s: apply failed %s\n", c->name, ok64str(ao));
            u8bUnMap(delta_buf);
            u8bUnMap(out);
            fail(TESTFAIL);
        }

        u64 produced = (u64)(og[1] - og[0]);
        if (produced != u8csLen(target) ||
            memcmp(og[0], target[0], produced) != 0) {
            fprintf(stderr, "DELT %s: result mismatch (%llu bytes)\n",
                    c->name, (unsigned long long)produced);
            u8bUnMap(delta_buf);
            u8bUnMap(out);
            fail(TESTFAIL);
        }

        u8bUnMap(out);
    }

    u8bUnMap(delta_buf);
    done;
}

ok64 DELTroundtrip() {
    sane(1);
    for (size_t i = 0; i < sizeof(CASES)/sizeof(CASES[0]); i++) {
        call(round_trip, &CASES[i]);
    }
    done;
}

//  Back-extension stress: a repeated prefix in the target that
//  overlaps the back-extension region.  Regression test for the old
//  "flush then back-extend" double-emit bug.
ok64 DELTbackext() {
    sane(1);

    a_cstr(base,   "aaaa_BBBB_cccc");
    a_cstr(target, "zzz_BBBB_cccc_aaaa_BBBB_cccc");

    Bu8 delta = {};
    call(u8bMap, delta, 4096);

    a_dup(u8c, bcs, base);
    a_dup(u8c, tcs, target);
    ok64 eo = DELTEncode(bcs, tcs, delta);
    if (eo != OK && eo != DELTFAIL) { u8bUnMap(delta); fail(TESTFAIL); }

    if (eo == OK) {
        Bu8 out = {};
        call(u8bMap, out, 4096);

        a_dup(u8c, dcs, u8bDataC(delta));
        a_dup(u8c, bcs2, base);
        u8g og = {u8bIdleHead(out), u8bIdleHead(out), u8bTerm(out)};
        call(DELTApply, dcs, bcs2, og);

        u64 n = (u64)(og[1] - og[0]);
        want(n == u8csLen(tcs));
        want(memcmp(og[0], tcs[0], n) == 0);
        u8bUnMap(out);
    }
    u8bUnMap(delta);
    done;
}

//  Long-run blob: base and target share a big middle section.  Delta
//  should be much smaller than target.
ok64 DELTlongrun() {
    sane(1);

    //  1 KiB of deterministic-but-random bytes with a deliberate edit.
    enum { N = 1024 };
    a_pad(u8, base,   N);
    a_pad(u8, target, N);
    for (u64 i = 0; i < N; i++) {
        u8 c = (u8)(((i * 1103515245u) + 12345u) >> 16);
        u8bFeed1(base, c);
        u8bFeed1(target, c);
    }
    //  Change 8 bytes in the middle of target.
    u8p t0 = u8bDataHead(target);
    for (int i = 0; i < 8; i++) t0[N/2 + i] = (u8)('A' + i);

    Bu8 delta = {};
    call(u8bMap, delta, 1u << 16);

    a_dup(u8c, bcs, u8bDataC(base));
    a_dup(u8c, tcs, u8bDataC(target));
    ok64 eo = DELTEncode(bcs, tcs, delta);
    want(eo == OK);
    //  Should be a clear win.
    want(u8bDataLen(delta) < N / 2);

    Bu8 out = {};
    call(u8bMap, out, 1u << 16);
    a_dup(u8c, dcs, u8bDataC(delta));
    a_dup(u8c, bcs2, u8bDataC(base));
    u8g og = {u8bIdleHead(out), u8bIdleHead(out), u8bTerm(out)};
    call(DELTApply, dcs, bcs2, og);

    u64 n = (u64)(og[1] - og[0]);
    want(n == N);
    want(memcmp(og[0], t0, N) == 0);

    u8bUnMap(delta);
    u8bUnMap(out);
    done;
}

//  Malformed-delta cases: each must reject without OOB read or UB.
//  Regression tests for varint shift overflow, truncated copy
//  instructions, and oversized literals.
typedef struct {
    const char *name;
    const u8 *delta;
    u64 delta_len;
    const char *base;
} bad_case;

static ok64 bad_apply(bad_case const *c) {
    sane(c);
    u8cs delta = {(u8cp)c->delta, (u8cp)c->delta + c->delta_len};
    u8cs base  = {(u8cp)c->base,  (u8cp)c->base  + strlen(c->base)};

    a_pad(u8, out_buf, 1024);
    u8g og = {u8bIdleHead(out_buf), u8bIdleHead(out_buf), u8bTerm(out_buf)};
    a_dup(u8c, dcs, delta);
    a_dup(u8c, bcs, base);

    ok64 rv = DELTApply(dcs, bcs, og);
    if (rv == OK) {
        fprintf(stderr, "DELT bad %s: accepted malformed input\n", c->name);
        fail(TESTFAIL);
    }
    done;
}

ok64 DELTbadcases() {
    sane(1);

    //  Varint with too many continuation bytes — exercises the
    //  shift-overflow guard in DELTDrainSize.
    static const u8 D_oversize_varint[] = {
        0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0x01,
    };

    //  base_sz=10, result_sz=4, then cmd=0x91 = copy with off-byte and
    //  size-byte, but only one trailing byte present.  Used to read
    //  past delta[1] before the bounds fix.
    static const u8 D_truncated_copy[] = { 0x0a, 0x04, 0x91, 0x00 };

    //  base_sz=0, result_sz=10, cmd=0x05 (insert 5 literal bytes) but
    //  only 2 follow — must reject, not over-read.
    static const u8 D_short_insert[] = { 0x00, 0x0a, 0x05, 'a', 'b' };

    //  base_sz=4, result_sz=10, cmd=0x91 with off=0, sz=20 — copy past
    //  end of base.
    static const u8 D_copy_past_base[] = { 0x04, 0x0a, 0x91, 0x00, 0x14 };

    //  cmd=0 reserved.
    static const u8 D_reserved_zero[] = { 0x00, 0x01, 0x00 };

    bad_case const cases[] = {
        {"oversize-varint",  D_oversize_varint, sizeof(D_oversize_varint), ""},
        {"truncated-copy",   D_truncated_copy,  sizeof(D_truncated_copy),  "0123456789"},
        {"short-insert",     D_short_insert,    sizeof(D_short_insert),    ""},
        {"copy-past-base",   D_copy_past_base,  sizeof(D_copy_past_base),  "abcd"},
        {"reserved-zero",    D_reserved_zero,   sizeof(D_reserved_zero),   "x"},
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        call(bad_apply, &cases[i]);
    }
    done;
}

ok64 maintest() {
    sane(1);
    call(DELTroundtrip);
    call(DELTbackext);
    call(DELTlongrun);
    call(DELTbadcases);
    done;
}

TEST(maintest)
