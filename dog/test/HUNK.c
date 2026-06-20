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
    //  BE-007 / POST-022: an empty child URI is a bannerless flat hunk
    //  (no module identity to mount), so it rebases to EMPTY — NOT a bare
    //  `<prefix>` banner line (the standalone sub-mount label wreckage).
    {"sub",        "",            ""},             // empty URI → stays empty
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
// BRO-003 — relaying a sub's row TABLE re-prefixes the per-row path
// column AND the hidden nav URI, not just the banner URI.  A child sniff
// emits one content hunk whose (text, toks) holds rows like
//   `<7sp> put core.c\ncat:core.c`
// with the path column tagged 'S' (ends in '\n') and the nav URI tagged
// 'U'.  Mounted at `vendor/sub`, the relay must rewrite the row to
//   `<7sp> put vendor/sub/core.c\ncat:vendor/sub/core.c`.
// =====================================================================

//  Append a tok32 (tag, eq side, end-offset) to a u32 buffer.
static ok64 hunk_test_tok(Bu32 b, u8 tag, u32 off) {
    sane(u32bOK(b));
    call(u32bFeed1, b, tok32PackSide(tag, TOK_SIDE_EQ, off));
    done;
}

static ok64 HUNKTestRelayBody() {
    sane(1);

    //  Build one table-shaped row body (status layout): 7 date spaces,
    //  ' ', "put", ' ', "core.c\n", then the hidden "cat:core.c" nav.
    //  Tokens: L date, S sep, verb-slot, S sep, S path(ends '\n'), U nav.
    static const char BODY[] = "        put core.c\ncat:core.c";
    //                          ^0123456 (7sp) ^7 ' ' ^8 "put" ^11 ' '
    //                          ^12 "core.c\n" → 19, ^19 "cat:core.c" → 29
    a_pad(u8, tb, 256);
    {
        HUNK_SLICE(body, BODY);
        call(u8bFeed, tb, body);
    }
    Bu32 toks = {};
    call(u32bAllocate, toks, 16);
    try(hunk_test_tok, toks, 'L', 7);    // date (7 spaces)
    then try(hunk_test_tok, toks, 'S', 8);    // separator ' '
    then try(hunk_test_tok, toks, 'Y', 11);   // verb "put" (palette slot)
    then try(hunk_test_tok, toks, 'S', 12);   // separator ' '
    then try(hunk_test_tok, toks, 'S', 19);   // path "core.c\n"
    then try(hunk_test_tok, toks, 'U', 29);   // nav "cat:core.c"
    if (__ != OK) { u32bFree(toks); fail(TESTFAIL); }

    hunk child = {.verb = HUNK_TEST_VERB,
                  .uri  = {(u8cp)"", (u8cp)""}};
    u8csMv(child.text, u8bDataC(tb));
    {
        tok32cs kv = {(tok32c *)u32bDataHead(toks),
                      (tok32c *)u32bDataHead(toks) + u32bDataLen(toks)};
        u32csMv(child.toks, kv);
    }
    a_pad(u8, cb, 4096);
    ok64 fe = HUNKu8sFeed(cb_idle, &child);
    u32bFree(toks);
    if (fe != OK) fail(TESTFAIL);

    //  Relay in TLV so we can drain the rewritten body back out.
    HUNKout saved = HUNKMode;
    HUNKMode = HUNKOutTLV;
    a_pad(u8, ob, 8192);
    HUNK_SLICE(pfx, "vendor/sub");
    a_dup(u8c, cin, u8bData(cb));
    ok64 rr = HUNKu8sRelay(ob_idle, pfx, cin);
    HUNKMode = saved;
    if (rr != OK) { fprintf(stderr, "FAIL relay-body: %s\n", ok64str(rr));
                    fail(TESTFAIL); }

    hunk got = {};
    a_dup(u8c, scan, u8bData(ob));
    call(HUNKu8sDrain, scan, &got);

    //  The rewritten body must contain BOTH the prefixed path column and
    //  the prefixed nav URI.
    static const char *WANT[] = {
        "put vendor/sub/core.c\n",   // path column re-prefixed
        "cat:vendor/sub/core.c",     // nav URI re-prefixed
    };
    for (size_t k = 0; k < sizeof(WANT) / sizeof(WANT[0]); k++) {
        size_t wl = strlen(WANT[k]);
        b8 found = NO;
        if ((size_t)$len(got.text) >= wl)
            for (u8c *p = got.text[0]; p + wl <= got.text[1]; p++)
                if (memcmp(p, WANT[k], wl) == 0) { found = YES; break; }
        if (!found) {
            fprintf(stderr, "FAIL relay-body: missing '%s' in '%.*s'\n",
                    WANT[k], (int)$len(got.text),
                    $empty(got.text) ? "" : (char *)got.text[0]);
            fail(TESTFAIL);
        }
    }
    //  The bare unprefixed `cat:core.c` must be GONE (no stray original);
    //  it can't be a substring of the prefixed `cat:vendor/sub/core.c`.
    {
        static const char STALE[] = "cat:core.c";
        size_t sl = strlen(STALE);
        if ((size_t)$len(got.text) >= sl)
            for (u8c *p = got.text[0]; p + sl <= got.text[1]; p++)
                if (memcmp(p, STALE, sl) == 0) {
                    fprintf(stderr, "FAIL relay-body: stale bare nav URI\n");
                    fail(TESTFAIL);
                }
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

// =====================================================================
// MEM-005 — drain-time validation of untrusted token records.
//
// `tok32Offset` is a 24-bit value taken verbatim from the wire; every
// renderer used to treat it as a byte index into `hk->text` with no
// cross-check against the (independently drained) TXT record length.  A
// relayed/received hunk whose largest offset exceeds the text length
// drove `a_part` / pointer-walk reads past `hk->text` (ASan heap OOB).
// Separately the 'K' value bytes were aliased directly as `tok32c *`
// (unaligned load + silent non-4-multiple-tail drop).  Both must be
// caught once, at drain time, so every renderer inherits a safe hunk.
// =====================================================================

//  Serialize one hunk to a stable TLV buffer, drain it back, then run
//  every renderer over the drained hunk.  Returns the drain rc; the
//  renderers run only when the drain accepted the hunk (a rejected
//  hunk's slices are not safe to render — that is the whole point).
static ok64 hunk_roundtrip_render(hunk const *hk, ok64 *drain_rc) {
    sane(u8sOK(NULL) || 1);
    a_pad(u8, wire, 1UL << 16);
    call(HUNKu8sFeed, wire_idle, hk);
    a_dup(u8c, scan, u8bData(wire));

    hunk got = {};
    ok64 dr = HUNKu8sDrain(scan, &got);
    *drain_rc = dr;
    if (dr != OK) done;   // rejected cleanly — nothing to render

    a_pad(u8, ob, 1UL << 16);
    call(HUNKu8sFeedText,  ob_idle, &got);
    a_pad(u8, oc, 1UL << 16);
    call(HUNKu8sFeedColor, oc_idle, &got);
    a_pad(u8, oh, 1UL << 16);
    call(HUNKu8sFeedHtml,  oh_idle, &got);
    done;
}

//  Build a raw hunk 'H' container with a TXT record of `tlen` bytes and
//  a 'K' (TOK) record of exactly `klen` raw bytes (caller controls the
//  bytes so we can force a non-4-multiple / misaligned tail), then run
//  the same drain+render path.  This reaches the aliasing hazard that a
//  well-typed `hunk` can't express.
static ok64 hunk_raw_render(u8csc text, u8csc kbytes, ok64 *drain_rc) {
    sane(1);
    a_pad(u8, wire, 1UL << 16);
    u8s inner = {};
    call(TLVu8sStart, wire_idle, inner, HUNK_TLV);
    a_cstr(uri, "cat:sample.c");
    call(TLVu8sFeed, inner, HUNK_TLV_URI, uri);
    if (!$empty(text)) call(TLVu8sFeed, inner, HUNK_TLV_TXT, text);
    if (!$empty(kbytes)) call(TLVu8sFeed, inner, HUNK_TLV_TOK, kbytes);
    call(TLVu8sEnd, wire_idle, inner, HUNK_TLV);

    a_dup(u8c, scan, u8bData(wire));
    hunk got = {};
    ok64 dr = HUNKu8sDrain(scan, &got);
    *drain_rc = dr;
    if (dr != OK) done;

    a_pad(u8, ob, 1UL << 16);
    call(HUNKu8sFeedText,  ob_idle, &got);
    a_pad(u8, oc, 1UL << 16);
    call(HUNKu8sFeedColor, oc_idle, &got);
    a_pad(u8, oh, 1UL << 16);
    call(HUNKu8sFeedHtml,  oh_idle, &got);
    done;
}

static ok64 HUNKTestDrainBounds() {
    sane(1);
    const char *src = "int answer = 42;\n";   // 17 bytes
    u32 tlen = (u32)strlen(src);

    //  (a) Largest tok32Offset == tlen exactly: legal upper bound, must
    //  drain OK and render without OOB.
    {
        HUNK_SLICE(text, src);
        tok32 toks_arr[] = {tok32Pack('S', tlen)};
        tok32cs toks = {toks_arr, toks_arr + 1};
        HUNK_SLICE(uri, "cat:sample.c");
        hunk hk = {.uri  = {uri[0], uri[1]},
                   .text = {text[0], text[1]},
                   .toks = {toks[0], toks[1]}};
        ok64 dr = 0;
        call(hunk_roundtrip_render, &hk, &dr);
        if (dr != OK) {
            fprintf(stderr, "FAIL bounds(a): in-range offset rejected: %s\n",
                    ok64str(dr));
            fail(TESTFAIL);
        }
    }

    //  (b) tok32Offset > tlen: out-of-bounds index.  Pre-fix this drove
    //  a heap OOB read in every renderer; post-fix the drain must reject
    //  it (HUNKTOKOOB) and never hand a poisoned hunk to a renderer.
    {
        HUNK_SLICE(text, src);
        tok32 toks_arr[] = {tok32Pack('S', tlen + 64)};   // way past text
        tok32cs toks = {toks_arr, toks_arr + 1};
        HUNK_SLICE(uri, "cat:sample.c");
        hunk hk = {.uri  = {uri[0], uri[1]},
                   .text = {text[0], text[1]},
                   .toks = {toks[0], toks[1]}};
        ok64 dr = 0;
        call(hunk_roundtrip_render, &hk, &dr);
        if (dr != HUNKTOKOOB) {
            fprintf(stderr, "FAIL bounds(b): OOB offset not rejected: %s\n",
                    ok64str(dr));
            fail(TESTFAIL);
        }
    }

    //  (c) 'K' record length not a multiple of sizeof(tok32): a tail of
    //  raw bytes that can't form a whole token.  Pre-fix the alias
    //  `(tok32c*)val[1]` silently dropped the tail (and the cast was an
    //  unaligned load); post-fix the drain must reject it (HUNKTOKLEN).
    {
        HUNK_SLICE(text, src);
        //  6 bytes = 1.5 tok32 — non-4-multiple length.
        static const u8 kbad[6] = {0x11, 0x00, 0x00, 0x06, 0xAB, 0xCD};
        u8csc kb = {kbad, kbad + sizeof(kbad)};
        HUNK_SLICE(tx, src);
        u8csc txt = {tx[0], tx[1]};
        (void)tlen;
        ok64 dr = 0;
        call(hunk_raw_render, txt, kb, &dr);
        if (dr != HUNKTOKLEN) {
            fprintf(stderr, "FAIL bounds(c): bad-length 'K' not rejected: %s\n",
                    ok64str(dr));
            fail(TESTFAIL);
        }
    }

    //  (d) Misaligned-but-valid-length 'K': 4 bytes whose encoded offset
    //  exceeds tlen.  The TLV header places the value at an arbitrary
    //  (odd) byte boundary, so aliasing it as `tok32c*` is an unaligned
    //  load; with the offset OOB the drain must reject (HUNKTOKOOB)
    //  after an *aligned* copy, never a raw aliased read.
    {
        HUNK_SLICE(text, src);
        //  one tok32 LE: tag 'S', offset = tlen+100 (0x6d = 109) → OOB.
        u32 packed = tok32Pack('S', tlen + 100);
        u8 kbuf[4] = {(u8)(packed), (u8)(packed >> 8),
                      (u8)(packed >> 16), (u8)(packed >> 24)};
        u8csc kb = {kbuf, kbuf + 4};
        HUNK_SLICE(tx, src);
        u8csc txt = {tx[0], tx[1]};
        ok64 dr = 0;
        call(hunk_raw_render, txt, kb, &dr);
        if (dr != HUNKTOKOOB) {
            fprintf(stderr, "FAIL bounds(d): OOB raw 'K' not rejected: %s\n",
                    ok64str(dr));
            fail(TESTFAIL);
        }
    }

    done;
}

// =====================================================================
// LineBased (unified-diff) render — feed-failure propagation.
//
// A `diff:`-scheme hunk renders via the internal HUNKu8sFeedLineBased
// path, which builds the unified-diff body in a fixed 64KB side buffer.
// The two static emitters (hk_emit_line / hunk_flush_region) used to
// swallow that buffer's BNOROOM into void, silently dropping or
// truncating diff lines so the emitted `@@` body could no longer be
// `git apply`-ed.  They now return ok64 and the caller propagates.
//
// (a) A small diff hunk renders OK and produces a `@@` header.
// (b) A diff hunk whose body overflows the 64KB buffer must surface
//     BNOROOM out of the public renderer — never a truncated body.
//
// The hunk is built by hand (as HUNKTestDrainBounds does) so the toks
// carry IN/EQ sides directly without a wire round-trip.  Each block is
// an inserted line followed by a context line, so the body grows across
// many tiny regions (each region's 4KB dels/adds stays well under its
// own limit — the only buffer that fills is the shared 64KB body).
// =====================================================================

//  Bytes per generated block: 60-byte '+' payload + '\n', then a 2-byte
//  context line.  ~1200 blocks ≈ 78KB of body > 64KB.
#define HK_LB_INS_LEN 60
#define HK_LB_BLOCKS  1200
#define HK_LB_TEXTCAP (HK_LB_BLOCKS * (HK_LB_INS_LEN + 1 + 2) + 4)

static ok64 hk_lb_render(u8csc uri, u8csc text, tok32csc toks) {
    sane(1);
    hunk hk = {.uri  = {uri[0], uri[1]},
               .text = {text[0], text[1]},
               .toks = {toks[0], toks[1]}};
    a_pad(u8, ob, 1UL << 18);   // 256KB sink — bigger than the 64KB body
    return HUNKu8sFeedText(ob_idle, &hk);
}

static ok64 HUNKTestLineBasedNoRoom(void) {
    sane(1);

    //  (a) Small diff hunk: one inserted line + a context line.
    //  Renders OK and the unified-diff header `@@` appears.
    {
        const char *src = "added line\ncontext\n";   // IN then EQ
        u32 tlen = (u32)strlen(src);
        u32 ins_end = (u32)strlen("added line\n");
        tok32 toks_arr[] = {
            tok32PackSide('S', TOK_SIDE_IN, ins_end),
            tok32PackSide('S', TOK_SIDE_EQ, tlen),
        };
        tok32csc toks = {toks_arr, toks_arr + 2};
        HUNK_SLICE(text, src);
        u8csc txt = {text[0], text[1]};
        a_cstr(uri, "diff:sample.c");
        ok64 rc = hk_lb_render(uri, txt, toks);
        if (rc != OK) {
            fprintf(stderr, "FAIL lb(a): small diff render: %s\n", ok64str(rc));
            fail(TESTFAIL);
        }
        //  Confirm it actually took the unified-diff path (`@@` header).
        hunk hk = {.uri  = {uri[0], uri[1]},
                   .text = {txt[0], txt[1]},
                   .toks = {toks[0], toks[1]}};
        a_pad(u8, chk, 1UL << 16);
        call(HUNKu8sFeedText, chk_idle, &hk);
        a_dup(u8c, got, u8bData(chk));
        b8 has_hdr = NO;
        for (u8c *p = got[0]; p + 1 < got[1]; p++) {
            if (p[0] == '@' && p[1] == '@') { has_hdr = YES; break; }
        }
        if (!has_hdr) {
            fprintf(stderr, "FAIL lb(a): no '@@' header in diff body\n");
            fail(TESTFAIL);
        }
    }

    //  (b) Oversized diff hunk: body exceeds the internal 64KB buffer.
    //  Pre-fix the emitters dropped lines and returned OK (truncated,
    //  un-appliable diff); post-fix the BNOROOM propagates.
    {
        static u8 big[HK_LB_TEXTCAP];
        static tok32 toks_arr[HK_LB_BLOCKS * 2];
        u32 off = 0;
        u32 nt = 0;
        for (u32 b = 0; b < HK_LB_BLOCKS; b++) {
            for (u32 k = 0; k < HK_LB_INS_LEN; k++) big[off++] = 'x';
            big[off++] = '\n';
            toks_arr[nt++] = tok32PackSide('S', TOK_SIDE_IN, off);
            big[off++] = 'c';
            big[off++] = '\n';
            toks_arr[nt++] = tok32PackSide('S', TOK_SIDE_EQ, off);
        }
        u8csc txt = {big, big + off};
        tok32csc toks = {toks_arr, toks_arr + nt};
        a_cstr(uri, "diff:big.c");
        ok64 rc = hk_lb_render(uri, txt, toks);
        //  The propagated emitter feed reports BNOROOM (u8bFeed on the
        //  64KB body); accept the NOROOM family in case the overflow
        //  lands on a 1-byte u8bFeed1 (SNOROOM) instead.  The defect is
        //  the *silent OK* — any surfaced NOROOM is the correct contract.
        if (rc != BNOROOM && rc != SNOROOM && rc != NOROOM) {
            fprintf(stderr,
                    "FAIL lb(b): oversized diff body did not surface NOROOM:"
                    " got %s\n", ok64str(rc));
            fail(TESTFAIL);
        }
    }

    done;
}

// =====================================================================
// HUNKu8sMakeURI — fragment percent-escaping (hunk_frag_esc).
// Non-ident symbols get wrapped in '…' and URI-illegal bytes (control,
// non-ASCII, '#', '%') percent-escaped as uppercase %XX; printable
// ASCII passes through.
// =====================================================================

typedef struct {
    const char *path;
    const char *symbol;  // raw symbol bytes (NUL-terminated; no embedded NUL)
    u32         lineno;
    const char *expect;
} MakeURICase;

static const MakeURICase MAKEURI_CASES[] = {
    // plain ident → verbatim, no quoting/escaping
    {"src/a.c", "foo",        0, "src/a.c#foo"},
    {"src/a.c", "foo_bar2",  42, "src/a.c#foo_bar2:L42"},
    // non-ident → quoted.  Only #/%/control/high bytes escape as %XX
    // uppercase; other printable ASCII (incl. space) passes through.
    {"f.c",     "a b",        0, "f.c#'a b'"},
    {"f.c",     "a#b",        0, "f.c#'a%23b'"},
    {"f.c",     "a%b",        0, "f.c#'a%25b'"},
    {"f.c",     "a\tb",       0, "f.c#'a%09b'"},   // tab = control → escaped
    {"f.c",     "caf\xc3\xa9", 0, "f.c#'caf%C3%A9'"}, // UTF-8 high bytes
    // operator-ish symbol body, printable ASCII passes through
    {"f.c",     "operator()", 7, "f.c#'operator()':L7"},
    // bare line, no symbol
    {"f.c",     "",          12, "f.c#L12"},
};

static ok64 HUNKTestMakeURIEsc(void) {
    sane(1);
    for (size_t i = 0; i < sizeof(MAKEURI_CASES)/sizeof(MAKEURI_CASES[0]); i++) {
        const MakeURICase *c = &MAKEURI_CASES[i];
        a_pad(u8, out, 256);
        HUNK_SLICE(path, c->path);
        u8csc pathc = {path[0], path[1]};
        HUNK_SLICE(sym, c->symbol);
        u8csc symc = {sym[0], sym[1]};
        call(HUNKu8sMakeURI, u8bIdle(out), pathc, symc, c->lineno);
        a_dup(u8c, got, u8bData(out));
        if (!hunk_slice_eq(got, c->expect)) {
            fprintf(stderr, "FAIL MakeURI[%zu]: want '%s' got '%.*s'\n",
                    i, c->expect, (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
    }
    done;
}

// =====================================================================
// BRO-001 — the status/header line renders as a pale-yellow, black-text
// banner in Color mode.
//
// A status hunk (ts||verb set, empty text/toks) used to render in Color
// mode as a grey date + verb-coloured verb + plain URI.  BRO-001 makes
// it a banner: abbreviated date + verb + URI painted black-on-pale-
// yellow, opened with one SGR (256-color fg 0 / bg 230) and closed with
// a reset.  The full-width fill (padding the band to the terminal edge)
// is the width-aware bro layer's job — the formatter stays width-
// agnostic and just frames the content, so we assert the framing + the
// content here, not a column count.
// =====================================================================

//  Does `hay` contain the byte run `needle` (len bytes)?
static b8 bytes_contain(u8cs hay, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0) return YES;
    if ((size_t)$len(hay) < nl) return NO;
    for (u8c *p = hay[0]; p + nl <= hay[1]; p++)
        if (memcmp(p, needle, nl) == 0) return YES;
    return NO;
}

static ok64 HUNKTestStatusBanner(void) {
    sane(1);
    //  The banner SGR is theme-independent (THEME_BANNER), so any active
    //  palette must produce the same framing bytes.
    for (size_t t = 0; t < sizeof(THEMES) / sizeof(THEMES[0]); t++) {
        if (THEMESelect(THEMES[t]) != OK) fail(TESTFAIL);

        const char *uri = "abc/MSET.h#MSETOpen:42";
        HUNK_SLICE(us, uri);
        //  ts set, empty text/toks → status hunk.  A real (non-zero) ts
        //  exercises the date column; the exact stamp text varies with
        //  wall-clock age, so we assert the framing + uri, not the date
        //  spelling (covered by dog/test/DOG.c).
        hunk hk = {.ts = 0x6000000000000000ULL,
                   .verb = HUNK_VERB_HUNK,
                   .uri = {us[0], us[1]}};

        a_pad(u8, ob, 4096);
        call(HUNKu8sFeedColor, ob_idle, &hk);
        a_dup(u8c, got, u8bData(ob));

        //  (a) Pale-yellow bg + black fg SGR open: ANSIu8sFeedDelta from
        //  ANSI_DEFAULT spells fg (38;5;0) then bg (48;5;230).
        if (!bytes_contain(got, "38;5;0;48;5;230m")) {
            fprintf(stderr,
                    "FAIL banner[%s]: no black-on-pale-yellow SGR; got '%.*s'\n",
                    THEMES[t], (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
        //  (b) The URI rides the banner.
        if (!bytes_contain(got, uri)) {
            fprintf(stderr, "FAIL banner[%s]: uri missing; got '%.*s'\n",
                    THEMES[t], (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
        //  (c) The verb (free data) rides the banner too — "hunk".
        if (!bytes_contain(got, "hunk")) {
            fprintf(stderr, "FAIL banner[%s]: verb missing; got '%.*s'\n",
                    THEMES[t], (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
        //  (d) The band is closed with a reset and ends with a newline.
        if (!bytes_contain(got, "\033[0m")) {
            fprintf(stderr, "FAIL banner[%s]: no SGR reset; got '%.*s'\n",
                    THEMES[t], (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
        u32 glen = (u32)$len(got);
        if (glen == 0 || got[0][glen - 1] != '\n') {
            fprintf(stderr, "FAIL banner[%s]: line not newline-terminated\n",
                    THEMES[t]);
            fail(TESTFAIL);
        }
        //  (e) The reset must come AFTER the last visible content so the
        //  whole row is inside the band (no bare bytes trailing the
        //  reset except the newline).  Find the last reset, ensure only
        //  '\n' follows.
        u8c *last_reset = NULL;
        for (u8c *p = got[0]; p + 4 <= got[1]; p++)
            if (memcmp(p, "\033[0m", 4) == 0) last_reset = p;
        if (!last_reset) fail(TESTFAIL);
        for (u8c *p = last_reset + 4; p < got[1]; p++) {
            if (*p != '\n') {
                fprintf(stderr,
                        "FAIL banner[%s]: visible byte 0x%02x after reset\n",
                        THEMES[t], *p);
                fail(TESTFAIL);
            }
        }
    }
    (void)THEMESelect(THEME_16);
    done;
}

// =====================================================================
// HEAD-003 — `be head`/`be log` rows colored on a TTY.
//
// The graf head/log producer ships each row as a CONTENT hunk: visible
// columns tagged 'L' (sha + date) / 'S' (subject) / 'P' (parens), plus a
// hidden `commit:?<sha>` click target tagged 'U'.  On a TTY (HUNKOutColor)
// these toks must drive per-column color via HUNKu8sFeedColor — the bug
// was that the producer only attached toks in TLV mode, so the direct
// color render fell back to the verbatim no-toks path (plain rows).
//
// Mirror that row shape and assert (a) color render paints the 'L'
// column (cyan), (b) BOTH color and plain render hide the 'U'-tagged URI
// (so attaching toks in plain stays byte-safe — no leaked URI).
// =====================================================================
static ok64 HUNKTestHeadRowColor(void) {
    sane(1);
    if (THEMESelect(THEME_16) != OK) fail(TESTFAIL);

    //  "abc1234" (sha, 'L') + " " + "commit:?<hex>" (hidden, 'U') +
    //  " 12:00 " (date, 'L') + "subject" ('S') + "\n".
    const char *vis_sha  = "abc1234";
    const char *uri      = "commit:?abc1234";
    const char *vis_rest = " 12:00 subject\n";
    a_pad(u8, tb, 256);
    a_pad(u32, kb, 32);
    HUNK_SLICE(s_sha,  vis_sha);
    HUNK_SLICE(s_uri,  uri);
    HUNK_SLICE(s_rest, vis_rest);
    (void)u8bFeed(tb, s_sha);
    (void)u32bFeed1(kb, tok32Pack('L', (u32)u8bDataLen(tb)));   // sha = L
    (void)u8bFeed(tb, s_uri);
    (void)u32bFeed1(kb, tok32Pack('U', (u32)u8bDataLen(tb)));   // URI hidden
    (void)u8bFeed(tb, s_rest);
    (void)u32bFeed1(kb, tok32Pack('L', (u32)u8bDataLen(tb)));   // date/rest = L

    HUNK_SLICE(huri, "head:?trunk");
    hunk hk = {.uri  = {huri[0], huri[1]},
               .text = {u8bDataHead(tb), u8bIdleHead(tb)},
               .toks = {(u32 const *)u32bDataHead(kb),
                        (u32 const *)u32bIdleHead(kb)}};

    //  (a) Color render colors the 'L' column (THEME_16 'L' = ESC[96m)
    //  and never leaks the hidden URI.
    HUNKout saved = HUNKMode;
    HUNKMode = HUNKOutColor;
    a_pad(u8, oc, 1024);
    ok64 cr = HUNKu8sFeedColor(oc_idle, &hk);
    HUNKMode = saved;
    if (cr != OK) fail(cr);
    a_dup(u8c, cgot, u8bData(oc));
    if (!bytes_contain(cgot, "\033[96m")) {
        fprintf(stderr, "FAIL head-row: no 'L' column color; got '%.*s'\n",
                (int)$len(cgot), (char *)cgot[0]);
        fail(TESTFAIL);
    }
    if (bytes_contain(cgot, uri)) {
        fprintf(stderr, "FAIL head-row: 'U' URI leaked into color render\n");
        fail(TESTFAIL);
    }

    //  (b) Plain render hides the same 'U'-tagged URI — attaching toks in
    //  plain mode must not change the visible bytes.
    a_pad(u8, op, 1024);
    call(HUNKu8sFeedText, op_idle, &hk);
    a_dup(u8c, pgot, u8bData(op));
    if (bytes_contain(pgot, uri)) {
        fprintf(stderr, "FAIL head-row: 'U' URI leaked into plain render\n");
        fail(TESTFAIL);
    }
    if (!bytes_contain(pgot, vis_sha) || !bytes_contain(pgot, "subject")) {
        fprintf(stderr, "FAIL head-row: visible columns dropped in plain\n");
        fail(TESTFAIL);
    }
    done;
}

ok64 HUNKtest() {
    sane(1);
    call(HUNKTestRebase);
    call(HUNKTestRelayRoundtrip);
    call(HUNKTestRelayBody);
    call(HUNKTestRedReserved);
    call(HUNKTestDrainBounds);
    call(HUNKTestLineBasedNoRoom);
    call(HUNKTestMakeURIEsc);
    call(HUNKTestStatusBanner);
    call(HUNKTestHeadRowColor);
    done;
}

TEST(HUNKtest);
