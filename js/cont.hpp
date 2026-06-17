#ifndef JABC_CONT_HPP
#define JABC_CONT_HPP
//  Shared boilerplate for the container bindings (heap.hpp, hash.hpp).
//  All glue lives in js/; abc is never modified.  A container is a JS-owned
//  typed array; the binding only forms a stack-local slice/buffer view over
//  its backing memory, runs the abc op, and returns — no memory held.
//
//  NOTE: a TU that includes hash.hpp must include abc/PRO.h first (HASHx uses
//  sane/done); §6 keeps PRO.h out of headers, so the .cpp provides it.
#include "JABC.hpp"

//  A lane array's backing: base pointer + capacity in ELEMENTS (offset-
//  adjusted via JABCBytesOf).  `esz` is sizeof(lane).
static inline bool JABCLaneArr(void** base, size_t* cap, JSContextRef ctx,
                               JSValueRef arg, size_t esz, JSValueRef* ex) {
  u8s b = {};
  if (!JABCBytesOf(b, ctx, arg, ex)) return false;
  *base = (void*)b[0];
  *cap = (size_t)$len(b) / esz;
  return true;
}

//  A two-element JS array, for returning (key,val) pairs.
static inline JSValueRef JABCPair(JSContextRef ctx, JSValueRef a, JSValueRef b) {
  JSValueRef e[2] = {a, b};
  return JSObjectMakeArray(ctx, 2, e, NULL);
}

//  A fresh engine-owned Uint8Array of `n` bytes holding a copy of `data`,
//  for returning a fixed-size byte blob (sha1/sha256 lanes).
static inline JSValueRef JABCBlob(JSContextRef ctx, const u8* data, size_t n) {
  JSValueRef ex = NULL;
  JSObjectRef ta = JSObjectMakeTypedArray(ctx, kJSTypedArrayTypeUint8Array, n, &ex);
  if (!ta) return JSValueMakeUndefined(ctx);
  void* p = JSObjectGetTypedArrayBytesPtr(ctx, ta, &ex);
  if (p) memcpy(p, data, n);
  return ta;
}

#endif
