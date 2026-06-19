//
//  WEAVE01 (DOG-003) — codec + from-blob round-trip.
//
//  blob -> WEAVENext(NULL) -> 'W' bytes -> WEAVEParse -> WEAVEAlive must
//  reproduce the blob byte-for-byte (a from-blob weave is all-spine/alive),
//  exercising the tokenizer, the 'X'/'K'/'I'/'M'/'C' codec, and the
//  rm-bit-clear alive scan.
//
#include "dog/WEAVE.h"

#include <stdio.h>
#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

static ok64 rt_case(char const *name, u8csc blob, u8csc ext) {
    sane(1);
    fprintf(stderr, "  %s ...", name);
    a_carve(u8, wbuf, 1UL << 20);
    call(WEAVENext, u8bIdle(wbuf), NULL, blob, ext, 0xC0FFEEu);
    weave w = {};
    call(WEAVEParse, &w, u8bDataC(wbuf));
    a_carve(u8, alive, 1UL << 20);
    call(WEAVEAlive, &w, alive);
    size_t glen = u8bDataLen(alive);
    if (glen != (size_t)$len(blob) ||
        (glen && memcmp(u8bDataHead(alive), blob[0], glen) != 0)) {
        fprintf(stderr, " ALIVE mismatch: got %zu of %ld bytes\n",
                glen, (long)$len(blob));
        fail(TESTFAIL);
    }
    fprintf(stderr, " ok\n");
    done;
}

typedef struct { char const *name; char const *blob; } RTcase;

static RTcase cases[] = {
    {"empty",        ""},
    {"one_token",    "abc"},
    {"one_line",     "int x = 1;\n"},
    {"no_trailing",  "int x = 1;"},
    {"multi_line",   "a\nb\nc\n"},
    {"c_snippet",    "int main(void){\n    return 0;\n}\n"},
    {"blank_lines",  "\n\n\nx\n"},
};

//  Scope-classified produce: WEAVEProduce over the bitmap of `active`
//  commit-ids must reproduce `want`.
static ok64 prod_check(char const *lbl, weave *w, u64cs active, u8csc want) {
    sane(1);
    //  Acquire DIRECTLY, never via call() — call() snapshots+rewinds BASS and
    //  would free the acquisition (api.mkd hazard), so `out` below would alias
    //  this scope and clobber it mid-produce.
    u1b sc = {};
    ok64 ar = u1bAcquire(ABC_BASS, &sc, (size_t)$len(w->commits) + 1);
    if (ar != OK) return ar;
    call(WEAVEScope, &sc, w, active);
    a_carve(u8, out, 1UL << 16);
    call(WEAVEProduce, w, u1bDataC(&sc), out);
    if (u8bDataLen(out) != (size_t)$len(want) ||
        ($len(want) && memcmp(u8bDataHead(out), want[0], (size_t)$len(want)))) {
        fprintf(stderr, " %s mismatch (%zu vs %ld)\n", lbl,
                u8bDataLen(out), (long)$len(want));
        fail(TESTFAIL);
    }
    done;
}

//  Diff fold: fold v2 onto v1.  Tip must be v2; rev {c1} reproduces v1;
//  rev {c1,c2} reproduces v2 — exercising WEAVENext's diff path, the
//  (commit,ordinal) identity, and scope-classified WEAVEProduce.
static ok64 diff_scenario(void) {
    sane(1);
    fprintf(stderr, "  diff_fold ...");
    a_cstr(cext, "c");
    u8csc v1 = {(u8c *)"a\nb\nc\n", (u8c *)"a\nb\nc\n" + 6};
    u8csc v2 = {(u8c *)"a\nB\nc\n", (u8c *)"a\nB\nc\n" + 6};
    a_carve(u8, w1b, 1UL << 16);
    call(WEAVENext, u8bIdle(w1b), NULL, v1, cext, 1);
    weave w1 = {};
    call(WEAVEParse, &w1, u8bDataC(w1b));
    a_carve(u8, w2b, 1UL << 16);
    call(WEAVENext, u8bIdle(w2b), &w1, v2, cext, 2);
    weave w2 = {};
    call(WEAVEParse, &w2, u8bDataC(w2b));
    a_carve(u8, al, 1UL << 16);
    call(WEAVEAlive, &w2, al);
    if (u8bDataLen(al) != 6 || memcmp(u8bDataHead(al), v2[0], 6)) {
        fprintf(stderr, " tip mismatch\n");
        fail(TESTFAIL);
    }
    a_carve(u64, a1, 1);
    call(u64bFeed1, a1, 1);
    call(prod_check, "rev1", &w2, u64bDataC(a1), v1);
    a_carve(u64, a2, 2);
    call(u64bFeed1, a2, 1);
    call(u64bFeed1, a2, 2);
    call(prod_check, "rev2", &w2, u64bDataC(a2), v2);
    fprintf(stderr, " ok\n");
    done;
}

