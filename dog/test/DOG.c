#include "dog/DOG.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

#include "dog/test/TESTBE.h"

typedef struct {
    const char *input;
    const char *scheme;
    const char *authority;
    const char *path;
    const char *query;
    const char *fragment;
} ParseCase;

static const ParseCase CASES[] = {

    // --- Normalized: bare host:path (no //) ---
    {"localhost:src/git/protocol.h",
        NULL, "localhost", "src/git/protocol.h", NULL, NULL},
    {"origin:docs/README.md",
        NULL, "origin", "docs/README.md", NULL, NULL},
    {"host:path?ref#frag",
        NULL, "host", "path", "ref", "frag"},
    {"localhost:src/git/protocol.h?v2.8.6#protocol_version",
        NULL, "localhost", "src/git/protocol.h",
        "v2.8.6", "protocol_version"},

    // --- Unchanged: proper URIs with // ---
    {"https://example.com/path",
        "https", "//example.com", "/path", NULL, NULL},
    {"file:///etc/passwd",
        "file", "//", "/etc/passwd", NULL, NULL},
    {"//localhost/src/file",
        NULL, "//localhost", "/src/file", NULL, NULL},

    // --- Unchanged: file:/path (scheme with rooted path) ---
    {"file:/etc/passwd",
        "file", NULL, "/etc/passwd", NULL, NULL},

    // --- No scheme: just path ---
    {"src/git/protocol.h",
        NULL, NULL, "src/git/protocol.h", NULL, NULL},
    {"/absolute/path",
        NULL, NULL, "/absolute/path", NULL, NULL},

    // --- Query/fragment only ---
    {"?v2.8.6",
        NULL, NULL, NULL, "v2.8.6", NULL},
    {"#symbol",
        NULL, NULL, NULL, NULL, "symbol"},

    // --- Non-numeric "port" glued back into path ---
    // RFC 3986 eats `src` as the port; we fix it up.
    {"ssh://localhost:src/dogs-sniff",
        "ssh", "//localhost", "src/dogs-sniff", NULL, NULL},
    {"ssh://host:repo",
        "ssh", "//host", "repo", NULL, NULL},
    // Numeric port left alone.
    {"ssh://host:22/repo",
        "ssh", "//host:22", "/repo", NULL, NULL},

    // --- View-projector schemes: exempt from scheme→authority promotion ---
    // `ls:`, `tree:`, `blob:`, `sha1:`, etc. stay as the scheme so
    // `ls:subdir` and `tree:src/?heads/feat` are view projections, not
    // scp-like remote paths.  See https://replicated.wiki/html/wiki/Projector.html §"View projectors".
    {"ls:",                   "ls", NULL, NULL,        NULL,         NULL},
    {"ls:subdir",             "ls", NULL, "subdir",    NULL,         NULL},
    {"ls:./subdir",           "ls", NULL, "./subdir",  NULL,         NULL},
    {"ls:?heads/feat",        "ls", NULL, NULL,        "heads/feat", NULL},
    {"ls:subdir?heads/feat",  "ls", NULL, "subdir",    "heads/feat", NULL},
    {"tree:src/?heads/feat", "tree", NULL, "src/",     "heads/feat", NULL},
    {"blob:file.c?abc",      "blob", NULL, "file.c",   "abc",        NULL},
    {"sha1:?heads/feat",     "sha1", NULL, NULL,       "heads/feat", NULL},
};

#define NCASES (sizeof(CASES) / sizeof(CASES[0]))

static b8 s_eq(u8cs s, const char *expect) {
    if (expect == NULL) return $empty(s);
    size_t elen = strlen(expect);
    if ((size_t)$len(s) != elen) return NO;
    if (elen == 0) return YES;
    return memcmp(s[0], expect, elen) == 0;
}

static b8 check(size_t i, const char *field, u8cs got, const char *expect) {
    if (s_eq(got, expect)) return YES;
    fprintf(stderr, "FAIL [%zu] '%s': %s got '%.*s' want '%s'\n",
            i, CASES[i].input, field,
            (int)$len(got),
            $empty(got) ? "" : (char *)got[0],
            expect ? expect : "(empty)");
    return NO;
}

ok64 DOGTestDOGParseURI() {
    sane(1);
    for (size_t i = 0; i < NCASES; i++) {
        const ParseCase *tc = &CASES[i];
        u8csc text = {(u8cp)tc->input, (u8cp)tc->input + strlen(tc->input)};

        uri u = {};
        ok64 o = DOGParseURI(&u, text);
        if (o != OK) {
            fprintf(stderr, "FAIL [%zu] '%s': parse error %s\n",
                    i, tc->input, ok64str(o));
            fail(TESTFAIL);
        }

        if (!check(i, "scheme",    u.scheme,    tc->scheme))    fail(TESTFAIL);
        if (!check(i, "authority", u.authority, tc->authority)) fail(TESTFAIL);
        if (!check(i, "path",      u.path,      tc->path))      fail(TESTFAIL);
        if (!check(i, "query",     u.query,     tc->query))     fail(TESTFAIL);
        if (!check(i, "fragment",  u.fragment,  tc->fragment))  fail(TESTFAIL);
    }
    done;
}

// --- Canonical round-trip: input → DOGNormalizeArg → DOGCanonURIFeed ---
//
// `expect` is what the canonical byte stream should be.  Covers the full
// pipeline: classification (query/fragment/path), dog normalisations
// (scheme→authority, port-fixup, @host split), and canonicalisation
// (scheme-stripping for transports, `file:` preservation, `//`+path,
// `?/` → `?` trunk fold, `?` strip from fragment).
//
// The query is an opaque local branch path — no `refs/` strip, no
// `heads/main`/`master`/`trunk` aliasing.  Git refname conventions
// live behind keeper/GIT.h's GITParseRef/GITFeedRef and apply only
// at the wire boundary.
typedef struct {
    const char *input;
    const char *expect;   // DOGCanonURIFeed(norm_arg)
} CanonCase;

