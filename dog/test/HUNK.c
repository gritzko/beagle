#include "dog/HUNK.h"

#include <string.h>

#include "abc/ANSI.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/THEME.h"
#include "dog/ULOG.h"
#include "dog/tok/TOK.h"

//  Verb used for the synthetic status hunks below.  Any non-zero
//  ron60 makes hunk_is_status() true (with empty text/toks), so the
//  relay round-trips a ULOG-shaped status row.
#define HUNK_TEST_VERB HUNK_VERB_HUNK

static b8 hunk_slice_eq(u8cs s, const char *expect) {
    if (expect == NULL) return $empty(s);
    size_t elen = strlen(expect);
    if ((size_t)$len(s) != elen) return NO;
    if (elen == 0) return YES;
    return memcmp(s[0], expect, elen) == 0;
}

//  u8cs is an array type, so it can't be returned from a helper —
//  construct the slice in place.
#define HUNK_SLICE(name, s)                                     \
    u8cs name = {(u8cp)(s), (u8cp)(s) + strlen(s)}

// =====================================================================
// HUNKu8sRebaseURI — prefix the path component, keep the rest.
// =====================================================================

typedef struct {
    const char *prefix;
    const char *uri;
    const char *expect;
} RebaseCase;

static const RebaseCase REBASE_CASES[] = {
    {"vendor/sub", "src/foo.c",   "vendor/sub/src/foo.c"},
    {"vendor/sub", "foo.c#L42",   "vendor/sub/foo.c#L42"},
    {"a",          "b.c",         "a/b.c"},
    {"",           "x.c",         "x.c"},          // empty prefix → passthrough
    {"sub",        "",            "sub"},          // empty URI → just the prefix
    {"outer/inner","deep.c",      "outer/inner/deep.c"},
    {"x",          "a/b/c.c#L7",  "x/a/b/c.c#L7"},
};

#define REBASE_N (sizeof(REBASE_CASES) / sizeof(REBASE_CASES[0]))

static ok64 HUNKTestRebase() {
    sane(1);
    for (size_t i = 0; i < REBASE_N; i++) {
        RebaseCase const *tc = &REBASE_CASES[i];
        HUNK_SLICE(pfx, tc->prefix);
        HUNK_SLICE(uri, tc->uri);

        a_pad(u8, ob, 1024);
        call(HUNKu8sRebaseURI, ob_idle, pfx, uri);
        a_dup(u8c, got, u8bData(ob));

        if (!hunk_slice_eq(got, tc->expect)) {
            fprintf(stderr,
                    "FAIL rebase[%zu] pfx='%s' uri='%s': got '%.*s' want '%s'\n",
                    i, tc->prefix, tc->uri, (int)$len(got),
                    $empty(got) ? "" : (char *)got[0], tc->expect);
            fail(TESTFAIL);
        }
    }
    done;
}

// =====================================================================
// HUNKu8sRelay — sequential relay of a child TLV stream with prefix.
// =====================================================================

//  Feed a status hunk (verb + uri, no body) into `into`.
static ok64 hunk_feed_status(u8s into, ron60 verb, const char *uri) {
    sane(u8sOK(into));
    hunk hk = {.verb = verb, .uri = {(u8cp)uri, (u8cp)uri + strlen(uri)}};
    return HUNKu8sFeed(into, &hk);
}