//  Fork/merge: base v0; branch A edits line 1, branch B edits line 3
//  (disjoint).  WEAVEMerge(A,B) tip must combine both ("A\nb\nC\n"), and
//  each rev-scope must reproduce its branch — the DIS-003 identity union.
static ok64 merge_scenario(void) {
    sane(1);
    fprintf(stderr, "  merge_fold ...");
    a_cstr(cext, "c");
    u8csc v0 = {(u8c *)"a\nb\nc\n", (u8c *)"a\nb\nc\n" + 6};
    u8csc vA = {(u8c *)"A\nb\nc\n", (u8c *)"A\nb\nc\n" + 6};
    u8csc vB = {(u8c *)"a\nb\nC\n", (u8c *)"a\nb\nC\n" + 6};
    u8csc vM = {(u8c *)"A\nb\nC\n", (u8c *)"A\nb\nC\n" + 6};
    a_carve(u8, w0b, 1UL << 16);  call(WEAVENext, u8bIdle(w0b), NULL, v0, cext, 1);
    weave w0 = {}; call(WEAVEParse, &w0, u8bDataC(w0b));
    a_carve(u8, wAb, 1UL << 16);  call(WEAVENext, u8bIdle(wAb), &w0, vA, cext, 2);
    weave wA = {}; call(WEAVEParse, &wA, u8bDataC(wAb));
    a_carve(u8, wBb, 1UL << 16);  call(WEAVENext, u8bIdle(wBb), &w0, vB, cext, 3);
    weave wB = {}; call(WEAVEParse, &wB, u8bDataC(wBb));
    a_carve(u8, wMb, 1UL << 16);  call(WEAVEMerge, u8bIdle(wMb), &wA, &wB, 4);
    weave wM = {}; call(WEAVEParse, &wM, u8bDataC(wMb));
    a_carve(u8, al, 1UL << 16);   call(WEAVEAlive, &wM, al);
    if (u8bDataLen(al) != 6 || memcmp(u8bDataHead(al), vM[0], 6)) {
        fprintf(stderr, " merge tip mismatch (%zu) got=", u8bDataLen(al));
        { u8c *g = u8bDataHead(al);
          for (size_t k = 0; k < u8bDataLen(al); k++) fprintf(stderr, "%02x", g[k]); }
        fprintf(stderr, "\n"); fail(TESTFAIL);
    }
    a_carve(u64, sc0, 1); call(u64bFeed1, sc0, 1);
    call(prod_check, "merge-rev0", &wM, u64bDataC(sc0), v0);
    a_carve(u64, scA, 2); call(u64bFeed1, scA, 1); call(u64bFeed1, scA, 2);
    call(prod_check, "merge-revA", &wM, u64bDataC(scA), vA);
    a_carve(u64, scB, 2); call(u64bFeed1, scB, 1); call(u64bFeed1, scB, 3);
    call(prod_check, "merge-revB", &wM, u64bDataC(scB), vB);
    fprintf(stderr, " ok\n");
    done;
}

