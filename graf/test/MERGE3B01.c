//
//  MERGE3B01 — Property tests for GRAFMerge3Bytes (the raw-bytes
//  WEAVE 3-way merge primitive driving `graf merge`).
//
//  Each case feeds three blobs (base / ours / theirs) and asserts on
//  the merged output.  Two acceptance modes:
//    expected != NULL          → byte-exact match against `expected`.
//    must_contain[i] != NULL   → every listed substring must be
//                                 present in the output (used for
//                                 cases where the surrounding bytes
//                                 are not pinned).
//
//  Regression: when the inputs contain literal `<<<<` / `||||` /
//  `>>>>` byte sequences in agreeing positions (e.g. source files
//  that quote graf's own conflict markers in strings or comments),
//  those sequences must survive the merge unchanged.  Previously
//  `weave_realign_conflicts` byte-scanned the post-emit output for
//  the marker shape and mis-treated source-literal byte sequences
//  as graf-emitted markers, framing them as a synthetic conflict
//  and stripping the 4-byte runs.  See the `marker_literals_*`
//  cases below.
//

#include "graf/GRAF.h"

#include <stdio.h>
#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

#define M3B_CSV(name, lit)                                                  \
    u8cs name = {(u8cp)(lit), (u8cp)(lit) + ((lit) ? strlen(lit) : 0)}

typedef struct {
    char const *name;
    char const *base;
    char const *ours;
    char const *theirs;
    //  Acceptance mode: prefer `expected` (byte-exact); else assert
    //  every non-NULL `must_contain[i]` is present.
    char const *expected;
    char const *must_contain[8];
    //  When YES, the output must NOT contain any of `<<<<` / `||||` /
    //  `>>>>` — guards against the realigner injecting markers around
    //  source-literal byte sequences.
    b8          must_be_clean;
} M3BCase;

static b8 m3b_contains(u8cs got, char const *needle) {
    size_t nl = strlen(needle);
    size_t gl = (size_t)$len(got);
    if (nl == 0 || gl < nl) return NO;
    for (size_t i = 0; i + nl <= gl; i++) {
        if (memcmp(got[0] + i, needle, nl) == 0) return YES;
    }
    return NO;
}

static b8 m3b_has_marker(u8cs got) {
    return m3b_contains(got, "<<<<") ||
           m3b_contains(got, "||||") ||
           m3b_contains(got, ">>>>");
}

static ok64 m3b_run(M3BCase const *c) {
    sane(1);
    fprintf(stderr, "  %s...", c->name);

    M3B_CSV(ext,    "c");
    M3B_CSV(base,   c->base);
    M3B_CSV(ours,   c->ours);
    M3B_CSV(theirs, c->theirs);

    Bu8 out = {};
    call(u8bMap, out, 1UL << 16);

    ok64 mr = GRAFMerge3Bytes(base, ours, theirs, ext, out);
    if (mr != OK) {
        fprintf(stderr, " FAIL: GRAFMerge3Bytes -> %s\n", ok64str(mr));
        u8bUnMap(out);
        fail(TESTFAIL);
    }

    u8cs got = {u8bDataHead(out),
                u8bDataHead(out) + u8bDataLen(out)};

    if (c->expected != NULL) {
        size_t want_len = strlen(c->expected);
        if ((size_t)$len(got) != want_len ||
            (want_len > 0 && memcmp(got[0], c->expected, want_len) != 0)) {
            fprintf(stderr,
                    " FAIL: merged bytes mismatch\n"
                    "  got  (%zu bytes): '%.*s'\n"
                    "  want (%zu bytes): '%s'\n",
                    (size_t)$len(got), (int)$len(got), (char *)got[0],
                    want_len, c->expected);
            u8bUnMap(out);
            fail(TESTFAIL);
        }
    }

    for (u32 i = 0; i < 8 && c->must_contain[i] != NULL; i++) {
        if (!m3b_contains(got, c->must_contain[i])) {
            fprintf(stderr,
                    " FAIL: missing substring '%s'\n  got: '%.*s'\n",
                    c->must_contain[i],
                    (int)$len(got), (char *)got[0]);
            u8bUnMap(out);
            fail(TESTFAIL);
        }
    }

    if (c->must_be_clean && m3b_has_marker(got)) {
        fprintf(stderr,
                " FAIL: synthetic markers in clean merge\n"
                "  got: '%.*s'\n",
                (int)$len(got), (char *)got[0]);
        u8bUnMap(out);
        fail(TESTFAIL);
    }

    fprintf(stderr, " ok\n");
    u8bUnMap(out);
    done;
}

