#include "dog/DOG.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

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

ok64 DOGtest() {
    sane(1);
    call(DOGTestDOGParseURI);
    call(DOGTestCanonical);
    call(DOGTestMsgFragment);
    call(DOGTestPathHash);
    call(DOGTestFeedDate);
    call(DOGTestIsFullSha);
    call(DOGTestCanonQueryParse);
    call(DOGTestGitTransport);
    call(DOGTestFromBe);
    done;
}

TEST(DOGtest);
