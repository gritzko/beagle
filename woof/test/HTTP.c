//  woof/test/HTTP.c — table-driven property test for woof's HTTP front.
//
//  Each case carries one raw HTTP request and the four observations
//  every woof dispatch must produce:
//      1. abc/HTTP parses method + request-target.
//      2. extract_be_uri strips the leading '/' and percent-decodes the
//         remainder — request-target → be-URI.
//      3. abc/URI re-parses the be-URI; scheme drives routing.
//      4. WOOFRouteFind maps scheme → worker binary.
//  want_err short-circuits the chain at the failing step (WOOFMETHOD on
//  the gate, WOOFBADREQ on decode, WOOFNOROUTE on lookup).
//
//  The extractor and the route table are inlined here pending WOOF.c —
//  they lift verbatim into woof/CONN.c and woof/ROUTE.c once those land.

#include "woof/WOOF.h"

#include "abc/HEX.h"     // BASE16rev for percent-decode
#include "abc/HTTP.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "abc/URI.h"

#include <stdio.h>
#include <string.h>

// --- extractor (slated for woof/CONN.c) ---

//  Strip the leading '/' from `target` and percent-decode the
//  remainder into `out` (reset first).  Refuses malformed '%XX' with
//  WOOFBADREQ.
static ok64 extract_be_uri(Bu8 out, u8cs target) {
    sane(1);
    u8bReset(out);
    test(!$empty(target) && *target[0] == '/', WOOFBADREQ);
    u8csUsed1(target);
    while (!$empty(target)) {
        u8 c = *target[0];
        if (c == '%') {
            test($len(target) >= 3, WOOFBADREQ);
            u8 hi = BASE16rev[target[0][1]];
            u8 lo = BASE16rev[target[0][2]];
            test(hi != 0xff && lo != 0xff, WOOFBADREQ);
            call(u8bFeed1, out, (u8)((hi << 4) | lo));
            u8csUsed(target, 3);
        } else {
            call(u8bFeed1, out, c);
            u8csUsed1(target);
        }
    }
    done;
}

// --- route table mirror (slated for woof/ROUTE.c) ---

static woof_route const TEST_ROUTES[] = {
    { u8slit("blob"),   u8slit("keeper") },
    { u8slit("tree"),   u8slit("keeper") },
    { u8slit("commit"), u8slit("keeper") },
    { u8slit("log"),    u8slit("graf")   },
    { u8slit("diff"),   u8slit("graf")   },
    { u8slit("head"),   u8slit("graf")   },
    { u8slit("spot"),   u8slit("spot")   },
    { u8slit("grep"),   u8slit("spot")   },
    { u8slit("regex"),  u8slit("spot")   },
    { u8slit(""),       u8slit("bro")    },   // verbless catch-all
    { u8slit(""),       u8slit("")       },   // terminator (empty binary)
};

static woof_route const *test_route_find(u8cs scheme) {
    for (size_t i = 0; ; i++) {
        woof_route const *r = &TEST_ROUTES[i];
        if ($empty(r->binary)) return NULL;
        if ($size(r->scheme) == $size(scheme)
            && ($size(scheme) == 0
                || memcmp(*r->scheme, *scheme, $size(scheme)) == 0)) {
            return r;
        }
    }
}

static b8 is_get_or_head(u8cs m) {
    a$str(get_s,  "GET");
    a$str(head_s, "HEAD");
    return ($size(m) == 3 && memcmp(*m, *get_s,  3) == 0)
        || ($size(m) == 4 && memcmp(*m, *head_s, 4) == 0);
}

// --- cases ---

typedef struct {
    char const *name;
    char const *request;       // raw HTTP request bytes
    char const *want_method;   // method as parsed
    char const *want_target;   // request-target as parsed
    char const *want_be_uri;   // post-strip + decode (NULL = skip)
    char const *want_scheme;   // URIutf8Drain(be_uri).scheme (NULL = skip)
    char const *want_binary;   // route binary (NULL = no route expected)
    ok64        want_err;      // OK or expected failure-step code
} request_case;

