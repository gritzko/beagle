//
//  WEAVE03 (DIS-045) — blank-line vs code-line EOL merge mis-anchor.
//
//  Reproduces test/patch/15-ancestor-skip step 2 through the DOG API
//  directly (the trunk's be-patch-15 is still GRAF-backed, DOG-005
//  unlanded, so it flakes and won't isolate the dog bug).  Builds the
//  lib.c weave DAG with WEAVENext / WEAVEMerge and asserts the step-2
//  merged tip (WEAVEAlive) equals 08.lib.want_step2.c byte-for-byte.
//
//  The bug: a re-stamped spine blank line `\n` and theirs's code-line
//  trailing EOL `\n` hash equal (identical CONTENT tokens), so the
//  content diff/merge pairs the wrong one and anchors the inserted line
//  on the wrong side of the blank.
//
#include "dog/WEAVE.h"

#include <stdio.h>
#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

//  --- the patch/15-ancestor-skip lib.c revisions (verbatim) -----------
#define T0 \
    "#include <stdio.h>\n" \
    "\n" \
    "int add(int x, int y) { return x + y; }\n" \
    "\n" \
    "const char *greet = \"hi\";\n" \
    "const char *bye = \"bye\";\n" \
    "\n" \
    "int main(void) { return 0; }\n"
#define T1 \
    "#include <stdio.h>\n" \
    "\n" \
    "int add(int x, int y) { return x + y; }\n" \
    "\n" \
    "const char *greet = \"hello\";\n" \
    "const char *bye = \"bye\";\n" \
    "\n" \
    "int main(void) { return 0; }\n"
#define F1 \
    "#include <stdio.h>\n" \
    "\n" \
    "int add(int x, int y) { return x + y; }\n" \
    "int sub(int x, int y) { return x - y; }\n" \
    "\n" \
    "const char *greet = \"hi\";\n" \
    "const char *bye = \"bye\";\n" \
    "\n" \
    "int main(void) { return 0; }\n"
#define F2 \
    "#include <stdio.h>\n" \
    "\n" \
    "int add(int x, int y) { return x + y; }\n" \
    "int sub(int x, int y) { return x - y; }\n" \
    "int mul(int x, int y) { return x * y; }\n" \
    "\n" \
    "const char *greet = \"hi\";\n" \
    "const char *bye = \"bye\";\n" \
    "\n" \
    "int main(void) { return 0; }\n"
#define F3 \
    "#include <stdio.h>\n" \
    "\n" \
    "int add(int x, int y) { return x + y; }\n" \
    "int sub(int x, int y) { return x - y; }\n" \
    "int mul(int x, int y) { return x * y; }\n" \
    "int divmod(int a, int b) { return a / b; }\n" \
    "\n" \
    "const char *greet = \"hi\";\n" \
    "const char *bye = \"bye\";\n" \
    "\n" \
    "int main(void) { return 0; }\n"
#define G1 \
    "#include <stdio.h>\n" \
    "\n" \
    "int add(int x, int y) { return x + y; }\n" \
    "int sub(int x, int y) { return x - y; }\n" \
    "int mul(int x, int y) { return x * y; }\n" \
    "\n" \
    "const char *greet = \"hi\";\n" \
    "const char *bye = \"farewell\";\n" \
    "\n" \
    "int main(void) { return 0; }\n"
#define WANT2 \
    "#include <stdio.h>\n" \
    "\n" \
    "int add(int x, int y) { return x + y; }\n" \
    "int sub(int x, int y) { return x - y; }\n" \
    "int mul(int x, int y) { return x * y; }\n" \
    "int divmod(int a, int b) { return a / b; }\n" \
    "\n" \
    "const char *greet = \"hello\";\n" \
    "const char *bye = \"farewell\";\n" \
    "\n" \
    "int main(void) { return 0; }\n"

#define LIT(s) {(u8c *)(s), (u8c *)(s) + sizeof(s) - 1}

//  Print a blob with '\n' rendered as a visible glyph for diffing.
static void show(char const *lbl, u8c *p, size_t n) {
    fprintf(stderr, "  %s (%zu):\n----\n", lbl, n);
    for (size_t k = 0; k < n; k++) fputc(p[k], stderr);
    fprintf(stderr, "----\n");
}

