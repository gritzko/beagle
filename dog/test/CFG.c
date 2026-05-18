//  CFG tests — gitconfig-family parser, both pull-mode CFGu8sFeed
//  (one event per call) and the line-iterator CFGu8sDrain.
//
//  Test inputs mirror the syntax described in `git help config` so the
//  parser stays compatible with real-world `.gitconfig`, `.git/config`,
//  and `.gitmodules` blobs.

#include <stdio.h>
#include <string.h>

#include "abc/PRO.h"
#include "abc/PATH.h"
#include "abc/S.h"
#include "abc/TEST.h"
#include "dog/git/CFG.h"

//  --- Helpers --------------------------------------------------------

static b8 slice_eq(u8cs s, char const *lit) {
    size_t l = strlen(lit);
    if ((size_t)u8csLen(s) != l) return NO;
    if (l == 0) return YES;
    return memcmp(s[0], lit, l) == 0;
}

//  --- CFGu8sFeed: low-level event stream tests -----------------------

typedef struct {
    char const *sec;        //  NULL marks end-of-events
    char const *sub;        //  "" iff no subname
    char const *key;        //  empty key ⇒ section change
    char const *val;
} feed_event;

static ok64 feed_case(char const *label, char const *blob_str,
                      feed_event const *want) {
    sane(1);

    a_cstr(blob_view, blob_str);
    a_dup(u8c, blob, blob_view);
    a_pad(u8, cfgbuf, 2048);
    CFGstate s = { .data = {blob[0], blob[1]}, .buf = cfgbuf };

    for (u32 i = 0; want[i].sec != NULL; i++) {
        ok64 o = CFGu8sFeed(&s);
        if (o != OK) {
            fprintf(stderr, "[%s] event #%u: rc=0x%lx (want OK)\n",
                    label, i, (unsigned long)o);
            fail(TESTFAIL);
        }
        if (!slice_eq(s.sec, want[i].sec)) {
            fprintf(stderr,
                    "[%s] #%u sec='%.*s' want='%s'\n",
                    label, i,
                    (int)u8csLen(s.sec),
                    s.sec[0] ? (char *)s.sec[0] : "",
                    want[i].sec);
            fail(TESTFAIL);
        }
        if (!slice_eq(s.sub, want[i].sub)) {
            fprintf(stderr,
                    "[%s] #%u sub='%.*s' want='%s'\n",
                    label, i,
                    (int)u8csLen(s.sub),
                    s.sub[0] ? (char *)s.sub[0] : "",
                    want[i].sub);
            fail(TESTFAIL);
        }
        if (!slice_eq(s.key, want[i].key)) {
            fprintf(stderr,
                    "[%s] #%u key='%.*s' want='%s'\n",
                    label, i,
                    (int)u8csLen(s.key),
                    s.key[0] ? (char *)s.key[0] : "",
                    want[i].key);
            fail(TESTFAIL);
        }
        if (!slice_eq(s.value, want[i].val)) {
            fprintf(stderr,
                    "[%s] #%u val='%.*s' want='%s'\n",
                    label, i,
                    (int)u8csLen(s.value),
                    s.value[0] ? (char *)s.value[0] : "",
                    want[i].val);
            fail(TESTFAIL);
        }
    }

    //  Drain remainder; expect NODATA.
    ok64 tail = CFGu8sFeed(&s);
    if (tail != NODATA) {
        fprintf(stderr, "[%s] tail rc=0x%lx (want NODATA)\n",
                label, (unsigned long)tail);
        fail(TESTFAIL);
    }
    done;
}

//  Empty input — first Feed returns NODATA, sec/sub/key/val empty.
static ok64 CFGFeedEmpty(void) {
    sane(1);
    feed_event want[] = {
        {NULL, NULL, NULL, NULL},   //  no events expected
    };
    call(feed_case, "empty", "", want);
    done;
}

//  Plain section + one key.  From `git help config` Example:
//      [user]
//              name = Foo Bar
static ok64 CFGFeedPlain(void) {
    sane(1);
    feed_event want[] = {
        {"user", "",      "",     ""},          //  section
        {"user", "",      "name", "Foo Bar"},   //  assignment
        {NULL,   NULL,    NULL,   NULL},
    };
    call(feed_case, "plain",
        "[user]\n"
        "\tname = Foo Bar\n",
        want);
    done;
}

//  Dotted section name (no quoted subsection): the whole name with
//  dots becomes `sec`, `sub` stays empty.  Per `git help config`,
//  dotted form is allowed only when the subsection is alnum-safe.
//      [remote.origin]
//              url = ...
static ok64 CFGFeedDotted(void) {
    sane(1);
    feed_event want[] = {
        {"remote.origin", "", "",    ""},
        {"remote.origin", "", "url", "ssh://host/repo.git"},
        {NULL, NULL, NULL, NULL},
    };
    call(feed_case, "dotted",
        "[remote.origin]\n"
        "\turl = ssh://host/repo.git\n",
        want);
    done;
}