//  DIS-003 fuzz repro (dog/fuzz DWEAVE, crash-8d679523, $HOME/Corpus/DWEAVE).
//  Root "_88888" forks to single-byte branches "a" / "j" that each delete the
//  whole root, so the merge leaves root tokens with TWO removers {A,B}.
//  Folding a third revision "b" onto that merge once overflowed wnext_diff's
//  rms column — it budgeted 8 bytes/token assuming a single remover, but a
//  merged token needs `count + N indices`.  WEAVENext returned NOROOM.  Each
//  byte is its own line so repeated bytes collide as identical tokens (the
//  positional ambiguity that exposed the holdout), and every rev must still
//  recover its own content under the multi-remover merge.
static ok64 merge_fold_scenario(void) {
    sane(1);
    fprintf(stderr, "  merge_fold_multirm ...");
    a_cstr(cext, "c");
    static char const c0[] = "_\n8\n8\n8\n8\n8\n";   // lineform("_88888")
    static char const cA[] = "a\n", cB[] = "j\n", cC[] = "b\n";
    u8csc v0 = {(u8c *)c0, (u8c *)c0 + sizeof(c0) - 1};
    u8csc vA = {(u8c *)cA, (u8c *)cA + sizeof(cA) - 1};
    u8csc vB = {(u8c *)cB, (u8c *)cB + sizeof(cB) - 1};
    u8csc vC = {(u8c *)cC, (u8c *)cC + sizeof(cC) - 1};
    a_carve(u8, w0b, 1UL << 16);  call(WEAVENext, u8bIdle(w0b), NULL, v0, cext, 1);
    weave w0 = {}; call(WEAVEParse, &w0, u8bDataC(w0b));
    a_carve(u8, wAb, 1UL << 16);  call(WEAVENext, u8bIdle(wAb), &w0, vA, cext, 2);
    weave wA = {}; call(WEAVEParse, &wA, u8bDataC(wAb));
    a_carve(u8, wBb, 1UL << 16);  call(WEAVENext, u8bIdle(wBb), &w0, vB, cext, 3);
    weave wB = {}; call(WEAVEParse, &wB, u8bDataC(wBb));
    a_carve(u8, wMb, 1UL << 16);  call(WEAVEMerge, u8bIdle(wMb), &wA, &wB, 0);
    weave wM = {}; call(WEAVEParse, &wM, u8bDataC(wMb));
    //  fold a third revision onto the multi-remover merge (the overflow path)
    a_carve(u8, wCb, 1UL << 16);  call(WEAVENext, u8bIdle(wCb), &wM, vC, cext, 4);
    weave wC = {}; call(WEAVEParse, &wC, u8bDataC(wCb));
    a_carve(u8, al, 1UL << 16);   call(WEAVEAlive, &wC, al);
    if (u8bDataLen(al) != 2 || memcmp(u8bDataHead(al), vC[0], 2)) {
        fprintf(stderr, " tip mismatch\n"); fail(TESTFAIL);
    }
    a_carve(u64, s0, 1); call(u64bFeed1, s0, 1);
    call(prod_check, "mf-rev0", &wC, u64bDataC(s0), v0);
    a_carve(u64, sA, 2); call(u64bFeed1, sA, 1); call(u64bFeed1, sA, 2);
    call(prod_check, "mf-revA", &wC, u64bDataC(sA), vA);
    a_carve(u64, sB, 2); call(u64bFeed1, sB, 1); call(u64bFeed1, sB, 3);
    call(prod_check, "mf-revB", &wC, u64bDataC(sB), vB);
    a_carve(u64, sC, 4);
    call(u64bFeed1, sC, 1); call(u64bFeed1, sC, 2);
    call(u64bFeed1, sC, 3); call(u64bFeed1, sC, 4);
    call(prod_check, "mf-revC", &wC, u64bDataC(sC), vC);
    fprintf(stderr, " ok\n");
    done;
}

ok64 WEAVErttest() {
    sane(1);
    a_cstr(cext, "c");
    u32 n = (u32)(sizeof(cases) / sizeof(cases[0]));
    for (u32 i = 0; i < n; i++) {
        u8csc blob = {(u8c *)cases[i].blob,
                      (u8c *)cases[i].blob + strlen(cases[i].blob)};
        call(rt_case, cases[i].name, blob, cext);
    }
    call(diff_scenario);
    call(merge_scenario);
    call(merge_fold_scenario);
    done;
}

TEST(WEAVErttest);
