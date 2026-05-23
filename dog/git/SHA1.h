#ifndef DOG_SHA1_H
#define DOG_SHA1_H

//  SHA1: git SHA-1 type + hashing via sha1dc.
//  Follows abc/SHA.h pattern: struct, Sum, Open/Feed/Close.

#include "abc/HEX.h"
#include "abc/INT.h"
#include "dog/sha1dc/sha1.h"

#define SHA1_HASHLEN_LEN 8

typedef struct {
    u8 data[20];
} sha1;

fun b8 sha1empty(sha1 const* s) {
    u64c* w = (u64c*)s->data;
    u32c* t = (u32c*)(s->data + 16);
    return (w[0] | w[1] | *t) == 0;
}

fun int sha1cmp(sha1 const* a, sha1 const* b) {
    return memcmp(a->data, b->data, 20);
}

fun b8 sha1Z(sha1 const* a, sha1 const* b) {
    return memcmp(a->data, b->data, 20) < 0;
}

// --- ABC type system ---

#define X(M, n) M##sha1##n
#include "abc/Bx.h"
#include "abc/HEXx.h"
#undef X

// --- Hashing ---

typedef SHA1_CTX SHA1state;

fun void SHA1Sum(sha1* hash, u8csc from) {
    SHA1_CTX ctx;
    SHA1DCInit(&ctx);
    SHA1DCSetSafeHash(&ctx, 0);
    SHA1DCUpdate(&ctx, (char const*)from[0], (size_t)u8csLen(from));
    SHA1DCFinal(hash->data, &ctx);
}

fun void SHA1Open(SHA1state* state) {
    SHA1DCInit(state);
    SHA1DCSetSafeHash(state, 0);
}

fun void SHA1Feed(SHA1state* state, u8csc data) {
    SHA1DCUpdate(state, (char const*)data[0], (size_t)u8csLen(data));
}

fun void SHA1Close(SHA1state* state, sha1* hash) {
    SHA1DCFinal(hash->data, state);
}

// --- Slice of sha1 as u8csc (for hashlet etc) ---

fun void sha1slice(u8csp out, sha1cp s) {
    out[0] = s->data;
    out[1] = s->data + 20;
}

//  Feed the first SHA1_HASHLEN_LEN hex chars of `s` into `into`.
//  All-or-nothing: returns SNOROOM if `into` lacks room.
fun ok64 SHA1u8sFeedHashlet(u8s into, sha1cp s) {
    if ((u64)($len(into)) < SHA1_HASHLEN_LEN) return SNOROOM;
    u8 hexbuf[SHA1_HASHLEN_LEN];
    u8s hx = {hexbuf, hexbuf + SHA1_HASHLEN_LEN};
    u8cs bn = {s->data, s->data + (SHA1_HASHLEN_LEN / 2)};
    HEXu8sFeedSome(hx, bn);
    u8cs hashlet = {hexbuf, hexbuf + SHA1_HASHLEN_LEN};
    return u8sFeed(into, hashlet);
}

//  Feed all 40 ASCII hex chars of `s` into a u8 buffer.
//  All-or-nothing: returns BNOROOM if `into` lacks room.
fun ok64 SHA1u8bFeedHex(u8b into, sha1cp s) {
    u8 hexbuf[40];
    u8s hx = {hexbuf, hexbuf + 40};
    u8cs bn = {s->data, s->data + 20};
    HEXu8sFeedSome(hx, bn);
    u8cs hex = {hexbuf, hexbuf + 40};
    return u8bFeed(into, hex);
}

//  Declare `name` as a u8cs view over the 40-hex form of `sha` (a
//  sha1cp).  Materialises the hex into a stack buffer named
//  `name##_hb`; the slice `name` is valid until the enclosing scope
//  exits.  Collapses the sha1hex+sha1hexFromSha1+a_rawc trio.
#define a_sha1hex(name, sha)                                    \
    u8 name##_hb[40];                                            \
    do {                                                         \
        u8s _hx = {name##_hb, name##_hb + 40};                  \
        u8cs _bn = {(sha)->data, (sha)->data + 20};             \
        HEXu8sFeedSome(_hx, _bn);                                \
    } while (0);                                                 \
    u8cs name = {name##_hb, name##_hb + 40}

// Copy 20 raw bytes from a slice into a sha1.  Returns BADRANGE
// when the slice is not exactly 20 bytes; otherwise OK.
fun ok64 sha1FromBin(sha1* out, u8cs bin) {
    if (u8csLen(bin) != 20) return BADRANGE;
    u8s dst = {out->data, out->data + 20};
    u8sCopy(dst, bin);
    return OK;
}

// Drain a 20-byte sha1 from `from`, advancing its head.
fun ok64 sha1Drain(u8cs from, sha1* into) {
    if (u8csLen(from) < 20) return NODATA;
    memcpy(into->data, *from, 20);
    *from += 20;
    return OK;
}

#endif
