//  REFS — thin ULOG glue: end-to-end property tests.
//
//  Covers: empty log → Load/Resolve no-op; Append+Load round-trip;
//  dedup by key (latest wins); multi-key; Resolve local + alias
//  (host-substring) + miss; SyncRecord; Compact; monotonic ts.

#include "keeper/REFS.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/S.h"
#include "abc/TEST.h"
#include "dog/ULOG.h"

// --- fixture ----------------------------------------------------------

typedef struct {
    char  dir[64];      // tmp dir, NUL-terminated
    u8cs  dir_s;        // const slice view
} fixture;

static ok64 fixture_open(fixture *fx, char const *tmpl) {
    sane(fx && tmpl);
    snprintf(fx->dir, sizeof(fx->dir), "%s", tmpl);
    if (mkdtemp(fx->dir) == NULL) fail(FAIL);
    fx->dir_s[0] = (u8cp)fx->dir;
    fx->dir_s[1] = (u8cp)fx->dir + strlen(fx->dir);
    done;
}

static void fixture_close(fixture *fx) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", fx->dir);
    int _ = system(cmd); (void)_;
}

//  Re-open the ULOG on <dir>/refs and report row count.
static u32 count_rows(u8cs dir) {
    a_cstr(fname, "refs");
    a_path(pbuf, dir, fname);
    a_dup(u8c, path, u8bDataC(pbuf));
    u8bp  data = NULL;
    Bkv64 idx  = {};
    if (ULOGOpen(&data, idx, path) != OK) return 0;
    u32 n = ULOGCount(idx);
    ULOGClose(data, idx, YES);
    return n;
}

//  Slice helpers (unique names so multiple fit in one scope).
#define SL(name, text) a_cstr(name, text)

static ok64 append(fixture *fx, char const *k, char const *v) {
    sane(fx && k && v);
    SL(ks, k); SL(vs, v);
    call(REFSAppend, fx->dir_s, ks, vs);
    done;
}

static ref const *find_key(ref const *arr, u32 n, char const *want) {
    size_t wl = strlen(want);
    for (u32 i = 0; i < n; i++) {
        if ((size_t)u8csLen(arr[i].key) == wl &&
            memcmp(arr[i].key[0], want, wl) == 0)
            return &arr[i];
    }
    return NULL;
}

static b8 val_eq(ref const *r, char const *want) {
    size_t wl = strlen(want);
    return (size_t)u8csLen(r->val) == wl &&
           memcmp(r->val[0], want, wl) == 0;
}

// --- 1. empty ---------------------------------------------------------

ok64 REFStest_empty() {
    sane(1); call(FILEInit);
    fixture fx = {};
    call(fixture_open, &fx, "/tmp/refs-e-XXXXXX");

    Bu8 arena = {};
    call(u8bMap, arena, 4096);
    ref arr[8] = {};
    u32 n = 99;
    call(REFSLoad, arr, &n, 8, arena, fx.dir_s);
    want(n == 0);

    uri res = {};
    SL(q, "?heads/main");
    want(REFSResolve(&res, arena, fx.dir_s, q) == REFSNONE);
    u8bUnMap(arena);

    fixture_close(&fx);
    done;
}

// --- 2-4. append / load / dedup / multi-key (one fixture) -------------

