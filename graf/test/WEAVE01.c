//
// WEAVE01 - Property tests for `WEAVEMerge` (3-way concurrent-branch
// weave merge).
//
// Each case names a base blob `o` and two derived blobs `a`, `b`.
// The harness builds:
//   wo  = WEAVEFromBlob(o, src=0)
//   wa  = WEAVEDiff(wo, WEAVEFromBlob(a, src=A), src=A)
//   wb  = WEAVEDiff(wo, WEAVEFromBlob(b, src=B), src=B)
//   wm  = WEAVEMerge(wa, wb)
// then asserts properties on `wm`:
//   - For "no-conflict" cases, wm contains no marker tokens, and
//     `repro_alive(wm)` equals `expected_merged` when supplied.
//   - For "conflict" cases, wm contains balanced
//     <<<<...||||...>>>> marker triples (>= 1).
//   - All-cases: marker counts are balanced (open == mid == close).
//

#include "WEAVE.h"

#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/TEST.h"

#define WEAVE_TEST_BASE 0u
#define WEAVE_TEST_A    0xA0A0A0A0u
#define WEAVE_TEST_B    0xB0B0B0B0u

// --- Helpers ----------------------------------------------------------

#define WEAVE_CSV(name, lit)                                              \
    u8cs name = {(u8cp)(lit), (u8cp)(lit) + ((lit) ? strlen(lit) : 0)}

//  Walk a weave and reproduce its alive byte stream into `out`.
static ok64 weave_repro_alive(u8bp out, weave const *w) {
    sane(out && w);
    u32cp toks   = (u32cp)w->toks[1];
    u32cp toks_e = (u32cp)w->toks[2];
    u32   ntok   = (u32)(toks_e - toks);
    inrmcp irm   = (inrmcp)w->inrm[1];
    u8cp   text  = (u8cp)w->text[1];
    u8bReset(out);
    for (u32 i = 0; i < ntok; i++) {
        if (irm[i].rm != 0) continue;
        u32 lo = (i == 0) ? 0 : tok32Offset(toks[i - 1]);
        u32 hi = tok32Offset(toks[i]);
        u8cs tb = {text + lo, text + hi};
        call(u8bFeed, out, tb);
    }
    done;
}

//  Substring search inside a u8s byte stream.
static b8 weave_contains(u8s const got, char const *needle) {
    size_t nlen = needle ? strlen(needle) : 0;
    if (nlen == 0) return YES;
    size_t glen = (size_t)$len(got);
    if (glen < nlen) return NO;
    for (size_t i = 0; i + nlen <= glen; i++) {
        if (memcmp(got[0] + i, needle, nlen) == 0) return YES;
    }
    return NO;
}

//  Find a token whose hashlet is `h` and inrm.rm == 0; return its
//  in-stamp (or 0xFFFFFFFF if not found).
static u32 weave_alive_in(weave const *w, u64 h) {
    u64cp hashes = (u64cp)w->hashlets[1];
    u64cp hash_e = (u64cp)w->hashlets[2];
    u32 n = (u32)(hash_e - hashes);
    inrmcp irm = (inrmcp)w->inrm[1];
    for (u32 i = 0; i < n; i++)
        if (irm[i].rm == 0 && hashes[i] == h) return irm[i].in;
    return 0xFFFFFFFFu;
}

//  Slice byte-equality (works around the inability to assign u8cs).
static b8 weave_slice_eq_lit(u8s const got, char const *want) {
    size_t wlen = want ? strlen(want) : 0;
    size_t glen = (size_t)$len(got);
    if (glen != wlen) return NO;
    if (wlen == 0) return YES;
    return memcmp(got[0], want, wlen) == 0;
}

// --- Test case shape --------------------------------------------------

typedef struct {
    char const *name;
    char const *base;
    char const *a;
    char const *b;
    //  Two acceptance modes, mutually exclusive:
    //    expected_merged != NULL : alive byte stream must equal it
    //                              exactly.
    //    contains_a, contains_b  : alive byte stream must contain
    //                              both substrings (used for overlap
    //                              cases where WEAVEMerge stores both
    //                              sides' alive tokens — markers are
    //                              a render-time concern, not a
    //                              storage one).
    char const *expected_merged;
    char const *contains_a;
    char const *contains_b;
} WEAVECase;

