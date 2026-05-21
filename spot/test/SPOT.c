#include "spot/SPOT.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

// Helper: tokenize C source into packed u32 buffer
static ok64 SPOTBuildToks(u32bp toks, const char *src) {
    sane(toks != NULL);
    u8csc source = {(u8cp)src, (u8cp)src + strlen(src)};
    u8cs ext = $u8str(".c");
    return SPOTTokenize(toks, source, ext);
}

// Helper: init SPOT from C source strings
static ok64 SPOTSetup(SPOTstate *st,
                       u32bp nbuf, u32bp hbuf,
                       const char *needle, const char *haystack) {
    sane(st != NULL);
    call(SPOTBuildToks, hbuf, haystack);
    u32cp hd = u32bDataHead(hbuf);
    u32cp hi = u32bIdleHead(hbuf);
    u32cs htoks = {(u32cp)hd, (u32cp)hi};
    test(!$empty(htoks), FAILSANITY);

    u8csc nsrc = {(u8cp)needle, (u8cp)needle + strlen(needle)};
    u8csc source = {(u8cp)haystack, (u8cp)haystack + strlen(haystack)};
    u8cs ext = $u8str(".c");
    call(SPOTInit, st, nbuf, nsrc, ext, htoks, source);
    done;
}

// Helper: check if text contains a C string
static b8 SPOTContainsStr(u8cs text, const char *needle) {
    size_t nlen = strlen(needle);
    size_t tlen = (size_t)$len(text);
    if (nlen > tlen) return NO;
    u8cp end = text[0] + tlen - nlen + 1;
    u8cp p = text[0];
    while (p < end) {
        if (memcmp(p, needle, nlen) == 0) return YES;
        p++;
    }
    return NO;
}

// ---- Test: error codes ----
// SPOTEND  = SPOTNext exhausted the haystack (clean finish, no more matches).
// SPOTBAD  = real input error: empty/oversized needle, malformed init.
// SPOTNONE = internal "no match at this position" sentinel — used by
//            spot_match_flat for backtracking; never escapes SPOTNext.
ok64 SPOTtestCodes() {
    sane(1);
    test(0 == strcmp(ok64str(SPOTEND),  "SPOTEND"),  FAILSANITY);
    test(0 == strcmp(ok64str(SPOTBAD),  "SPOTBAD"),  FAILSANITY);
    test(0 == strcmp(ok64str(SPOTNONE), "SPOTNONE"), FAILSANITY);
    done;
}

// ---- Table-driven match/no-match tests ----

typedef struct {
    const char *name;
    const char *needle;
    const char *haystack;
    ok64 expect;            // OK = match expected, SPOTEND = no match
    const char *must_have;  // if non-NULL, matched source must contain this
    const char *must_lack;  // if non-NULL, matched source must NOT contain this
} SPOTcase;

