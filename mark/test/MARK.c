//  mark tests — table-driven: render snippets, assert HTML content; plus
//  a strict-mode budget-violation case.

#include "mark/MARK.h"

#include "abc/FILE.h"
#include "abc/PATH.h"
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
        {"olist", "#   T\n\n 1. first\n", "<ol>\n<li>first</li>"},
        {"ol-close", "#   T\n\n 1. a\n 2. b\n", "<li>a</li>\n<li>b</li>\n</ol>"},
        {"ol-to-ul", "#   T\n\n 1. a\n -  b\n",
         "</ol>\n<ul>\n<li>b</li>"},
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
        //  DIS-029: a bare 4-space indent opens a <div> with an implied <p>,
        //  the gutter is stripped, and a depth drop closes the div (the
        //  depth-0 line lands in its own <p>).  Whitespace matches the
        //  existing multiline <p> style: <div>\n<p>\ncontent\n</p>\n</div>\n.
        {"div-indent", "#   Header\n    Indented\nNon indented\n",
         "<div>\n<p>\nIndented\n</p>\n</div>\n"},
        {"div-dedent", "#   Header\n    Indented\nNon indented\n",
         "</div>\n<p>\nNon indented\n</p>\n"},
        {"div-nested", "#   Header\n        Deeper\n",
         "<div>\n<div>\n<p>\nDeeper\n</p>\n</div>\n</div>\n"},
        //  depth-restore: indent, dedent to 0, then a list — the div must be
        //  closed before the <ul> opens.
        {"div-restore", "#   T\n\n    Inner\n\n -  item\n",
         "<div>\n<p>\nInner\n</p>\n</div>\n<ul>\n<li>item</li>"},
        //  line-wrapped inline spans: a paragraph's soft line breaks are
        //  joined into one logical line (newline -> space), so a link / image
        //  / emphasis that crosses a wrap is recognized, not split into raw
        //  bracket leakage.  These all used to emit broken HTML.
        {"wrap-para", "#   T\n\nhello\nworld\n", "<p>\nhello world\n</p>"},
        {"wrap-reflink", "#   T\n\nsee [two\nwords][1] x\n\n[1]: x.mkd\n",
         "<a href=\"x.html\">two words</a>"},
        {"wrap-reflink3", "#   T\n\nsee [a\nb\nc][1] x\n\n[1]: x.mkd\n",
         "<a href=\"x.html\">a b c</a>"},
        {"wrap-shortcut", "#   T\n\nsee [Long\nName] x\n\n[Long Name]: p.mkd\n",
         "<a href=\"p.html\">Long Name</a>"},
        {"wrap-image", "#   T\n\nsee ![big\nimage][1] x\n\n[1]: p.png\n",
         "<img src=\"p.png\" alt=\"big image\">"},
        {"wrap-emph", "#   T\n\nsay *bo\nld* x\n", "<strong>bo ld</strong>"},
        //  a wrap between ]/[ is not a full reference link (CommonMark: the
        //  label must immediately follow the text); both halves stay shortcuts.
        //  An undefined non-absolute shortcut keeps the prior empty href.
        {"wrap-bracket-gap", "#   T\n\nsee [text]\n[1] x\n\n[1]: x.mkd\n",
         "<a href=\"\">text</a> <a href=\"x.html\">1</a>"},
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

//  ---- abs/path links: existence-driven extension + relative resolution ----

static ok64 render_at(u8bp out, const char *src, u8cs root, const char *page) {
    sane(out != NULL);
    u8bReset(out);
    u8cs s = {(u8c *)src, (u8c *)src + strlen(src)};
    u8cs title = {};
    markopts opts = {};
    opts.root[0] = root[0];
    opts.root[1] = root[1];
    a_cstr(pg, page ? page : "");
    if (page != NULL) {
        opts.page[0] = pg[0];
        opts.page[1] = pg[1];
    }
    return MARKRenderDoc(out, s, title, opts);
}

static ok64 mk_dir(u8cs root, const char *rel) {
    sane(1);
    a_path(p, root);
    a_cstr(r, rel);
    call(PATHu8bAdd, p, r);
    call(FILEMakeDirP, $path(p));
    done;
}

