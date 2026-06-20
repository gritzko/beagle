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

//  DIS-044: the RGA tie-break must be a CAUSAL rank, not the raw commit-id.
//  Real 60-bit commit hashlets are arbitrary, so a descendant edit may have a
//  SMALLER id than its base (spine).  When it does, the old commit-id-DESC
//  tie-break stranded a replace-edit's inserted token behind the dead
//  original (`int x = 10` -> `int x = ;` ... `10` at EOF) and let a same-line
//  divergence splice clean instead of conflicting.  These cases assign ids so
//  the spine is ABOVE the edits (base > ours/theirs) — the exact toggle that
//  hides the bug under the monotonic line-index oracle.  Same content, same
//  topology as merge_scenario; only the ids differ, so a RED here is purely
//  the order-key bug, GREEN once the tie-break is the causal commit index.
static ok64 spine_above_replace(void) {
    sane(1);
    fprintf(stderr, "  spine_above_replace ...");
    a_cstr(cext, "c");
    //  spine id 9000 > edit ids 2/3.  Ours replaces line 2 (X->O); theirs
    //  replaces line 3 (c->C) — DISJOINT, so the merge is a clean splice.  The
    //  replace-edit on line 2 is the stranding trigger: pre-fix its inserted
    //  'O' lands behind the dead 'X' subtree at EOF.
    u8csc v0 = {(u8c *)"a\nX\nc\n", (u8c *)"a\nX\nc\n" + 6};
    u8csc vA = {(u8c *)"a\nO\nc\n", (u8c *)"a\nO\nc\n" + 6};   // ours   X->O
    u8csc vB = {(u8c *)"a\nX\nC\n", (u8c *)"a\nX\nC\n" + 6};   // theirs c->C (disjoint)
    a_carve(u8, w0b, 1UL << 16);  call(WEAVENext, u8bIdle(w0b), NULL, v0, cext, 9000);
    weave w0 = {}; call(WEAVEParse, &w0, u8bDataC(w0b));
    a_carve(u8, wAb, 1UL << 16);  call(WEAVENext, u8bIdle(wAb), &w0, vA, cext, 2);
    weave wA = {}; call(WEAVEParse, &wA, u8bDataC(wAb));
    a_carve(u8, wBb, 1UL << 16);  call(WEAVENext, u8bIdle(wBb), &w0, vB, cext, 3);
    weave wB = {}; call(WEAVEParse, &wB, u8bDataC(wBb));
    a_carve(u8, wMb, 1UL << 16);  call(WEAVEMerge, u8bIdle(wMb), &wA, &wB, 0);
    weave wM = {}; call(WEAVEParse, &wM, u8bDataC(wMb));
    //  ours O (line 2) + theirs C (line 3): a clean disjoint merge "a\nO\nC\n".
    u8csc vMrep = {(u8c *)"a\nO\nC\n", (u8c *)"a\nO\nC\n" + 6};
    a_carve(u8, al, 1UL << 16);   call(WEAVEAlive, &wM, al);
    if (u8bDataLen(al) != 6 || memcmp(u8bDataHead(al), vMrep[0], 6)) {
        fprintf(stderr, " replace tip mismatch (%zu) got=", u8bDataLen(al));
        { u8c *g = u8bDataHead(al);
          for (size_t k = 0; k < u8bDataLen(al); k++)
              fputc(g[k] == '\n' ? '.' : g[k], stderr); }
        fprintf(stderr, "\n"); fail(TESTFAIL);
    }
    //  Every scope must still recover its own content (the stranded-token bug
    //  desyncs base recovery: "a\nX\nc\n" -> "a\n\nc\nX\n" pre-fix).
    a_carve(u64, sc0, 1); call(u64bFeed1, sc0, 9000);
    call(prod_check, "rep-base", &wM, u64bDataC(sc0), v0);
    a_carve(u64, scA, 2); call(u64bFeed1, scA, 9000); call(u64bFeed1, scA, 2);
    call(prod_check, "rep-ours", &wM, u64bDataC(scA), vA);
    a_carve(u64, scB, 2); call(u64bFeed1, scB, 9000); call(u64bFeed1, scB, 3);
    call(prod_check, "rep-theirs", &wM, u64bDataC(scB), vB);
    fprintf(stderr, " ok\n");
    done;
}