static ok64 check_alive(char const *lbl, weave const *w, u8csc want) {
    sane(1);
    a_carve(u8, al, 1UL << 16);
    call(WEAVEAlive, w, al);
    size_t gl = u8bDataLen(al), wl = (size_t)$len(want);
    if (gl != wl || (wl && memcmp(u8bDataHead(al), want[0], wl))) {
        fprintf(stderr, " %s MISMATCH (%zu vs %zu)\n", lbl, gl, wl);
        show("got", u8bDataHead(al), gl);
        show("want", (u8c *)want[0], wl);
        fail(TESTFAIL);
    }
    done;
}

#define WANT1 \
    "#include <stdio.h>\n" \
    "\n" \
    "int add(int x, int y) { return x + y; }\n" \
    "int sub(int x, int y) { return x - y; }\n" \
    "int mul(int x, int y) { return x * y; }\n" \
    "\n" \
    "const char *greet = \"hello\";\n" \
    "const char *bye = \"farewell\";\n" \
    "\n" \
    "int main(void) { return 0; }\n"

//  patch/15-ancestor-skip step 2, driven through the DOG API as a true
//  3-way merge with a SHARED base (the GRAFMerge3 / GRAFMergeWtFile dog
//  model: build base→ours and base→theirs weaves, then WEAVEMerge):
//
//    T0 ─ T1                          (trunk)
//      \
//       F1 ─ F2 ─ F3                  (feat)
//              \
//               G1                    (feat/gizmo, shares F1,F2)
//
//  Ancestor-skip: F1,F2 are already absorbed via the step-1 foster chain,
//  so the step-2 merge BASE is F2 (NOT T0).  ours = step-1 result (WANT1),
//  theirs = F3.  Both are folded onto the SAME F2 base weave so the F2
//  spine — including its blank line `\n` — is shared identity; F3's added
//  `divmod` code line carries a trailing `\n` that hashes EQUAL to that
//  re-stamped spine blank.  The merged tip must equal 08.lib.want_step2.c.
static ok64 dis045_blank_eol(void) {
    sane(1);
    fprintf(stderr, "  dis045_blank_eol ...\n");
    a_cstr(cext, "c");
    //  commit ids (arbitrary; the merge keys off identity not order)
    enum { C_F2=4, C_OURS=7, C_THEIRS=5 };
    u8csc vF2 = LIT(F2), vW1 = LIT(WANT1), vF3 = LIT(F3), vW2 = LIT(WANT2);

    //  shared base F2 (the ancestor-skip merge base)
    a_carve(u8, bF2, 1UL << 16); call(WEAVENext, u8bIdle(bF2), NULL, vF2, cext, C_F2);
    weave wF2 = {}; call(WEAVEParse, &wF2, u8bDataC(bF2));

    //  ours = fold WANT1 (step-1 content) onto base F2
    a_carve(u8, bO, 1UL << 16); call(WEAVENext, u8bIdle(bO), &wF2, vW1, cext, C_OURS);
    weave wO = {}; call(WEAVEParse, &wO, u8bDataC(bO));
    call(check_alive, "ours tip", &wO, vW1);

    //  theirs = fold F3 onto base F2
    a_carve(u8, bT, 1UL << 16); call(WEAVENext, u8bIdle(bT), &wF2, vF3, cext, C_THEIRS);
    weave wT = {}; call(WEAVEParse, &wT, u8bDataC(bT));
    call(check_alive, "theirs tip", &wT, vF3);

    //  step 2: merge(ours, theirs) — must yield 08.lib.want_step2.c
    a_carve(u8, bM, 1UL << 16); call(WEAVEMerge, u8bIdle(bM), &wO, &wT, 0);
    weave wM = {}; call(WEAVEParse, &wM, u8bDataC(bM));
    call(check_alive, "step2 merged", &wM, vW2);

    fprintf(stderr, "  dis045_blank_eol ... ok\n");
    done;
}

ok64 WEAVE03test() {
    sane(1);
    call(dis045_blank_eol);
    done;
}

TEST(WEAVE03test);