static request_case const CASES[] = {
    { "root verbless → bro",
      "GET / HTTP/1.1\r\nHost: x\r\n\r\n",
      "GET", "/", "", "", "bro", OK },

    { "blob with branch ref",
      "GET /blob:abc/MSET.h?main HTTP/1.1\r\nHost: x\r\n\r\n",
      "GET", "/blob:abc/MSET.h?main",
      "blob:abc/MSET.h?main", "blob", "keeper", OK },

    { "log projector with query",
      "GET /log:?feat HTTP/1.1\r\nHost: x\r\n\r\n",
      "GET", "/log:?feat", "log:?feat", "log", "graf", OK },

    { "spot: %23 decodes to '#'",
      "GET /spot:%23TODO.c HTTP/1.1\r\nHost: x\r\n\r\n",
      "GET", "/spot:%23TODO.c", "spot:#TODO.c", "spot", "spot", OK },

    { "blob with line fragment",
      "GET /blob:abc/MSET.h%23L42 HTTP/1.1\r\nHost: x\r\n\r\n",
      "GET", "/blob:abc/MSET.h%23L42",
      "blob:abc/MSET.h#L42", "blob", "keeper", OK },

    { "HEAD method passes the gate",
      "HEAD /tree: HTTP/1.1\r\nHost: x\r\n\r\n",
      "HEAD", "/tree:", "tree:", "tree", "keeper", OK },

    { "POST refused (405)",
      "POST / HTTP/1.1\r\nHost: x\r\n\r\n",
      "POST", "/", NULL, NULL, NULL, WOOFMETHOD },

    { "unknown scheme → 404",
      "GET /xyzzy: HTTP/1.1\r\nHost: x\r\n\r\n",
      "GET", "/xyzzy:", "xyzzy:", "xyzzy", NULL, WOOFNOROUTE },

    { "malformed percent escape → 400",
      "GET /spot:%2 HTTP/1.1\r\nHost: x\r\n\r\n",
      "GET", "/spot:%2", NULL, NULL, NULL, WOOFBADREQ },
};

#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

// --- driver ---

static ok64 run_case(request_case const *c) {
    sane(1);

    // 1. parse request
    HTTPstate req = {};
    aBpad2(u8cs, hdrs, 16);
    req.headers = hdrsidle;
    a$str(raw, c->request);
    call(HTTPutf8Drain, raw, &req);

    // 2. method + request-target always assert
    a$str(want_m, c->want_method);
    $testeq(req.method, want_m);
    a$str(want_t, c->want_target);
    $testeq(req.uri, want_t);

    // 3. method gate — woof v1 is GET / HEAD only
    if (!is_get_or_head(req.method)) {
        testeqv((long long)c->want_err, (long long)WOOFMETHOD, "%lld");
        done;
    }

    // 4. extract be-URI
    Bu8 beuri = {};
    call(u8bAllocate, beuri, MAX_URI_LEN);
    ok64 ex = extract_be_uri(beuri, req.uri);
    if (c->want_err == WOOFBADREQ) {
        u8bFree(beuri);
        testeqv((long long)ex, (long long)WOOFBADREQ, "%lld");
        done;
    }
    if (ex != OK) { u8bFree(beuri); fail(ex); }

    // 5. extracted bytes match
    {
        a$str(want_be, c->want_be_uri);
        a_dup(u8c, got, u8bData(beuri));
        $testeq(got, want_be);
    }

    // 6. URI parse + scheme
    uri u = {};
    a_dup(u8c, beuri_view, u8bData(beuri));
    ok64 ur = URIutf8Drain(beuri_view, &u);
    if (ur != OK) { u8bFree(beuri); fail(ur); }
    a$str(want_sc, c->want_scheme);
    $testeq(u.scheme, want_sc);

    // 7. route lookup
    woof_route const *r = test_route_find(u.scheme);
    if (c->want_err == WOOFNOROUTE) {
        u8bFree(beuri);
        test(r == NULL, WOOFFAIL);
        done;
    }
    if (r == NULL) { u8bFree(beuri); fail(WOOFNOROUTE); }
    a$str(want_bin, c->want_binary);
    $testeq(r->binary, want_bin);

    u8bFree(beuri);
    done;
}

ok64 WOOFHTTPtest() {
    sane(1);
    for (size_t i = 0; i < CASE_COUNT; i++) {
        ok64 r = run_case(&CASES[i]);
        if (r != OK) {
            fprintf(stderr, "woof/test/HTTP FAIL: case[%zu] '%s'\n",
                    i, CASES[i].name);
            return r;
        }
    }
    done;
}

TEST(WOOFHTTPtest);
