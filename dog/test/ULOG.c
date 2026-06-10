//  ULOG — append-only URI event log: round-trip + lookup coverage.
//
//  Cases:
//    1. Open-empty + append three rows + Count/Head/Tail.
//    2. Persistence: close, reopen, verify all rows survive.
//    3. Random access: Row(i) recovers per-row ts + verb + URI.
//    4. Seek/Find/Has: lower_bound, exact, membership.
//    5. Monotonicity: AppendAt with stale ts returns ULOGCLOCK.
//    6. FindVerb: reverse scan picks the latest row for that verb.
//    7. Truncate: keep_n rewinds both book and idx.
//    8. Streaming Feed / Drain round-trip.
//    9. Whitespace tolerance: multi-space / multi-tab separators.

#include "dog/ULOG.h"

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

static ok64 rm_tmp(char const *p) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", p);
    int _ = system(cmd);
    (void)_;
    return OK;
}

#define LOGPATH(name, text)                                             \
    a_pad(u8, name##_buf, FILE_PATH_MAX_LEN);                           \
    a_cstr(name##_cstr, text);                                          \
    u8bFeed(name##_buf, name##_cstr);                                   \
    u8bFeed1(name##_buf, 0);                                            \
    a_dup(u8c, name, u8bData(name##_buf))

//  Parse + keep a raw-text copy so test predicates can compare on the
//  original URI bytes independent of URILexer's consume semantics.
typedef struct { uri u; char raw[512]; } saved_uri;

static ok64 parse_uri_lit(saved_uri *s, char const *text) {
    size_t n = strlen(text);
    if (n >= sizeof(s->raw)) return TESTFAIL;
    memcpy(s->raw, text, n);
    s->raw[n] = 0;
    zero(s->u);
    s->u.data[0] = (u8cp)s->raw;
    s->u.data[1] = (u8cp)s->raw + n;
    return URILexer(&s->u);
}

//  After URILexer, data is consumed; re-seed from the preserved raw
//  text before handing to ULOGAppend (which re-serialises components).
static uricp saved_uri_for_append(saved_uri *s) {
    size_t n = strlen(s->raw);
    s->u.data[0] = (u8cp)s->raw;
    s->u.data[1] = (u8cp)s->raw + n;
    return &s->u;
}

//  Re-serialise a parsed uri into a local buffer and compare bytes
//  against a literal.  Captures the round-trip through URIutf8Feed,
//  which is what ULOG emits.
static b8 uri_serializes_to(uricp u, char const *expect) {
    a_pad(u8, buf, 512);
    if (URIutf8Feed(u8bIdle(buf), u) != OK) return NO;
    a_dup(u8c, got, u8bData(buf));
    size_t el = strlen(expect);
    return (size_t)$len(got) == el && memcmp(got[0], expect, el) == 0;
}

static ron60 verb_of(char const *s) {
    ron60 v = 0;
    u8cs slice = {(u8cp)s, (u8cp)s + strlen(s)};
    a_dup(u8c, dup, slice);
    RONutf8sDrain(&v, dup);
    return v;
}

//  Compose a ulogrec from saved_uri + ts + verb (caller-provided).
static ulogrec rec_of(ron60 ts, ron60 verb, saved_uri *s) {
    ulogrec r = {.ts = ts, .verb = verb, .uri = *saved_uri_for_append(s)};
    return r;
}

static ok64 T_roundtrip(void) {
    sane(1);
    call(FILEInit);
    rm_tmp("/tmp/ulog-rt.log");
    LOGPATH(path, "/tmp/ulog-rt.log");

    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);
    want(ULOGCount(l_idx) == 0);

    saved_uri s1 = {}, s2 = {}, s3 = {};
    call(parse_uri_lit, &s1, "//localhost/repo?heads/master");
    call(parse_uri_lit, &s2, "//localhost/repo?staging/abcd1234");
    call(parse_uri_lit, &s3, "?heads/main");

    ulogrec r1 = rec_of(1000, verb_of("get"),  &s1);
    ulogrec r2 = rec_of(1001, verb_of("put"),  &s2);
    ulogrec r3 = rec_of(1002, verb_of("post"), &s3);
    call(ULOGAppendAt, l_data, l_idx, &r1);
    call(ULOGAppendAt, l_data, l_idx, &r2);
    call(ULOGAppendAt, l_data, l_idx, &r3);
    want(ULOGCount(l_idx) == 3);

    ulogrec g = {};
    call(ULOGHead, l_data, l_idx, &g);
    want(g.verb == verb_of("get"));
    want(uri_serializes_to(&g.uri, "//localhost/repo?heads/master"));
    call(ULOGTail, l_data, l_idx, &g);
    want(g.verb == verb_of("post"));
    want(uri_serializes_to(&g.uri, "?heads/main"));

    call(ULOGClose, l_data, &l_idx, YES);
    done;
}

static ok64 T_persist(void) {
    sane(1);
    LOGPATH(path, "/tmp/ulog-rt.log");

    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);
    want(ULOGCount(l_idx) == 3);

    ulogrec g = {};
    call(ULOGRow, l_data, l_idx, 1, &g);
    want(g.verb == verb_of("put"));
    want(uri_serializes_to(&g.uri, "//localhost/repo?staging/abcd1234"));

    call(ULOGClose, l_data, &l_idx, YES);
    rm_tmp("/tmp/ulog-rt.log");
    done;
}

static ok64 T_seek(void) {
    sane(1);
    rm_tmp("/tmp/ulog-sk.log");
    LOGPATH(path, "/tmp/ulog-sk.log");

    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);

    ron60 stamps[] = {100, 200, 300, 400};
    saved_uri s = {};
    call(parse_uri_lit, &s, "//h/p");
    for (u32 i = 0; i < 4; i++) {
        ulogrec r = rec_of(stamps[i], verb_of("get"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
    }
    want(ULOGCount(l_idx) == 4);

    u32 i = 99;
    call(ULOGSeek, l_idx, 250, &i);    want(i == 2);
    call(ULOGSeek, l_idx, 200, &i);    want(i == 1);
    call(ULOGSeek, l_idx, 500, &i);    want(i == 4);
    call(ULOGSeek, l_idx, 50,  &i);    want(i == 0);

    call(ULOGFind, l_idx, 300, &i);    want(i == 2);
    want(ULOGHas(l_idx, 100) == YES);
    want(ULOGHas(l_idx, 250) == NO);
    want(ULOGHas(l_idx, 400) == YES);
    want(ULOGFind(l_idx, 250, &i) == ULOGNONE);

    call(ULOGClose, l_data, &l_idx, YES);
    rm_tmp("/tmp/ulog-sk.log");
    done;
}

static ok64 T_clock(void) {
    sane(1);
    rm_tmp("/tmp/ulog-ck.log");
    LOGPATH(path, "/tmp/ulog-ck.log");

    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);

    saved_uri s = {};
    call(parse_uri_lit, &s, "//h/p");
    {
        ulogrec r = rec_of(1000, verb_of("get"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
    }
    {
        ulogrec r = rec_of(1000, verb_of("get"), &s);
        want(ULOGAppendAt(l_data, l_idx, &r) == ULOGCLOCK);
    }
    {
        ulogrec r = rec_of(999, verb_of("get"), &s);
        want(ULOGAppendAt(l_data, l_idx, &r) == ULOGCLOCK);
    }
    {
        ulogrec r = rec_of(1001, verb_of("get"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
    }
    want(ULOGCount(l_idx) == 2);

    call(ULOGClose, l_data, &l_idx, YES);
    rm_tmp("/tmp/ulog-ck.log");
    done;
}

static ok64 T_findverb(void) {
    sane(1);
    rm_tmp("/tmp/ulog-fv.log");
    LOGPATH(path, "/tmp/ulog-fv.log");

    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);

    saved_uri s1 = {}, s2 = {}, s3 = {}, s4 = {};
    call(parse_uri_lit, &s1, "?heads/master");
    call(parse_uri_lit, &s2, "?staging/deadbeef");
    call(parse_uri_lit, &s3, "?heads/main");
    call(parse_uri_lit, &s4, "?staging/cafef00d");

    {
        ulogrec r = rec_of(10, verb_of("get"),  &s1); call(ULOGAppendAt, l_data, l_idx, &r);
    } {
        ulogrec r = rec_of(20, verb_of("put"),  &s2); call(ULOGAppendAt, l_data, l_idx, &r);
    } {
        ulogrec r = rec_of(30, verb_of("post"), &s3); call(ULOGAppendAt, l_data, l_idx, &r);
    } {
        ulogrec r = rec_of(40, verb_of("put"),  &s4); call(ULOGAppendAt, l_data, l_idx, &r);
    }

    ulogrec g = {};
    call(ULOGFindVerb, l_data, l_idx, verb_of("put"), &g);
    want(g.ts == 40);
    want(uri_serializes_to(&g.uri, "?staging/cafef00d"));

    call(ULOGFindVerb, l_data, l_idx, verb_of("post"), &g);
    want(g.ts == 30);
    want(uri_serializes_to(&g.uri, "?heads/main"));

    want(ULOGFindVerb(l_data, l_idx, verb_of("patch"), &g) == ULOGNONE);

    call(ULOGClose, l_data, &l_idx, YES);
    rm_tmp("/tmp/ulog-fv.log");
    done;
}

static ok64 T_truncate(void) {
    sane(1);
    rm_tmp("/tmp/ulog-tr.log");
    LOGPATH(path, "/tmp/ulog-tr.log");

    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);

    saved_uri s = {};
    call(parse_uri_lit, &s, "//h/p");
    for (u32 i = 0; i < 5; i++) {
        ulogrec r = rec_of((ron60)(100 + i), verb_of("get"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
    }
    want(ULOGCount(l_idx) == 5);

    call(ULOGTruncate, l_data, l_idx, 3);
    want(ULOGCount(l_idx) == 3);

    ulogrec g = {};
    call(ULOGTail, l_data, l_idx, &g);
    want(g.ts == 102);

    {
        ulogrec r = rec_of(200, verb_of("get"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
    }
    want(ULOGCount(l_idx) == 4);

    call(ULOGClose, l_data, &l_idx, YES);

    u8bp  l2_data = NULL;
    wh128bp l2_idx  = NULL;
    call(ULOGOpen, &l2_data, &l2_idx, path);
    want(ULOGCount(l2_idx) == 4);
    call(ULOGTail, l2_data, l2_idx, &g);
    want(g.ts == 200);
    call(ULOGClose, l2_data, &l2_idx, YES);
    rm_tmp("/tmp/ulog-tr.log");
    done;
}

//  Feed N rows into a scratch buffer, Drain them back; verb, ts and
//  the re-serialized URI must match.
static ok64 T_stream(void) {
    sane(1);
    a_pad(u8, buf, 1024);

    saved_uri s1 = {}, s2 = {};
    call(parse_uri_lit, &s1, "//host/path?heads/main");
    call(parse_uri_lit, &s2, "?staging/0123");

    {
        ulogrec r = rec_of(1000, verb_of("get"), &s1);
        call(ULOGu8sFeed, u8bIdle(buf), &r);
    }
    {
        ulogrec r = rec_of(1001, verb_of("put"), &s2);
        call(ULOGu8sFeed, u8bIdle(buf), &r);
    }

    a_dup(u8c, data, u8bData(buf));
    u8cs scan = {data[0], data[1]};

    ulogrec g = {};
    call(ULOGu8sDrain, scan, &g);
    want(g.ts == 1000 && g.verb == verb_of("get"));
    want(uri_serializes_to(&g.uri, "//host/path?heads/main"));

    call(ULOGu8sDrain, scan, &g);
    want(g.ts == 1001 && g.verb == verb_of("put"));
    want(uri_serializes_to(&g.uri, "?staging/0123"));

    want(u8csEmpty(scan) == YES);
    want(ULOGu8sDrain(scan, &g) == NODATA);

    //  Partial row (no '\n') → NODATA, scan unchanged.
    u8c partial[] = "1000\tget\thttp://x";
    u8cs pscan = {partial, partial + sizeof(partial) - 1};
    u8cp pstart = pscan[0];
    want(ULOGu8sDrain(pscan, &g) == NODATA);
    want(pscan[0] == pstart);
    done;
}

//  Multi-space / multi-tab separators must parse the same as a single
//  tab.  Trailing whitespace after the URI is not part of the spec
//  (URIs terminate at '\n') so we don't test that case.
static ok64 T_whitespace(void) {
    sane(1);
    //  Build a canonical single-tab row via Feed, then synthesize a
    //  sloppy variant with wide whitespace and confirm Drain parses
    //  both the same way.
    a_pad(u8, canon, 256);
    saved_uri su = {};
    call(parse_uri_lit, &su, "//host/path");
    {
        ulogrec r = rec_of(1000, verb_of("post"), &su);
        call(ULOGu8sFeed, u8bIdle(canon), &r);
    }

    u8c wide_row[] = "Fd  \t post \t\t //host/path\n";
    u8cs wide_scan = {wide_row, wide_row + sizeof(wide_row) - 1};

    ulogrec g = {};
    call(ULOGu8sDrain, wide_scan, &g);
    want(g.ts == 1000);
    want(g.verb == verb_of("post"));
    want(uri_serializes_to(&g.uri, "//host/path"));
    done;
}

// --- latest-per-key helpers & tests ----------------------------------

typedef struct {
    u32    n;
    ron60  ts[16];
    ron60  verb[16];
    char   uri[16][128];
} each_collect;

static ok64 each_cb(ulogreccp rec, void *ctx) {
    sane(rec && ctx);
    each_collect *c = (each_collect *)ctx;
    if (c->n >= 16) fail(FAIL);
    c->ts[c->n]   = rec->ts;
    c->verb[c->n] = rec->verb;
    a_pad(u8, buf, 128);
    uri u = rec->uri;
    call(URIutf8Feed, u8bIdle(buf), &u);
    size_t L = u8bDataLen(buf);
    if (L >= sizeof(c->uri[0])) L = sizeof(c->uri[0]) - 1;
    memcpy(c->uri[c->n], u8bDataHead(buf), L);
    c->uri[c->n][L] = 0;
    c->n++;
    done;
}

static ok64 T_each_latest(void) {
    sane(1);
    rm_tmp("/tmp/ulog-el.log");
    LOGPATH(path, "/tmp/ulog-el.log");

    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);

    //  Two keys, three revisions: main@1, feat@2, main@3, main@4, feat@5.
    //  Plus one `get` row to verify verb filter (`set` only).
    saved_uri m1 = {}, f2 = {}, m3 = {}, m4 = {}, f5 = {}, g6 = {};
    call(parse_uri_lit, &m1, "?heads/main#?1111");
    call(parse_uri_lit, &f2, "?heads/feat#?aaaa");
    call(parse_uri_lit, &m3, "?heads/main#?3333");
    call(parse_uri_lit, &m4, "?heads/main#?4444");
    call(parse_uri_lit, &f5, "?heads/feat#?bbbb");
    call(parse_uri_lit, &g6, "?heads/main#?6666");

    ron60 set_v = verb_of("set"), get_v = verb_of("get");
    {
        ulogrec r = rec_of(1, set_v, &m1); call(ULOGAppendAt, l_data, l_idx, &r);
    } {
        ulogrec r = rec_of(2, set_v, &f2); call(ULOGAppendAt, l_data, l_idx, &r);
    } {
        ulogrec r = rec_of(3, set_v, &m3); call(ULOGAppendAt, l_data, l_idx, &r);
    } {
        ulogrec r = rec_of(4, set_v, &m4); call(ULOGAppendAt, l_data, l_idx, &r);
    } {
        ulogrec r = rec_of(5, set_v, &f5); call(ULOGAppendAt, l_data, l_idx, &r);
    } {
        ulogrec r = rec_of(6, get_v, &g6); call(ULOGAppendAt, l_data, l_idx, &r);
    }

    //  Filter = `set`: expect (feat@5) then (main@4) — reverse order,
    //  one row per key, get_v row skipped entirely.
    each_collect c = {};
    call(ULOGeachLatest, l_data, l_idx, set_v, each_cb, &c);
    want(c.n == 2);
    want(c.ts[0] == 5);
    want(strcmp(c.uri[0], "?heads/feat#?bbbb") == 0);
    want(c.ts[1] == 4);
    want(strcmp(c.uri[1], "?heads/main#?4444") == 0);

    //  No filter: the `get` row appears too — still its own (verb, key)
    //  so it dedups independently of set's rows.
    each_collect c_all = {};
    call(ULOGeachLatest, l_data, l_idx, 0, each_cb, &c_all);
    want(c_all.n == 3);        // get/main@6, set/feat@5, set/main@4
    want(c_all.verb[0] == get_v);
    want(c_all.ts[0] == 6);
    want(c_all.verb[1] == set_v);
    want(c_all.ts[1] == 5);
    want(c_all.verb[2] == set_v);
    want(c_all.ts[2] == 4);

    call(ULOGClose, l_data, &l_idx, YES);
    rm_tmp("/tmp/ulog-el.log");
    done;
}

static ok64 T_compact_latest(void) {
    sane(1);
    rm_tmp("/tmp/ulog-cl.log");
    LOGPATH(path, "/tmp/ulog-cl.log");

    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);

    saved_uri s[5] = {};
    call(parse_uri_lit, &s[0], "?heads/main#?1111");
    call(parse_uri_lit, &s[1], "?heads/feat#?aaaa");
    call(parse_uri_lit, &s[2], "?heads/main#?2222");  // shadowed
    call(parse_uri_lit, &s[3], "?heads/feat#?bbbb");  // shadowed later
    call(parse_uri_lit, &s[4], "?heads/feat#?cccc");

    ron60 set_v = verb_of("set");
    for (u32 i = 0; i < 5; i++) {
        ulogrec r = rec_of(10 + i, set_v, &s[i]);
        call(ULOGAppendAt, l_data, l_idx, &r);
    }
    want(ULOGCount(l_idx) == 5);

    call(ULOGCompactLatest, &l_data, &l_idx, path, set_v);
    want(ULOGCount(l_idx) == 2);

    //  Survivors in ts order: main@12, feat@14.
    ulogrec g = {};
    call(ULOGRow, l_data, l_idx, 0, &g);
    want(g.ts == 12);
    want(uri_serializes_to(&g.uri, "?heads/main#?2222"));
    call(ULOGRow, l_data, l_idx, 1, &g);
    want(g.ts == 14);
    want(uri_serializes_to(&g.uri, "?heads/feat#?cccc"));

    call(ULOGClose, l_data, &l_idx, YES);
    rm_tmp("/tmp/ulog-cl.log");
    rm_tmp("/tmp/ulog-cl.log.tmp");
    done;
}

//  Regression: aborted RW open leaves a sparse, page-sized file with
//  no real content (FILEBookCreate ftruncate's to a page before any
//  row gets written; if the writer crashes / is killed before
//  ULOGAppendAt, the file persists in this state).  A subsequent RO
//  open via ULOGOpenRO must NOT crash on the first byte read of a
//  sparse hole — it must surface as an empty log so callers fall
//  through to the "no rows" branch (e.g., SNIFFAtTailOf returning
//  SNIFFATNONE for a fresh worktree).
static ok64 T_aborted_leftover(void) {
    sane(1);
    char const *home = getenv("HOME");
    if (!home) home = "/tmp";
    char tpath[512];
    snprintf(tpath, sizeof(tpath), "%s/tmp/ulog-al.log", home);
    {
        char mkd[512];
        snprintf(mkd, sizeof(mkd), "mkdir -p %s/tmp", home);
        int _ = system(mkd); (void)_;
    }
    rm_tmp(tpath);
    a_pad(u8, path_buf, FILE_PATH_MAX_LEN);
    {
        size_t L = strlen(tpath);
        u8cs s = {(u8cp)tpath, (u8cp)tpath + L};
        u8bFeed(path_buf, s);
        u8bFeed1(path_buf, 0);
    }
    a_dup(u8c, path, u8bData(path_buf));

    //  Mirror FILEBookCreate's body without writing any content:
    //  create the file, ftruncate it to one page, close.  This is
    //  exactly the on-disk state an aborted RW open leaves behind.
    //  Hardcoded 4096 (vs sysconf) because FILESysPage isn't in the
    //  public header and any sane page size triggers the same path.
    int fd = FILE_CLOSED;
    call(FILECreate, &fd, path);
    call(FILEResize, &fd, 4096);
    call(FILEClose, &fd);

    //  ULOGOpenRO must succeed and surface zero rows — the previous
    //  symptom was SIGBUS on *scan[0] inside ulog_rebuild_idx.
    u8bp  l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpenRO, &l_data, &l_idx, path);
    want(ULOGCount(l_idx) == 0);
    call(ULOGClose, l_data, &l_idx, NO);

    rm_tmp(tpath);
    done;
}

// --- Sidecar tests ---------------------------------------------------

//  Open writes the sidecar; reopen finds it fresh and reuses it
//  (no rebuild from log).  Verified by checking the sidecar file
//  exists on disk and the verb-hash field round-trips through it.
static ok64 T_sidecar_reuse(void) {
    sane(1);
    rm_tmp("/tmp/ulog-sc.log");
    rm_tmp("/tmp/.ulog-sc.log.idx");
    LOGPATH(path, "/tmp/ulog-sc.log");

    saved_uri s = {};
    call(parse_uri_lit, &s, "?heads/main");

    {   //  Session 1: create + append.
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        ulogrec r = rec_of(1000, verb_of("get"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
        call(ULOGClose, l_data, &l_idx, YES);
    }

    //  Sidecar must exist on disk now.
    {
        struct stat st = {};
        want(stat("/tmp/.ulog-sc.log.idx", &st) == 0);
        //  1 row + 1 sentinel = 2 entries × 16 B.
        want((size_t)st.st_size == 2 * 16);
    }

    {   //  Session 2: reopen, verify row data + verb-hash field.
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        want(ULOGCount(l_idx) == 1);

        //  Verb prefilter byte-equal: index entry 0 has the hash of
        //  `get`; ULOGFindVerb must locate the row without scanning
        //  past it.
        ulogrec g = {};
        call(ULOGFindVerb, l_data, l_idx, verb_of("get"), &g);
        want(g.ts == 1000);

        //  A different verb's hash must NOT match — ULOGFindVerb
        //  returns ULOGNONE without dereferencing.
        want(ULOGFindVerb(l_data, l_idx, verb_of("put"), &g) == ULOGNONE);

        call(ULOGClose, l_data, &l_idx, YES);
    }

    rm_tmp("/tmp/ulog-sc.log");
    rm_tmp("/tmp/.ulog-sc.log.idx");
    done;
}

//  A stale sidecar (sentinel offset disagrees with actual log size)
//  must be rebuilt from the log on the next Open.  Simulated by
//  overwriting the sidecar with a single bogus sentinel.
static ok64 T_sidecar_stale_rebuild(void) {
    sane(1);
    rm_tmp("/tmp/ulog-st.log");
    rm_tmp("/tmp/.ulog-st.log.idx");
    LOGPATH(path, "/tmp/ulog-st.log");

    saved_uri s = {};
    call(parse_uri_lit, &s, "?heads/main");

    //  Append three rows, close (writes sidecar).
    {
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        for (u32 i = 0; i < 3; i++) {
            ulogrec r = rec_of((ron60)(100 + i), verb_of("get"), &s);
            call(ULOGAppendAt, l_data, l_idx, &r);
        }
        call(ULOGClose, l_data, &l_idx, YES);
    }

    //  Corrupt the sidecar: rewrite as a single sentinel claiming
    //  size=0.  Reopen must detect the mismatch (log file size != 0)
    //  and rebuild — surfacing all 3 rows.
    {
        int fd = open("/tmp/.ulog-st.log.idx", O_RDWR | O_TRUNC);
        if (fd < 0) fail(TESTFAIL);
        u8 sentinel[16] = {};   // ts=0, val=0 → off=0
        if (write(fd, sentinel, 16) != 16) { close(fd); fail(TESTFAIL); }
        close(fd);
    }

    {
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        want(ULOGCount(l_idx) == 3);    // rebuilt
        ulogrec g = {};
        call(ULOGTail, l_data, l_idx, &g);
        want(g.ts == 102);
        call(ULOGClose, l_data, &l_idx, YES);
    }

    rm_tmp("/tmp/ulog-st.log");
    rm_tmp("/tmp/.ulog-st.log.idx");
    done;
}

//  DIS-033: a "false-fresh" sidecar — its tail SENTINEL records the
//  current log's (size, mtime) but its ROW entries describe only an
//  older, shorter PREFIX (rows appended after that prefix are missing).
//  The (size, mtime) signal alone is too coarse to catch this: a
//  sidecar streamed/copied during a clone, or one whose sentinel was
//  refreshed without its rows being rebuilt, lands in exactly this
//  shape.  Reading it drops the tail rows — `SNIFFAtTailOf` then misses
//  the wt's current tip and `be log:` resolves no `--at`.  Open must
//  detect that the indexed rows do not span the whole log and rebuild,
//  surfacing every row (here: all 4, with tail ts == 403).
static ok64 T_sidecar_false_fresh(void) {
    sane(1);
    rm_tmp("/tmp/ulog-ff.log");
    rm_tmp("/tmp/.ulog-ff.log.idx");
    LOGPATH(path, "/tmp/ulog-ff.log");

    saved_uri s = {};
    call(parse_uri_lit, &s, "?heads/main");

    //  Session 1: append the first THREE rows, close (writes a sidecar
    //  whose entries cover exactly those 3 rows).  Snapshot that sidecar.
    {
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        for (u32 i = 0; i < 3; i++) {
            ulogrec r = rec_of((ron60)(400 + i), verb_of("get"), &s);
            call(ULOGAppendAt, l_data, l_idx, &r);
        }
        call(ULOGClose, l_data, &l_idx, YES);
    }
    //  Snapshot the 3-row sidecar bytes (rows-prefix; its own sentinel
    //  is discarded below — we want a sentinel that matches the FULL log).
    u8 prefix_idx[4 * 16] = {};
    u32 prefix_len = 0;
    {
        int fd = open("/tmp/.ulog-ff.log.idx", O_RDONLY);
        if (fd < 0) fail(TESTFAIL);
        ssize_t got = read(fd, prefix_idx, sizeof(prefix_idx));
        close(fd);
        if (got <= 16) fail(TESTFAIL);
        prefix_len = (u32)got;              // 3 rows + 1 sentinel = 64 B
    }

    //  Session 2: append a FOURTH row, close (sidecar now covers 4 rows
    //  and its sentinel records the full log's size+mtime).  Snapshot the
    //  full-log sentinel (the last 16 bytes).
    u8 full_sentinel[16] = {};
    {
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        ulogrec r = rec_of((ron60)403, verb_of("get"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
        call(ULOGClose, l_data, &l_idx, YES);
    }
    {
        int fd = open("/tmp/.ulog-ff.log.idx", O_RDONLY);
        if (fd < 0) fail(TESTFAIL);
        u8 buf[5 * 16] = {};
        ssize_t got = read(fd, buf, sizeof(buf));
        close(fd);
        if (got < 32) fail(TESTFAIL);       // at least 1 row + sentinel
        memcpy(full_sentinel, buf + (got - 16), 16);
    }

    //  Forge the false-fresh sidecar: the 3-row PREFIX entries (drop the
    //  prefix's own size=3 sentinel) followed by the FULL-log sentinel.
    //  Now (size, mtime) match the 4-row log, but the row entries only
    //  describe the first 3 rows.
    {
        int fd = open("/tmp/.ulog-ff.log.idx", O_RDWR | O_TRUNC);
        if (fd < 0) fail(TESTFAIL);
        u32 rows_bytes = prefix_len - 16;   // drop prefix sentinel
        if (write(fd, prefix_idx, rows_bytes) != (ssize_t)rows_bytes) {
            close(fd); fail(TESTFAIL);
        }
        if (write(fd, full_sentinel, 16) != 16) { close(fd); fail(TESTFAIL); }
        close(fd);
    }

    //  Reopen: the freshness check sees matching (size, mtime) but the
    //  indexed rows stop short of the log's end — it must REBUILD and
    //  surface all 4 rows (tail ts == 403), not the stale 3-row prefix.
    {
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        want(ULOGCount(l_idx) == 4);        // rebuilt — not the stale 3
        ulogrec g = {};
        call(ULOGTail, l_data, l_idx, &g);
        want(g.ts == 403);
        call(ULOGClose, l_data, &l_idx, YES);
    }

    rm_tmp("/tmp/ulog-ff.log");
    rm_tmp("/tmp/.ulog-ff.log.idx");
    done;
}

//  Sidecar fast-path: when the log is unchanged (same size AND same
//  mtime), Open must trust the on-disk sidecar verbatim — NO rebuild
//  scan.  Verified by poking a bogus verb-hash into a row entry
//  between close and reopen; the bogus byte survives iff the rebuild
//  was skipped (a rebuild would recompute the verb hash from the log).
static ok64 T_sidecar_fast_path(void) {
    sane(1);
    rm_tmp("/tmp/ulog-fp.log");
    rm_tmp("/tmp/.ulog-fp.log.idx");
    LOGPATH(path, "/tmp/ulog-fp.log");

    saved_uri s = {};
    call(parse_uri_lit, &s, "?heads/main");

    {   //  Append two rows, close.
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        for (u32 i = 0; i < 2; i++) {
            ulogrec r = rec_of((ron60)(200 + i), verb_of("get"), &s);
            call(ULOGAppendAt, l_data, l_idx, &r);
        }
        call(ULOGClose, l_data, &l_idx, YES);
    }

    //  Poke a sentinel verb-hash value into the FIRST row's val (the
    //  20-bit id field of wh64).  Real `get` hash is whatever
    //  ulogVerbHash(verb_of("get")) returns; we slam a known different
    //  20-bit pattern into the bits and check it survives.
    u32 marker_hash = 0x12345;
    {
        int fd = open("/tmp/.ulog-fp.log.idx", O_RDWR);
        if (fd < 0) fail(TESTFAIL);
        wh128 e = {};
        if (pread(fd, &e, sizeof(e), 0) != (ssize_t)sizeof(e)) {
            close(fd); fail(TESTFAIL);
        }
        //  Replace the 20-bit id field (verb hash) in val.
        u64 v = e.val;
        v &= ~(WHIFF_ID_MASK << WHIFF_ID_SHIFT);
        v |=  ((u64)marker_hash & WHIFF_ID_MASK) << WHIFF_ID_SHIFT;
        e.val = v;
        if (pwrite(fd, &e, sizeof(e), 0) != (ssize_t)sizeof(e)) {
            close(fd); fail(TESTFAIL);
        }
        close(fd);
    }

    //  Reopen: if the fast path triggers, our marker is preserved;
    //  if a rebuild ran, the real `get`-hash overwrites it.
    {
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        want(ULOGCount(l_idx) == 2);
        wh128 const *a = (wh128 const *)l_idx[0];
        want(wh64Id(a[0].val) == marker_hash);
        call(ULOGClose, l_data, &l_idx, YES);
    }

    rm_tmp("/tmp/ulog-fp.log");
    rm_tmp("/tmp/.ulog-fp.log.idx");
    done;
}

//  RO open with no sidecar present: must succeed via the anonymous
//  mmap fallback and surface every row from the log.  The idx slot
//  must NOT register as booked (FILEIsBooked == NO).
static ok64 T_sidecar_ro_fallback(void) {
    sane(1);
    rm_tmp("/tmp/ulog-rof.log");
    rm_tmp("/tmp/.ulog-rof.log.idx");
    LOGPATH(path, "/tmp/ulog-rof.log");

    saved_uri s = {};
    call(parse_uri_lit, &s, "?heads/main");

    //  RW: append 2 rows, close, then unlink the sidecar to simulate
    //  RO mount / vanished sidecar.
    {
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpen, &l_data, &l_idx, path);
        ulogrec r1 = rec_of(10, verb_of("get"), &s);
        ulogrec r2 = rec_of(20, verb_of("put"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r1);
        call(ULOGAppendAt, l_data, l_idx, &r2);
        call(ULOGClose, l_data, &l_idx, YES);
    }
    rm_tmp("/tmp/.ulog-rof.log.idx");

    //  RO open: sidecar absent → quiet anonymous-mmap fallback.
    {
        u8bp    l_data = NULL;
        wh128bp l_idx  = NULL;
        call(ULOGOpenRO, &l_data, &l_idx, path);
        want(ULOGCount(l_idx) == 2);
        want(FILEIsBooked((u8bp)l_idx) == NO);   // anonymous fallback

        ulogrec g = {};
        call(ULOGRow, l_data, l_idx, 0, &g);
        want(g.ts == 10);
        call(ULOGRow, l_data, l_idx, 1, &g);
        want(g.ts == 20);

        call(ULOGClose, l_data, &l_idx, NO);
    }

    rm_tmp("/tmp/ulog-rof.log");
    done;
}

//  Verb prefilter correctness: ULOGFindVerb must reject rows whose
//  20-bit verb hash doesn't match before parsing them.  Insert many
//  rows of one verb and one row of another; FindVerb on the second
//  must locate it correctly without touching the first verb's rows.
static ok64 T_verb_prefilter(void) {
    sane(1);
    rm_tmp("/tmp/ulog-vf.log");
    rm_tmp("/tmp/.ulog-vf.log.idx");
    LOGPATH(path, "/tmp/ulog-vf.log");

    u8bp    l_data = NULL;
    wh128bp l_idx  = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);

    saved_uri s = {};
    call(parse_uri_lit, &s, "?heads/main");

    //  100 `get` rows then one `delete` row.
    for (u32 i = 0; i < 100; i++) {
        ulogrec r = rec_of((ron60)(i + 1), verb_of("get"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
    }
    {
        ulogrec r = rec_of(200, verb_of("delete"), &s);
        call(ULOGAppendAt, l_data, l_idx, &r);
    }

    ulogrec g = {};
    call(ULOGFindVerb, l_data, l_idx, verb_of("delete"), &g);
    want(g.ts == 200);
    want(g.verb == verb_of("delete"));

    //  An absent verb still returns ULOGNONE.
    want(ULOGFindVerb(l_data, l_idx, verb_of("patch"), &g) == ULOGNONE);

    call(ULOGClose, l_data, &l_idx, YES);
    rm_tmp("/tmp/ulog-vf.log");
    rm_tmp("/tmp/.ulog-vf.log.idx");
    done;
}

//  Regression: a deep ignored directory holding tens of thousands of
//  entries must not overflow ULOGu8bScanWt's sort scratch.  The walk
//  descends into ignored dirs unconditionally (the skip predicate
//  filters per-file, *after* FILEScanSorted has loaded+sorted the whole
//  directory into scratch), so a real `Corpus/` dir blew the old fixed
//  1 MB heap buffer and `be diff:` failed with NOROOM.  Carving 16 MB
//  from BASS fixes it.  Build `big/` with enough long-named files that
//  their entry list (capped at ~half the scratch by LSMSort tmp space)
//  exceeds 1 MB but stays well under 16 MB.

static b8 scanwt_skip_big(u8cs rel, void *ctx) {
    (void)ctx;
    a_cstr(pfx, "big/");
    return u8csHasPrefix(rel, pfx);
}

static ok64 T_scanwt_big_ignored(void) {
    sane(1);
    call(FILEInit);
    rm_tmp("/tmp/ulog-scanwt");

    //  ~15000 files with ~68-char names under an ignored `big/` dir,
    //  plus two emitted files at the root.  awk formats the names so
    //  xargs can batch the touches (no per-file fork).
    {
        char cmd[1024];
        snprintf(cmd, sizeof cmd,
            "mkdir -p /tmp/ulog-scanwt/big && "
            "printf 'hello\\n' > /tmp/ulog-scanwt/a.txt && "
            "printf 'world\\n' > /tmp/ulog-scanwt/b.txt && "
            "cd /tmp/ulog-scanwt/big && seq 1 15000 | awk "
            "'{printf \"padding_padding_padding_padding_padding_padding_"
            "padding_%%012d\\n\",$1}' | xargs touch");
        int rc = system(cmd);
        if (rc != 0) return TESTFAIL;
    }

    a_carve(u8, out, 1UL << 16);
    a_cstr(root, "/tmp/ulog-scanwt");   // clean slice (no trailing NUL)

    //  The regression: old code returned NOROOM here.
    call(ULOGu8bScanWt, root, verb_of("wt"), scanwt_skip_big, NULL, out);

    //  Root files emitted; nothing from the ignored `big/` subtree.
    char buf[4096];
    a_dup(u8c, od, u8bData(out));
    size_t n = (size_t)$len(od);
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, od[0], n);
    buf[n] = 0;
    want(strstr(buf, "a.txt") != NULL);
    want(strstr(buf, "b.txt") != NULL);
    want(strstr(buf, "big/")  == NULL);
    want(strstr(buf, "padding_padding") == NULL);

    rm_tmp("/tmp/ulog-scanwt");
    done;
}

//  Count the process's currently-open file descriptors (entries in
//  /proc/self/fd, minus the opendir's own handle).  Used to assert a
//  failed open did not leak the booked log's fd.
static int open_fd_count(void) {
    DIR *d = opendir("/proc/self/fd");
    if (!d) return -1;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        n++;
    }
    closedir(d);
    return n - 1;   // discount the opendir fd itself
}

//  Test hook (dog/ULOG.c): force the next ulog_idx_alloc_anon to fail.
extern u32 ULOG_FAULT_ALLOC_ANON;

//  MEM-016 repro: ULOGOpenBooked / ULOGOpenRO FILEBook the log, then
//  call ulog_idx_alloc_anon for a scratch scan idx.  When that scratch
//  alloc fails (memory pressure), the old `call()` returned immediately
//  WITHOUT unbooking the log — leaking the mmap + fd for the process
//  lifetime.  We force the alloc failure right after the book and assert
//  the open fails AND the booked fd was released (no leak), for both the
//  RW and RO open paths.
static ok64 T_open_alloc_fail_no_leak(void) {
    sane(1);
    call(FILEInit);
    rm_tmp("/tmp/ulog-leak.log");
    LOGPATH(path, "/tmp/ulog-leak.log");

    //  Seed a valid one-row log so the file exists for both opens.
    u8bp l_data = NULL; wh128bp l_idx = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);
    saved_uri s1 = {};
    call(parse_uri_lit, &s1, "//localhost/repo?heads/master");
    ulogrec r1 = rec_of(2000, verb_of("get"), &s1);
    call(ULOGAppendAt, l_data, l_idx, &r1);
    call(ULOGClose, l_data, &l_idx, YES);

    //  --- RW open path (ULOGOpenBooked) ---
    int fd0 = open_fd_count();
    want(fd0 >= 0);

    ULOG_FAULT_ALLOC_ANON = 1;       // trip the scratch alloc after book
    u8bp d2 = NULL; wh128bp i2 = NULL;
    ok64 rc = ULOGOpenBooked(&d2, &i2, path, 4096, 4096);
    ULOG_FAULT_ALLOC_ANON = 0;
    want(rc != OK);                  // the forced alloc failure propagates
    want(d2 == NULL || d2[0] == NULL);   // booked slot was unbooked
    want(open_fd_count() == fd0);    // no leaked fd

    //  --- RO open path (ULOGOpenRO) ---
    ULOG_FAULT_ALLOC_ANON = 1;
    u8bp d3 = NULL; wh128bp i3 = NULL;
    ok64 rc2 = ULOGOpenRO(&d3, &i3, path);
    ULOG_FAULT_ALLOC_ANON = 0;
    want(rc2 != OK);
    want(d3 == NULL || d3[0] == NULL);
    want(open_fd_count() == fd0);    // still no leaked fd

    rm_tmp("/tmp/ulog-leak.log");
    done;
}

//  Regression (the .be/wtlog wipe): an EXISTING log whose FILEBook fails
//  must NOT fall through to the O_TRUNC FILEBookCreate and destroy the
//  bytes — ULOGOpenBooked must propagate the error and leave content
//  intact.  In the wild the trigger was a 9p mount rejecting
//  mmap(MAP_SHARED) with EINVAL.  We reproduce a comparable FILEBook
//  failure portably: grow the file PAST the book_size so FILEBookFD's
//  `map_size <= book_size` guard returns BADARG.  The file stays
//  writable, so the OLD unconditional-create fallback WOULD truncate it.
static ok64 T_no_truncate_on_book_fail(void) {
    sane(1);
    call(FILEInit);
    rm_tmp("/tmp/ulog-nt.log");
    LOGPATH(path, "/tmp/ulog-nt.log");

    //  Seed a valid one-row log and close it.
    u8bp l_data = NULL; wh128bp l_idx = NULL;
    call(ULOGOpen, &l_data, &l_idx, path);
    saved_uri s1 = {};
    call(parse_uri_lit, &s1, "//localhost/repo?heads/master");
    ulogrec r1 = rec_of(2000, verb_of("get"), &s1);
    call(ULOGAppendAt, l_data, l_idx, &r1);
    call(ULOGClose, l_data, &l_idx, YES);

    //  Grow it to 1 MiB — comfortably larger than any page-rounded
    //  4 KiB book_size, so the next FILEBook fails the size guard.
    {
        int fd = open("/tmp/ulog-nt.log", O_RDWR);
        want(fd >= 0);
        want(ftruncate(fd, 1UL << 20) == 0);
        close(fd);
    }
    struct stat st0 = {};
    want(stat("/tmp/ulog-nt.log", &st0) == 0);
    want(st0.st_size == (off_t)(1UL << 20));

    //  FILEBook fails on an EXISTING, writable file → must propagate,
    //  NOT truncate.  (Old bug: file shrinks to the book/init size.)
    u8bp d2 = NULL; wh128bp i2 = NULL;
    ok64 rc = ULOGOpenBooked(&d2, &i2, path, 4096, 4096);
    want(rc != OK);

    struct stat st1 = {};
    want(stat("/tmp/ulog-nt.log", &st1) == 0);
    want(st1.st_size == st0.st_size);   // preserved; the bug zeroed/shrank it

    rm_tmp("/tmp/ulog-nt.log");
    done;
}

//  ULOG-001: a VALID multi-row log whose first byte got clobbered to
//  NUL (torn / killed-mid / page-cache-lost write, or a genuinely
//  zero-padded create that a crash left behind) must NOT be silently
//  presented as an empty log and then TRUNCATED TO 0 by the next
//  RW open/close.  The old behaviour: ulog_scan_log stops at the
//  leading NUL → in-memory data length 0 → ULOGClose's FILETrimBook
//  ftruncate(fd, 0) destroyed the surviving on-disk history (objects
//  intact, refs/wtlog zeroed — the production forensics).  The fix
//  makes the scan refuse a torn log (ULOGTORN), so the open errors
//  out and the live bytes are never trimmed away.
static ok64 T_torn_first_byte_no_zero(void) {
    sane(1);
    call(FILEInit);
    rm_tmp("/tmp/ulog-torn1.log");
    rm_tmp("/tmp/.ulog-torn1.log.idx");
    LOGPATH(path, "/tmp/ulog-torn1.log");

    //  Seed a valid three-row log.
    u8bp d = NULL; wh128bp i = NULL;
    call(ULOGOpen, &d, &i, path);
    saved_uri s1 = {}, s2 = {}, s3 = {};
    call(parse_uri_lit, &s1, "?heads/master");
    call(parse_uri_lit, &s2, "?heads/main");
    call(parse_uri_lit, &s3, "?heads/dev");
    ulogrec r1 = rec_of(3001, verb_of("put"), &s1);
    ulogrec r2 = rec_of(3002, verb_of("put"), &s2);
    ulogrec r3 = rec_of(3003, verb_of("put"), &s3);
    call(ULOGAppendAt, d, i, &r1);
    call(ULOGAppendAt, d, i, &r2);
    call(ULOGAppendAt, d, i, &r3);
    call(ULOGClose, d, &i, YES);

    struct stat st0 = {};
    want(stat("/tmp/ulog-torn1.log", &st0) == 0);
    want(st0.st_size > 0);

    //  Clobber byte 0 to NUL — the torn-write signature.
    {
        int fd = open("/tmp/ulog-torn1.log", O_RDWR);
        want(fd >= 0);
        char z = 0;
        want(pwrite(fd, &z, 1, 0) == 1);
        close(fd);
    }
    //  A reopen must NOT succeed-as-empty.  Either it errors (preferred:
    //  ULOGTORN) or it recovers the rows; what it must NEVER do is
    //  silently report 0 rows AND then zero the file on close.
    u8bp d2 = NULL; wh128bp i2 = NULL;
    ok64 oo = ULOGOpen(&d2, &i2, path);
    if (oo == OK) {
        //  If a future fix chooses to recover instead of refuse, the
        //  rows must be back — never an empty log.
        want(ULOGCount(i2) > 0);
        call(ULOGClose, d2, &i2, YES);
    } else {
        want(oo == ULOGTORN || oo == ULOGCLOCK || oo == ULOGBADFMT);
    }

    //  The decisive assertion: the on-disk bytes survived.  The bug
    //  shrank this to 0; the fix leaves the original size untouched.
    struct stat st1 = {};
    want(stat("/tmp/ulog-torn1.log", &st1) == 0);
    want(st1.st_size == st0.st_size);

    rm_tmp("/tmp/ulog-torn1.log");
    rm_tmp("/tmp/.ulog-torn1.log.idx");
    done;
}

//  Companion: a NUL planted mid-file (after the first complete row)
//  must not be trusted as a clean tail either — the scan must refuse
//  rather than silently drop every row past the NUL on the next trim.
static ok64 T_torn_mid_no_zero(void) {
    sane(1);
    call(FILEInit);
    rm_tmp("/tmp/ulog-torn2.log");
    rm_tmp("/tmp/.ulog-torn2.log.idx");
    LOGPATH(path, "/tmp/ulog-torn2.log");

    u8bp d = NULL; wh128bp i = NULL;
    call(ULOGOpen, &d, &i, path);
    saved_uri s1 = {}, s2 = {}, s3 = {};
    call(parse_uri_lit, &s1, "?heads/master");
    call(parse_uri_lit, &s2, "?heads/main");
    call(parse_uri_lit, &s3, "?heads/dev");
    ulogrec r1 = rec_of(4001, verb_of("put"), &s1);
    ulogrec r2 = rec_of(4002, verb_of("put"), &s2);
    ulogrec r3 = rec_of(4003, verb_of("put"), &s3);
    call(ULOGAppendAt, d, i, &r1);
    call(ULOGAppendAt, d, i, &r2);
    call(ULOGAppendAt, d, i, &r3);
    call(ULOGClose, d, &i, YES);

    struct stat st0 = {};
    want(stat("/tmp/ulog-torn2.log", &st0) == 0);
    off_t orig = st0.st_size;

    //  NUL one byte past the first row's newline (so row 0 parses, then
    //  the scan hits a NUL with real rows still ahead of it).
    {
        int fd = open("/tmp/ulog-torn2.log", O_RDWR);
        want(fd >= 0);
        char z = 0;
        want(pwrite(fd, &z, 1, orig / 2) == 1);
        close(fd);
    }
    u8bp d2 = NULL; wh128bp i2 = NULL;
    ok64 oo = ULOGOpen(&d2, &i2, path);
    if (oo == OK) { call(ULOGClose, d2, &i2, YES); }
    else          { want(oo == ULOGTORN || oo == ULOGBADFMT || oo == ULOGCLOCK); }

    //  On-disk bytes preserved — no destructive trim of the live log.
    struct stat st1 = {};
    want(stat("/tmp/ulog-torn2.log", &st1) == 0);
    want(st1.st_size == orig);

    rm_tmp("/tmp/ulog-torn2.log");
    rm_tmp("/tmp/.ulog-torn2.log.idx");
    done;
}

ok64 ULOGtest(void) {
    sane(1);
    fprintf(stderr, "T_torn_first_byte_no_zero...\n"); call(T_torn_first_byte_no_zero);
    fprintf(stderr, "T_torn_mid_no_zero...\n");        call(T_torn_mid_no_zero);
    fprintf(stderr, "T_open_alloc_fail_no_leak...\n"); call(T_open_alloc_fail_no_leak);
    fprintf(stderr, "T_no_truncate_on_book_fail...\n"); call(T_no_truncate_on_book_fail);
    fprintf(stderr, "T_scanwt_big_ignored...\n"); call(T_scanwt_big_ignored);
    fprintf(stderr, "T_roundtrip...\n");     call(T_roundtrip);
    fprintf(stderr, "T_persist...\n");       call(T_persist);
    fprintf(stderr, "T_seek...\n");          call(T_seek);
    fprintf(stderr, "T_clock...\n");         call(T_clock);
    fprintf(stderr, "T_findverb...\n");      call(T_findverb);
    fprintf(stderr, "T_truncate...\n");      call(T_truncate);
    fprintf(stderr, "T_stream...\n");        call(T_stream);
    fprintf(stderr, "T_whitespace...\n");    call(T_whitespace);
    fprintf(stderr, "T_each_latest...\n");   call(T_each_latest);
    fprintf(stderr, "T_compact_latest...\n");call(T_compact_latest);
    fprintf(stderr, "T_aborted_leftover...\n"); call(T_aborted_leftover);
    fprintf(stderr, "T_sidecar_reuse...\n");        call(T_sidecar_reuse);
    fprintf(stderr, "T_sidecar_stale_rebuild...\n");call(T_sidecar_stale_rebuild);
    fprintf(stderr, "T_sidecar_false_fresh...\n");  call(T_sidecar_false_fresh);
    fprintf(stderr, "T_sidecar_fast_path...\n");    call(T_sidecar_fast_path);
    fprintf(stderr, "T_sidecar_ro_fallback...\n");  call(T_sidecar_ro_fallback);
    fprintf(stderr, "T_verb_prefilter...\n");       call(T_verb_prefilter);
    done;
}

TEST(ULOGtest);