static const SPOTcase SPOT_CASES[] = {

    // --- Basic exact matching ---

    {
        "ExactReturn0",
        "return 0;",
        "int foo() { return 0; }\n",
        OK, "return", NULL
    },
    {
        "ExactReturn1",
        "return 1;",
        "int foo() { return 0; }\n",
        SPOTEND, NULL, NULL
    },
    {
        "NoMatch_goto",
        "goto x;",
        "int foo() { return 0; }\n",
        SPOTEND, NULL, NULL
    },

    // --- Lowercase placeholder: name binding ---

    {
        "PlaceholderName",
        "return x;",
        "int foo() { return counter; }\n",
        OK, "counter", NULL
    },

    // --- Lowercase placeholder consistency ---

    {
        "ConsistencyMatch",
        "x = x + 1",
        "void f() { y = y + 1; }\n",
        OK, NULL, NULL
    },
    {
        "ConsistencyFail",
        "x = x + 1",
        "void f() { y = z + 1; }\n",
        SPOTEND, NULL, NULL
    },

    // --- Uppercase placeholder: subtree binding ---

    {
        "SubtreePlaceholder",
        "return X;",
        "int foo() { return a + b; }\n",
        OK, "return", NULL
    },

    // --- Skip comments on haystack side ---

    {
        "SkipComment",
        "a = b",
        "void f() { a /* comment */ = b; }\n",
        OK, NULL, NULL
    },

    // --- Gap handling: SKIP (2+ spaces in needle) ---

    {
        "SkipSiblings_intDecl",
        "int  x",
        "void f() { int foo, bar, baz; }\n",
        OK, "int", NULL
    },
    {
        "SkipSiblings_noType",
        "int  x",
        "void f() { char foo, bar; }\n",
        SPOTEND, NULL, NULL
    },
    {
        "SkipSiblings_return",
        "return  x",
        "int f() { int q = 1; return q; }\n",
        OK, "return", NULL
    },

    // --- Gap: 3+ spaces also skip ---

    {
        "SkipDFS_finds",
        "int   x",
        "void f() { int val; }\n",
        OK, "int", NULL
    },
    {
        "SkipDFS_noType",
        "int   x",
        "void f() { char arr[10]; }\n",
        SPOTEND, NULL, NULL
    },

    // --- Literal keyword matching ---

    {
        "KeywordWhile",
        "while (x)",
        "void f() { while (cond) { foo(); } }\n",
        OK, "while", NULL
    },
    {
        "KeywordIf",
        "if (x)",
        "void f() { if (ready) { go(); } }\n",
        OK, "if", NULL
    },

    // --- Mixed: literal + placeholder in expression ---

    {
        "AssignLiteral",
        "x = 0",
        "void f() { count = 0; }\n",
        OK, "count", NULL
    },

    // --- Two-letter names are NOT placeholders ---

    {
        "TwoLetterLiteral",
        "return ab;",
        "int f() { return ab; }\n",
        OK, "ab", NULL
    },
    {
        "TwoLetterNoMatch",
        "return ab;",
        "int f() { return cd; }\n",
        SPOTEND, NULL, NULL
    },

    // --- Gap: 2-space skip with multiple declarations ---

    {
        "SkipSibl_multiDecl",
        "int  x",
        "void f() { int aaa, bbb, ccc; }\n",
        OK, "int", NULL
    },

    // --- Gap: 3-space skip with init_declarator ---

    {
        "SkipDFS_initDecl",
        "int   x",
        "void f() { int val = 42; }\n",
        OK, "val", NULL
    },

    // --- Gap: 4-space also skip ---

    {
        "SkipDFS_4space",
        "int    x",
        "void f() { int val = 42; }\n",
        OK, "val", NULL
    },

    // --- Gap: 1-space is EXACT ---

    {
        "ExactGap_1space",
        "return x;",
        "int f() { return val; }\n",
        OK, "val", NULL
    },

    // --- Skip: return with extra tokens between ---

    {
        "SkipSibl_returnExpr",
        "return  x;",
        "int f() { return 42; }\n",
        OK, "42", NULL
    },

    // --- Skip: finds target past nested blocks ---

    {
        "SkipDFS_pastBlock",
        "return   x;",
        "int f() { if (1) {} return 99; }\n",
        OK, "99", NULL
    },

    // --- Uppercase placeholder: no cross-statement extension ---

    {
        "UpperNoCrossStmt",
        "T n = {X[0],X[1]};",
        "int a = 1; u8cs b = {arr[0], arr[1]};\n",
        OK, "arr", "int"
    },
    {
        "UpperNoCrossFunc",
        "T n = {X[0],X[1]};",
        "void f() { int x; } u8cs b = {arr[0], arr[1]};\n",
        OK, "arr", "void f"
    },

    // --- Uppercase placeholder consistency (content, not just length) ---

    {
        "UpperConsistMatch",
        "T n = {X[0],X[1]};",
        "void f() { int x = {arr[0], arr[1]}; }\n",
        OK, "arr", NULL
    },
    {
        "UpperConsistFail",
        "T n = {X[0],X[1]};",
        "void f() { int x = {foo[0], bar[1]}; }\n",
        SPOTEND, NULL, NULL
    },

    // --- Uppercase placeholder: multi-token and bracket captures ---

    {
        "UpperMultiTok",
        "return X;",
        "int f() { return a + b; }\n",
        OK, "a + b", NULL
    },
    {
        "UpperBracketCap",
        "X[0]",
        "void f() { f(x)[0]; }\n",
        OK, "f(x)", NULL
    },

    // --- Lowercase placeholder: balanced bracket group ---

    {
        "LowerBracketSimple",
        "T n = {a[0],a[1]};",
        "void f() { int x = {arr[0], arr[1]}; }\n",
        OK, "arr", NULL
    },
    {
        // Lowercase placeholder is strictly single-token; (*fp) is
        // multi-token so it must NOT bind to lowercase 'a'.  Use an
        // uppercase placeholder for that.
        "LowerBracketParens",
        "T n = {a[0],a[1]};",
        "void f() { u8cs x = {(*fp)[0], (*fp)[1]}; }\n",
        SPOTEND, NULL, NULL
    },
    {
        "LowerBracketNoMatch",
        "T n = {a[0],a[1]};",
        "void f() { int y = {ptr[0], qtr[1]}; }\n",
        SPOTEND, NULL, NULL
    },
    {
        "LowerBracketConsist",
        "T n = {a[0],a[1]};",
        "void f() { u8cs z = {(*fp)[0], (*gp)[1]}; }\n",
        SPOTEND, NULL, NULL
    },

    // --- Lowercase must reject punct tokens (operators, brackets, ;,) ---
    // Regression: lowercase 'n' was matching 'P'-tagged tokens, so
    // patterns like "T n = ..." would bind n to '=' or ']' etc.
    {
        "LowerRejectsEquals",
        "T n = X;",
        "void f() { int x = 1; }\n",
        OK, "x", NULL  // n binds 'x', not '='
    },
    {
        "LowerRejectsCloseBracket",
        "T n = X;",
        "void f() { a[i] = 1; }\n",
        SPOTEND, NULL, NULL  // n cannot bind to '[', ']', or '='
    },
    {
        "LowerRejectsSemicolon",
        "a;",
        "void f() { ;; ; x; }\n",
        OK, "x", NULL  // a binds 'x', skipping bare ';' tokens
    },

    // --- Unbound uppercase must not start on a closing bracket ---
    // Regression: 'T' could bind to a leading ')' or ']' producing
    // unbalanced captures like "T n = " matching against "...] x = ...".
    {
        "UpperRejectsLeadingClose",
        "T n = X;",
        "void f() { foo(arr[0]); int x = 1; }\n",
        OK, "int", NULL  // T must start at 'int', not at ')' or ']'
    },
    {
        "UpperRejectsLeadingCloseDeep",
        "T x = 0;",
        "void f() { foo(arr[0]); int x = 0; }\n",
        OK, "int", NULL  // T binds 'int', not the ')' or ']' before it
    },
};