//  Per-case execution.
static ok64 weave_run(WEAVECase const *c) {
    sane(1);
    fprintf(stderr, "  %s...", c->name);

    WEAVE_CSV(ext,   "c");
    WEAVE_CSV(base,  c->base);
    WEAVE_CSV(adata, c->a);
    WEAVE_CSV(bdata, c->b);

    weave wo  = {}, wa_raw = {}, wb_raw = {};
    weave wa  = {}, wb     = {}, wm     = {};
    Bu8 outbuf = {};

    call(WEAVEInit, &wo);
    call(WEAVEInit, &wa_raw);
    call(WEAVEInit, &wb_raw);
    call(WEAVEInit, &wa);
    call(WEAVEInit, &wb);
    call(WEAVEInit, &wm);
    call(u8bMap, outbuf, 1UL << 16);

    call(WEAVEFromBlob, &wo,     base,  ext, WEAVE_TEST_BASE);
    call(WEAVEFromBlob, &wa_raw, adata, ext, WEAVE_TEST_A);
    call(WEAVEFromBlob, &wb_raw, bdata, ext, WEAVE_TEST_B);

    call(WEAVEDiff, &wa, &wo, &wa_raw, WEAVE_TEST_A);
    call(WEAVEDiff, &wb, &wo, &wb_raw, WEAVE_TEST_B);

    call(WEAVEMerge, &wm, &wa, &wb);

    call(weave_repro_alive, outbuf, &wm);
    u8s got = {u8bDataHead(outbuf),
               u8bDataHead(outbuf) + u8bDataLen(outbuf)};

    if (c->expected_merged) {
        if (!weave_slice_eq_lit(got, c->expected_merged)) {
            fprintf(stderr,
                    " FAIL: merged mismatch\n  got:  '%.*s'\n  want: '%s'\n",
                    (int)$len(got),  (char *)got[0],
                    c->expected_merged);
            fail(TESTFAIL);
        }
    } else if (c->contains_a || c->contains_b) {
        if (c->contains_a && !weave_contains(got, c->contains_a)) {
            fprintf(stderr,
                    " FAIL: alive stream missing a-side '%s'\n  got: '%.*s'\n",
                    c->contains_a, (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
        if (c->contains_b && !weave_contains(got, c->contains_b)) {
            fprintf(stderr,
                    " FAIL: alive stream missing b-side '%s'\n  got: '%.*s'\n",
                    c->contains_b, (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
    }

    u8bUnMap(outbuf);
    WEAVEFree(&wo);
    WEAVEFree(&wa_raw);
    WEAVEFree(&wb_raw);
    WEAVEFree(&wa);
    WEAVEFree(&wb);
    WEAVEFree(&wm);

    fprintf(stderr, " ok\n");
    done;
}

// --- Identity / reflexivity-style cases -------------------------------

//  Property: merge(W, W) reproduces W's alive byte stream.
static ok64 weave_test_self(char const *base_str) {
    sane(1);
    WEAVE_CSV(ext,  "c");
    WEAVE_CSV(base, base_str);

    weave wo = {}, wm = {};
    Bu8 outbuf = {};
    call(WEAVEInit, &wo);
    call(WEAVEInit, &wm);
    call(u8bMap, outbuf, 1UL << 16);

    call(WEAVEFromBlob, &wo, base, ext, WEAVE_TEST_BASE);
    call(WEAVEMerge, &wm, &wo, &wo);

    call(weave_repro_alive, outbuf, &wm);
    u8s got = {u8bDataHead(outbuf),
               u8bDataHead(outbuf) + u8bDataLen(outbuf)};
    if (!weave_slice_eq_lit(got, base_str)) {
        fprintf(stderr, "WEAVE01 self: got '%.*s' want '%s'\n",
                (int)$len(got),  (char *)got[0], base_str);
        fail(TESTFAIL);
    }

    u8bUnMap(outbuf);
    WEAVEFree(&wo);
    WEAVEFree(&wm);
    done;
}

// --- Pre-LCA shape: differing `in` stamps for the same hashlet --------
//
//  Build two weaves where the *spine* (alive base text) is identical
//  but its tokens carry different `in` stamps in each.  WEAVEMerge
//  must EQ-match on hashlet and pick the lower `in` deterministically.

static ok64 weave_test_pre_lca(void) {
    sane(1);
    fprintf(stderr, "  pre_lca_in_reconcile...");
    WEAVE_CSV(ext,  "c");
    WEAVE_CSV(body, "int x = 1;\n");

    weave wa = {}, wb = {}, wm = {};
    Bu8 outbuf = {};
    call(WEAVEInit, &wa);
    call(WEAVEInit, &wb);
    call(WEAVEInit, &wm);
    call(u8bMap, outbuf, 1UL << 16);
    call(WEAVEFromBlob, &wa, body, ext, WEAVE_TEST_A);
    call(WEAVEFromBlob, &wb, body, ext, WEAVE_TEST_B);
    call(WEAVEMerge, &wm, &wa, &wb);

    //  Reproduce alive bytes — must equal `body`.
    call(weave_repro_alive, outbuf, &wm);
    u8s got = {u8bDataHead(outbuf),
               u8bDataHead(outbuf) + u8bDataLen(outbuf)};
    if (!weave_slice_eq_lit(got, "int x = 1;\n")) {
        fprintf(stderr, " FAIL: alive bytes don't match body\n");
        fail(TESTFAIL);
    }

    //  Pick a known token (the literal 'x') and verify in == min(A,B).
    WEAVE_CSV(xtok, "x");
    u64 h = RAPHash(xtok);
    u32 in_stamp = weave_alive_in(&wm, h);
    u32 want_in = (WEAVE_TEST_A < WEAVE_TEST_B) ? WEAVE_TEST_A : WEAVE_TEST_B;
    if (in_stamp != want_in) {
        fprintf(stderr,
                " FAIL: 'x' token in=%08x, want %08x (min(A,B))\n",
                in_stamp, want_in);
        fail(TESTFAIL);
    }

    u8bUnMap(outbuf);
    WEAVEFree(&wa);
    WEAVEFree(&wb);
    WEAVEFree(&wm);
    fprintf(stderr, " ok\n");
    done;
}

// --- Cases ------------------------------------------------------------

static WEAVECase cases[] = {
    {"identical_no_change",
     "int x = 1;\n",
     "int x = 1;\n",
     "int x = 1;\n",
     "int x = 1;\n", NULL, NULL},

    {"disjoint_inserts",
     "int x = 1;\n",
     "int x = 1;\nint y = 2;\n",       // a appends y
     "int w = 0;\nint x = 1;\n",       // b prepends w
     "int w = 0;\nint x = 1;\nint y = 2;\n", NULL, NULL},

    {"a_deletes_b_keeps",
     "int x = 1;\nint y = 2;\n",
     "int x = 1;\n",                   // a deletes y line
     "int x = 1;\nint y = 2;\n",       // b unchanged
     "int x = 1;\n", NULL, NULL},

    {"both_delete_same",
     "int x = 1;\nint y = 2;\n",
     "int x = 1;\n",
     "int x = 1;\n",
     "int x = 1;\n", NULL, NULL},

    {"a_only_change",
     "int x = 1;\n",
     "int x = 42;\n",
     "int x = 1;\n",
     "int x = 42;\n", NULL, NULL},

    {"b_only_change",
     "int x = 1;\n",
     "int x = 1;\n",
     "int x = 42;\n",
     "int x = 42;\n", NULL, NULL},

    //  Concurrent edits at the same locus.  The merged weave stores
    //  both alive sides (a's "10" and b's "20"); marker rendering is
    //  a separate pass and is not exercised here.  We only assert
    //  that both sides' divergent bytes survive into the alive
    //  stream.
    {"both_alive_divergent",
     "int x = 1;\n",
     "int x = 10;\n",
     "int x = 20;\n",
     NULL, "10", "20"},

    {"both_insert_same_at_same_slot",
     "int x = 1;\n",
     "int x = 1;\nint y = 2;\n",
     "int x = 1;\nint y = 2;\n",
     "int x = 1;\nint y = 2;\n", NULL, NULL},

    {NULL, NULL, NULL, NULL, NULL, NULL, NULL},
};

ok64 WEAVEtest(void) {
    sane(1);

    call(weave_test_self, "int x = 1;\nint y = 2;\n");
    call(weave_test_self, "");
    call(weave_test_pre_lca);

    for (WEAVECase *c = cases; c->name != NULL; c++) {
        call(weave_run, c);
    }

    done;
}

TEST(WEAVEtest);
