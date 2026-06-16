#ifndef DOG_WHIFF_H
#define DOG_WHIFF_H

//  WHIFF: 64-bit tagged word with offset[40] | id[20] | type[4].
//
//  Hashlet in the MS 40 bits so entries sort by hashlet first.
//  When id=0, the hashlet effectively spans 60 bits (id bits
//  become additional hashlet bits).
//
//  Hashlets are big-endian: first SHA byte in the top bits.
//  Construct via flip64(memcpy 8 SHA bytes) >> shift.
//
//  Canonical layout used across dogs:
//    keeper: hashlet=sha_prefix, id=file_number, type=pack_format
//    graf:   hashlet=sha_prefix, id=generation,  type=entry_kind
//    sniff:  off=mtime,          id=path_index,  type=flags

#include "abc/INT.h"
#include "abc/HEX.h"

typedef u64 wh64;

fun b8 wh64Z(wh64 const *a, wh64 const *b) { return *a < *b; }

//  Bx instantiation for the wh64 value type — gives us `wh64b`,
//  `wh64s`, `wh64sFeed1`, etc.  Distinct from `u64*` even though
//  wh64 is a u64 alias, so call sites can declare intent.
#define X(M, name) M##wh64##name
#include "abc/Bx.h"
#undef X

#define WHIFF_TYPE_BITS   4
#define WHIFF_TYPE_MASK   0xfULL
#define WHIFF_ID_SHIFT    WHIFF_TYPE_BITS       // bits 4-23
#define WHIFF_ID_BITS     20
#define WHIFF_ID_MASK     ((1ULL << WHIFF_ID_BITS) - 1)
#define WHIFF_OFF_SHIFT   (WHIFF_TYPE_BITS + WHIFF_ID_BITS)  // bits 24-63
#define WHIFF_OFF_BITS    40
#define WHIFF_OFF_MASK    ((1ULL << WHIFF_OFF_BITS) - 1)

fun wh64 wh64Pack(u8 type, u32 id, u64 off) {
    return ((u64)type & WHIFF_TYPE_MASK) |
           (((u64)id & WHIFF_ID_MASK) << WHIFF_ID_SHIFT) |
           ((off & WHIFF_OFF_MASK) << WHIFF_OFF_SHIFT);
}

fun u8  wh64Type(wh64 v) { return (u8)(v & WHIFF_TYPE_MASK); }
fun u32 wh64Id(wh64 v)   { return (u32)((v >> WHIFF_ID_SHIFT) & WHIFF_ID_MASK); }
fun u64 wh64Off(wh64 v)  { return (v >> WHIFF_OFF_SHIFT) & WHIFF_OFF_MASK; }

// --- wh128: (key, val) pair, equality on both fields ---

typedef struct {
    wh64 key;
    wh64 val;
} wh128;

fun u64  wh128hash(wh128 const *v) { return mix64(v->key ^ v->val); }
fun b8   wh128hashEq(wh128 const *a, wh128 const *b) {
    return a->key == b->key && a->val == b->val;
}

fun b8 wh128Z(wh128 const *a, wh128 const *b) {
    if (a->key != b->key) return a->key < b->key;
    return a->val < b->val;
}

#define X(M, name) M##wh128##name
#include "abc/Bx.h"
#undef X

//  Bx instantiation for wh128cs slots (the slice-of-wh128 type
//  itself).  Gives us `wh128csb` (4-pointer buffer of wh128cs) and
//  `wh128cssFeed1` (push one wh128cs slot through a wh128css head),
//  paralleling BUF.h's u8csb / u8cssFeed1.  ABC_X_$ flag is set
//  because wh128cs is array-typed.
typedef wh128cs const *wh128cscp;