static ok64 HUNKTestRelayRoundtrip() {
    sane(1);

    static const char *CHILD_URIS[] = {"a.c", "dir/b.c#L3", "c.h"};
    static const char *WANT_URIS[]  = {"mod/a.c", "mod/dir/b.c#L3", "mod/c.h"};
    enum { N = 3 };

    //  Build the child TLV stream (a `be --tlv` report would look like
    //  this: N back-to-back 'H' records).
    a_pad(u8, cb, 4096);
    for (int i = 0; i < N; i++)
        call(hunk_feed_status, cb_idle, HUNK_TEST_VERB, CHILD_URIS[i]);
    a_dup(u8c, child, u8bData(cb));

    //  Relay it in TLV mode so we can drain + assert the rewritten URIs.
    HUNKout saved = HUNKMode;
    HUNKMode = HUNKOutTLV;
    a_pad(u8, ob, 8192);
    HUNK_SLICE(pfx, "mod");
    ok64 rr = HUNKu8sRelay(ob_idle, pfx, child);
    HUNKMode = saved;
    if (rr != OK) {
        fprintf(stderr, "FAIL relay: rc %s\n", ok64str(rr));
        fail(TESTFAIL);
    }

    //  Drain the relayed stream back and check each hunk.
    a_dup(u8c, scan, u8bData(ob));
    for (int i = 0; i < N; i++) {
        hunk hk = {};
        ok64 dr = HUNKu8sDrain(scan, &hk);
        if (dr != OK) {
            fprintf(stderr, "FAIL relay drain[%d]: rc %s\n", i, ok64str(dr));
            fail(TESTFAIL);
        }
        if (hk.verb != HUNK_TEST_VERB) {
            fprintf(stderr, "FAIL relay verb[%d]: got %llx want %llx\n",
                    i, (unsigned long long)hk.verb,
                    (unsigned long long)HUNK_TEST_VERB);
            fail(TESTFAIL);
        }
        if (!hunk_slice_eq(hk.uri, WANT_URIS[i])) {
            fprintf(stderr, "FAIL relay uri[%d]: got '%.*s' want '%s'\n",
                    i, (int)$len(hk.uri),
                    $empty(hk.uri) ? "" : (char *)hk.uri[0], WANT_URIS[i]);
            fail(TESTFAIL);
        }
    }
    //  Stream must be fully consumed (exactly N records, no trailing).
    if (!$empty(scan)) {
        fprintf(stderr, "FAIL relay: %d trailing bytes after %d hunks\n",
                (int)$len(scan), N);
        fail(TESTFAIL);
    }
    done;
}

// =====================================================================
// BE-001 — red is reserved for CONFLICT statuses (conf/modl) only.
//
// Two render sites historically over-applied the bright-red palette
// slot because the THEME table indexes a 32-slot array by a single
// ASCII tag letter, and the tok-namespace "default" tag ('S', emitted
// for identifiers / whitespace and used as the neutral status column
// fill) collided with the status-namespace conflict slot.  That painted
// ordinary code tokens and clean status rows bright red.  Red must
// appear ONLY for the genuine conflict statuses.
// =====================================================================

//  An ansi64 is "red" iff its fg carries one of the red SGR/256 values.
static b8 ansi_is_red(ansi64 c) {
    u8  mode = ansi64FgMode(c);
    u32 fg   = ansi64Fg(c);
    if (mode == ANSI_MODE_BASIC) return fg == DARK_RED || fg == LIGHT_RED;
    if (mode == ANSI_MODE_256)   return fg == 160 || fg == 196;
    return NO;
}

//  Render text wrapped in the named theme, scan the emitted bytes for a
//  red SGR sequence (basic 31/91 or 256 38;5;160 / 38;5;196).
static b8 bytes_have_red_sgr(u8cs out) {
    static const char *RED[] = {
        "\033[31m", "\033[91m", "\033[38;5;160m", "\033[38;5;196m",
    };
    for (size_t k = 0; k < sizeof(RED) / sizeof(RED[0]); k++) {
        size_t rl = strlen(RED[k]);
        if ((size_t)$len(out) < rl) continue;
        for (u8c *p = out[0]; p + rl <= out[1]; p++)
            if (memcmp(p, RED[k], rl) == 0) return YES;
    }
    return NO;
}

//  Tag-level contract: only the conflict slot 'S' is red; every other
//  populated tok / status slot is non-red.  Checked across all themes.
typedef struct {
    u8 tag;
    b8 want_red;
} TagRedCase;

static const TagRedCase TAG_RED_CASES[] = {
    {'M', YES},   // conflict family (mis/conf/modl) — the ONLY red slot
    {'S', NO},    // DEFAULT tok / neutral status column — must NOT be red
    {'D', NO},    // comment
    {'G', NO},    // string
    {'L', NO},    // number
    {'H', NO},    // preproc
    {'R', NO},    // keyword  (was historically red; must not be now)
    {'P', NO},    // punctuation
    {'N', NO},    // defname
    {'C', NO},    // funcall
    {'F', NO},    // filename
    {'T', NO},    // title
    {'E', NO},    // mod
    {'W', NO},    // new / applied
    {'V', NO},    // mov / post
    {'X', NO},    // del
    {'Q', NO},    // unk / dirty
    {'Y', NO},    // put / upd
    {'Z', NO},    // mrg / merged
    {'B', NO},    // eq / hunk
};
#define TAG_RED_N (sizeof(TAG_RED_CASES) / sizeof(TAG_RED_CASES[0]))

