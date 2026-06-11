#include "dog/WHIFF.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

//  WHIFF 40/60 hashlet helpers (CODE-003): the width-parameterized
//  cores `whiff_hashlet` / `whiff_hex_feed` / `whiff_hex_hashlet` must
//  reproduce the historical 40-bit (10 hex) and 60-bit (15 hex)
//  behavior byte-for-byte.  These table cases pin the contract:
//   - hashlet extracts the leading SHA bytes, big-endian, right-aligned;
//   - hex-feed renders exactly N hex chars, MSB nibble first;
//   - hex→hashlet parses up to N leading hex digits, left-aligned, and
//     stops at the first non-hex byte.

static void fill_sha1(sha1 *s, u8 const *bytes, size_t n) {
    memset(s->data, 0, 20);
    if (n > 20) n = 20;
    memcpy(s->data, bytes, n);
}

static b8 hex_eq(u8b out, char const *want) {
    a_dup(u8c, got, u8bData(out));
    size_t wn = strlen(want);
    if ((size_t)$len(got) != wn) return NO;
    return memcmp(got[0], want, wn) == 0;
}

static ok64 WHIFFTestHashlet(void) {
    sane(1);
    //  SHA bytes: 01 23 45 67 89 ab cd ef 10 ... → first bytes drive the
    //  hashlet.  40-bit hashlet = first 5 bytes (10 hex), 60-bit = first
    //  7.5 bytes (15 hex).
    u8 raw[20] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                  0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
                  0x11, 0x22, 0x33, 0x44};
    sha1 s = {};
    fill_sha1(&s, raw, 20);

    u64 h40 = WHIFFHashlet40(&s);
    u64 h60 = WHIFFHashlet60(&s);
    //  40-bit: first 5 bytes 01 23 45 67 89 → 0x0123456789
    if (h40 != 0x0123456789ULL) {
        fprintf(stderr, "Hashlet40: got %llx want 0123456789\n",
                (unsigned long long)h40);
        fail(TESTFAIL);
    }
    //  60-bit: first 7.5 bytes 01 23 45 67 89 ab cd e → 0x0123456789abcde
    if (h60 != 0x0123456789abcdeULL) {
        fprintf(stderr, "Hashlet60: got %llx want 0123456789abcde\n",
                (unsigned long long)h60);
        fail(TESTFAIL);
    }

    //  hex-feed round-trips the hashlet to its canonical hex string.
    a_pad(u8, o40, 16);
    call(WHIFFHexFeed40, u8bIdle(o40), h40);
    if (!hex_eq(o40, "0123456789")) {
        a_dup(u8c, g, u8bData(o40));
        fprintf(stderr, "HexFeed40: got '%.*s'\n", (int)$len(g), (char *)g[0]);
        fail(TESTFAIL);
    }
    a_pad(u8, o60, 16);
    call(WHIFFHexFeed60, u8bIdle(o60), h60);
    if (!hex_eq(o60, "0123456789abcde")) {
        a_dup(u8c, g, u8bData(o60));
        fprintf(stderr, "HexFeed60: got '%.*s'\n", (int)$len(g), (char *)g[0]);
        fail(TESTFAIL);
    }
    done;
}

static ok64 WHIFFTestHexHashlet(void) {
    sane(1);
    struct { char const *hex; u64 want40; u64 want60; } cases[] = {
        //  full-width inputs (left-aligned within the chars*4-bit field)
        {"0123456789",        0x123456789ULL,     0x12345678900000ULL},
        {"0123456789abcde",   0x123456789ULL,     0x123456789abcdeULL},
        //  short input: left-aligned within the field
        {"ab",                0xab00000000ULL,    0xab0000000000000ULL},
        {"",                  0,                  0},
        //  stops at the first non-hex byte (field width keeps left-align)
        {"12zz",              0x12000000ULL,      0x1200000000000ULL},
        //  over-width input is truncated to the field width
        {"0123456789abcdef",  0x123456789ULL,     0x123456789abcdeULL},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        a_cstr(src, cases[i].hex);
        u8csc hx = {src[0], src[1]};
        u64 g40 = WHIFFHexHashlet40(hx);
        u64 g60 = WHIFFHexHashlet60(hx);
        if (g40 != cases[i].want40) {
            fprintf(stderr, "HexHashlet40('%s'): got %llx want %llx\n",
                    cases[i].hex, (unsigned long long)g40,
                    (unsigned long long)cases[i].want40);
            fail(TESTFAIL);
        }
        if (g60 != cases[i].want60) {
            fprintf(stderr, "HexHashlet60('%s'): got %llx want %llx\n",
                    cases[i].hex, (unsigned long long)g60,
                    (unsigned long long)cases[i].want60);
            fail(TESTFAIL);
        }
    }
    done;
}

//  sha1FromHex (CODE-016): the canonical hex→sha1 decoder lives in
//  dog/git/SHA1.h next to sha1FromBin.  Contract:
//   - >= 40 hex chars: decode the leading 40 → 20 bytes, return OK,
//     and the bytes round-trip back through SHA1u8sFeedHex;
//   - < 40 chars: BADRANGE, output untouched;
//   - non-hex inside the leading 40: HEXBAD.
static ok64 WHIFFTestSha1FromHex(void) {
    sane(1);
    struct {
        char const *hex;
        ok64        want;
    } cases[] = {
        //  valid 40-hex → OK (lower-case)
        {"0123456789abcdef0123456789abcdef01234567", OK},
        //  valid 40-hex → OK (upper-case accepted by BASE16rev)
        {"0123456789ABCDEF0123456789ABCDEF01234567", OK},
        //  all-zero sha → OK
        {"0000000000000000000000000000000000000000", OK},
        //  longer than 40 → OK, only the leading 40 decode
        {"0123456789abcdef0123456789abcdef0123456789abcdef", OK},
        //  exactly 39 chars → BADRANGE (too short)
        {"0123456789abcdef0123456789abcdef0123456", BADRANGE},
        //  empty → BADRANGE
        {"", BADRANGE},
        //  40 chars with a non-hex byte inside → HEXBAD
        {"0123456789abcdef0123456789abcdefzz234567", HEXBAD},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        a_cstr(src, cases[i].hex);
        u8csc hx = {src[0], src[1]};
        sha1  got = {};
        ok64  o = sha1FromHex(&got, hx);
        if (o != cases[i].want) {
            fprintf(stderr,
                    "sha1FromHex('%s'): got %llx want %llx\n",
                    cases[i].hex, (unsigned long long)o,
                    (unsigned long long)cases[i].want);
            fail(TESTFAIL);
        }
        //  On OK, re-encode the decoded sha and confirm the first 40
        //  hex chars match the input (round-trip, case-insensitive).
        if (o == OK) {
            a_sha1hex(re, &got);
            for (int j = 0; j < 40; j++) {
                u8 a = re[0][j];
                u8 b = (u8)cases[i].hex[j];
                if (b >= 'A' && b <= 'F') b = (u8)(b - 'A' + 'a');
                if (a != b) {
                    fprintf(stderr,
                            "sha1FromHex('%s'): round-trip mismatch at "
                            "%d: '%c' vs '%c'\n",
                            cases[i].hex, j, a, b);
                    fail(TESTFAIL);
                }
            }
        }
    }
    done;
}

ok64 WHIFFtest() {
    sane(1);
    call(WHIFFTestHashlet);
    call(WHIFFTestHexHashlet);
    call(WHIFFTestSha1FromHex);
    done;
}

TEST(WHIFFtest);