//  Table-driven cases.  Keep the marker-literal regressions at the
//  top — they are the load-bearing reason this file exists.
static M3BCase const M3B_CASES[] = {
    //  --- Regression: source-literal marker bytes must survive ---
    //
    //  All three sides agree on the `<<<<` / `||||` / `>>>>` strings;
    //  only the surrounding `int` declarations diverge (ours edits x,
    //  theirs edits y).  Output must keep the marker strings intact
    //  and must NOT add any synthetic markers.
    {
        .name   = "marker_literals_inside_strings",
        .base   = "int x = 1;\n"
                  "const char *open = \"<<<<\";\n"
                  "const char *mid  = \"||||\";\n"
                  "const char *clos = \">>>>\";\n"
                  "int y = 2;\n",
        .ours   = "int x = 10;\n"
                  "const char *open = \"<<<<\";\n"
                  "const char *mid  = \"||||\";\n"
                  "const char *clos = \">>>>\";\n"
                  "int y = 2;\n",
        .theirs = "int x = 1;\n"
                  "const char *open = \"<<<<\";\n"
                  "const char *mid  = \"||||\";\n"
                  "const char *clos = \">>>>\";\n"
                  "int y = 20;\n",
        .expected = "int x = 10;\n"
                  "const char *open = \"<<<<\";\n"
                  "const char *mid  = \"||||\";\n"
                  "const char *clos = \">>>>\";\n"
                  "int y = 20;\n",
        .must_be_clean = NO,
    },
    //  Same shape, marker bytes inside a comment block.
    {
        .name   = "marker_literals_inside_comments",
        .base   = "// emit `<<<<`, then `||||`, then `>>>>`.\n"
                  "int a = 1;\n",
        .ours   = "// emit `<<<<`, then `||||`, then `>>>>`.\n"
                  "int a = 10;\n",
        .theirs = "// emit `<<<<`, then `||||`, then `>>>>`.\n"
                  "int a = 1;\nint b = 2;\n",
        .expected = "// emit `<<<<`, then `||||`, then `>>>>`.\n"
                  "int a = 10;\nint b = 2;\n",
        .must_be_clean = NO,
    },
    //  Marker bytes in only one side (theirs adds a new literal-using
    //  line).  The newly-introduced `<<<<` must show up intact in the
    //  output without graf re-framing.
    {
        .name   = "marker_literals_introduced_on_one_side",
        .base   = "int a = 1;\n",
        .ours   = "int a = 10;\n",
        .theirs = "int a = 1;\n"
                  "const char *m = \"<<<<\";\n",
        .expected = "int a = 10;\n"
                  "const char *m = \"<<<<\";\n",
        .must_be_clean = NO,
    },

    //  --- Sanity cases (non-overlapping clean merges) ---
    {
        .name   = "non_overlapping_edits",
        .base   = "int a = 1;\nint b = 2;\nint c = 3;\n",
        .ours   = "int a = 10;\nint b = 2;\nint c = 3;\n",
        .theirs = "int a = 1;\nint b = 2;\nint c = 30;\n",
        .expected = "int a = 10;\nint b = 2;\nint c = 30;\n",
        .must_be_clean = YES,
    },
    {
        .name   = "ours_empty_yields_theirs",
        .base   = "x\n",
        .ours   = "",
        .theirs = "y\n",
        .expected = "y\n",
        .must_be_clean = YES,
    },
    {
        .name   = "theirs_empty_yields_ours",
        .base   = "x\n",
        .ours   = "z\n",
        .theirs = "",
        .expected = "z\n",
        .must_be_clean = YES,
    },
};

#define M3B_NCASES (sizeof(M3B_CASES) / sizeof(M3B_CASES[0]))

ok64 MERGE3Btest(void) {
    sane(1);
    for (size_t i = 0; i < M3B_NCASES; i++) {
        call(m3b_run, &M3B_CASES[i]);
    }
    done;
}

TEST(MERGE3Btest);