ok64 REFStest_load_dedup() {
    sane(1); call(FILEInit);
    fixture fx = {};
    call(fixture_open, &fx, "/tmp/refs-ld-XXXXXX");

    //  Two keys × two revisions each, + one third key.
    call(append, &fx, "?heads/main", "1111111111111111111111111111111111111111");
    call(append, &fx, "?heads/feat", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    call(append, &fx, "?heads/main", "2222222222222222222222222222222222222222");
    call(append, &fx, "?tags/v1",    "3333333333333333333333333333333333333333");
    call(append, &fx, "?heads/feat", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    want(count_rows(fx.dir_s) == 5);

    Bu8 arena = {};
    call(u8bMap, arena, 4096);
    ref arr[8] = {};
    u32 n = 0;
    call(REFSLoad, arr, &n, 8, arena, fx.dir_s);
    want(n == 3);  // dedup to 3 unique keys

    ref const *m = find_key(arr, n, "?heads/main");
    ref const *f = find_key(arr, n, "?heads/feat");
    ref const *t = find_key(arr, n, "?tags/v1");
    want(m && f && t);
    //  Latest val wins.  Vals are bare 40-hex.
    want(val_eq(m, "2222222222222222222222222222222222222222"));
    want(val_eq(f, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
    want(val_eq(t, "3333333333333333333333333333333333333333"));

    u8bUnMap(arena);
    fixture_close(&fx);
    done;
}

// --- 5-7. resolve: local / alias / miss -------------------------------

typedef struct {
    char const *query;
    char const *expect_sha;    // NULL = expect miss
    char const *expect_scheme; // optional, NULL to skip
    char const *expect_refname;// optional, NULL to skip
} resolve_case;

ok64 REFStest_resolve_table() {
    sane(1); call(FILEInit);
    fixture fx = {};
    call(fixture_open, &fx, "/tmp/refs-rt-XXXXXX");

    //  Local query is an opaque branch path — plain string equality, no
    //  name aliasing.  Trunk = empty (`?`); other branches keep their
    //  literal path.  Wire-side aliasing (heads/main ⇔ trunk) is the
    //  job of keeper/GIT.h GITParseRef/GITFeedRef and lives in the
    //  WIRE layer, not here.
    call(append, &fx, "?feature",
         "cafef00dcafef00dcafef00dcafef00dcafef00d");
    call(append, &fx, "https://github.com/torvalds/linux.git?master",
         "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef");

    resolve_case const cases[] = {
        //  Literal local-branch hit.
        {"?feature",         "cafef00dcafef00dcafef00dcafef00dcafef00d", NULL,    "feature"},
        //  Same local key under a peer-prefixed query.
        {"//github?master",  "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef", "https", "master"},
        //  Authority-only lookup (no `?`): wildcard over peer rows; the
        //  most recent matching row's `?query` is recovered.
        {"//github",         "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef", "https", "master"},
        //  Misses: literal mismatch and unknown branch.
        {"?heads/feature",   NULL,                                       NULL,    NULL},
        {"?nonexistent",     NULL,                                       NULL,    NULL},
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(*cases); i++) {
        Bu8 arena = {};
        call(u8bMap, arena, 4096);
        uri res = {};
        SL(q_s, cases[i].query);
        ok64 o = REFSResolve(&res, arena, fx.dir_s, q_s);
        if (cases[i].expect_sha == NULL) {
            want(o == REFSNONE);
        } else {
            want(o == OK);
            want(u8csLen(res.query) == 40);
            want(memcmp(res.query[0], cases[i].expect_sha, 40) == 0);
            if (cases[i].expect_scheme) {
                size_t sl = strlen(cases[i].expect_scheme);
                want((size_t)u8csLen(res.scheme) == sl);
                want(memcmp(res.scheme[0], cases[i].expect_scheme, sl) == 0);
            }
            if (cases[i].expect_refname) {
                size_t rl = strlen(cases[i].expect_refname);
                want((size_t)u8csLen(res.fragment) == rl);
                want(memcmp(res.fragment[0], cases[i].expect_refname, rl) == 0);
            }
        }
        u8bUnMap(arena);
    }

    fixture_close(&fx);
    done;
}

// --- 8. SyncRecord + Compact round-trip -------------------------------

ok64 REFStest_sync_compact() {
    sane(1); call(FILEInit);
    fixture fx = {};
    call(fixture_open, &fx, "/tmp/refs-sc-XXXXXX");

    //  SyncRecord path: bulk-append three fresh keys.  Vals are bare
    //  40-hex (canonical fragment form).
    ref entries[3] = {};
    SL(k0, "?heads/main"); SL(v0, "1111111111111111111111111111111111111111");
    SL(k1, "?heads/feat"); SL(v1, "2222222222222222222222222222222222222222");
    SL(k2, "?tags/v1");    SL(v2, "3333333333333333333333333333333333333333");
    entries[0].key[0] = k0[0]; entries[0].key[1] = k0[1];
    entries[0].val[0] = v0[0]; entries[0].val[1] = v0[1];
    entries[1].key[0] = k1[0]; entries[1].key[1] = k1[1];
    entries[1].val[0] = v1[0]; entries[1].val[1] = v1[1];
    entries[2].key[0] = k2[0]; entries[2].key[1] = k2[1];
    entries[2].val[0] = v2[0]; entries[2].val[1] = v2[1];
    call(REFSSyncRecord, fx.dir_s, entries, 3);
    want(count_rows(fx.dir_s) == 3);

    //  Churn: two more revisions per key → 7 rows total.
    call(append, &fx, "?heads/main", "4444444444444444444444444444444444444444");
    call(append, &fx, "?heads/feat", "5555555555555555555555555555555555555555");
    call(append, &fx, "?heads/main", "6666666666666666666666666666666666666666");
    call(append, &fx, "?heads/feat", "7777777777777777777777777777777777777777");
    want(count_rows(fx.dir_s) == 7);

    call(REFSCompact, fx.dir_s);
    want(count_rows(fx.dir_s) == 3);

    //  Post-compact semantics: latest-per-key preserved.
    Bu8 arena = {};
    call(u8bMap, arena, 4096);
    ref arr[8] = {};
    u32 n = 0;
    call(REFSLoad, arr, &n, 8, arena, fx.dir_s);
    want(n == 3);
    ref const *m = find_key(arr, n, "?heads/main");
    ref const *f = find_key(arr, n, "?heads/feat");
    ref const *t = find_key(arr, n, "?tags/v1");
    want(m && f && t);
    want(val_eq(m, "6666666666666666666666666666666666666666"));
    want(val_eq(f, "7777777777777777777777777777777777777777"));
    want(val_eq(t, "3333333333333333333333333333333333333333"));
    u8bUnMap(arena);

    fixture_close(&fx);
    done;
}

// --- 9. monotonicity: rapid appends get distinct ts -------------------

ok64 REFStest_monotonic_ts() {
    sane(1); call(FILEInit);
    fixture fx = {};
    call(fixture_open, &fx, "/tmp/refs-mo-XXXXXX");

    //  Five rapid appends — ULOG enforces strict monotonicity and
    //  REFSAppend clamps past the tail, so all five must land.
    for (int i = 0; i < 5; i++) {
        char v[64];
        snprintf(v, sizeof(v), "%040d", i);
        call(append, &fx, "?heads/main", v);
    }
    want(count_rows(fx.dir_s) == 5);

    //  Re-open ULOG, walk rows, verify strictly increasing ts.
    a_cstr(fname, "refs");
    a_path(pbuf, fx.dir_s, fname);
    a_dup(u8c, path, u8bDataC(pbuf));
    u8bp  data = NULL;
    Bkv64 idx  = {};
    call(ULOGOpen, &data, idx, path);
    ron60 prev = 0;
    for (u32 i = 0; i < ULOGCount(idx); i++) {
        ron60 ts = 0, verb = 0;
        uri u = {};
        call(ULOGRow, data, idx, i, &ts, &verb, &u);
        want(ts > prev);
        prev = ts;
    }
    ULOGClose(data, idx, YES);

    fixture_close(&fx);
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "REFStest_empty...\n");          call(REFStest_empty);
    fprintf(stderr, "REFStest_load_dedup...\n");     call(REFStest_load_dedup);
    fprintf(stderr, "REFStest_resolve_table...\n");  call(REFStest_resolve_table);
    fprintf(stderr, "REFStest_sync_compact...\n");   call(REFStest_sync_compact);
    fprintf(stderr, "REFStest_monotonic_ts...\n");   call(REFStest_monotonic_ts);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
