#include "dog/FRAG.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

typedef struct {
    const char *input;
    u8 type;
    const char *body;   // NULL = empty
    u32 line;
    u32 line_end;
    const char *exts[FRAG_MAX_EXTS]; // NULL-terminated
} FRAGCase;

// FRAG was simplified — search-shape fragments (`#name`, `#'body'`,
// `#/regex/`) moved to the projector schemes `spot:`, `grep:`, `regex:`
// (VERBS.md §"View projectors").  FRAG now only classifies line jumps
// and ext filters.
static const FRAGCase FRAG_CASES[] = {

    // --- Line number ---
    {"42",                FRAG_LINE,  NULL, 42, 0, {NULL}},
    {"1",                 FRAG_LINE,  NULL, 1, 0, {NULL}},
    {"999",               FRAG_LINE,  NULL, 999, 0, {NULL}},

    // --- Line range ---
    {"10-20",             FRAG_LINE,  NULL, 10, 20, {NULL}},
    {"1-1000",            FRAG_LINE,  NULL, 1, 1000, {NULL}},

    // --- Line + ext ---
    {"42.c",              FRAG_LINE,  NULL, 42, 0, {"c", NULL}},
    {"10-20.c.h",         FRAG_LINE,  NULL, 10, 20, {"c", "h", NULL}},

    // --- Ext-only ---
    {".c",                FRAG_NONE,  NULL, 0, 0, {"c", NULL}},
    {".c.h",              FRAG_NONE,  NULL, 0, 0, {"c", "h", NULL}},
    {".cpp",              FRAG_NONE,  NULL, 0, 0, {"cpp", NULL}},
};

#define FRAG_NCASES (sizeof(FRAG_CASES) / sizeof(FRAG_CASES[0]))

static b8 frag_eq(u8cs s, const char *expect) {
    if (expect == NULL) return $empty(s);
    size_t elen = strlen(expect);
    if ((size_t)$len(s) != elen) return NO;
    if (elen == 0) return YES;
    return memcmp(s[0], expect, elen) == 0;
}

ok64 FRAGTestTable() {
    sane(1);
    for (size_t i = 0; i < FRAG_NCASES; i++) {
        const FRAGCase *tc = &FRAG_CASES[i];
        u8cs input = {(u8cp)tc->input, (u8cp)tc->input + strlen(tc->input)};

        frag f = {};
        ok64 o = FRAGu8sDrain(input, &f);
        if (o != OK) {
            fprintf(stderr, "FAIL [%zu] '%s': parse error %s\n",
                    i, tc->input, ok64str(o));
            fail(TESTFAIL);
        }

        if (f.type != tc->type) {
            fprintf(stderr, "FAIL [%zu] '%s': type got %d want %d\n",
                    i, tc->input, f.type, tc->type);
            fail(TESTFAIL);
        }

        if (!frag_eq(f.body, tc->body)) {
            fprintf(stderr, "FAIL [%zu] '%s': body got '%.*s' want '%s'\n",
                    i, tc->input,
                    (int)$len(f.body), $empty(f.body) ? "" : (char *)f.body[0],
                    tc->body ? tc->body : "(null)");
            fail(TESTFAIL);
        }

        if (f.line != tc->line) {
            fprintf(stderr, "FAIL [%zu] '%s': line got %u want %u\n",
                    i, tc->input, f.line, tc->line);
            fail(TESTFAIL);
        }

        if (f.line_end != tc->line_end) {
            fprintf(stderr, "FAIL [%zu] '%s': line_end got %u want %u\n",
                    i, tc->input, f.line_end, tc->line_end);
            fail(TESTFAIL);
        }

        // Check ext filters
        for (int e = 0; e < FRAG_MAX_EXTS; e++) {
            if (tc->exts[e] == NULL) {
                if (e != f.nexts) {
                    fprintf(stderr, "FAIL [%zu] '%s': nexts got %d want %d\n",
                            i, tc->input, f.nexts, e);
                    fail(TESTFAIL);
                }
                break;
            }
            if (e >= f.nexts) {
                fprintf(stderr, "FAIL [%zu] '%s': missing ext[%d]='%s'\n",
                        i, tc->input, e, tc->exts[e]);
                fail(TESTFAIL);
            }
            if (!frag_eq(f.exts[e], tc->exts[e])) {
                fprintf(stderr, "FAIL [%zu] '%s': ext[%d] got '%.*s' want '%s'\n",
                        i, tc->input, e,
                        (int)$len(f.exts[e]),
                        $empty(f.exts[e]) ? "" : (char *)f.exts[e][0],
                        tc->exts[e]);
                fail(TESTFAIL);
            }
        }
    }
    done;
}

ok64 FRAGTestEsc() {
    sane(1);
    // Roundtrip: raw → esc → unesc should equal raw
    const char *cases[] = {
        "HUNKu8sFeed",
        "ok64 HUNKu8sFeed(u8s into, hunk const *hk)",
        "tab\there",
        "line\nbreak",
        "hash#middle",
        "pct%already",
        "",
        NULL,
    };
    for (int i = 0; cases[i]; i++) {
        size_t rlen = strlen(cases[i]);
        u8cs raw = {(u8cp)cases[i], (u8cp)cases[i] + rlen};

        u8 ebuf[1024] = {};
        u8s esc = {ebuf, ebuf + sizeof(ebuf)};
        ok64 o = FRAGu8sEsc(esc, raw);
        test(o == OK, TESTFAIL);

        u8cs edata = {ebuf, esc[0]};
        u8 dbuf[1024] = {};
        u8s dec = {dbuf, dbuf + sizeof(dbuf)};
        o = FRAGu8sUnesc(dec, edata);
        test(o == OK, TESTFAIL);

        u8cs ddata = {dbuf, dec[0]};
        if ((size_t)$len(ddata) != rlen ||
            (rlen > 0 && memcmp(ddata[0], cases[i], rlen) != 0)) {
            fprintf(stderr, "FAIL esc roundtrip [%d] '%s'\n", i, cases[i]);
            fail(TESTFAIL);
        }
    }

    // Plain ident should pass through unchanged
    const char *plain = "HUNKu8sFeed";
    u8cs pr = {(u8cp)plain, (u8cp)plain + strlen(plain)};
    u8 pbuf[256] = {};
    u8s ps = {pbuf, pbuf + sizeof(pbuf)};
    call(FRAGu8sEsc, ps, pr);
    u8cs pd = {pbuf, ps[0]};
    test((size_t)$len(pd) == strlen(plain), TESTFAIL);
    test(memcmp(pd[0], plain, strlen(plain)) == 0, TESTFAIL);

    // # and % must be escaped
    const char *special = "#%";
    u8cs sr = {(u8cp)special, (u8cp)special + 2};
    u8 sbuf[32] = {};
    u8s ss = {sbuf, sbuf + sizeof(sbuf)};
    call(FRAGu8sEsc, ss, sr);
    u8cs sd = {sbuf, ss[0]};
    // "#%" → "%23%25" (6 bytes)
    test($len(sd) == 6, TESTFAIL);
    test(memcmp(sd[0], "%23%25", 6) == 0, TESTFAIL);

    done;
}

ok64 FRAGtest() {
    sane(1);
    call(FRAGTestTable);
    call(FRAGTestEsc);
    done;
}

TEST(FRAGtest);