#define SPOT_NCASES (sizeof(SPOT_CASES) / sizeof(SPOT_CASES[0]))

ok64 SPOTtestTable() {
    sane(1);

    for (size_t i = 0; i < SPOT_NCASES; i++) {
        const SPOTcase *tc = &SPOT_CASES[i];
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf, tc->needle, tc->haystack);

        ok64 o = SPOTNext(&st);

        if (o != tc->expect) {
            fprintf(stderr, "FAIL [%s]: expected %s got %s\n",
                    tc->name, ok64str(tc->expect), ok64str(o));
            fail(FAILEQ);
        }

        if (o == OK && tc->must_have) {
            // Check matched source range contains expected text
            u8cs matched = {st.source[0] + st.src_rng.lo,
                            st.source[0] + st.src_rng.hi};
            if (!SPOTContainsStr(matched, tc->must_have)) {
                fprintf(stderr, "FAIL [%s]: matched range missing \"%s\"\n",
                        tc->name, tc->must_have);
                fail(FAILEQ);
            }
        }

        if (o == OK && tc->must_lack) {
            u8cs matched = {st.source[0] + st.src_rng.lo,
                            st.source[0] + st.src_rng.hi};
            if (SPOTContainsStr(matched, tc->must_lack)) {
                fprintf(stderr, "FAIL [%s]: matched range has unwanted \"%s\"\n",
                        tc->name, tc->must_lack);
                fail(FAILEQ);
            }
        }
    }
    done;
}