//  DIS-044: a same-line divergence with the spine ABOVE both edits must keep
//  BOTH concurrent tokens (a conflict the renderer frames), never splice one
//  away.  Verified structurally: producing each branch's scope recovers its
//  OWN edit, and the merged tip carries both inserted bytes.
static ok64 spine_above_diverge(void) {
    sane(1);
    fprintf(stderr, "  spine_above_diverge ...");
    a_cstr(cext, "c");
    u8csc v0 = {(u8c *)"a\nX\nc\n", (u8c *)"a\nX\nc\n" + 6};
    u8csc vA = {(u8c *)"a\nO\nc\n", (u8c *)"a\nO\nc\n" + 6};   // ours  X->O
    u8csc vB = {(u8c *)"a\nT\nc\n", (u8c *)"a\nT\nc\n" + 6};   // theirs X->T (same line!)
    a_carve(u8, w0b, 1UL << 16);  call(WEAVENext, u8bIdle(w0b), NULL, v0, cext, 9000);
    weave w0 = {}; call(WEAVEParse, &w0, u8bDataC(w0b));
    a_carve(u8, wAb, 1UL << 16);  call(WEAVENext, u8bIdle(wAb), &w0, vA, cext, 2);
    weave wA = {}; call(WEAVEParse, &wA, u8bDataC(wAb));
    a_carve(u8, wBb, 1UL << 16);  call(WEAVENext, u8bIdle(wBb), &w0, vB, cext, 3);
    weave wB = {}; call(WEAVEParse, &wB, u8bDataC(wBb));
    a_carve(u8, wMb, 1UL << 16);  call(WEAVEMerge, u8bIdle(wMb), &wA, &wB, 0);
    weave wM = {}; call(WEAVEParse, &wM, u8bDataC(wMb));
    //  Both edits survive as concurrent siblings: ours-scope -> O, theirs -> T,
    //  base -> X.  A splice would drop one branch's byte from recovery.
    a_carve(u64, sc0, 1); call(u64bFeed1, sc0, 9000);
    call(prod_check, "div-base", &wM, u64bDataC(sc0), v0);
    a_carve(u64, scA, 2); call(u64bFeed1, scA, 9000); call(u64bFeed1, scA, 2);
    call(prod_check, "div-ours", &wM, u64bDataC(scA), vA);
    a_carve(u64, scB, 2); call(u64bFeed1, scB, 9000); call(u64bFeed1, scB, 3);
    call(prod_check, "div-theirs", &wM, u64bDataC(scB), vB);
    //  The merged tip must still hold BOTH 'O' and 'T' bytes (both alive, both
    //  concurrent) — a splice would leave only one.
    a_carve(u8, al, 1UL << 16);   call(WEAVEAlive, &wM, al);
    b8 haveO = NO, haveT = NO;
    { u8c *g = u8bDataHead(al);
      for (size_t k = 0; k < u8bDataLen(al); k++) {
          if (g[k] == 'O') haveO = YES; if (g[k] == 'T') haveT = YES; } }
    if (!haveO || !haveT) {
        fprintf(stderr, " diverge spliced: O=%d T=%d\n", haveO, haveT);
        fail(TESTFAIL);
    }
    fprintf(stderr, " ok\n");
    done;
}

// =====================================================================
//  DIS-043 criss-cross: a stable RGA order makes recovery path-independent.
//  A compact DAG-driven harness mirroring dog/fuzz/WEAVE (the real oracle):
//  commit-id = node index; each content byte b -> the line "b\n" (so repeated
//  bytes are IDENTICAL tokens, the positional stress); a node with >=2 parents
//  fold-merges them pairwise then folds its own content; then EVERY ancestor a
//  of node i must recover lineform(content[a]) from w[i] byte-for-byte.  These
//  rows DUPLICATED tokens (e.g. root `888888` -> `8888888888`) before the fix.
// =====================================================================
#define DAG_MAX 32