static const CanonCase CANON_CASES[] = {
    // Scheme preserved — ssh/https/git/file/be all pass through.
    {"ssh://localhost/src/repo?master",    "ssh://localhost/src/repo?master"},
    {"https://localhost/src/repo?master",  "https://localhost/src/repo?master"},
    {"git://localhost/src/repo?master",    "git://localhost/src/repo?master"},
    // `file:` preserved.
    {"file:///etc/passwd",                 "file:///etc/passwd"},
    // Scp-like form: `host:path`, non-numeric port glued back.
    {"ssh://localhost:src/repo?master",    "ssh://localhost/src/repo?master"},
    {"localhost:src/repo?master",          "//localhost/src/repo?master"},
    // User@host form — no `//`, promoted via the @-split rule.
    {"gritzko@pm.me/proj?main",            "//gritzko@pm.me/proj?main"},
    {"gritzko@pm.me?main",                 "//gritzko@pm.me?main"},
    // Already canonical.
    {"//localhost/src/repo?master",        "//localhost/src/repo?master"},
    // Path-only.
    {"/absolute/path",                     "/absolute/path"},
    // Bare token — RFC 3986 path-noscheme; verbs that expect a ref
    // (post, patch) demote path → query themselves.
    {"master",                             "master"},
    {"main",                               "main"},
    {"trunk",                              "trunk"},
    {"feature",                            "feature"},
    // No `refs/` strip — query is opaque.
    {"?refs/heads/main",                   "?refs/heads/main"},
    {"?heads/main",                        "?heads/main"},
    {"?refs/tags/v1.0",                    "?refs/tags/v1.0"},
    {"?tags/v2.8.6",                       "?tags/v2.8.6"},
    // Trunk: `?` and `?/` both fold to `?`.
    {"?",                                  "?"},
    {"?/",                                 "?"},
    // 40-hex SHA — bare path; verbs that expect a sha demote.
    {"0123456789abcdef0123456789abcdef01234567",
        "0123456789abcdef0123456789abcdef01234567"},
    // Whitespace — classified as fragment (commit msg).
    {"fix the typo",                       "#fix the typo"},
    // Whitespace with explicit leading `#` — `#` consumed, body kept.
    {"#fix the typo",                      "#fix the typo"},
    // Whitespace + unknown-scheme prefix (conventional-commit subject).
    // `bro:` is not a known projector or transport; the whole token
    // must classify as fragment, NOT as scheme=bro + path=` …`.
    {"bro: uniformize the navigation",     "#bro: uniformize the navigation"},
    {"feat: add foo",                      "#feat: add foo"},
    // Whitespace fragment with single-quoted spot body.
    {"#'u8sFeed( a, b )'",                 "#'u8sFeed( a, b )'"},
    // POST-007 (1): a commit message carrying a `scheme://` (or any
    // `//`) must round-trip BYTE-IDENTICAL through the canonical
    // pipeline — the message slot is opaque free text, NOT a path, so
    // the doubled slash is NEVER collapsed (`be://` must not become
    // `be:/`).  Both the explicit `#`-led and the whitespace-bypass
    // forms land in the fragment verbatim.
    {"#fix the be:// scheme",              "#fix the be:// scheme"},
    {"fix the be:// scheme",               "#fix the be:// scheme"},
    {"#see be://localhost/x for details",  "#see be://localhost/x for details"},
    // Bare fragment.
    {"#symbol",                            "#symbol"},
    // Bare query (version-like).
    {"?v2.8.6",                            "?v2.8.6"},
    // Numeric port preserved (real port, not a glued path).
    {"ssh://host:22/repo",                 "ssh://host:22/repo"},
    // Trunk-move row: `?#<sha>` — empty-but-present query, non-empty
    // fragment with leading `?` stripped.
    {"?#?0123456789abcdef0123456789abcdef01234567",
        "?#0123456789abcdef0123456789abcdef01234567"},
    // Remote branch observation: scheme + host + path + query +
    // fragment round-trip.
    {"ssh://peer/src/repo?heads/feat#?0123456789abcdef0123456789abcdef01234567",
        "ssh://peer/src/repo?heads/feat#0123456789abcdef0123456789abcdef01234567"},
    // Deletion row: `?branch#` — non-empty query, empty-but-present fragment.
    {"?feature/fix1#",                     "?feature/fix1#"},
    // Search projectors: scheme + body in path slot, whitespace OK.
    {"spot:u8sFeed",                       "spot:u8sFeed"},
    {"grep:u8sFeed",                       "grep:u8sFeed"},
    {"spot:'u8sFeed( a, b )'",             "spot:'u8sFeed( a, b )'"},
    // View projectors round-trip with scheme preserved.
    {"ls:subdir",                          "ls:subdir"},
    {"ls:?heads/feat",                     "ls:?heads/feat"},
    {"ls:?master",                         "ls:?master"},
    {"tree:src/?heads/feat",               "tree:src/?heads/feat"},
    {"sha1:?heads/feat",                   "sha1:?heads/feat"},
};

#define NCANON (sizeof(CANON_CASES) / sizeof(CANON_CASES[0]))

ok64 DOGTestCanonical() {
    sane(1);
    for (size_t i = 0; i < NCANON; i++) {
        const CanonCase *tc = &CANON_CASES[i];
        u8csc text = {(u8cp)tc->input, (u8cp)tc->input + strlen(tc->input)};

        uri u = {};
        ok64 o = DOGNormalizeArg(&u, text);
        if (o != OK) {
            fprintf(stderr, "FAIL [%zu] '%s': normalize error %s\n",
                    i, tc->input, ok64str(o));
            fail(TESTFAIL);
        }

        a_pad(u8, canbuf, 1024);
        call(DOGCanonURIFeed, canbuf, &u);

        a_dup(u8c, got, u8bData(canbuf));
        a_cstr(want, tc->expect);
        if (!$eq(got, want)) {
            fprintf(stderr, "FAIL [%zu] '%s':\n  got    '%.*s'\n  expect '%s'\n",
                    i, tc->input,
                    (int)$len(got), (char *)got[0], tc->expect);
            fail(TESTFAIL);
        }
    }
    done;
}

// --- DOGNormalizeArg: `#`-led message → fragment verbatim ----------
//
// POST-002 repro.  A `#`-led arg is a commit message: the whole body
// AFTER the `#` lands in `u->fragment` verbatim (newlines, dots, and
// embedded `//` included), with EMPTY path/query/authority/scheme.
// The pre-`#` byte is stripped exactly as the URILexer fragment rule
// does (`#fix` → `fix`).  Before the fix a multi-line dotted body
// (e.g. a `Co-Authored-By:` trailer) spilled its tail into `u->path`
// and POST refused it as path-form.
typedef struct {
    const char *input;
    const char *fragment;   // whole post-`#` body, verbatim
} MsgFragCase;

static const MsgFragCase MSGFRAG_CASES[] = {
    // Plain single-line message.
    {"#fix",                                 "fix"},
    // Multi-line body with a dotted Co-Authored-By trailer — the bug.
    {"#a\n\nCo-Authored-By: X <a@b.c>",      "a\n\nCo-Authored-By: X <a@b.c>"},
    // Body containing a `//` transport-ish marker — kept verbatim, no
    // `//`-collapse / normalization.
    {"#see be://localhost/x for details",    "see be://localhost/x for details"},
    // POST-007 (1) repro: a `scheme://` in the subject must NOT have
    // its doubled slash collapsed (`be://` must stay `be://`, never
    // `be:/`).  The message slot is opaque text — no path normalization.
    {"#fix the be:// scheme",                "fix the be:// scheme"},
    {"#be://x",                              "be://x"},
    {"#a//b",                                "a//b"},
    // Dotted single-line subject.
    {"#refactor URI.c.rl and DOG.c",         "refactor URI.c.rl and DOG.c"},
    // Empty message (`#` alone) → present-but-empty fragment.
    {"#",                                     ""},
};

