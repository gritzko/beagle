#ifndef JABC_HASH_HPP
#define JABC_HASH_HPP
//  HASH bindings: an open-addressed hash table living in a JS-owned typed
//  array (the whole region is the table; length MUST be a power of 2 and the
//  region zeroed — the JS constructor guarantees both).  Each leaf forms a
//  stack-local lane slice over the backing memory and runs abc HASH<lane>.
//
//  Leaves: _hash_<lane>_put(arr, key[, val))   ; _get(arr, key) -> val|undef
//          _hash_<lane>_del(arr, key)
//  IMPORTANT: the including TU must include abc/PRO.h first (HASHx uses
//  sane/done); §6 keeps PRO.h out of headers.
#include "cont.hpp"

extern "C" {
#include "abc/KV.h"
#include "dog/WHIFF.h"
#include "dog/git/SHA1.h"  // sha1 (20-byte blob) + sha1Z + Bx slice
#include "abc/SHA.h"        // sha256 (32-byte blob) + sha256Z + Bx slice

//  Hashers abc doesn't define for these lanes — supplied here in js/, never
//  in abc (cf. DIJK defining kv64Zval locally).  Keying is the value itself
//  (scalar set) or the wh64 value.
fun u64 u8hash(u8 const* v) { return mix64((u64)*v); }
fun b8 u8hashEq(u8 const* a, u8 const* b) { return *a == *b; }
fun u64 u16hash(u16 const* v) { return mix64((u64)*v); }
fun b8 u16hashEq(u16 const* a, u16 const* b) { return *a == *b; }
fun u64 wh64hash(wh64 const* v) { return mix64(*v); }
fun b8 wh64hashEq(wh64 const* a, wh64 const* b) { return *a == *b; }
//  Blob lanes are scalar sets keyed by the whole element: hash the first 8
//  bytes (a sha is uniformly random, any 8 bytes work) and equate by memcmp.
fun u64 sha1hash(sha1 const* v) { u64 w; memcpy(&w, v->data, 8); return mix64(w); }
fun b8 sha1hashEq(sha1 const* a, sha1 const* b) {
  return memcmp(a->data, b->data, sizeof(*a)) == 0;
}
fun u64 sha256hash(sha256 const* v) { u64 w; memcpy(&w, v->data, 8); return mix64(w); }
fun b8 sha256hashEq(sha256 const* a, sha256 const* b) {
  return memcmp(a->data, b->data, sizeof(*a)) == 0;
}
}

#define ABC_HASH_CONVERGE 1
#define X(M, name) M##u8##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##u16##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##u32##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##u64##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##kv32##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##kv64##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##wh64##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##wh128##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##sha1##name
#include "abc/HASHx.h"
#undef X
#define X(M, name) M##sha256##name
#include "abc/HASHx.h"
#undef X