typedef struct { char const *par; char const *content; } dagnode;
typedef struct { char const *name; dagnode const *nodes; u32 n; } dagcase;

//  DIS-044: per-node commit-id is ARBITRARY / NON-monotonic (SplitMix64 of the
//  node index, == dog/fuzz/WEAVE's w2_cid), so a base/spine routinely outranks
//  its edits — the real-hashlet order the old line-index ids hid.  Both the
//  build and verify/scope sides call this, so commits[]-table membership still
//  matches by value.  A SplitMix64 finalizer is a u64 bijection: distinct
//  nodes get distinct ids, never a false identity collision.
static u64 dag_cid(u32 i) {
    u64 x = (u64)i + 0x9E3779B97F4A7C15ULL;
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27; x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
}

//  ancestor closure of `start` over [0..n): parents always precede.
static void dag_closure(dagnode const *nd, u32 n, u32 start, u8 *anc) {
    (void)n; anc[start] = 1;
    for (u32 i = start + 1; i-- > 0;) {
        if (!anc[i]) continue;
        for (char const *r = nd[i].par; *r; r++) anc[(u8)(*r - '0')] = 1;
    }
}

static ok64 dag_lineform(u8b dst, char const *s) {
    sane(1); u8bReset(dst);
    for (char const *p = s; *p; p++) { call(u8bFeed1, dst, (u8)*p); call(u8bFeed1, dst, (u8)'\n'); }
    done;
}

//  Recover every ancestor of node i from W[i]; FAIL on the first mismatch.
static ok64 dag_verify(weave const *W, dagnode const *nd, u32 n, u32 i) {
    sane(1);
    u8 anc_i[DAG_MAX]; memset(anc_i, 0, n);
    dag_closure(nd, n, i, anc_i);
    u1b sc = {}; ok64 ar = u1bAcquire(ABC_BASS, &sc, (size_t)n + 1); if (ar != OK) return ar;
    a_carve(u8, out, 1UL << 16);
    a_carve(u8, exp, 1UL << 16);
    a_carve(u64, active, (size_t)n + 1);
    for (u32 a = 0; a < n; a++) {
        if (!anc_i[a]) continue;
        u8 anc_a[DAG_MAX]; memset(anc_a, 0, n);
        dag_closure(nd, n, a, anc_a);
        u64bReset(active);
        for (u32 j = 0; j < n; j++) if (anc_a[j]) call(u64bFeed1, active, dag_cid(j));
        call(WEAVEScope, &sc, &W[i], u64bDataC(active));
        call(WEAVEProduce, &W[i], u1bDataC(&sc), out);
        call(dag_lineform, exp, nd[a].content);
        size_t gl = u8bDataLen(out), wl = u8bDataLen(exp);
        if (gl != wl || (wl && memcmp(u8bDataHead(out), u8bDataHead(exp), wl))) {
            fprintf(stderr, " w[%u] rev=%u want(%zu) got(%zu)\n", i, a, wl, gl);
            fail(TESTFAIL);
        }
    }
    done;
}