#define NMSGFRAG (sizeof(MSGFRAG_CASES) / sizeof(MSGFRAG_CASES[0]))

ok64 DOGTestMsgFragment() {
    sane(1);
    for (size_t i = 0; i < NMSGFRAG; i++) {
        const MsgFragCase *tc = &MSGFRAG_CASES[i];
        u8csc text = {(u8cp)tc->input, (u8cp)tc->input + strlen(tc->input)};

        uri u = {};
        ok64 o = DOGNormalizeArg(&u, text);
        if (o != OK) {
            fprintf(stderr, "FAIL [%zu] '%s': normalize error %s\n",
                    i, tc->input, ok64str(o));
            fail(TESTFAIL);
        }
        struct { const char *field; u8cs got; const char *want; } slots[] = {
            {"fragment",  {u.fragment[0],  u.fragment[1]},  tc->fragment},
            {"path",      {u.path[0],      u.path[1]},      NULL},
            {"query",     {u.query[0],     u.query[1]},     NULL},
            {"authority", {u.authority[0], u.authority[1]}, NULL},
            {"scheme",    {u.scheme[0],    u.scheme[1]},    NULL},
        };
        for (size_t k = 0; k < sizeof(slots)/sizeof(slots[0]); k++) {
            if (s_eq(slots[k].got, slots[k].want)) continue;
            fprintf(stderr, "FAIL [%zu] '%s': %s got '%.*s' want '%s'\n",
                    i, tc->input, slots[k].field,
                    (int)$len(slots[k].got),
                    $empty(slots[k].got) ? "" : (char *)slots[k].got[0],
                    slots[k].want ? slots[k].want : "(empty)");
            fail(TESTFAIL);
        }
    }
    done;
}

// Folding leaves through DOGChildPathHash from ROOT must match
// DOGPathHash on the joined path.
ok64 DOGTestPathHash() {
    sane(1);
    a_cstr(a, "a");
    a_cstr(b, "b");
    a_cstr(c, "c");
    u64 step = DOGChildPathHash(a, ROOT);
    step = DOGChildPathHash(b, step);
    step = DOGChildPathHash(c, step);

    a_cstr(abc, "a/b/c");
    u64 whole = DOGPathHash(abc);
    if (step != whole) {
        fprintf(stderr, "FAIL hash(a/b/c)=%llx step=%llx\n",
                (unsigned long long)whole, (unsigned long long)step);
        fail(TESTFAIL);
    }

    // Empty path (and "/") must equal ROOT.
    a_cstr(empty, "");
    if (DOGPathHash(empty) != ROOT) fail(TESTFAIL);
    a_cstr(slash, "/");
    if (DOGPathHash(slash) != ROOT) fail(TESTFAIL);

    // Repeated/leading/trailing slashes must not change the hash.
    a_cstr(messy, "/a//b/c/");
    if (DOGPathHash(messy) != whole) fail(TESTFAIL);
    done;
}

// --- DOGutf8sFeedDate: short relative date format -----------------
//
// All cases anchor on `now = 2025-04-30 12:00:00 UTC` (1746014400, a
// Wednesday) so the table reads as wall-clock minus a known reference.

typedef struct {
    i64         ts;
    char const *expect;
} DateCase;

static i64 const DOG_DATE_NOW = 1746014400;  // 2025-04-30 12:00:00 UTC (Wed)

//  All outputs padded to 7 cols, centred (lead = (7-len)/2, trail rest).
static DateCase const DATE_CASES[] = {
    //  Same day / < 12hr → "HH:MM" in UTC (anchor is 12:00 UTC).
    {DOG_DATE_NOW,            " 12:00 "},   // exact now
    {DOG_DATE_NOW - 30,       " 11:59 "},   // 30s ago
    {DOG_DATE_NOW - 120,      " 11:58 "},   // 2 min
    {DOG_DATE_NOW - 59 * 60,  " 11:01 "},   // just under 1hr
    {DOG_DATE_NOW - 3600,     " 11:00 "},   // 1 hr
    //  23hr back: not <12hr and Tue 2025-04-29 13:00 (different
    //  calendar day) → weekday + DD bucket.
    {DOG_DATE_NOW - 23*3600,  " Tue29 "},
    //  3 days back from Wed = Sun 2025-04-27.
    {DOG_DATE_NOW - 3*86400,  " Sun27 "},
    //  10 days back: < 6 months → "DDMon".  2025-04-20 → "20Apr".
    {DOG_DATE_NOW - 10*86400, " 20Apr "},
    //  ~14 months back: not <6mo and not same year → "DDMonYY".
    //  2024-02-19 12:00 UTC.
    {1708344000,              "19Feb24"},
    //  ts <= 0: "?"
    {0,                       "   ?   "},
    {-1,                      "   ?   "},
};

#define NDATE (sizeof(DATE_CASES) / sizeof(DATE_CASES[0]))

ok64 DOGTestFeedDate() {
    sane(1);
    //  DATE_CASES expectations are anchored to UTC wall-clock; pin
    //  TZ so localtime() in DOGutf8sFeedDate matches regardless of
    //  the build host's locale.
    setenv("TZ", "UTC", 1);
    tzset();
    for (size_t i = 0; i < NDATE; i++) {
        DateCase const *tc = &DATE_CASES[i];
        a_pad(u8, buf, 16);
        call(DOGutf8sFeedDate, u8bIdle(buf), tc->ts, DOG_DATE_NOW);
        a_dup(u8c, got, u8bData(buf));
        a_cstr(want, tc->expect);
        if ($len(got) != 7) {
            fprintf(stderr, "FAIL [%zu] ts=%lld: '%.*s' is %d chars; want exactly 7\n",
                    i, (long long)tc->ts,
                    (int)$len(got), (char *)got[0], (int)$len(got));
            fail(TESTFAIL);
        }
        if (!$eq(got, want)) {
            fprintf(stderr, "FAIL [%zu] ts=%lld:\n  got    '%.*s'\n  expect '%s'\n",
                    i, (long long)tc->ts,
                    (int)$len(got), (char *)got[0], tc->expect);
            fail(TESTFAIL);
        }
    }
    done;
}

// --- canonical-query parse helpers ----------------------------------

static b8 slice_eq_cstr_dog(u8cs s, char const *c) {
    size_t n = strlen(c);
    if (u8csLen(s) != n) return NO;
    if (n == 0) return YES;
    return memcmp(s[0], (u8 const *)c, n) == 0;
}

// --- DOGIsFullSha (URI-001 Stage 2 length-agnostic recognizer) ------