//  Quoted subsection (case-sensitive).  From `git help config`:
//      [section "subsection"]
//              key = value
static ok64 CFGFeedQuotedSubsec(void) {
    sane(1);
    feed_event want[] = {
        {"submodule", "vendor/sub", "",     ""},
        {"submodule", "vendor/sub", "path", "vendor/sub"},
        {"submodule", "vendor/sub", "url",  "ssh://host/vendor/sub.git"},
        {NULL, NULL, NULL, NULL},
    };
    call(feed_case, "quoted-subsec",
        "[submodule \"vendor/sub\"]\n"
        "\tpath = vendor/sub\n"
        "\turl = ssh://host/vendor/sub.git\n",
        want);
    done;
}

//  Comments (`#` and `;`) — accepted but not emitted as events.
//  Trailing comments after a value get stripped at val_b_close.
static ok64 CFGFeedComments(void) {
    sane(1);
    feed_event want[] = {
        {"core", "", "",       ""},
        {"core", "", "editor", "vim"},          //  trailing `; …` stripped
        {NULL, NULL, NULL, NULL},
    };
    call(feed_case, "comments",
        "# leading hash comment\n"
        "; leading semi comment\n"
        "[core]\n"
        "\teditor = vim ; trailing comment\n",
        want);
    done;
}

//  Whitespace flexibility: around `=`, around keys, and between
//  section brackets.  From `git help config`'s parsing rules.
static ok64 CFGFeedWhitespace(void) {
    sane(1);
    feed_event want[] = {
        {"core", "", "",          ""},
        {"core", "", "filemode",  "true"},
        {NULL, NULL, NULL, NULL},
    };
    call(feed_case, "whitespace",
        "[ core ]\n"
        "  filemode    =     true   \n",        //  surrounding ws trimmed
        want);
    done;
}

//  Multiple sections back-to-back (no blank-line separator).
static ok64 CFGFeedMultiSections(void) {
    sane(1);
    feed_event want[] = {
        {"a",  "", "",  ""},
        {"a",  "", "k", "1"},
        {"b",  "", "",  ""},
        {"b",  "", "k", "2"},
        {NULL, NULL, NULL, NULL},
    };
    call(feed_case, "multi-sections",
        "[a]\n"
        "\tk = 1\n"
        "[b]\n"
        "\tk = 2\n",
        want);
    done;
}

//  Blank lines between sections; trailing blank tolerated.
static ok64 CFGFeedBlanks(void) {
    sane(1);
    feed_event want[] = {
        {"a",  "", "",  ""},
        {"a",  "", "k", "v"},
        {"b",  "", "",  ""},
        {"b",  "", "k", "v"},
        {NULL, NULL, NULL, NULL},
    };
    call(feed_case, "blanks",
        "[a]\n"
        "\n"
        "\tk = v\n"
        "\n"
        "[b]\n"
        "\tk = v\n"
        "\n",
        want);
    done;
}

//  Quoted value with internal escapes.  `git help config` documents
//  `\n`, `\t`, `\b`, `\"`, `\\`.
static ok64 CFGFeedQuotedEscapes(void) {
    sane(1);
    feed_event want[] = {
        {"alias", "",  "", ""},
        //  Decoded: `echo "hi\nthere\\"` — 17 chars total.  The
        //  parser stores the escape-decoded bytes in value.
        {"alias", "", "say", "echo \"hi\nthere\\\""},
        {NULL, NULL, NULL, NULL},
    };
    call(feed_case, "quoted-escapes",
        "[alias]\n"
        "\tsay = \"echo \\\"hi\\nthere\\\\\\\"\"\n",
        want);
    done;
}

//  CRLF line endings (Windows-saved configs).  hsp includes \r so
//  trailing \r is treated as horizontal space.
static ok64 CFGFeedCRLF(void) {
    sane(1);
    feed_event want[] = {
        {"core", "", "",       ""},
        {"core", "", "editor", "vim"},
        {NULL, NULL, NULL, NULL},
    };
    call(feed_case, "crlf",
        "[core]\r\n"
        "\teditor = vim\r\n",
        want);
    done;
}

