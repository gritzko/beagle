#ifndef JABC_HIT_HPP
#define JABC_HIT_HPP
//  HIT bindings: bulk ops over SORTED runs (typed arrays), not a container.
//   - sort:      in-place sort of a container's live [0,size) by the lane Z
//                (QSORTx <lane>sSort).
//   - merge:     k-way sorted, deduplicated union of N runs (HITx Merge).
//   - intersect: values present in ALL N runs (HITx Intersect).
//  The heap-of-iterators is a tiny stack array (<=64 runs); inputs stay alive
//  as JS args; the output is a fresh engine-owned typed array.
//
//  Leaves: _sort_<lane>(arr, size)
//          _merge_<lane>(runs[]) / _isect_<lane>(runs[]) -> Uint8Array
#include "cont.hpp"

extern "C" {
#include "abc/KV.h"
#include "dog/WHIFF.h"
#include "dog/git/SHA1.h"
#include "abc/SHA.h"

//  HIT's one prerequisite (HIT.md): swap two slice entries.  Array types
//  can't go through Sx.h, so it's defined per lane here in js/.
#define CSSWAP(L)                                              \
  fun void L##csSwap(L##cs* a, L##cs* b) {                     \
    L const* t0 = (*a)[0];                                     \
    L const* t1 = (*a)[1];                                     \
    (*a)[0] = (*b)[0];                                         \
    (*a)[1] = (*b)[1];                                         \
    (*b)[0] = t0;                                              \
    (*b)[1] = t1;                                              \
  }
//  u8 and wh128 already have csSwap (defined elsewhere in the include graph);
//  define it for the remaining lanes only.
CSSWAP(u16) CSSWAP(u32) CSSWAP(u64)
CSSWAP(kv32) CSSWAP(kv64) CSSWAP(wh64)
CSSWAP(sha1) CSSWAP(sha256)
}

//  QSORTx (sSort) ONLY for lanes abc/dog don't already instantiate.
//  Already present: u8/u16/u32/u64 (INT.h), kv64 (KV.h).  Missing → add:
#define X(M, name) M##kv32##name
#include "abc/QSORTx.h"
#undef X
#define X(M, name) M##wh64##name
#include "abc/QSORTx.h"
#undef X
#define X(M, name) M##wh128##name
#include "abc/QSORTx.h"
#undef X
#define X(M, name) M##sha1##name
#include "abc/QSORTx.h"
#undef X
#define X(M, name) M##sha256##name
#include "abc/QSORTx.h"
#undef X

//  HITx (Start/Merge/Intersect) for every lane — not pre-instantiated
//  anywhere; needs the csSwap defined above.
#define X(M, name) M##u8##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##u16##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##u32##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##u64##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##kv32##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##kv64##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##wh64##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##wh128##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##sha1##name
#include "abc/HITx.h"
#undef X
#define X(M, name) M##sha256##name
#include "abc/HITx.h"
#undef X

//  sort leaf: sort the live [0,n) region in place by the lane Z.
#define SORT_LEAF(L)                                                          \
  static JABC_FN(jsort_##L) {                                                 \
    void* base;                                                               \
    size_t cap;                                                               \
    if (!JABCLaneArr(&base, &cap, ctx, args[0], sizeof(L), exception))        \
      return JSValueMakeUndefined(ctx);                                       \
    size_t n = (size_t)JSValueToNumber(ctx, args[1], exception);              \
    if (n > cap) n = cap;                                                     \
    L* bb = (L*)base;                                                         \
    L##s sl = {bb, bb + n};                                                   \
    L##sSort(sl);                                                             \
    return JSValueMakeUndefined(ctx);                                         \
  }