// ---- Test: reentrant multiple matches ----
ok64 SPOTtestReentrant() {
    sane(1);
    aBpad(u32, nbuf, 4096);
    aBpad(u32, hbuf, 65536);

    SPOTstate st = {};
    call(SPOTSetup, &st, nbuf, hbuf,
         "return x;",
         "int a() { return 1; }\n"
         "int b() { return 2; }\n"
         "int c() { return 3; }\n");

    int matches = 0;
    for (int i = 0; i < 10; i++) {
        ok64 o = SPOTNext(&st);
        if (o == OK)
            matches++;
        else if (o == SPOTEND)
            break;
        else
            fail(o);
    }
    testeq(matches, 3);
    done;
}

// ---- Test: gap handling specifics ----

ok64 SPOTtestGapSkipSiblings() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "int  x",
             "void f() { int alpha, beta, gamma; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "return  x",
             "int f() { return 42; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);
    }

    done;
}

ok64 SPOTtestGapSkipDFS() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "int   x",
             "void f() { int val = 5; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);

        // Verify match contains the declaration
        u8cs matched = {st.source[0] + st.src_rng.lo,
                        st.source[0] + st.src_rng.hi};
        test(SPOTContainsStr(matched, "int"), FAILSANITY);
        test(SPOTContainsStr(matched, "val"), FAILSANITY);
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "int   x",
             "void f() { char *p = 0; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, SPOTEND);
    }

    done;
}

ok64 SPOTtestGapExact() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "return x;",
             "int f() { return val; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);

        u8cs matched = {st.source[0] + st.src_rng.lo,
                        st.source[0] + st.src_rng.hi};
        test(SPOTContainsStr(matched, "val"), FAILSANITY);
    }

    done;
}

// ---- Test: multi-statement needle ----
ok64 SPOTtestMultiStmt() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "int x;  x = A;",
             "int a; int b; a = 1; b = 2;\n");

        int matches = 0;
        for (int i = 0; i < 10; i++) {
            ok64 o = SPOTNext(&st);
            if (o == OK)
                matches++;
            else if (o == SPOTEND)
                break;
            else
                fail(o);
        }
        if (matches != 2) {
            fprintf(stderr, "FAIL [MultiStmt]: expected 2 matches, got %d\n",
                    matches);
            fail(FAILEQ);
        }
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "int x;  x = A;",
             "int a; a = 1;\n");

        ok64 o1 = SPOTNext(&st);
        testeq(o1, OK);

        ok64 o2 = SPOTNext(&st);
        testeq(o2, SPOTEND);
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "int x;  x = A;",
             "int a; b = 1;\n");

        ok64 o = SPOTNext(&st);
        testeq(o, SPOTEND);
    }

    done;
}

// ---- Test: consistency across skip ----
ok64 SPOTtestConsistencySkip() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;  x = B;",
             "void f() { a = 1; b = 2; a = 3; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;  x = B;",
             "void f() { a = 1; b = 2; c = 3; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, SPOTEND);
    }

    done;
}

// ---- Test: swap pattern ----
ok64 SPOTtestSwapPattern() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = y;  y = x;",
             "void f() { a = b; c = d; b = a; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = y;  y = x;",
             "void f() { a = b; c = d; e = f; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, SPOTEND);
    }

    done;
}

// ---- Test: backtracking ----
ok64 SPOTtestBacktrack() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;  y = B;  x = y;",
             "void f() { a = 1; c = 3; b = 2; a = b; }\n");

        ok64 o = SPOTNext(&st);
        if (o != OK) {
            fprintf(stderr, "FAIL [Backtrack]: expected OK, got %s "
                    "(backtracking not implemented)\n", ok64str(o));
            fail(FAILEQ);
        }
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;  y = B;  x = y;",
             "void f() { a = 1; b = 2; a = b; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;  y = B;  x = y;",
             "void f() { a = 1; b = 2; c = 3; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, SPOTEND);
    }

    done;
}