//  put / get / del leaf triple per lane.  RDPUT fills `v` for insertion,
//  RDKEY fills `v`'s key for lookup, WRGET turns a found `v` into a JS value.
#define HASH_DEF(LANE, RDPUT, RDKEY, WRGET)                                 \
  static JABC_FN(jhash_##LANE##_put) {                                      \
    void* base;                                                            \
    size_t cap;                                                            \
    if (!JABCLaneArr(&base, &cap, ctx, args[0], sizeof(LANE), exception))   \
      return JSValueMakeUndefined(ctx);                                     \
    LANE v;                                                                 \
    RDPUT;                                                                  \
    LANE* bb = (LANE*)base;                                                 \
    LANE* s[2] = {bb, bb + cap};                                           \
    if (HASH##LANE##Put(s, &v) != OK) JABC_THROW("hash: full");             \
    return JSValueMakeUndefined(ctx);                                       \
  }                                                                         \
  static JABC_FN(jhash_##LANE##_get) {                                      \
    void* base;                                                            \
    size_t cap;                                                            \
    if (!JABCLaneArr(&base, &cap, ctx, args[0], sizeof(LANE), exception))   \
      return JSValueMakeUndefined(ctx);                                     \
    LANE v;                                                                 \
    RDKEY;                                                                  \
    LANE* bb = (LANE*)base;                                                 \
    LANE* s[2] = {bb, bb + cap};                                           \
    if (HASH##LANE##Get(&v, s) != OK) return JSValueMakeUndefined(ctx);     \
    return (WRGET);                                                         \
  }                                                                         \
  static JABC_FN(jhash_##LANE##_del) {                                      \
    void* base;                                                            \
    size_t cap;                                                            \
    if (!JABCLaneArr(&base, &cap, ctx, args[0], sizeof(LANE), exception))   \
      return JSValueMakeUndefined(ctx);                                     \
    LANE v;                                                                 \
    RDKEY;                                                                  \
    LANE* bb = (LANE*)base;                                                 \
    LANE* s[2] = {bb, bb + cap};                                           \
    HASH##LANE##Del(s, &v);                                                 \
    return JSValueMakeUndefined(ctx);                                       \
  }

//  Marshal shapes.  Scalar lanes key on the value itself; pair lanes key on
//  .key and store/return .val.
#define HASH_NUM(LANE)                                                      \
  HASH_DEF(LANE, v = (LANE)JSValueToNumber(ctx, args[1], exception),        \
           v = (LANE)JSValueToNumber(ctx, args[1], exception),             \
           JSValueMakeNumber(ctx, (double)v))
#define HASH_U64(LANE)                                                      \
  HASH_DEF(LANE, v = (LANE)JSValueToUInt64(ctx, args[1], exception),        \
           v = (LANE)JSValueToUInt64(ctx, args[1], exception),             \
           JSBigIntCreateWithUInt64(ctx, (uint64_t)v, exception))
#define HASH_PN(LANE)                                                       \
  HASH_DEF(LANE,                                                            \
           (v.key = (u32)JSValueToNumber(ctx, args[1], exception),          \
            v.val = (u32)JSValueToNumber(ctx, args[2], exception)),         \
           v.key = (u32)JSValueToNumber(ctx, args[1], exception),           \
           JSValueMakeNumber(ctx, (double)v.val))
#define HASH_PB(LANE)                                                       \
  HASH_DEF(LANE,                                                            \
           (v.key = (u64)JSValueToUInt64(ctx, args[1], exception),          \
            v.val = (u64)JSValueToUInt64(ctx, args[2], exception)),         \
           v.key = (u64)JSValueToUInt64(ctx, args[1], exception),           \
           JSBigIntCreateWithUInt64(ctx, (uint64_t)v.val, exception))
//  wh128 keys on the FULL (key,val) record (wh128hashEq compares both): a set
//  of pairs, not a key->val map, so a lookup must supply both fields.
#define HASH_SET_PB(LANE)                                                   \
  HASH_DEF(LANE,                                                            \
           (v.key = (u64)JSValueToUInt64(ctx, args[1], exception),          \
            v.val = (u64)JSValueToUInt64(ctx, args[2], exception)),         \
           (v.key = (u64)JSValueToUInt64(ctx, args[1], exception),          \
            v.val = (u64)JSValueToUInt64(ctx, args[2], exception)),         \
           JABCPair(ctx,                                                    \
                    JSBigIntCreateWithUInt64(ctx, (uint64_t)v.key, exception), \
                    JSBigIntCreateWithUInt64(ctx, (uint64_t)v.val, exception)))

//  Blob lane: a scalar set keyed by the whole element.  The key IS the blob,
//  so put/get/del all read a Uint8Array of exactly sizeof(LANE) into `v.data`;
//  get returns a fresh Uint8Array copy of the stored element.
#define HASH_BLOB_RD                                                          \
  {                                                                           \
    u8s _b = {};                                                              \
    if (!JABCBytesOf(_b, ctx, args[1], exception))                            \
      return JSValueMakeUndefined(ctx);                                      \
    if ((size_t)$len(_b) != sizeof(v))                                        \
      JABC_THROW("hash: blob size");                                          \
    memcpy(v.data, _b[0], sizeof(v));                                         \
  }
#define HASH_BLOB(LANE)                                                       \
  HASH_DEF(LANE, HASH_BLOB_RD, HASH_BLOB_RD, JABCBlob(ctx, v.data, sizeof(v)))

HASH_NUM(u8)
HASH_NUM(u16)
HASH_NUM(u32)
HASH_U64(u64)
HASH_PN(kv32)
HASH_PB(kv64)
HASH_U64(wh64)
HASH_SET_PB(wh128)
HASH_BLOB(sha1)
HASH_BLOB(sha256)

#define HASH_REG(LANE)                                          \
  JABC_API_FN(o, "_hash_" #LANE "_put", jhash_##LANE##_put);    \
  JABC_API_FN(o, "_hash_" #LANE "_get", jhash_##LANE##_get);    \
  JABC_API_FN(o, "_hash_" #LANE "_del", jhash_##LANE##_del)

static inline void JABCHashInstall(JSObjectRef o) {
  HASH_REG(u8);
  HASH_REG(u16);
  HASH_REG(u32);
  HASH_REG(u64);
  HASH_REG(kv32);
  HASH_REG(kv64);
  HASH_REG(wh64);
  HASH_REG(wh128);
  HASH_REG(sha1);
  HASH_REG(sha256);
}

#endif