static ok64 mk_file(u8cs root, const char *rel) {
    sane(1);
    a_path(p, root);
    a_cstr(r, rel);
    call(PATHu8bAdd, p, r);
    int fd = -1;
    call(FILECreate, &fd, $path(p));
    a_cstr(c, "x\n");
    callsafe(FILEFeedAll(fd, c), (void)FILEClose(&fd));
    call(FILEClose, &fd);
    done;
}

static ok64 MARKpathlinks() {
    sane(1);
    call(FILEInit);
    u8b out = {};
    call(u8bAllocate, out, 1UL << 20);

    //  Hermetic fixture tree: only the *targets* probed for existence need to
    //  exist (the page being rendered comes from the `src` string).
    a_cstr(rootname, ".markpathtest");
    a_path(root, rootname);
    FILERmDir($path(root), 1);
    ok64 setup = FILEMakeDirP($path(root));
    a_dup(u8c, rootv, u8bDataC(root));
    if (setup == OK) setup = mk_dir(rootv, "wiki");
    if (setup == OK) setup = mk_file(rootv, "wiki/StrictMark.mkd");
    if (setup == OK) setup = mk_file(rootv, "wiki/Dog.mkd");
    if (setup == OK) setup = mk_file(rootv, "LICENSE");
    if (setup != OK) {
        FILERmDir($path(root), 1);
        u8bFree(out);
        return setup;
    }

    struct {
        const char *name;
        const char *page;
        const char *src;
        const char *needle;
    } T[] = {
        //  extensionless + .mkd source present -> .html, relative to the page
        {"exist-abs", "wiki/Foo.mkd", "#   T\n\nsee [/wiki/StrictMark] x\n",
         "<a href=\"StrictMark.html\">StrictMark</a>"},
        {"exist-mkd", "wiki/Foo.mkd", "#   T\n\nsee [/wiki/StrictMark.mkd] x\n",
         "<a href=\"StrictMark.html\">StrictMark</a>"},
        //  plural suffix glues on outside the anchor (link covers the page)
        {"exist-plural", "wiki/Foo.mkd", "#   T\n\nmany [/wiki/Dog]s x\n",
         "<a href=\"Dog.html\">Dog</a>s"},
        //  a real file with no .mkd source -> verbatim (not .html)
        {"nonpage-file", "wiki/Foo.mkd", "#   T\n\nthe [/LICENSE] x\n",
         "<a href=\"../LICENSE\">LICENSE</a>"},
        //  explicit non-page extension -> verbatim, basename kept
        {"asset-ext", "wiki/Foo.mkd", "#   T\n\nsee [/img/logo.png] x\n",
         "<a href=\"../img/logo.png\">logo.png</a>"},
        //  extensionless with no .mkd/.md source -> verbatim
        {"missing-page", "wiki/Foo.mkd", "#   T\n\nsee [/wiki/Nope] x\n",
         "<a href=\"Nope\">Nope</a>"},
        //  depth: same link resolves differently per page location
        {"from-root", "Home.mkd", "#   T\n\nsee [/wiki/StrictMark] x\n",
         "<a href=\"wiki/StrictMark.html\">StrictMark</a>"},
        {"from-subdir", "blog/Post.mkd", "#   T\n\nsee [/wiki/StrictMark] x\n",
         "<a href=\"../wiki/StrictMark.html\">StrictMark</a>"},
    };
    ok64 ro = OK;
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); ++i) {
        ro = render_at(out, T[i].src, rootv, T[i].page);
        if (ro != OK) break;
        u8cs body = {u8bDataHead(out), u8bIdleHead(out)};
        a_cstr(n, T[i].needle);
        a_dup(u8c, h, body);
        if (u8csFindS(h, n) != OK) {
            fprintf(stderr, "mark pathlink: case '%s' missing '%s'\n",
                    T[i].name, T[i].needle);
            ro = TESTFAIL;
            break;
        }
    }
    FILERmDir($path(root), true);
    u8bFree(out);
    return ro;
}

static ok64 maintest() {
    sane(1);
    call(MARKrender_cases);
    call(MARKlimits);
    call(MARKpathlinks);
    done;
}

TEST(maintest);