static ok64 DOGTestIsFullSha(void) {
    sane(1);
    char sha1hex[]   = "d268baf01234567890123456789012345678abcd";  // 40
    char sha256hex[] = "d268baf01234567890123456789012345678abcd"
                       "0123456789abcdef01234567";                  // 40+24 = 64
    struct { char const *in; b8 want; } cases[] = {
        {sha1hex,    YES},   // 40 hex  → sha1
        {sha256hex,  YES},   // 64 hex  → sha256
        {"abc1234",  NO},    // hashlet, not full
        {"feat",     NO},    // branch name
        {"feat/sub", NO},    // path
        {"",         NO},    // empty
        // 40 chars but a non-hex byte ('g') → not a sha
        {"g268baf01234567890123456789012345678abcd", NO},
        // 39 hex (one short of sha1)
        {"268baf01234567890123456789012345678abcd",  NO},
        // 41 hex (one over sha1, under sha256)
        {"d268baf01234567890123456789012345678abcd1", NO},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        a_cstr(src, cases[i].in);
        a_dup(u8c, in, src);
        b8 got = DOGIsFullSha(in);
        if (got != cases[i].want) {
            fprintf(stderr, "IsFullSha '%s': want %d got %d\n",
                    cases[i].in, cases[i].want, got);
            fail(FAIL);
        }
    }
    done;
}

// --- DOGIsHashlet (6..40 hex prefix recognizer) ---------------------

static ok64 DOGTestIsHashlet(void) {
    sane(1);
    struct { char const *in; b8 want; } cases[] = {
        {"abc123",   YES},   // 6 hex  → min hashlet
        {"d268baf01234567890123456789012345678abcd", YES},  // 40 hex
        {"DEADBEEF", YES},   // uppercase ok
        {"abc12",    NO},    // 5 hex  → one short of min
        {"abcde",    NO},    // 5 hex
        {"",         NO},    // empty
        {"feat",     NO},    // 4 chars, branch name
        // 40 chars but a non-hex byte ('g') → not a hashlet
        {"g268baf01234567890123456789012345678abcd", NO},
        // 41 hex → one over max width
        {"d268baf01234567890123456789012345678abcd1", NO},
        {"abc/12",   NO},    // path-ish, non-hex byte
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        a_cstr(src, cases[i].in);
        a_dup(u8c, in, src);
        b8 got = DOGIsHashlet(in);
        if (got != cases[i].want) {
            fprintf(stderr, "IsHashlet '%s': want %d got %d\n",
                    cases[i].in, cases[i].want, got);
            fail(FAIL);
        }
    }
    done;
}

// --- DOGCanonQueryParse ---------------------------------------------
//
//  The canonical resolved query splits into (project, branch, pin).  The
//  three structural shapes (URI.mkd "Ref shapes"; user-confirmed):
//    /<proj>/<sha>          TRUNK    → branch empty, pin = sha
//    /<proj>//<sha>         DETACHED → branch empty, pin = sha
//    /<proj>/<branch>/<sha> BRANCH   → branch = path, pin = sha
//  (trunk vs detached are byte-distinct but both parse to an empty
//  branch — REFSQueryKind owns that distinction; this parser only
//  extracts project/branch/pin.)

#define CQ_H40 "507226561c499d3d167f0b2f03b9035f0816bc82"

static ok64 DOGTestCanonQueryParse(void) {
    sane(1);
    struct {
        char const *in;
        b8          want_ok;
        char const *want_proj;
        char const *want_branch;
        char const *want_pin;
    } cases[] = {
        //  trunk waypoint: single slash → empty branch
        {"/proj/" CQ_H40,              YES, "proj", "",         CQ_H40},
        //  detached: double slash → empty branch
        {"/proj//" CQ_H40,             YES, "proj", "",         CQ_H40},
        //  named branch + nested branch
        {"/proj/feat/" CQ_H40,         YES, "proj", "feat",     CQ_H40},
        {"/proj/feat/fix/" CQ_H40,     YES, "proj", "feat/fix", CQ_H40},
        //  not canonical
        {"feat",                       NO,  "", "", ""},        // no leading /
        {"/proj/feat",                 NO,  "", "", ""},        // no 40-hex pin
        {"/proj/" "5072265",           NO,  "", "", ""},        // short pin
        {"//" CQ_H40,                  NO,  "", "", ""},        // empty project
        {"",                           NO,  "", "", ""},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        a_cstr(src, cases[i].in);
        u8cs proj = {}, branch = {}, pin = {};
        b8 ok = DOGCanonQueryParse(src, proj, branch, pin);
        if (ok != cases[i].want_ok) {
            fprintf(stderr, "CanonQueryParse '%s': ok want %d got %d\n",
                    cases[i].in, cases[i].want_ok, ok);
            fail(FAIL);
        }
        if (!ok) continue;
        if (!slice_eq_cstr_dog(proj, cases[i].want_proj) ||
            !slice_eq_cstr_dog(branch, cases[i].want_branch) ||
            !slice_eq_cstr_dog(pin, cases[i].want_pin)) {
            fprintf(stderr,
                "CanonQueryParse '%s': got (%.*s, %.*s, %.*s) want (%s, %s, %s)\n",
                cases[i].in,
                (int)u8csLen(proj), (char *)proj[0],
                (int)u8csLen(branch), (char *)branch[0],
                (int)u8csLen(pin), (char *)pin[0],
                cases[i].want_proj, cases[i].want_branch, cases[i].want_pin);
            fail(FAIL);
        }
    }
    done;
}

//  DOGIsTransport / DOGIsGitTransport scheme classification (DIS-019).
//  Git transports (ssh/https/http/git) speak git-{upload,receive}-pack;
//  be/keeper speak the beagle protocol; file is a local exec.  A be-only
//  synthetic dot-branch must never be pushed to a git transport.
static ok64 DOGTestGitTransport(void) {
    sane(1);
    struct {
        char const *scheme;
        b8          is_transport;
        b8          is_git;
    } cases[] = {
        {"ssh",    YES, YES},
        {"https",  YES, YES},
        {"http",   YES, YES},
        {"git",    YES, YES},
        {"be",     YES, NO},    // beagle protocol — dot-branch is fine
        {"keeper", NO,  NO},    // not even a known transport here
        {"file",   YES, NO},    // local exec; path decides git-vs-keeper
        {"sha1",   NO,  NO},    // projector, not a transport
        {"",       NO,  NO},    // empty
        {"bogus",  NO,  NO},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        a_cstr(s, cases[i].scheme);
        a_dup(u8c, sc, s);
        b8 t = DOGIsTransport(sc);
        a_dup(u8c, sc2, s);
        b8 g = DOGIsGitTransport(sc2);
        if (t != cases[i].is_transport) {
            fprintf(stderr, "DOGIsTransport('%s') want %d got %d\n",
                    cases[i].scheme, cases[i].is_transport, t);
            fail(FAIL);
        }
        if (g != cases[i].is_git) {
            fprintf(stderr, "DOGIsGitTransport('%s') want %d got %d\n",
                    cases[i].scheme, cases[i].is_git, g);
            fail(FAIL);
        }
        //  A git transport is always a transport; the converse may not
        //  hold (be/file).  Pin the invariant.
        if (g && !t) {
            fprintf(stderr, "DOGIsGitTransport('%s') YES but "
                    "DOGIsTransport NO — invariant broken\n",
                    cases[i].scheme);
            fail(FAIL);
        }
    }
    done;
}

