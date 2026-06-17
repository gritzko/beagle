#ifndef JABC_HEAP_HPP
#define JABC_HEAP_HPP
//  HEAP bindings: a binary heap living in a JS-owned typed array's DATA
//  region [0, size).  Each leaf forms a stack-local lane buffer over the
//  backing memory and runs the abc HEAP<lane> op (sift in C, one boundary
//  crossing per call).  Native ordering only — no JS comparator.
//
//  Leaves: _heap_<lane>_push(arr, size, v[, val]) -> newsize
//          _heap_<lane>_pop(arr, size)            -> value | [key,val]
//  The cursor (size) lives in JS (buffer.watermark); native gets it as an arg.
#include "cont.hpp"

extern "C" {
#include "abc/KV.h"      // kv32/kv64 (key,val) + Z
#include "dog/WHIFF.h"   // wh64 (=u64) + Z ; wh128 (key,val) + Z
#include "dog/git/SHA1.h"  // sha1 (20-byte blob) + sha1Z + Bx slice
#include "abc/SHA.h"        // sha256 (32-byte blob) + sha256Z + Bx slice
}

//  Instantiate the HEAP template for each lane (js-side; abc untouched).
#define X(M, name) M##u8##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##u16##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##u32##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##u64##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##kv32##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##kv64##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##wh64##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##wh128##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##sha1##name
#include "abc/HEAPx.h"
#undef X
#define X(M, name) M##sha256##name
#include "abc/HEAPx.h"
#undef X

//  One push/pop leaf pair per lane.  RDV fills `v` from args[2..]; WRV turns a
//  popped `v` into a JS value.  The cap/buffer/error boilerplate is generated
//  once here and shared across every lane.
#define HEAP_DEF(LANE, RDV, WRV)                                              \
  static JABC_FN(jheap_##LANE##_push) {                                       \
    void* base;                                                              \
    size_t cap;                                                             \
    if (!JABCLaneArr(&base, &cap, ctx, args[0], sizeof(LANE), exception))    \
      return JSValueMakeUndefined(ctx);                                      \
    size_t n = (size_t)JSValueToNumber(ctx, args[1], exception);             \
    if (n >= cap) JABC_THROW("heap: full");                                  \
    LANE v;                                                                  \
    RDV;                                                                     \
    LANE* bb = (LANE*)base;                                                  \
    LANE* buf[4] = {bb, bb, bb + n, bb + cap};                              \
    if (HEAP##LANE##Push(buf, &v) != OK) JABC_THROW("heap: push");           \
    return JSValueMakeNumber(ctx, (double)(buf[2] - bb));                   \
  }                                                                          \
  static JABC_FN(jheap_##LANE##_pop) {                                       \
    void* base;                                                              \
    size_t cap;                                                             \
    if (!JABCLaneArr(&base, &cap, ctx, args[0], sizeof(LANE), exception))    \
      return JSValueMakeUndefined(ctx);                                      \
    size_t n = (size_t)JSValueToNumber(ctx, args[1], exception);             \
    if (n == 0) return JSValueMakeUndefined(ctx);                            \
    LANE* bb = (LANE*)base;                                                  \
    LANE* buf[4] = {bb, bb, bb + n, bb + cap};                              \
    LANE v;                                                                  \
    if (HEAP##LANE##Pop(&v, buf) != OK) return JSValueMakeUndefined(ctx);    \
    return (WRV);                                                            \
  }

//  Marshal shapes.
#define HEAP_NUM(LANE)                                                    \
  HEAP_DEF(LANE, v = (LANE)JSValueToNumber(ctx, args[2], exception),      \
           JSValueMakeNumber(ctx, (double)v))
#define HEAP_U64(LANE)                                                    \
  HEAP_DEF(LANE, v = (LANE)JSValueToUInt64(ctx, args[2], exception),      \
           JSBigIntCreateWithUInt64(ctx, (uint64_t)v, exception))
#define HEAP_PN(LANE)                                                        \
  HEAP_DEF(LANE,                                                             \
           (v.key = (u32)JSValueToNumber(ctx, args[2], exception),           \
            v.val = (u32)JSValueToNumber(ctx, args[3], exception)),          \
           JABCPair(ctx, JSValueMakeNumber(ctx, (double)v.key),              \
                    JSValueMakeNumber(ctx, (double)v.val)))
#define HEAP_PB(LANE)                                                        \
  HEAP_DEF(LANE,                                                             \
           (v.key = (u64)JSValueToUInt64(ctx, args[2], exception),           \
            v.val = (u64)JSValueToUInt64(ctx, args[3], exception)),          \
           JABCPair(ctx, JSBigIntCreateWithUInt64(ctx, (uint64_t)v.key,      \
                                                  exception),               \
                    JSBigIntCreateWithUInt64(ctx, (uint64_t)v.val,           \
                                             exception)))

//  A blob lane element is a fixed-size byte struct; the JS side passes a
//  Uint8Array of exactly sizeof(LANE).  RDV reads+validates it into `v.data`;
//  WRV returns a fresh Uint8Array copy of the popped struct.
#define HEAP_BLOB(LANE)                                                       \
  HEAP_DEF(LANE,                                                              \
           {                                                                  \
             u8s _b = {};                                                     \
             if (!JABCBytesOf(_b, ctx, args[2], exception))                   \
               return JSValueMakeUndefined(ctx);                             \
             if ((size_t)$len(_b) != sizeof(LANE))                            \
               JABC_THROW("heap: blob size");                                 \
             memcpy(v.data, _b[0], sizeof(LANE));                             \
           },                                                                 \
           JABCBlob(ctx, v.data, sizeof(LANE)))

HEAP_NUM(u8)
HEAP_NUM(u16)
HEAP_NUM(u32)
HEAP_U64(u64)
HEAP_U64(wh64)
HEAP_PN(kv32)
HEAP_PB(kv64)
HEAP_PB(wh128)
HEAP_BLOB(sha1)
HEAP_BLOB(sha256)

#define HEAP_REG(LANE)                                              \
  JABC_API_FN(o, "_heap_" #LANE "_push", jheap_##LANE##_push);      \
  JABC_API_FN(o, "_heap_" #LANE "_pop", jheap_##LANE##_pop)

static inline void JABCHeapInstall(JSObjectRef o) {
  HEAP_REG(u8);
  HEAP_REG(u16);
  HEAP_REG(u32);
  HEAP_REG(u64);
  HEAP_REG(kv32);
  HEAP_REG(kv64);
  HEAP_REG(wh64);
  HEAP_REG(wh128);
  HEAP_REG(sha1);
  HEAP_REG(sha256);
}

#endif
