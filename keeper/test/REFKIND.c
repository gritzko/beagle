//  REFKIND — REFSQueryKind: syntactic kind probe over the canonical
//  resolved ref form (URI-001 Stage 3).  Pure string→enum classifier,
//  no store, no allocation.  Table-driven.
//
//  Canonical forms (revised 2026-06-07; URI.mkd "Ref shapes"): scope in
//  the query, resolved sha in the fragment, except detached (bare pin):
//      ?/<project>#<sha>             TRUNK     (commit on trunk)
//      ?/<project>/<branch>#<sha>    BRANCH    (commit on branch)
//      ?/<project>/<sha>             DETACHED  (bare commit, no branch)

#include "keeper/REFS.h"

#include <stdio.h>

#include "abc/S.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

//  A real 40-hex sha (DIS-025's SUBS-008 tip), plus a 64-hex (sha256)
//  string to prove the probe is hash-length-agnostic via DOGIsFullSha.
#define H40  "507226561c499d3d167f0b2f03b9035f0816bc82"
#define H64  H40 "0123456789abcdef01234567"

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
        //  --- canonical scopes (no fragment pin) ---
        { "?/proj",                       REFKIND_TRUNK    },
        { "?/proj/feat",                  REFKIND_BRANCH   },
        { "?/proj/feat/fix",              REFKIND_BRANCH   },  // nested
        { "?/proj/" H40,                  REFKIND_DETACHED },  // full sha
        //  hash-length-agnostic: a 64-hex (sha256) → detached too
        { "?/proj/" H64,                  REFKIND_DETACHED },
        //  leading '?' is optional
        { "/proj",                        REFKIND_TRUNK    },
        { "/proj/feat",                   REFKIND_BRANCH   },
        { "/proj/" H40,                   REFKIND_DETACHED },
        //  longer / dotted project label
        { "?/replicated",                 REFKIND_TRUNK    },
        { "?/replicated/main",            REFKIND_BRANCH   },

        //  --- not a canonical scope: REFKIND_NONE ---
        { "",                             REFKIND_NONE },   // empty
        { "?",                            REFKIND_NONE },   // just the marker
        { "?/",                           REFKIND_NONE },
        { "?feat",                        REFKIND_NONE },   // not project-qualified
        { "?feat/",                       REFKIND_NONE },   // not project-qualified
        { "?./fix",                       REFKIND_NONE },   // relative ref
        { "?//" H40,                      REFKIND_NONE },   // empty project
        { "//host?feat",                  REFKIND_NONE },   // authority form

        //  --- a non-full-sha post-project segment is a BRANCH name ---
        { "?/proj/5072265",               REFKIND_BRANCH }, // short hashlet
        { "?/proj/507226561c499d3d167f0b2f03b9035f0816bc8",  REFKIND_BRANCH }, // 39 hex
        { "?/proj/507226561c499d3d167f0b2f03b9035f0816bcZZ", REFKIND_BRANCH }, // 40 non-hex
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