//  DOGIsProjector / DOGProjectorDog lookup over the u8slit DOG_PROJECTORS
//  table.  `dog` is NULL iff the scheme isn't a projector; this also pins
//  the empty-scheme sentinel walk (empty/unknown -> not a projector).
static ok64 DOGTestProjector(void) {
    sane(1);
    struct {
        char const *scheme;
        char const *dog;     // expected dog, or NULL if not a projector
    } cases[] = {
        {"sha1",   "keeper"},
        {"tree",   "keeper"},
        {"log",    "graf"},
        {"diff",   "graf"},
        {"ls",     "sniff"},
        {"status", "sniff"},
        {"spot",   "spot"},
        {"regex",  "spot"},
        {"file",   NULL},     // transport, not a projector
        {"be",     NULL},
        {"",       NULL},     // empty — sentinel walk
        {"bogus",  NULL},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        a_cstr(s, cases[i].scheme);
        a_dup(u8c, sc, s);
        b8 is = DOGIsProjector(sc);
        a_dup(u8c, sc2, s);
        char const *dog = DOGProjectorDog(sc2);
        b8 want = cases[i].dog != NULL;
        if (is != want) {
            fprintf(stderr, "DOGIsProjector('%s') want %d got %d\n",
                    cases[i].scheme, want, is);
            fail(FAIL);
        }
        if (want) {
            if (dog == NULL || strcmp(dog, cases[i].dog) != 0) {
                fprintf(stderr, "DOGProjectorDog('%s') want '%s' got '%s'\n",
                        cases[i].scheme, cases[i].dog, dog ? dog : "(null)");
                fail(FAIL);
            }
        } else if (dog != NULL) {
            fprintf(stderr, "DOGProjectorDog('%s') want (null) got '%s'\n",
                    cases[i].scheme, dog);
            fail(FAIL);
        }
    }
    done;
}

//  DOGSuggestCommand (BE-002) — "did you mean" over the projector
//  vocabulary plus the caller's extra verb list.  Threshold is edit
//  distance <=2 AND < len(word); the closest in-threshold name wins,
//  and the returned slice must point into the static table / cstr
//  storage (we deref it after the call returns, which would crash on a
//  dangling stack slice).
static char const *const SUGGEST_VERBS[] = {
    "head", "get", "post", "put", "delete", "patch", NULL
};

static ok64 DOGTestSuggestCommand(void) {
    sane(1);
    struct {
        char const *word;
        b8          want_hit;
        char const *want_sugg;   // when want_hit
    } cases[] = {
        //  Mistyped projectors (within 1-2 edits).
        {"difff",  YES, "diff"},    // BE-002 headline case (1 deletion)
        {"dif",    YES, "diff"},    // 1 deletion
        {"statu",  YES, "status"},  // 1 deletion
        {"grepp",  YES, "grep"},    // 1 insertion
        //  Mistyped verbs (extra list).
        {"pos",    YES, "post"},    // 1 deletion
        {"psot",   YES, "post"},    // 2-edit tie post(verb) vs spot(proj):
                                    // verb wins (scanned first) — BE-002
        {"delet",  YES, "delete"},  // one deletion
        {"ptch",   YES, "patch"},   // one deletion
        //  Exact names ARE within distance 0 — but d < len(word) holds,
        //  so an exact verb/projector still "suggests" itself.  (The be
        //  dispatcher never reaches the suggester for a real command;
        //  this just pins the math.)
        {"diff",   YES, "diff"},
        {"post",   YES, "post"},
        //  Far-off garbage — no suggestion.
        {"totallynotacommand", NO, NULL},
        {"xyzzy",  NO, NULL},
        //  Too-short word: threshold `d < len(word)` rejects a 2-char
        //  word that is 2 edits from everything; even `ge` (1 from
        //  `get`) needs d < 2, so dist-1 passes but dist-2 fails.
        {"ge",     YES, "get"},     // 1 edit, 1 < 2 → ok
        {"zz",     NO, NULL},       // 2+ edits from all 2-char-window cmds
        {"",       NO, NULL},       // empty word
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        a_cstr(src, cases[i].word);
        a_dup(u8c, w, src);
        u8cs out = {};
        b8 hit = DOGSuggestCommand(w, SUGGEST_VERBS, out);
        if (hit != cases[i].want_hit) {
            fprintf(stderr, "Suggest '%s': hit want %d got %d (got '%.*s')\n",
                    cases[i].word, cases[i].want_hit, hit,
                    (int)$len(out), $empty(out) ? "" : (char *)out[0]);
            fail(TESTFAIL);
        }
        if (hit && !s_eq(out, cases[i].want_sugg)) {
            fprintf(stderr, "Suggest '%s': got '%.*s' want '%s'\n",
                    cases[i].word, (int)$len(out),
                    $empty(out) ? "" : (char *)out[0], cases[i].want_sugg);
            fail(TESTFAIL);
        }
        //  When a hit fires, the returned slice must be non-NULL and
        //  point at readable static storage (we already deref'd it above
        //  for s_eq; ASan would have caught a dangling stack slice).
        if (hit && out[0] == NULL) {
            fprintf(stderr, "Suggest '%s': hit but NULL out\n", cases[i].word);
            fail(TESTFAIL);
        }
    }
    //  NULL extra list must still work (projector-only vocabulary).
    {
        a_cstr(src, "difff");
        a_dup(u8c, w, src);
        u8cs out = {};
        b8 hit = DOGSuggestCommand(w, NULL, out);
        if (!hit || !s_eq(out, "diff")) {
            fprintf(stderr, "Suggest 'difff' (NULL extra): hit=%d '%.*s'\n",
                    hit, (int)$len(out), $empty(out) ? "" : (char *)out[0]);
            fail(TESTFAIL);
        }
    }
    done;
}

// --- DOGRepoFromBe / DOGProjectFromBe / DOGBranchFromBe -------------
//
//  The three `/.be/`-splitting anchor scanners.  MEM-017: the loop
//  bound was `end = p[1]-4` while the probe reads `q[0..4]` (5 bytes),
//  so on the final iteration `q[4]` dereferenced `*p[1]` — one byte
//  past the slice.  This fires whenever the path *ends* in `/.be`
//  (the legacy single-project anchor): the `/.be/` literal is not
//  found (no trailing slash), and the last loop start sits at the
//  `.` of `.be`, reading one byte past the end.
//
//  To make the over-read observable under ASan we feed a
//  HEAP-allocated, NON-NUL-terminated slice whose bytes end exactly
//  at `…/.be`; the byte past `p[1]` then lands in the malloc redzone.
//  (URI/ULOG field slices are non-terminated in production, hence the
//  real OOB.)

