//  REFKIND — REFSQueryKind: syntactic kind probe over the canonical
//  resolved query form (DIS-025 Stage 2).  Pure string→enum classifier,
//  no store, no allocation.  Table-driven.
//
//  Canonical forms (user-confirmed 2026-06-07; URI.mkd in flux):
//      ?/<project>/<40hex>            TRUNK     (trunk waypoint)
//      ?/<project>//<40hex>           DETACHED  (no branch slot)
//      ?/<project>/<branch>/<40hex>   BRANCH    (named branch)

#include "keeper/REFS.h"

#include <stdio.h>

#include "abc/S.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

//  A real 40-hex sha (DIS-025's SUBS-008 tip) and a few derived strings.
#define H40  "507226561c499d3d167f0b2f03b9035f0816bc82"

typedef struct {
    char const *q;       // input query (canonical or not)
    refkind     want;    // expected classification
} kind_case;

static char const *kindname(refkind k) {
    switch (k) {
        case REFKIND_NONE:     return "NONE";
        case REFKIND_TRUNK:    return "TRUNK";
        case REFKIND_DETACHED: return "DETACHED";
        case REFKIND_BRANCH:   return "BRANCH";
        default:               return "?";
    }
}

ok64 REFKINDtest_table() {
    sane(1);
    static kind_case const cases[] = {
        //  --- the three canonical shapes ---
        { "?/proj/" H40,                  REFKIND_TRUNK    },
        { "?/proj//" H40,                 REFKIND_DETACHED },
        { "?/proj/feat/" H40,             REFKIND_BRANCH   },
        //  multi-segment branch path
        { "?/proj/feat/fix/" H40,         REFKIND_BRANCH   },
        //  leading '?' is optional
        { "/proj/" H40,                   REFKIND_TRUNK    },
        { "/proj//" H40,                  REFKIND_DETACHED },
        { "/proj/feat/" H40,              REFKIND_BRANCH   },
        //  longer / dotted project label
        { "?/replicated/" H40,            REFKIND_TRUNK    },
        { "?/replicated//" H40,           REFKIND_DETACHED },

        //  --- non-canonical: REFKIND_NONE ---
        { "",                             REFKIND_NONE },   // empty
        { "?",                            REFKIND_NONE },   // just the marker
        { "?/",                           REFKIND_NONE },
        { "?feat",                        REFKIND_NONE },   // bareword
        { "?feat/",                       REFKIND_NONE },   // branch, no pin
        { "?./fix",                       REFKIND_NONE },   // relative ref
        { "?/proj/feat",                  REFKIND_NONE },   // no pin
        { "?/proj/" "5072265",            REFKIND_NONE },   // short hashlet
        { "?/proj/507226561c499d3d167f0b2f03b9035f0816bc8",  REFKIND_NONE }, // 39 hex
        { "?/proj/507226561c499d3d167f0b2f03b9035f0816bcZZ", REFKIND_NONE }, // 40 non-hex
        { "?//" H40,                      REFKIND_NONE },   // empty project (detached)
        { "?/" H40,                       REFKIND_NONE },   // empty project (trunk)
        { "//host?feat",                  REFKIND_NONE },   // authority form
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(*cases); i++) {
        a_cstr(q, cases[i].q);
        refkind got = REFSQueryKind(q);
        if (got != cases[i].want) {
            fprintf(stderr,
                "case %zu '%s': got %s, want %s\n",
                i, cases[i].q, kindname(got), kindname(cases[i].want));
            fail(FAIL);
        }
    }
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "REFKINDtest_table...\n"); call(REFKINDtest_table);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