//  Build every node's weave inline (BASS acquires must survive — call() would
//  rewind them), then verify recovery for all nodes.
static ok64 dag_run(dagcase const *dc) {
    sane(1);
    fprintf(stderr, "  %s ...", dc->name);
    a_cstr(cext, "c");
    must(dc->n <= DAG_MAX, "dag too big");
    u8 *wbuf[DAG_MAX][4] = {};
    weave W[DAG_MAX] = {};
    a_carve(u8, lf, 1UL << 16);
    u8 *mtmp[2][4] = {};
    ok64 ar;
    if ((ar = u8bAcquire(ABC_BASS, mtmp[0], 1UL << 18)) != OK) return ar;
    if ((ar = u8bAcquire(ABC_BASS, mtmp[1], 1UL << 18)) != OK) return ar;
    u8cs v = {};
    for (u32 i = 0; i < dc->n; i++) {
        if ((ar = u8bAcquire(ABC_BASS, wbuf[i], 1UL << 18)) != OK) return ar;
        call(dag_lineform, lf, dc->nodes[i].content);
        v[0] = u8bDataHead(lf); v[1] = u8bDataHead(lf) + u8bDataLen(lf);
        char const *par = dc->nodes[i].par;
        u32 np = (u32)strlen(par);
        if (np == 0) {                                  // root: from-blob
            call(WEAVENext, u8bIdle(wbuf[i]), NULL, v, cext, dag_cid(i));
        } else if (np == 1) {                           // linear fold
            u32 p = (u8)(par[0] - '0');
            call(WEAVENext, u8bIdle(wbuf[i]), &W[p], v, cext, dag_cid(i));
        } else {                                        // merge fold + content
            u32 p0 = (u8)(par[0] - '0'), p1 = (u8)(par[1] - '0');
            u8bReset(mtmp[0]);
            call(WEAVEMerge, u8bIdle(mtmp[0]), &W[p0], &W[p1], 0);
            weave wm = {}; call(WEAVEParse, &wm, u8bDataC(mtmp[0]));
            u32 d = 1;
            for (u32 k = 2; k < np; k++) {
                u32 pk = (u8)(par[k] - '0');
                u8bReset(mtmp[d]);
                call(WEAVEMerge, u8bIdle(mtmp[d]), &wm, &W[pk], 0);
                call(WEAVEParse, &wm, u8bDataC(mtmp[d]));
                d ^= 1;
            }
            call(WEAVENext, u8bIdle(wbuf[i]), &wm, v, cext, dag_cid(i));
        }
        call(WEAVEParse, &W[i], u8bDataC(wbuf[i]));
    }
    for (u32 i = 0; i < dc->n; i++) call(dag_verify, W, dc->nodes, dc->n, i);
    fprintf(stderr, " ok\n");
    done;
}

//  crisscross_recovery: the minimal merge(merge,merge) shape — a node reaching
//  the final merge via two paths used to be emitted twice (root `88` recovered
//  as `888`).  Node 5=merge(2,3) and node 6=merge(2,4) share commit 2's tokens;
//  node 7 fold-merges 5 and 6, where the OLD two-pointer laid commit 2's tokens
//  in opposite relative order on the two paths and desynced the join.
static dagnode const cc_nodes[] = {
    {"",   "88"},    // 0 root: two identical tokens
    {"0",  "ae"},    // 1
    {"0",  "ar"},    // 2
    {"1",  "bd"},    // 3
    {"1",  "ad"},    // 4
    {"23", "ec"},    // 5 merge(2,3)
    {"24", "ab"},    // 6 merge(2,4)
    {"56", "ab"},    // 7 merge(5,6)  -- doubled root `88` -> `888` pre-fix
};

//  crash_597: the minimised crash-597 corpus DAG (12-line prefix of $HOME/
//  Corpus/DWEAVE/crash-597…, normalized parents from the fuzzer).  Recovering
//  w[11]'s ancestors doubled the root's trailing 8s pre-fix (`a_aa888888` ->
//  `a_aa8888888888`).  Node 11 fold-merges five parents (5..9), the multi-path
//  reach that desynced the old two-pointer join.
static dagnode const c597_nodes[] = {
    {"",      "a_aa888888"},  // 0 root
    {"0",     "aae"},         // 1
    {"0",     "aar"},         // 2
    {"1",     "bcde"},        // 3
    {"1",     "abcdd"},       // 4
    {"1",     "de"},          // 5
    {"23",    "ecbcde"},      // 6 merge(2,3)
    {"1",     "e"},           // 7
    {"24",    "abcde"},       // 8 merge(2,4)
    {"23",    "ecbcde"},      // 9 merge(2,3)
    {"1",     "e"},           // 10
    {"56789", "abcde"},       // 11 fold-merge(5..9) -- doubled root pre-fix
};

