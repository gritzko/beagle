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

//  Dump a weave's tokens with inrm + bytes to stderr.  Used by
//  failure paths in test cases to localise WEAVE pipeline bugs.
static void weave_dump_tokens(char const *label, weave const *w) {
    fprintf(stderr, "  %s tokens:\n", label);
    weavecur c;
    WEAVECurInit(&c, w);
    u32 i = 0;
    while (WEAVECurNext(&c)) {
        fprintf(stderr, "    [%u] in={", i++);
        for (u32 k = 0; k < c.ni; k++)
            fprintf(stderr, "%08x%s", c.iset[k], k + 1 < c.ni ? "," : "");
        fprintf(stderr, "} rm={");
        for (u32 k = 0; k < c.nr; k++)
            fprintf(stderr, "%08x%s", c.rset[k], k + 1 < c.nr ? "," : "");
        fprintf(stderr, "} \"");
        u8cp tp = (u8cp)c.text[0], te = (u8cp)c.text[1];
        for (u8cp q = tp; q < te && q < tp + 16; q++) {
            u8 ch = *q;
            if (ch >= 0x20 && ch < 0x7f) fputc(ch, stderr);
            else fprintf(stderr, "\\x%02x", ch);
        }
        fprintf(stderr, "\"\n");
    }
}