typedef struct {
    char const *input;      // path fed to the *FromBe scanners
    char const *repo;       // DOGRepoFromBe result
    char const *project;    // DOGProjectFromBe result
    char const *branch;     // DOGBranchFromBe result
} BeCase;

static BeCase const BE_CASES[] = {
    //  Sharded anchors: `/.be/<project>[/<branch>]`.
    {"/abs/path/.be/beagle",        "/abs/path", "beagle", "beagle"},
    {"/abs/path/.be/beagle/",       "/abs/path", "beagle", "beagle"},
    {"/abs/path/.be/beagle/feat",   "/abs/path", "beagle", "beagle/feat"},
    {"/abs/path/.be/beagle/feat/x", "/abs/path", "beagle", "beagle/feat/x"},
    //  Bare `/.be/` — project elided (legacy single-project).
    {"/abs/path/.be/",              "/abs/path", "",       ""},
    //  *** MEM-017 over-read trigger ***  Path ENDS in `/.be` with no
    //  trailing slash: `/.be/` literal is absent, last probe start is
    //  the `.` of `.be`, `q[4]` reads one past the slice end.
    {"/abs/path/.be",               "/abs/path", "",       ""},
    {"/x/.be",                      "/x",        "",       ""},
    //  No `/.be/` at all — fallback path, no over-read but pins behaviour.
    {"/abs/path/repo",              "/abs/path/repo", "",  ""},
    //  GET-004: a doubled store dir `/.be/.be/` must NOT yield `.be` as
    //  the project — the store dir is never a project.  DOGProjectFromBe
    //  drops the `.be` segment and reports the project as elided ("").
    //  (Branch still carries the after-first-`/.be/` tail per the legacy
    //  path encoding; project is the load-bearing guarantee here.)
    {"/abs/path/.be/.be/",          "/abs/path", "",       ".be"},
    {"/abs/path/.be/.be",           "/abs/path", "",       ".be"},
};

#define NBE (sizeof(BE_CASES) / sizeof(BE_CASES[0]))

static ok64 DOGTestFromBe(void) {
    sane(1);
    for (size_t i = 0; i < NBE; i++) {
        BeCase const *tc = &BE_CASES[i];
        size_t n = strlen(tc->input);
        //  Heap copy WITHOUT a NUL terminator: the byte after the
        //  slice end is a malloc redzone, so any over-read past `p[1]`
        //  is caught by ASan.  This mirrors the non-NUL-terminated
        //  URI/ULOG field slices the scanners run on in production.
        u8 *raw = (u8 *)malloc(n ? n : 1);
        if (n) memcpy(raw, tc->input, n);
        u8cs in = {raw, raw + n};

        a_pad(u8, repobuf, 256);
        a_pad(u8, projbuf, 256);
        a_pad(u8, brbuf, 256);

        a_dup(u8c, in1, in);
        DOGRepoFromBe(in1, repobuf);
        a_dup(u8c, in2, in);
        DOGProjectFromBe(in2, projbuf);
        a_dup(u8c, in3, in);
        DOGBranchFromBe(in3, brbuf);

        a_dup(u8c, repo_got, u8bData(repobuf));
        a_dup(u8c, proj_got, u8bData(projbuf));
        a_dup(u8c, br_got,   u8bData(brbuf));

        b8 bad = NO;
        if (!s_eq(repo_got, tc->repo)) {
            fprintf(stderr, "FromBe[%zu] '%s' repo: got '%.*s' want '%s'\n",
                    i, tc->input, (int)$len(repo_got),
                    $empty(repo_got) ? "" : (char *)repo_got[0], tc->repo);
            bad = YES;
        }
        if (!s_eq(proj_got, tc->project)) {
            fprintf(stderr, "FromBe[%zu] '%s' project: got '%.*s' want '%s'\n",
                    i, tc->input, (int)$len(proj_got),
                    $empty(proj_got) ? "" : (char *)proj_got[0], tc->project);
            bad = YES;
        }
        if (!s_eq(br_got, tc->branch)) {
            fprintf(stderr, "FromBe[%zu] '%s' branch: got '%.*s' want '%s'\n",
                    i, tc->input, (int)$len(br_got),
                    $empty(br_got) ? "" : (char *)br_got[0], tc->branch);
            bad = YES;
        }
        free(raw);
        if (bad) fail(TESTFAIL);
    }
    done;
}

// --- URI-002 bang factor: DOGDebangSlice / DOGDebang / DOGDebangFeed --
//
//  REPRO-FIRST (CLAUDE.md §17).  One bitflag byte, one debanger; every
//  parser debangs uniformly; `be` (the sole canonicalizer) re-emits the
//  `!` so the bang survives a resolve.  The cross-process leg (a bang on
//  a forwarded/persisted/wired URI honored by the receiving dog, and a
//  bang surviving `be`'s canonicalize) is covered end-to-end by the
//  shell cases test/patch/10-squash-foster (`be patch ?feat!`) and
//  test/get/41-force-get; this table pins the dog-side primitives the
//  whole migration funnels through.

//  (1) DOGDebangSlice — the typed tail-shed primitive.  Sheds at most
//  ONE trailing `!`; a present-but-empty slice is preserved; `!!` keeps
//  its leading `!` (the receiver's POSTBANG case).
static ok64 DOGTestDebangSlice(void) {
    sane(1);
    struct {
        char const *in;
        b8          want_shed;   //  DOGDebangSlice return
        char const *want_out;    //  slice bytes after the shed
    } cases[] = {
        {"feat!",     YES, "feat"},     //  branch ref + whole-branch bang
        {"feat",      NO,  "feat"},     //  no bang
        {"",          NO,  ""},         //  empty: nothing to shed
        {"!",         YES, ""},         //  `?!` body → bang + empty (present)
        {"fix it!!",  YES, "fix it!"},  //  only ONE `!` shed (POSTBANG tail)
        {"a!b",       NO,  "a!b"},      //  interior `!` is not a modifier
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        a_cstr(src, cases[i].in);
        a_dup(u8c, s, src);
        b8 shed = DOGDebangSlice(s);
        if (shed != cases[i].want_shed) {
            fprintf(stderr, "DebangSlice '%s': shed want %d got %d\n",
                    cases[i].in, cases[i].want_shed, shed);
            fail(TESTFAIL);
        }
        if (!s_eq(s, cases[i].want_out)) {
            fprintf(stderr, "DebangSlice '%s': out got '%.*s' want '%s'\n",
                    cases[i].in, (int)$len(s),
                    $empty(s) ? "" : (char *)s[0], cases[i].want_out);
            fail(TESTFAIL);
        }
        //  Present-but-empty must stay PRESENT (non-NULL) — `?!` → empty
        //  query, not absent.  Pins the `?!` round-trip the model needs.
        if (!u8csEmpty(src) && s[0] == NULL) {
            fprintf(stderr, "DebangSlice '%s': slice went NULL\n", cases[i].in);
            fail(TESTFAIL);
        }
    }
    done;
}