// ---- Test: backtracking with DFS skip ----
ok64 SPOTtestBacktrackDFS() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;   y = B;   x = y;",
             "void f() { a = 1; if(1) { c = 3; } b = 2; a = b; }\n");

        ok64 o = SPOTNext(&st);
        if (o != OK) {
            fprintf(stderr, "FAIL [BacktrackDFS]: expected OK, got %s "
                    "(backtracking not implemented)\n", ok64str(o));
            fail(FAILEQ);
        }
    }

    done;
}

// ---- Test: backtracking with reentrant multiple matches ----
ok64 SPOTtestBacktrackReentrant() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;  y = B;  x = y;",
             "void f() { a = 1; c = 3; b = 2; a = b; }\n"
             "void g() { d = 4; e = 5; d = e; }\n");

        int matches = 0;
        for (int i = 0; i < 10; i++) {
            ok64 o = SPOTNext(&st);
            if (o == OK)
                matches++;
            else if (o == SPOTEND)
                break;
            else
                fail(o);
        }
        if (matches != 2) {
            fprintf(stderr, "FAIL [BacktrackReentrant]: expected 2 matches, "
                    "got %d\n", matches);
            fail(FAILEQ);
        }
    }

    done;
}

// ---- Test: submatch offsets ----
ok64 SPOTtestSubs() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;  y = B;  x = y;",
             "void f() { a = 1; b = 2; a = b; }\n");

        ok64 o = SPOTNext(&st);
        if (o != OK) {
            fprintf(stderr, "FAIL [Subs3]: expected OK, got %s\n", ok64str(o));
            fail(FAILEQ);
        }
        if (st.nsubs != 3) {
            fprintf(stderr, "FAIL [Subs3]: nsubs=%d, expected 3\n", st.nsubs);
            fail(FAILEQ);
        }
        test(st.subs[0].lo > 0, FAILSANITY);
        test(st.subs[1].lo > st.subs[0].lo, FAILSANITY);
        test(st.subs[2].lo > st.subs[1].lo, FAILSANITY);
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "return x;",
             "int f() { return 42; }\n");

        ok64 o = SPOTNext(&st);
        if (o != OK) {
            fprintf(stderr, "FAIL [Subs1]: expected OK, got %s\n", ok64str(o));
            fail(FAILEQ);
        }
        if (st.nsubs != 1) {
            fprintf(stderr, "FAIL [Subs1]: nsubs=%d, expected 1\n", st.nsubs);
            fail(FAILEQ);
        }
        test(st.subs[0].lo > 0, FAILSANITY);
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "int   x",
             "void f() { int val = 5; }\n");

        ok64 o = SPOTNext(&st);
        if (o != OK) {
            fprintf(stderr, "FAIL [Subs_dfs]: expected OK, got %s\n", ok64str(o));
            fail(FAILEQ);
        }
        if (st.nsubs != 2) {
            fprintf(stderr, "FAIL [Subs_dfs]: nsubs=%d, expected 2\n", st.nsubs);
            fail(FAILEQ);
        }
    }

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;  return x;",
             "int f() { a = 1; return a; }\n");

        ok64 o = SPOTNext(&st);
        if (o != OK) {
            fprintf(stderr, "FAIL [Subs2]: expected OK, got %s\n", ok64str(o));
            fail(FAILEQ);
        }
        if (st.nsubs != 2) {
            fprintf(stderr, "FAIL [Subs2]: nsubs=%d, expected 2\n", st.nsubs);
            fail(FAILEQ);
        }
    }

    done;
}