//  Shared k-way run: build the iterator heap from a JS array of typed-array
//  runs, then HIT Merge (isect=0) or Intersect (isect=1) into a fresh output.
#define HIT_RUN(L)                                                            \
  static JSValueRef JABChit_##L(JSContextRef ctx, size_t argc,                \
                                const JSValueRef args[], JSValueRef* exception,\
                                int isect) {                                   \
    if (argc < 1 || !JSValueIsObject(ctx, args[0]))                           \
      JABC_THROW("merge/intersect([runs])");                                  \
    JSObjectRef arr = (JSObjectRef)args[0];                                   \
    JSStringRef lk = JSStringCreateWithUTF8CString("length");                 \
    size_t N = (size_t)JSValueToNumber(                                       \
        ctx, JSObjectGetProperty(ctx, arr, lk, exception), exception);        \
    JSStringRelease(lk);                                                      \
    if (N > 64) JABC_THROW("merge: too many runs (max 64)");                  \
    L##cs ent[64];                                                            \
    size_t total = 0;                                                         \
    for (size_t i = 0; i < N; i++) {                                          \
      JSValueRef el =                                                         \
          JSObjectGetPropertyAtIndex(ctx, arr, (unsigned)i, exception);       \
      u8s b = {};                                                             \
      if (!JABCBytesOf(b, ctx, el, exception))                                \
        return JSValueMakeUndefined(ctx);                                     \
      ent[i][0] = (const L*)b[0];                                             \
      ent[i][1] = (const L*)b[1];                                             \
      total += (size_t)$len(b) / sizeof(L);                                   \
    }                                                                         \
    /* destination given (args[1]) -> write in place, return the count; the  \
       caller's container is sized to the Sum upper bound and trimmed on      \
       close (abc.book). */                                                   \
    if (argc >= 2 &&                                                          \
        JSValueGetTypedArrayType(ctx, args[1], NULL) !=                       \
            kJSTypedArrayTypeNone) {                                          \
      u8s d = {};                                                             \
      if (!JABCBytesOf(d, ctx, args[1], exception))                           \
        return JSValueMakeUndefined(ctx);                                     \
      if ((size_t)$len(d) < total * sizeof(L)) JABC_THROW("merge: out too small");\
      L* dp = (L*)d[0];                                                       \
      L* db = dp;                                                             \
      if (N > 0) {                                                            \
        L##css heap = {ent, ent + N};                                         \
        HIT##L##Start(heap);                                                  \
        if (isect) HIT##L##Intersect(heap, &dp, N);                          \
        else HIT##L##Merge(heap, &dp);                                        \
      }                                                                       \
      return JSValueMakeNumber(ctx, (double)(size_t)(dp - db));               \
    }                                                                         \
    JSObjectRef out = JSObjectMakeTypedArray(                                 \
        ctx, kJSTypedArrayTypeUint8Array, total * sizeof(L), exception);      \
    if (*exception || !out) return JSValueMakeUndefined(ctx);                 \
    L* op = (L*)JSObjectGetTypedArrayBytesPtr(ctx, out, exception);           \
    L* ob = op;                                                               \
    if (N > 0) {                                                              \
      L##css heap = {ent, ent + N};                                           \
      HIT##L##Start(heap);                                                    \
      if (isect)                                                              \
        HIT##L##Intersect(heap, &op, N);                                      \
      else                                                                    \
        HIT##L##Merge(heap, &op);                                             \
    }                                                                         \
    size_t cnt = (size_t)(op - ob);                                          \
    JSObjectRef buf = JSObjectGetTypedArrayBuffer(ctx, out, exception);       \
    return JSObjectMakeTypedArrayWithArrayBufferAndOffset(                    \
        ctx, kJSTypedArrayTypeUint8Array, buf, 0, cnt * sizeof(L), exception);\
  }                                                                          \
  static JABC_FN(jmerge_##L) {                                                \
    return JABChit_##L(ctx, argc, args, exception, 0);                        \
  }                                                                          \
  static JABC_FN(jisect_##L) {                                                \
    return JABChit_##L(ctx, argc, args, exception, 1);                        \
  }

#define HIT_DEF(L) SORT_LEAF(L) HIT_RUN(L)

HIT_DEF(u8)
HIT_DEF(u16)
HIT_DEF(u32)
HIT_DEF(u64)
HIT_DEF(kv32)
HIT_DEF(kv64)
HIT_DEF(wh64)
HIT_DEF(wh128)
HIT_DEF(sha1)
HIT_DEF(sha256)

#define HIT_REG(L)                                                  \
  JABC_API_FN(o, "_sort_" #L, jsort_##L);                           \
  JABC_API_FN(o, "_merge_" #L, jmerge_##L);                         \
  JABC_API_FN(o, "_isect_" #L, jisect_##L)

static inline void JABCHitInstall(JSObjectRef o) {
  HIT_REG(u8);
  HIT_REG(u16);
  HIT_REG(u32);
  HIT_REG(u64);
  HIT_REG(kv32);
  HIT_REG(kv64);
  HIT_REG(wh64);
  HIT_REG(wh128);
  HIT_REG(sha1);
  HIT_REG(sha256);
}

#endif
