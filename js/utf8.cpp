#include "JABC.hpp"
#include "abc/UTF8.h"

//  utf8.encodeInto(str, dst) -> n
//
//  Encode `str` as UTF-8 directly into the caller-owned typed array `dst`,
//  return the byte count written.  No allocation: the buffer is JS-owned, the
//  binding only fills it.  Walks the string's UTF-16 units (no-copy pointer
//  from JSC), combines surrogate pairs, and feeds whole code points via
//  abc's utf8sFeed32 — so it stops on a clean code-point boundary when `dst`
//  is full (never a half-written multibyte char) and returns what fit.  Lone
//  surrogates degrade to U+FFFD.
static JABC_FN(JABCutf8EncodeInto) {
  if (argc < 2 || !JSValueIsString(ctx, args[0]))
    JABC_THROW("utf8.encodeInto(string, Uint8Array) -> n");
  u8s dst = {};
  if (!JABCBytesOf(dst, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);

  JSStringRef str = JSValueToStringCopy(ctx, args[0], exception);
  if (*exception || str == NULL) return JSValueMakeUndefined(ctx);
  const JSChar* u = JSStringGetCharactersPtr(str);
  size_t n = JSStringGetLength(str);

  u8* base = dst[0];
  utf8s into = {(utf8*)dst[0], (utf8*)dst[1]};
  for (size_t i = 0; i < n;) {
    u32 cp = u[i++];
    if (cp >= 0xD800 && cp <= 0xDBFF) {  //  high surrogate
      if (i < n && u[i] >= 0xDC00 && u[i] <= 0xDFFF)
        cp = 0x10000 + ((cp - 0xD800) << 10) + (u[i++] - 0xDC00);
      else
        cp = 0xFFFD;  //  unpaired high
    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
      cp = 0xFFFD;  //  unpaired low
    }
    if (utf8sFeed32(into, cp) != OK) break;  //  dst full: stop at boundary
  }
  JSStringRelease(str);
  size_t wrote = (size_t)((u8*)into[0] - base);
  return JSValueMakeNumber(ctx, (double)wrote);
}

//  utf8.Decode(Uint8Array) -> string
//
//  Validate inbound UTF-8 (rejecting overlong forms, lone surrogates,
//  truncated multibyte, bad continuation bytes and > U+10FFFF via abc's
//  utf8sValid) BEFORE building any JS string, then decode code-point by
//  code-point into UTF-16 units (JSStringCreateWithCharacters takes an
//  explicit length, so embedded NULs survive — unlike JSC's lenient
//  NUL-terminated UTF-8 constructor).
static JABC_FN(JABCutf8Decode) {
  if (argc < 1) JABC_THROW("utf8.Decode(Uint8Array) -> string");
  u8s src = {};
  if (!JABCBytesOf(src, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);

  utf8cs scan = {(utf8c*)src[0], (utf8c*)src[1]};
  if (utf8sValid(scan) != OK) JABC_THROW("utf8.Decode(): malformed UTF-8");

  size_t len = $len(src);
  //  Code-point count <= byte count; an astral char is 4 bytes -> 2 units,
  //  so `len` units is always enough room.
  JSChar* units = (JSChar*)malloc((len + 1) * sizeof(JSChar));
  if (units == NULL) JABC_THROW("utf8.Decode(): out of memory");
  size_t k = 0;
  utf8cs cur = {(utf8c*)src[0], (utf8c*)src[1]};
  while (!$empty(cur)) {
    u32 cp = 0;
    if (utf8sDrain32(&cp, cur) != OK) break;  //  validated above
    if (cp <= 0xFFFF) {
      units[k++] = (JSChar)cp;
    } else {
      cp -= 0x10000;
      units[k++] = (JSChar)(0xD800 + (cp >> 10));
      units[k++] = (JSChar)(0xDC00 + (cp & 0x3FF));
    }
  }
  JSStringRef out = JSStringCreateWithCharacters(units, k);
  free(units);
  JSValueRef val = JSValueMakeString(ctx, out);
  JSStringRelease(out);
  return val;
}

JSValueRef JSOfCString(const char* str) {
  JSStringRef tmp = JSStringCreateWithUTF8CString(str);
  JSValueRef v = JSValueMakeString(JABC_CONTEXT, tmp);
  JSStringRelease(tmp);
  return v;
}

//  Shared boundary helper: a typed array's backing range as a u8 slice.
b8 JABCBytesOf(u8s out, JSContextRef ctx, JSValueRef arg, JSValueRef* exception) {
  if (JSValueGetTypedArrayType(ctx, arg, NULL) == kJSTypedArrayTypeNone) {
    *exception = JSOfCString("expected a typed array");
    return NO;
  }
  JSObjectRef obj = JSValueToObject(ctx, arg, exception);
  if (*exception) return NO;
  u8* base = (u8*)JSObjectGetTypedArrayBytesPtr(ctx, obj, exception);
  if (*exception) return NO;
  size_t len = JSObjectGetTypedArrayByteLength(ctx, obj, exception);
  if (*exception) return NO;
  //  A detached/neutered ArrayBuffer yields a NULL bytes ptr (len may be 0).
  if (base == NULL && len != 0) {
    *exception = JSOfCString("detached buffer");
    return NO;
  }
  //  BytesPtr is the ArrayBuffer base, NOT the view's start: a subarray
  //  (byteOffset > 0) shares the buffer, so add the offset or every
  //  offset view reads/writes the wrong bytes.
  size_t off = JSObjectGetTypedArrayByteOffset(ctx, obj, exception);
  if (*exception) return NO;
  out[0] = base + off;
  out[1] = base + off + len;
  return YES;
}

ok64 JABCutf8Install() {
  JABC_API_OBJECT(utf8);
  JABC_API_FN(utf8, "encodeInto", JABCutf8EncodeInto);
  JABC_API_FN(utf8, "Decode", JABCutf8Decode);
  return OK;
}