//  (2) DOGDebang on a full forwarded URI — the receiving dog parses the
//  literal text (DOGParseURI), then the uniform debanger extracts the
//  per-component bits AND strips the `!` from the component bytes.  This
//  is exactly how every migrated reader (sniff PATCH/AT, keeper WIRECLI,
//  SNIFF.exe forget) now honors a bang that rode through as text.
static ok64 DOGTestDebangURI(void) {
    sane(1);
    struct {
        char const *uri;       //  full forwarded URI text (`!` rides along)
        u8          want_bang; //  bits DOGDebang should set
        char const *want_path;
        char const *want_query;
        char const *want_frag;
    } cases[] = {
        //  `?feat!` — query-bang (PATCH whole-branch); query debangs to `feat`.
        {"?feat!",   DOG_BANG_QUERY, NULL, "feat", NULL},
        //  `#msg!` — fragment-bang (forget); fragment debangs to `msg`.
        {"#msg!",    DOG_BANG_FRAG,  NULL, NULL,   "msg"},
        //  Bare path with a trailing `!` — path-bang.
        {"path!",    DOG_BANG_PATH,  "path", NULL, NULL},
        //  No bang anywhere.
        {"?feat",    0,              NULL, "feat", NULL},
        //  `?!` — present-but-empty query keeps the bang and stays present.
        {"?!",       DOG_BANG_QUERY, NULL, "",     NULL},
        //  Resolved-sha query with a whole-branch bang (the persisted
        //  patch-row / wire shape `?<sha>!`).
        {"?0123456789abcdef0123456789abcdef01234567!",
                     DOG_BANG_QUERY, NULL,
                     "0123456789abcdef0123456789abcdef01234567", NULL},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        u8csc text = {(u8cp)cases[i].uri, (u8cp)cases[i].uri + strlen(cases[i].uri)};
        uri u = {};
        ok64 o = DOGParseURI(&u, text);
        if (o != OK) {
            fprintf(stderr, "DebangURI '%s': parse %s\n", cases[i].uri, ok64str(o));
            fail(TESTFAIL);
        }
        u8 bang = 0;
        DOGDebang(&u, &bang);
        if (bang != cases[i].want_bang) {
            fprintf(stderr, "DebangURI '%s': bang want 0x%x got 0x%x\n",
                    cases[i].uri, cases[i].want_bang, bang);
            fail(TESTFAIL);
        }
        if (!check(i, "path",     u.path,     cases[i].want_path))  fail(TESTFAIL);
        if (!check(i, "query",    u.query,    cases[i].want_query)) fail(TESTFAIL);
        if (!check(i, "fragment", u.fragment, cases[i].want_frag))  fail(TESTFAIL);
    }
    done;
}

//  (3) `be` canonicalize preserves bangs (St.3).  Simulate the
//  canonicalizer: debang the input query, REWRITE the query to a
//  resolved sha (as REFSResolveURI does, losing the literal `!`), then
//  re-emit via DOGDebangFeed.  The `!` must survive the rewrite — this
//  is the dog-side core of the `be patch ?feat!` → `?<sha>!` repro.
static ok64 DOGTestDebangCanonSurvive(void) {
    sane(1);
    //  Input: `?feat!` (whole-branch).  Debang → bang=QUERY, bare `feat`.
    a_cstr(qsrc, "feat!");
    a_dup(u8c, q, qsrc);
    u8 bang = 0;
    if (DOGDebangSlice(q)) bang |= DOG_BANG_QUERY;
    if (bang != DOG_BANG_QUERY) fail(TESTFAIL);

    //  Rewrite: the resolver replaces the bare ref with a canonical
    //  `/proj/feat/<sha>` (the `!` is GONE from the rewritten bytes).
    a_pad(u8, out, 128);
    u8bFeed1(out, '?');
    a_cstr(canon, "/proj/feat/0123456789abcdef0123456789abcdef01234567");
    u8bFeed(out, canon);
    //  Re-emit the bang — the whole point of St.3.
    DOGDebangFeed(out, bang, DOG_BANG_QUERY);

    a_dup(u8c, got, u8bData(out));
    a_cstr(want, "?/proj/feat/0123456789abcdef0123456789abcdef01234567!");
    if (!$eq(got, want)) {
        fprintf(stderr, "DebangCanonSurvive: got '%.*s' want '%s'\n",
                (int)$len(got), (char *)got[0],
                "?/proj/feat/<sha>!");
        fail(TESTFAIL);
    }

    //  Negative: a clear bit must NOT emit a `!`.
    a_pad(u8, out2, 32);
    a_cstr(plain, "?master");
    u8bFeed(out2, plain);
    DOGDebangFeed(out2, 0, DOG_BANG_QUERY);
    a_dup(u8c, got2, u8bData(out2));
    a_cstr(want2, "?master");
    if (!$eq(got2, want2)) fail(TESTFAIL);
    done;
}

ok64 DOGTestDebang(void) {
    sane(1);
    call(DOGTestDebangSlice);
    call(DOGTestDebangURI);
    call(DOGTestDebangCanonSurvive);
    done;
}

// --- MEM-043: DOGPup error-path leaks --------------------------------
//
//  REPRO-FIRST (CLAUDE.md §17).  Two leaks live in the puppy-stack
//  helpers, neither of which LSan catches (the leaked fd/mmap/DIR
//  handle is reachable from no live pointer but its kernel resource
//  outlives the call).  We make them observable by counting open file
//  descriptors across many iterations: a leak grows the fd table
//  monotonically, a fixed function keeps it flat.
//
//    (1) pups-overflow — DOGPupOpenAll FILEMapRO+books a puppy file,
//        then pushes (key, fd) into the bounded `pups`.  When that push
//        overflows, the early return drops the just-mapped fd+mmap on
//        the floor (it was never recorded in `pups`, so DOGPupClose
//        cannot reclaim it).
//    (2) non-END FILENext — when the directory scan aborts with a real
//        error (PATHu8bPush BNOROOM on a path that overflows the 1024-
//        byte path buffer), `seen(END)` short-circuits the return and
//        SKIPS the following FILEIterClose, leaking the DIR handle.

//  Portable live open-fd count: probe with fcntl(F_GETFD).  Works on
//  Linux, FreeBSD (including linuxulator where /proc/self/fd is stubby)
//  and macOS — no /proc dependency.
static int dog_open_fd_count(void) {
    long max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd <= 0 || max_fd > 4096) max_fd = 4096;
    int n = 0;
    for (int fd = 0; fd < (int)max_fd; fd++) {
        if (fcntl(fd, F_GETFD) != -1) n++;
    }
    return n;
}