//  dis044_spine: minimised DWEAVEfuzz crash under arbitrary commit-ids
//  (15-byte ` a\n0 g\n0 A\n12 X`).  Node 0's SplitMix id outranks nodes 1/2's,
//  so the spine sits ABOVE its edits; the old commit-id-DESC tie-break
//  reordered node 1's content on recovery (`g` -> `.g`).  GREEN once the
//  tie-break is the causal commit INDEX.
static dagnode const d044_nodes[] = {
    {"",   "a"},   // 0 root  (largest SplitMix id == spine above edits)
    {"0",  "g"},   // 1
    {"0",  "A"},   // 2
    {"12", "X"},   // 3 merge(1,2)
};

static dagcase const dagcases[] = {
    {"crisscross_recovery", cc_nodes,    8},
    {"crash_597",           c597_nodes, 12},
    {"dis044_spine_above",  d044_nodes,  4},
};

//  Associativity: merge(merge(a,b),c) and merge(a,merge(b,c)) must SERIALISE
//  byte-identically — the RGA order depends only on immutable anchors, not the
//  merge path.  Three concurrent single-edit branches off one base.
static ok64 assoc_scenario(void) {
    sane(1);
    fprintf(stderr, "  merge_associativity ...");
    a_cstr(cext, "c");
    u8csc v0 = {(u8c *)"x\ny\nz\n", (u8c *)"x\ny\nz\n" + 6};
    u8csc va = {(u8c *)"A\ny\nz\n", (u8c *)"A\ny\nz\n" + 6};
    u8csc vb = {(u8c *)"x\nB\nz\n", (u8c *)"x\nB\nz\n" + 6};
    u8csc vc = {(u8c *)"x\ny\nC\n", (u8c *)"x\ny\nC\n" + 6};
    a_carve(u8, w0b, 1UL << 16); call(WEAVENext, u8bIdle(w0b), NULL, v0, cext, 1);
    weave w0 = {}; call(WEAVEParse, &w0, u8bDataC(w0b));
    a_carve(u8, wab, 1UL << 16); call(WEAVENext, u8bIdle(wab), &w0, va, cext, 2);
    weave wa = {}; call(WEAVEParse, &wa, u8bDataC(wab));
    a_carve(u8, wbb, 1UL << 16); call(WEAVENext, u8bIdle(wbb), &w0, vb, cext, 3);
    weave wb = {}; call(WEAVEParse, &wb, u8bDataC(wbb));
    a_carve(u8, wcb, 1UL << 16); call(WEAVENext, u8bIdle(wcb), &w0, vc, cext, 4);
    weave wc = {}; call(WEAVEParse, &wc, u8bDataC(wcb));
    //  left:  merge(merge(a,b),c)
    a_carve(u8, lab, 1UL << 16); call(WEAVEMerge, u8bIdle(lab), &wa, &wb, 0);
    weave wab2 = {}; call(WEAVEParse, &wab2, u8bDataC(lab));
    a_carve(u8, lf, 1UL << 16);  call(WEAVEMerge, u8bIdle(lf), &wab2, &wc, 0);
    //  right: merge(a,merge(b,c))
    a_carve(u8, rbc, 1UL << 16); call(WEAVEMerge, u8bIdle(rbc), &wb, &wc, 0);
    weave wbc = {}; call(WEAVEParse, &wbc, u8bDataC(rbc));
    a_carve(u8, rf, 1UL << 16);  call(WEAVEMerge, u8bIdle(rf), &wa, &wbc, 0);
    if (u8bDataLen(lf) != u8bDataLen(rf) ||
        memcmp(u8bDataHead(lf), u8bDataHead(rf), u8bDataLen(lf))) {
        fprintf(stderr, " not associative (%zu vs %zu)\n",
                u8bDataLen(lf), u8bDataLen(rf));
        fail(TESTFAIL);
    }
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
    call(spine_above_replace);
    call(spine_above_diverge);
    u32 nd = (u32)(sizeof(dagcases) / sizeof(dagcases[0]));
    for (u32 i = 0; i < nd; i++) call(dag_run, &dagcases[i]);
    call(assoc_scenario);
    done;
}

TEST(WEAVErttest);