//  Malformed: missing closing `]` should surface as CFGBAD on the
//  next Feed.
static ok64 CFGFeedMalformed(void) {
    sane(1);
    a_cstr(src, "[core\n\teditor = vim\n");
    a_dup(u8c, blob, src);
    a_pad(u8, cfgbuf, 1024);
    CFGstate s = { .data = {blob[0], blob[1]}, .buf = cfgbuf };
    ok64 o = CFGu8sFeed(&s);
    if (o != CFGBAD) {
        fprintf(stderr, "malformed: rc=0x%lx (want CFGBAD)\n",
                (unsigned long)o);
        fail(TESTFAIL);
    }
    done;
}

//  --- CFGu8sDrain: line-iterator (sticky section path) ---------------

ok64 CFGDrainTest(void) {
    sane(1);
    static const char blob[] =
        "[submodule \"abc\"]\n"
        "\tpath = lib/abc\n"
        "\turl = https://github.com/gritzko/libabc.git\n"
        "\n"
        "; an ini-style comment line\n"
        "[submodule \"ragel-runtime\"]\n"
        "\tpath = vendor/ragel\n"
        "\turl = git@github.com:colmnet/ragel.git\n"
        "[core]\n"
        "\teditor = vim ; trailing comment\n";

    struct expect {
        const char *section;
        const char *key;
        const char *val;
    } want[] = {
        {"submodule/abc",           "path",   "lib/abc"},
        {"submodule/abc",           "url",    "https://github.com/gritzko/libabc.git"},
        {"submodule/ragel-runtime", "path",   "vendor/ragel"},
        {"submodule/ragel-runtime", "url",    "git@github.com:colmnet/ragel.git"},
        {"core",                    "editor", "vim"},
    };
    int n_want = sizeof(want) / sizeof(want[0]);

    a_pad(u8, section, 256);
    a_pad(u8, cfgbuf, 1024);
    u8bReset(section);

    u8cs ini = {(u8c *)blob, (u8c *)blob + sizeof(blob) - 1};
    int i = 0;
    for (;;) {
        u8cs k = {}, v = {};
        ok64 o = CFGu8sDrain(ini, cfgbuf, section, k, v);
        if (o == NODATA) break;
        if (o != OK) {
            fprintf(stderr, "CFGDrain failed at #%d: %s\n", i, ok64str(o));
            fail(TESTFAIL);
        }
        if (i >= n_want) {
            fprintf(stderr, "CFGDrain produced too many pairs\n");
            fail(TESTFAIL);
        }
        a_dup(u8c, sec, u8bDataC(section));
        if (!slice_eq(sec, want[i].section)) {
            fprintf(stderr,
                    "CFGDrain #%d section: got '%.*s' want '%s'\n",
                    i, (int)u8csLen(sec), (char *)sec[0], want[i].section);
            fail(TESTFAIL);
        }
        if (!slice_eq(k, want[i].key)) {
            fprintf(stderr,
                    "CFGDrain #%d key: got '%.*s' want '%s'\n",
                    i, (int)u8csLen(k), (char *)k[0], want[i].key);
            fail(TESTFAIL);
        }
        if (!slice_eq(v, want[i].val)) {
            fprintf(stderr,
                    "CFGDrain #%d val: got '%.*s' want '%s'\n",
                    i, (int)u8csLen(v), (char *)v[0], want[i].val);
            fail(TESTFAIL);
        }
        ++i;
    }
    if (i != n_want) {
        fprintf(stderr, "CFGDrain produced %d pairs, want %d\n", i, n_want);
        fail(TESTFAIL);
    }
    done;
}

//  --- Main -----------------------------------------------------------

ok64 maintest(void) {
    sane(1);
    fprintf(stderr, "CFGFeedEmpty...\n");           call(CFGFeedEmpty);
    fprintf(stderr, "CFGFeedPlain...\n");           call(CFGFeedPlain);
    fprintf(stderr, "CFGFeedDotted...\n");          call(CFGFeedDotted);
    fprintf(stderr, "CFGFeedQuotedSubsec...\n");    call(CFGFeedQuotedSubsec);
    fprintf(stderr, "CFGFeedComments...\n");        call(CFGFeedComments);
    fprintf(stderr, "CFGFeedWhitespace...\n");      call(CFGFeedWhitespace);
    fprintf(stderr, "CFGFeedMultiSections...\n");   call(CFGFeedMultiSections);
    fprintf(stderr, "CFGFeedBlanks...\n");          call(CFGFeedBlanks);
    fprintf(stderr, "CFGFeedQuotedEscapes...\n");   call(CFGFeedQuotedEscapes);
    fprintf(stderr, "CFGFeedCRLF...\n");            call(CFGFeedCRLF);
    fprintf(stderr, "CFGFeedMalformed...\n");       call(CFGFeedMalformed);
    fprintf(stderr, "CFGDrainTest...\n");           call(CFGDrainTest);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