//  (1) Force a pups-overflow and assert no fd/mmap leak.
//
//  Create N puppy files in a scratch dir, then repeatedly re-open them
//  into a `pups` buffer too small to hold them all (cap < N): each
//  DOGPupOpenAll maps every file in key order and pushes until the
//  buffer fills — the (cap+1)-th FILEMapRO is the one whose fd+mmap
//  leaks on the failed push.  Before the fix the fd table grows by
//  (N-cap) per iteration; after the fix it is flat.
static ok64 DOGTestPupOverflowLeak(void) {
    sane(1);
    call(FILEInit);

    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);

    a_cstr(ext, ".test.idx");
    enum { NPUP = 8, PUPCAP = 3, ITERS = 32 };

    //  Materialize NPUP puppy files on disk via DOGPupCreate (roomy pups).
    {
        a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
        Bkv64 mk = {};
        call(kv64bAllocate, mk, NPUP + 2);
        for (int i = 0; i < NPUP; i++) {
            a_cstr(body, "puppy-bytes");
            a_dup(u8c, b, body);
            //  distinct explicit keys so all NPUP files coexist
            try(DOGPupCreateAt, mk, $path(dir), ext, b, (u64)(1000 + i));
            if (__ != OK) { DOGPupClose(mk); want(0); }
        }
        DOGPupClose(mk);  //  unmaps all, frees mk — disk files remain
    }

    int fd0 = dog_open_fd_count();
    want(fd0 > 0);

    for (int it = 0; it < ITERS; it++) {
        a_path(dir, ((u8cs){(u8 *)tmp, (u8 *)tmp + strlen(tmp)}));
        Bkv64 pups = {};
        call(kv64bAllocate, pups, PUPCAP);  //  too small for NPUP
        try(DOGPupOpenAll, pups, $path(dir), ext);
        //  overflow → SNOROOM expected; the leak (if any) already happened
        b8 overflowed = (__ == SNOROOM);
        DOGPupClose(pups);
        if (!overflowed) {        //  repro precondition not met
            TESTBErmrf(tmp);
            fprintf(stderr, "PupOverflowLeak: no overflow on iter %d\n", it);
            fail(TESTFAIL);
        }
    }

    int fd1 = dog_open_fd_count();
    TESTBErmrf(tmp);
    if (fd1 > fd0) {
        fprintf(stderr,
            "PupOverflowLeak: fd count grew %d -> %d across %d iters "
            "(leaked ~%d fds/iter)\n",
            fd0, fd1, ITERS, (fd1 - fd0) / ITERS);
        fail(TESTFAIL);
    }
    __ = OK;  //  the expected overflow status must not leak out as failure
    done;
}

//  (2) Force a non-END FILENext error and assert no DIR-handle leak.
//
//  DOGPupOpenAll's FILEIter shares a 1024-byte path buffer seeded with
//  `dir`.  Make `dir` long enough that appending ANY entry name
//  overflows the buffer: PATHu8bPush then returns BNOROOM (a non-END,
//  non-PATHBAD error), the scan aborts, and the pre-fix `seen(END)`
//  returns BEFORE FILEIterClose — leaking the opendir() handle.  One
//  leaked DIR fd per iteration; the loop makes it unmistakable.
static ok64 DOGTestPupIterErrLeak(void) {
    sane(1);
#if defined(__FreeBSD__) || defined(__APPLE__)
    //  This repro needs OS PATH_MAX > FILE_PATH_MAX_LEN (1024) so the
    //  on-disk file can be created while the iterator's bounded push
    //  still overflows.  FreeBSD's and Darwin's kernel MAXPATHLEN is
    //  1024, so the precondition cannot be reached — open(ENAMETOOLONG)s
    //  before the scan ever observes an entry.  The leak fix itself is
    //  platform-agnostic and PupOverflowLeak above still exercises the
    //  close path.
    done;
#endif
    call(FILEInit);

    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);

    //  Build a deeply-nested dir whose path is within a few bytes of
    //  FILE_PATH_MAX_LEN (1024), so any entry name push overflows.
    char deep[1100];
    int dlen = snprintf(deep, sizeof deep, "%s", tmp);
    //  255-byte segments (NAME_MAX) until we are ~within an entry-name of cap.
    char seg[256];
    memset(seg, 'a', 200);
    seg[200] = 0;
    while (dlen + 1 + 200 < 1015) {
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", deep, seg);
        if (mkdir(path, 0755) != 0) { TESTBErmrf(tmp); want(0); }
        dlen = snprintf(deep, sizeof deep, "%s", path);
    }
    //  Put a regular file inside so the scan visits an entry and the
    //  push (deep-path + filename) overflows the 1024 buffer.
    {
        char fpath[1200];
        snprintf(fpath, sizeof fpath, "%s/%s", deep, seg);
        int fd = open(fpath, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) close(fd);
    }

    a_cstr(ext, ".test.idx");
    enum { ITERS = 64 };

    int fd0 = dog_open_fd_count();
    want(fd0 > 0);

    for (int it = 0; it < ITERS; it++) {
        a_path(dir, ((u8cs){(u8 *)deep, (u8 *)deep + strlen(deep)}));
        Bkv64 pups = {};
        call(kv64bAllocate, pups, 16);
        try(DOGPupOpenAll, pups, $path(dir), ext);
        //  expect a non-END iterator error (PATHNOROOM) to have aborted
        //  the scan; OK/END would mean the precondition wasn't met
        b8 iter_erred = (__ != OK);
        DOGPupClose(pups);
        if (!iter_erred) {
            TESTBErmrf(tmp);
            fprintf(stderr, "PupIterErrLeak: no iter error on iter %d\n", it);
            fail(TESTFAIL);
        }
    }

    int fd1 = dog_open_fd_count();
    TESTBErmrf(tmp);
    if (fd1 > fd0) {
        fprintf(stderr,
            "PupIterErrLeak: fd count grew %d -> %d across %d iters "
            "(leaked DIR handles)\n",
            fd0, fd1, ITERS);
        fail(TESTFAIL);
    }
    __ = OK;  //  the expected iterator error must not leak out as failure
    done;
}

ok64 DOGTestPupLeaks(void) {
    sane(1);
    call(DOGTestPupOverflowLeak);
    call(DOGTestPupIterErrLeak);
    done;
}

ok64 DOGtest() {
    sane(1);
    call(DOGTestDOGParseURI);
    call(DOGTestDebang);
    call(DOGTestCanonical);
    call(DOGTestMsgFragment);
    call(DOGTestPathHash);
    call(DOGTestFeedDate);
    call(DOGTestIsFullSha);
    call(DOGTestIsHashlet);
    call(DOGTestCanonQueryParse);
    call(DOGTestGitTransport);
    call(DOGTestProjector);
    call(DOGTestSuggestCommand);
    call(DOGTestFromBe);
    call(DOGTestPupLeaks);
    done;
}

TEST(DOGtest);