// ---- Test: ranges buffer ----
ok64 SPOTtestRanges() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "x = A;",
             "void f() { a = 1; b = 2; a = b; }\n");

        // Set up ranges buffer
        aBpad(match32, ranges, 64);
        st.ranges[0] = ranges[0]; st.ranges[1] = ranges[1];
        st.ranges[2] = ranges[2]; st.ranges[3] = ranges[3];

        ok64 o = SPOTNext(&st);
        if (o != OK) {
            fprintf(stderr, "FAIL [Ranges]: expected OK, got %s\n", ok64str(o));
            fail(FAILEQ);
        }

        // Ranges buffer should have entries
        size_t rlen = match32bDataLen(st.ranges);
        test(rlen > 0, FAILSANITY);

        // Each range should have valid hay range
        match32 *rp = (match32 *)st.ranges[1];
        for (size_t i = 0; i < rlen; i++) {
            test(rp[i].hay.hi >= rp[i].hay.lo, FAILSANITY);
        }

        // bind_matches: x (idx 23) and A (idx 26) should be set
        test(st.bound & (1ULL << 23), FAILSANITY);  // x
        test(st.bound & (1ULL << 26), FAILSANITY);  // A
        test(st.bind_matches[23].hay.lo != st.bind_matches[23].hay.hi, FAILSANITY);
        test(st.bind_matches[26].hay.lo != st.bind_matches[26].hay.hi, FAILSANITY);
    }

    // Without ranges: no crash
    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "return x;",
             "int f() { return 42; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);
    }

    done;
}

// ---- Test: bracket-constrained matching ----
ok64 SPOTtestBrackets() {
    sane(1);

    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "   ok64 f(P){   ++*a;   }",
             "ok64 func1(int x) { x += 1; return x; }\n"
             "ok64 func2(int *y) { ++*y; return 0; }\n");

        int matches = 0;
        u32 first_lo = 0;
        for (int i = 0; i < 10; i++) {
            ok64 o = SPOTNext(&st);
            if (o == OK) {
                if (matches == 0) {
                    first_lo = st.src_rng.lo;
                }
                matches++;
            } else if (o == SPOTEND) {
                break;
            } else {
                fail(o);
            }
        }
        if (matches != 1) {
            fprintf(stderr, "FAIL [Brackets]: expected 1 match, got %d\n",
                    matches);
            fail(FAILEQ);
        }
        if (first_lo == 0) {
            fprintf(stderr, "FAIL [Brackets]: match starts at byte 0 "
                    "(spans from func1)\n");
            fail(FAILEQ);
        }
    }

    // No match: single function without ++*
    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "   ok64 f(P){   ++*a;   }",
             "ok64 func1(int x) { x += 1; return x; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, SPOTEND);
    }

    // Closing bracket matches function scope, not inner scope
    {
        aBpad(u32, nbuf, 4096);
        aBpad(u32, hbuf, 65536);

        SPOTstate st = {};
        call(SPOTSetup, &st, nbuf, hbuf,
             "   void f(P){   ++*a;   }",
             "void g(int *p) { while(1) { ++*p; } return; }\n");

        ok64 o = SPOTNext(&st);
        testeq(o, OK);
        u32 func_end = (u32)strlen("void g(int *p) { while(1) { ++*p; } return; }") - 1;
        if (st.src_rng.hi < func_end - 1) {
            fprintf(stderr, "FAIL [BracketScope]: src_hi=%u, expected >= %u "
                    "(matched inner } not function })\n",
                    st.src_rng.hi, func_end - 1);
            fail(FAILEQ);
        }
    }

    done;
}

// ---- Main test aggregator ----

ok64 SPOTtest() {
    sane(1);
    call(SPOTtestCodes);
    call(SPOTtestTable);
    call(SPOTtestReentrant);
    call(SPOTtestGapExact);
    call(SPOTtestGapSkipSiblings);
    call(SPOTtestGapSkipDFS);
    call(SPOTtestMultiStmt);
    call(SPOTtestConsistencySkip);
    call(SPOTtestSwapPattern);
    call(SPOTtestBacktrack);
    call(SPOTtestBacktrackDFS);
    call(SPOTtestBacktrackReentrant);
    call(SPOTtestSubs);
    call(SPOTtestRanges);
    call(SPOTtestBrackets);
    done;
}

TEST(SPOTtest);