fun b8 wh128csZ(wh128cs const *a, wh128cs const *b) {
    size_t la = (size_t)((*a)[1] - (*a)[0]);
    size_t lb = (size_t)((*b)[1] - (*b)[0]);
    size_t n = la < lb ? la : lb;
    for (size_t i = 0; i < n; i++) {
        if (wh128Z(&(*a)[0][i], &(*b)[0][i])) return YES;
        if (wh128Z(&(*b)[0][i], &(*a)[0][i])) return NO;
    }
    return la < lb;
}

#define X(M, name) M##wh128cs##name
#define ABC_X_$
#include "abc/Bx.h"
#undef ABC_X_$
#undef X

// --- SHA-1 hashlet helpers ---
//
// Two widths:
//   40-bit (10 hex chars): for vals where id field is used separately
//   60-bit (15 hex chars): for keys where id=0 (hashlet spans both fields)
//
// Both are big-endian: first SHA byte in the most significant bits.
// Input: sha1cp (typed, 20 bytes).

#include "dog/git/SHA1.h"

// 40-bit hashlet: first 5 bytes of SHA (10 hex chars)
#define WHIFF_HASHLET40_BITS  40
#define WHIFF_HASHLET40_MASK  WHIFF_OFF_MASK

// 60-bit hashlet: first 7.5 bytes of SHA (15 hex chars)
#define WHIFF_HASHLET60_BITS  60
#define WHIFF_HASHLET60_MASK  ((1ULL << 60) - 1)

// Width-parameterized core for the 40/60 hashlet twins.  `chars` is the
// hashlet width in hex digits (10 → 40-bit, 15 → 60-bit); bit width is
// `chars*4`.  Big-endian: first SHA byte lands in the most significant
// nibble, right-aligned within the `chars*4`-bit field.
fun u64 whiff_hashlet(sha1cp s, int chars) {
    u64 h = 0;
    memcpy(&h, s->data, 8);
    int bits = chars * 4;
    u64 mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1);
    return (flip64(h) >> (64 - bits)) & mask;
}

fun u64 WHIFFHashlet40(sha1cp s) { return whiff_hashlet(s, 10); }
fun u64 WHIFFHashlet60(sha1cp s) { return whiff_hashlet(s, 15); }

// --- Hashlet to hex ---

// Emit `chars` hex digits of `hashlet`, most-significant nibble first.
fun ok64 whiff_hex_feed(u8s out, u64 hashlet, int chars) {
    for (int i = 0; i < chars && !$empty(out); i++) {
        u8 nib = (u8)((hashlet >> ((chars - 1 - i) * 4)) & 0xf);
        u8sFeed1(out, $at(BASE16, nib));
    }
    return OK;
}

fun ok64 WHIFFHexFeed40(u8s out, u64 hashlet) {
    return whiff_hex_feed(out, hashlet, 10);
}

fun ok64 WHIFFHexFeed60(u8s out, u64 hashlet) {
    return whiff_hex_feed(out, hashlet, 15);
}

// --- Hex to hashlet ---

// Width-parameterized core for the 40/60 hex→hashlet twins.  `chars` is
// the hashlet width in hex digits (10 → 40-bit, 15 → 60-bit).  Reads up
// to `chars` leading hex digits of `hex` (stops at the first non-hex
// byte) and left-aligns the value into the `chars*4`-bit field.
fun u64 whiff_hex_hashlet(u8csc hex, int chars) {
    size_t nchars = u8csLen(hex);
    if (nchars > (size_t)chars) nchars = (size_t)chars;
    u64 h = 0;
    $for(u8c, p, hex) {
        if ((size_t)(p - hex[0]) >= nchars) break;
        u8 nib = BASE16rev[*p];
        if (nib == 0xff) break;
        h = (h << 4) | nib;
    }
    h <<= ((size_t)chars - nchars) * 4;
    return h;
}

fun u64 WHIFFHexHashlet40(u8csc hex) { return whiff_hex_hashlet(hex, 10); }
fun u64 WHIFFHexHashlet60(u8csc hex) { return whiff_hex_hashlet(hex, 15); }

#endif