//  Walk a weave and reproduce its alive byte stream into `out`.
static ok64 weave_repro_alive(u8bp out, weave const *w) {
    sane(out && w);
    call(WEAVEAliveBytes, w, out);
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

//  Find the first ALIVE token whose hashlet is `h`; copy its I-set into
//  out[0..cap) and return the element count (0 if not found).
static u32 weave_alive_iset(weave const *w, u64 h, u32 *out, u32 cap) {
    weavecur c;
    WEAVECurInit(&c, w);
    while (WEAVECurNext(&c)) {
        if (c.nr != 0) continue;
        if (RAPHash(c.text) == h) {
            u32 nn = c.ni < cap ? c.ni : cap;
            for (u32 i = 0; i < nn; i++) out[i] = c.iset[i];
            return nn;
        }
    }
    return 0;
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

    //  Sanity: each post-WEAVEDiff weave's alive bytes must equal
    //  its source blob (a's alive == adata, b's alive == bdata).
    //  Catches bugs in WEAVEDiff before they propagate into the
    //  WEAVEMerge assertions below.
    call(weave_repro_alive, outbuf, &wa);
    {
        u8s wa_got = {u8bDataHead(outbuf),
                      u8bDataHead(outbuf) + u8bDataLen(outbuf)};
        if (!weave_slice_eq_lit(wa_got, c->a)) {
            fprintf(stderr,
                    " FAIL: WEAVEDiff(wo,a) alive != a\n"
                    "  got:  '%.*s'\n  want: '%s'\n",
                    (int)$len(wa_got), (char *)wa_got[0], c->a);
            fail(TESTFAIL);
        }
    }
    call(weave_repro_alive, outbuf, &wb);
    {
        u8s wb_got = {u8bDataHead(outbuf),
                      u8bDataHead(outbuf) + u8bDataLen(outbuf)};
        if (!weave_slice_eq_lit(wb_got, c->b)) {
            fprintf(stderr,
                    " FAIL: WEAVEDiff(wo,b) alive != b\n"
                    "  got:  '%.*s'\n  want: '%s'\n",
                    (int)$len(wb_got), (char *)wb_got[0], c->b);
            fail(TESTFAIL);
        }
    }

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
            weave_dump_tokens("wo", &wo);
            weave_dump_tokens("wa (post WEAVEDiff)", &wa);
            weave_dump_tokens("wb (post WEAVEDiff)", &wb);
            weave_dump_tokens("wm (post WEAVEMerge)", &wm);
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

    //  Pick a known token (the literal 'x') and verify its provenance.
    //  Concurrent identical inserts off two independent blobs (src=A,B)
    //  collapse to ONE alive token whose I-set carries BOTH inserters,
    //  sorted {A,B} (see graf/WEAVE.c WEAVEMerge union branch and
    //  graf/test/WEAVE2 dedup_merge_post_fork).
    WEAVE_CSV(xtok, "x");
    u64 h = RAPHash(xtok);
    u32 iset[8];
    u32 ni = weave_alive_iset(&wm, h, iset, 8);
    u32 lo = (WEAVE_TEST_A < WEAVE_TEST_B) ? WEAVE_TEST_A : WEAVE_TEST_B;
    u32 hi = (WEAVE_TEST_A < WEAVE_TEST_B) ? WEAVE_TEST_B : WEAVE_TEST_A;
    if (ni != 2 || iset[0] != lo || iset[1] != hi) {
        fprintf(stderr,
                " FAIL: 'x' I-set must be {%08x,%08x} (both inserters), "
                "got %u elems\n", lo, hi, ni);
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

    //  Repeated-token LCS ambiguity + theirs DEL-then-INS + ours
    //  tail-append.  Reduced repro of the test/patch/19-feature-stack-
    //  rebase iter-2 failure.  Base has two `"0"` tokens (both with
    //  in=0).  Theirs replaces the second `"0"` with `"f(0)"` (DEL
    //  the `"0"` + INS `"f("`, `"0"`, `")"`).  Ours appends `"TAG\n"`
    //  at end-of-file.  Correct merged alive bytes:
    //  `"a=0;\nb=f(0);\nTAG\n"` — theirs's replacement applies, ours's
    //  tail preserved.  Pre-fix WEAVE drops theirs's DEL and emits
    //  ours's `"b=0;\n"` alive alongside theirs's `"f(0);\n"`.
    {"del_ins_plus_tail_repeats",
     "a=0;\nb=0;\n",
     "a=0;\nb=0;\nTAG\n",
     "a=0;\nb=f(0);\n",
     "a=0;\nb=f(0);\nTAG\n", NULL, NULL},

    //  Same shape as above but with the DEL-then-INS landing inside a
    //  function body (`{ }` brace pair around the changed token), and
    //  one extra base zone declaration to broaden the LCS-ambiguity
    //  surface.  Closer mirror of test/patch/19-feature-stack-rebase
    //  iter 2: base has 3 zones (a=0, b=0, c=0) plus a `main(){return
    //  0;}` line; theirs replaces main's `"0"` with `"f(0)"`; ours
    //  appends a comment line.  Correct merged alive bytes have
    //  theirs's replacement applied AND ours's tail preserved with no
    //  duplicate spine.
    {"del_ins_in_func_plus_tail_zones",
     "a=0;\nb=0;\nc=0;\nm(){return 0;}\n",
     "a=0;\nb=0;\nc=0;\nm(){return 0;}\nT\n",
     "a=0;\nb=0;\nc=0;\nm(){return f(0);}\n",
     "a=0;\nb=0;\nc=0;\nm(){return f(0);}\nT\n", NULL, NULL},

    {NULL, NULL, NULL, NULL, NULL, NULL, NULL},
};

// --- Multi-version ours-chain replay vs single-version theirs ---------
//
//  Regression guard for the patch-15 "transposed inserted line vs spine
//  blank" merge bug (test/patch/15-ancestor-skip).  Unlike the 2-layer
//  `cases` table (single base->a, base->b), this drives the SQUASH 3-way
//  shape that GET.c::build_tip_weave_tunable builds: the ours side is one
//  weave accumulated by WEAVEFromBlob(v0) then a chain of WEAVEDiff over
//  an ordered list of blob versions (each with its own src id), and the
//  intermediate list deliberately reverts mid-chain so the accumulated
//  weave carries DEAD-DUPLICATE regions (dead `sub`/`mul`-style blocks
//  with their own inrm).  Theirs is a single diff off the same NCA base.
//
//  The reduced analog mirrors the fixtures' geometry: a code line `m;`,
//  a blank spine line, another code line `g;`; ours inserts `s;`,`u;`,
//  `d;` after `m;` (mirroring sub/mul/divmod) with a revert to t0 in the
//  middle; theirs changes `g;`->`G;`.  The invariant the bug violated:
//  the last inserted code line `d;` must land BEFORE the spine blank in
//  the merged alive stream — never transposed after it.
//
//  NOTE: with the current (fixed) WEAVE.c + WEAVEDiff this passes for any
//  replay order; the actual transposition required a dead-blank RESTAMP
//  that only DAG's foster-descent replay-order construction produced
//  (graf/DAG.c, the fix site).  This guard pins the observable WEAVE-layer
//  invariant so a regression in the replay-accumulate/merge path that let
//  a dead-duplicate leak would be caught here.
static ok64 weave_test_replay_insert_order(void) {
    sane(1);
    fprintf(stderr, "  replay_insert_order...");
    WEAVE_CSV(ext, "c");

    //  Reduced patch-15 analog.  Ours-chain replay order is BAD on
    //  purpose: t0 -> +s -> revert(t0) -> +s,u,d  (the revert makes a
    //  dead-duplicate region in the accumulated ours weave).
    char const *vers[] = {
        "m;\n\ng;\n",            // v0 (NCA base, src=0)
        "m;\ns;\n\ng;\n",        // +s;
        "m;\n\ng;\n",            // revert -> dead-dup
        "m;\ns;\nu;\nd;\n\ng;\n",// +s;u;d;
    };
    u32 nvers = (u32)(sizeof(vers) / sizeof(vers[0]));
    char const *theirs   = "m;\n\nG;\n";              // g; -> G;
    char const *expected = "m;\ns;\nu;\nd;\n\nG;\n";  // d; before blank

    weave wo = {}, wA = {}, wB = {}, wnu = {}, wt = {}, wtn = {}, wm = {};
    Bu8 outbuf = {};
    call(WEAVEInit, &wo);
    call(WEAVEInit, &wA);
    call(WEAVEInit, &wB);
    call(WEAVEInit, &wnu);
    call(WEAVEInit, &wt);
    call(WEAVEInit, &wtn);
    call(WEAVEInit, &wm);
    call(u8bMap, outbuf, 1UL << 16);

    //  Shared NCA base weave (src=0 spine), reused for ours and theirs.
    { WEAVE_CSV(v0, vers[0]); call(WEAVEFromBlob, &wo, v0, ext, WEAVE_TEST_BASE); }

    //  ours: accumulate over the (bad-order) version list, mirroring
    //  build_tip_weave_tunable's src/dst/nu swap with a distinct src per
    //  version so any restamp/collision could surface.
    weave *wsrc = &wA, *wdst = &wB;
    { WEAVE_CSV(v0, vers[0]); call(WEAVEFromBlob, wsrc, v0, ext, WEAVE_TEST_BASE); }
    for (u32 i = 1; i < nvers; i++) {
        WEAVE_CSV(vd, vers[i]);
        u32 sc = 0x10000000u + i;
        WEAVEReset(&wnu);
        call(WEAVEFromBlob, &wnu, vd, ext, sc);
        call(WEAVEDiff, wdst, wsrc, &wnu, sc);
        weave *t = wsrc; wsrc = wdst; wdst = t;
    }

    //  theirs: single diff off the shared base.
    { WEAVE_CSV(td, theirs); call(WEAVEFromBlob, &wtn, td, ext, WEAVE_TEST_B); }
    call(WEAVEDiff, &wt, &wo, &wtn, WEAVE_TEST_B);

    call(WEAVEMerge, &wm, wsrc, &wt);
    call(weave_repro_alive, outbuf, &wm);
    u8s got = {u8bDataHead(outbuf),
               u8bDataHead(outbuf) + u8bDataLen(outbuf)};
    if (!weave_slice_eq_lit(got, expected)) {
        fprintf(stderr,
                " FAIL: replay merge mismatch\n  got:  '%.*s'\n  want: '%s'\n",
                (int)$len(got), (char *)got[0], expected);
        weave_dump_tokens("wm", &wm);
        fail(TESTFAIL);
    }
    //  Explicit transposition guard: `d;` line must precede the spine
    //  blank, and the blank must not appear before `d;`.
    if (!weave_contains(got, "d;\n\n")) {
        fprintf(stderr, " FAIL: inserted 'd;' not immediately before blank\n");
        fail(TESTFAIL);
    }

    u8bUnMap(outbuf);
    WEAVEFree(&wo);
    WEAVEFree(&wA); WEAVEFree(&wB); WEAVEFree(&wnu);
    WEAVEFree(&wt); WEAVEFree(&wtn); WEAVEFree(&wm);
    fprintf(stderr, " ok\n");
    done;
}

ok64 WEAVEtest(void) {
    sane(1);

    call(weave_test_self, "int x = 1;\nint y = 2;\n");
    call(weave_test_self, "");
    call(weave_test_pre_lca);
    call(weave_test_replay_insert_order);

    for (WEAVECase *c = cases; c->name != NULL; c++) {
        call(weave_run, c);
    }

    done;
}

TEST(WEAVEtest);
