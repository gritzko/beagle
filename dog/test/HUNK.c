#include "dog/HUNK.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

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

ok64 HUNKtest() {
    sane(1);
    call(HUNKTestRebase);
    call(HUNKTestRelayRoundtrip);
    done;
}

TEST(HUNKtest);