//  Verb-level contract: conf/modl map red; ordinary verbs and the
//  zero / unknown fallback map to a non-red colour.
typedef struct {
    const char *verb;
    b8          want_red;
} VerbRedCase;

static const VerbRedCase VERB_RED_CASES[] = {
    {"conf", YES},   // genuine WEAVE conflict
    {"modl", YES},   // modify/delete divergence
    {"new",  NO},
    {"mod",  NO},
    {"del",  NO},
    {"mov",  NO},
    {"mrg",  NO},
    {"unk",  NO},
    {"put",  NO},
    {"upd",  NO},
    {"applied", NO},
    {"merged",  NO},
    {"",     NO},    // unknown / zero verb → default (must not be red)
};
#define VERB_RED_N (sizeof(VERB_RED_CASES) / sizeof(VERB_RED_CASES[0]))

static const char *const THEMES[] = {THEME_16, THEME_DARK, THEME_LIGHT};

static ok64 HUNKTestRedReserved() {
    sane(1);
    for (size_t t = 0; t < sizeof(THEMES) / sizeof(THEMES[0]); t++) {
        if (THEMESelect(THEMES[t]) != OK) fail(TESTFAIL);

        //  (a) Tag palette: only 'S' (conflict) is red.
        for (size_t i = 0; i < TAG_RED_N; i++) {
            TagRedCase const *tc = &TAG_RED_CASES[i];
            b8 red = ansi_is_red(THEMEAt(tc->tag));
            if (red != tc->want_red) {
                fprintf(stderr,
                        "FAIL theme=%s tag '%c': red=%d want=%d\n",
                        THEMES[t], tc->tag, red, tc->want_red);
                fail(TESTFAIL);
            }
        }

        //  (b) Code-render path: a content hunk made of default 'S'
        //  tokens (ordinary identifiers / whitespace) must emit NO red
        //  SGR.  This is the `bro file.c` / `be` code-block render.
        {
            const char *src = "int answer = 42;\n";
            HUNK_SLICE(text, src);
            //  one default-tagged span covering the whole line
            tok32 toks_arr[] = {tok32Pack('S', (u32)strlen(src))};
            tok32cs toks = {toks_arr, toks_arr + 1};
            HUNK_SLICE(uri, "cat:sample.c");
            hunk hk = {.uri = {uri[0], uri[1]},
                       .text = {text[0], text[1]},
                       .toks = {toks[0], toks[1]}};
            a_pad(u8, ob, 4096);
            call(HUNKu8sFeedColor, ob_idle, &hk);
            a_dup(u8c, got, u8bData(ob));
            if (bytes_have_red_sgr(got)) {
                fprintf(stderr,
                        "FAIL theme=%s code-render: red SGR on default "
                        "tokens\n", THEMES[t]);
                fail(TESTFAIL);
            }
        }

        //  (c) Status / ulog rows: conf/modl red, everything else not.
        for (size_t i = 0; i < VERB_RED_N; i++) {
            VerbRedCase const *vc = &VERB_RED_CASES[i];
            ron60 verb = 0;
            if (vc->verb[0] != 0) {
                a_cstr(vs, vc->verb);
                a_dup(u8c, vd, vs);
                (void)RONutf8sDrain(&verb, vd);
            }
            b8 red = ansi_is_red(ULOGVerbColor(verb));
            if (red != vc->want_red) {
                fprintf(stderr,
                        "FAIL theme=%s verb '%s': red=%d want=%d\n",
                        THEMES[t], vc->verb, red, vc->want_red);
                fail(TESTFAIL);
            }
        }
    }
    (void)THEMESelect(THEME_16);
    done;
}

ok64 HUNKtest() {
    sane(1);
    call(HUNKTestRebase);
    call(HUNKTestRelayRoundtrip);
    call(HUNKTestRedReserved);
    done;
}

TEST(HUNKtest);
