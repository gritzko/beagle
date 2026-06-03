//  mark tests — table-driven: render snippets, assert HTML content; plus
//  a strict-mode budget-violation case.

#include "mark/MARK.h"

#include "abc/PRO.h"
#include "abc/TEST.h"

#include <string.h>

static ok64 render(u8bp out, const char *src, b8 strict) {
    sane(out != NULL);
    u8bReset(out);
    u8cs s = {(u8c *)src, (u8c *)src + strlen(src)};
    u8cs title = {};
    markopts opts = {.strict = strict};
    return MARKRenderDoc(out, s, title, opts);
}

static b8 contains(u8bp out, const char *needle) {
    u8cs body = {u8bDataHead(out), u8bIdleHead(out)};
    a_cstr(n, needle);
    a_dup(u8c, h, body);
    return u8csFindS(h, n) == OK;
}

typedef struct {
    const char *name;
    const char *src;
    const char *needle;
} tcase;

static ok64 MARKrender_cases() {
    sane(1);
    u8b out = {};
    call(u8bAllocate, out, 1UL << 20);

    tcase T[] = {
        {"h1", "#   Title\n", "<h1>Title</h1>"},
        {"h2", "##  Sub\n", "<h2>Sub</h2>"},
        {"para", "#   T\n\nhello world\n", "<p>"},
        {"bullet", "#   T\n\n -  item one\n", "<li>item one</li>"},
        {"ul", "#   T\n\n -  a\n -  b\n", "<ul>"},
        {"strong", "#   T\n\nsay *bold* now\n", "<strong>bold</strong>"},
        {"emph", "#   T\n\nsay _it_ now\n", "<em>it</em>"},
        {"code", "#   T\n\nuse `x` here\n", "<code>x</code>"},
        {"escape", "#   T\n\na < b & c\n", "a &lt; b &amp; c"},
        {"shortcut", "#   T\n\nsee [Home]\n\n[Home]: Home.mkd\n",
         "<a href=\"Home.html\">Home</a>"},
        {"reflink", "#   T\n\nsee [it][1]\n\n[1]: http://e.com\n",
         "href=\"http://e.com\""},
        {"fence", "#   T\n\n````\n    a<b\n````\n", "<pre><code>"},
        {"fence-esc", "#   T\n\n````\n    a<b\n````\n", "a&lt;b"},
        //  repros: inline bytes outside MKDT's punct set used to MKDTBAD.
        {"question", "#   T\n\nIs it git-based? Yes it is.\n",
         "git-based? Yes it is."},
        {"percent", "#   T\n\n100% git compatible here\n",
         "100% git compatible here"},
        {"emdash", "#   T\n\nold hardware — heavy fuzzing\n",
         "old hardware \xe2\x80\x94 heavy fuzzing"},
        {"uri-q", "#   T\n\nthe `?ref` slot, 50% done\n", "<code>?ref</code>"},
        {"short-suffix", "#   T\n\nan [URI]s link\n\n[URI]: URI.mkd\n",
         "<a href=\"URI.html\">URI</a>s"},
        //  the two link cases, and that collapsed [page][] is NOT special.
        {"explicit-multiword", "#   T\n\nread [two words][1] now\n\n[1]: x.mkd\n",
         "<a href=\"x.html\">two words</a>"},
        {"no-collapsed", "#   T\n\nsee [Home][] now\n\n[Home]: Home.mkd\n",
         "<a href=\"Home.html\">Home</a>[]"},
        //  DIS-014: \* escape emits a literal * (no backslash) and cancels
        //  that char's bracketing role, suppressing the following strong span.
        {"escape-star", "#   T\n\na \\* b *strong* c\n", "a * b *strong* c"},
        //  DIS-014: a 3-dash --- line is a ruler, like ----.
        {"hr3", "#   T\n\nbefore\n\n---\n\nafter\n", "<hr"},
        {"hr4", "#   T\n\nbefore\n\n----\n\nafter\n", "<hr"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); ++i) {
        call(render, out, T[i].src, NO);
        if (!contains(out, T[i].needle)) {
            fprintf(stderr, "mark test: case '%s' missing '%s'\n", T[i].name,
                    T[i].needle);
            u8bFree(out);
            fail(TESTFAIL);
        }
    }
    u8bFree(out);
    done;
}

static ok64 MARKlimits() {
    sane(1);
    u8b out = {};
    call(u8bAllocate, out, 1UL << 20);

    //  A 70-char header under --strict must fail with MARKLIMIT.
    const char *longh =
        "#   "
        "1234567890123456789012345678901234567890123456789012345678901234567890\n";
    try(render, out, longh, YES);
    ok64 ro = __;

    //  The same header without --strict renders fine (warning only).
    ok64 lax = OK;
    try(render, out, longh, NO);
    lax = __;

    u8bFree(out);
    want(ro == MARKLIMIT);
    want(lax == OK);
    done;
}

static ok64 maintest() {
    sane(1);
    call(MARKrender_cases);
    call(MARKlimits);
    done;
}

TEST(maintest);
