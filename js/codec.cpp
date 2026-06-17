#include "JABC.hpp"
#include "cont.hpp"  //  JABCBlob (engine Uint8Array copy)
extern "C" {
#include "abc/HEX.h"
#include "abc/RON.h"
#include "abc/SHA.h"
#include "dog/git/SHA1.h"
}

//  hex + sha: pure byte transforms (no state).  hex mirrors utf8 (a string on
//  the text side, bytes on the binary side); sha1/sha256 hash bytes -> the
//  sha1 lane (Uint8Array(20)/(32)).

//  hex.encode(Uint8Array) -> lowercase hex string
static JABC_FN(JABChexEncode) {
  if (argc < 1) JABC_THROW("hex.encode(Uint8Array)");
  u8s bin = {};
  if (!JABCBytesOf(bin, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t n = (size_t)$len(bin);
  char* h = (char*)malloc(n * 2 + 1);
  if (h == NULL) JABC_THROW("hex.encode: oom");
  u8s hx = {(u8*)h, (u8*)h + n * 2};
  u8cs b = {bin[0], bin[1]};
  HEXu8sFeedSome(hx, b);
  h[n * 2] = 0;
  JSStringRef js = JSStringCreateWithUTF8CString(h);
  free(h);
  JSValueRef v = JSValueMakeString(ctx, js);
  JSStringRelease(js);
  return v;
}

//  hex.encodeInto(src, dst) -> hex chars written (provided-buffer form)
static JABC_FN(JABChexEncodeInto) {
  if (argc < 2) JABC_THROW("hex.encodeInto(src, dst)");
  u8s bin = {}, dst = {};
  if (!JABCBytesOf(bin, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  if (!JABCBytesOf(dst, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);
  u8s hx = {dst[0], dst[1]};
  u8cs b = {bin[0], bin[1]};
  HEXu8sFeedSome(hx, b);
  return JSValueMakeNumber(ctx, (double)(size_t)(hx[0] - dst[0]));
}

//  hex.decode(string) -> Uint8Array
static JABC_FN(JABChexDecode) {
  if (argc < 1 || !JSValueIsString(ctx, args[0])) JABC_THROW("hex.decode(string)");
  JSStringRef s = JSValueToStringCopy(ctx, args[0], exception);
  if (*exception || s == NULL) return JSValueMakeUndefined(ctx);
  size_t max = JSStringGetMaximumUTF8CStringSize(s);
  char* hb = (char*)malloc(max);
  if (hb == NULL) { JSStringRelease(s); JABC_THROW("hex.decode: oom"); }
  size_t got = JSStringGetUTF8CString(s, hb, max);
  JSStringRelease(s);
  size_t hlen = got ? got - 1 : 0;  //  drop the NUL the API counts
  u8cs hex = {(u8*)hb, (u8*)hb + hlen};
  if ((hlen & 1) || !HEXu8sValid(hex)) { free(hb); JABC_THROW("hex.decode: bad hex"); }
  size_t n = hlen / 2;
  JSObjectRef ta =
      JSObjectMakeTypedArray(ctx, kJSTypedArrayTypeUint8Array, n, exception);
  if (*exception || ta == NULL) { free(hb); return JSValueMakeUndefined(ctx); }
  u8* p = (u8*)JSObjectGetTypedArrayBytesPtr(ctx, ta, exception);
  if (p && n) {
    u8s bin = {p, p + n};
    HEXu8sDrainSome(bin, hex);
  }
  free(hb);
  return ta;
}

//  sha1(Uint8Array) -> Uint8Array(20)
static JABC_FN(JABCsha1) {
  if (argc < 1) JABC_THROW("sha1(Uint8Array)");
  u8s bin = {};
  if (!JABCBytesOf(bin, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  sha1 h = {};
  u8cs from = {bin[0], bin[1]};
  SHA1Sum(&h, from);
  return JABCBlob(ctx, h.data, sizeof(h.data));
}

//  sha256(Uint8Array) -> Uint8Array(32)
static JABC_FN(JABCsha256) {
  if (argc < 1) JABC_THROW("sha256(Uint8Array)");
  u8s bin = {};
  if (!JABCBytesOf(bin, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  sha256 h = {};
  u8cs from = {bin[0], bin[1]};
  SHASum(&h, from);
  return JABCBlob(ctx, h.data, sizeof(h.data));
}

//  ron.encode(BigInt) -> RON base64 string  (timestamps, verbs, ok64 codes)
static JABC_FN(JABCronEncode) {
  if (argc < 1) JABC_THROW("ron.encode(BigInt)");
  uint64_t v = JSValueToUInt64(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  u8 b[16];
  u8s into = {b, b + sizeof(b)};
  RONutf8sFeed(into, (ok64)v);
  size_t n = (size_t)(into[0] - b);
  char tmp[17];
  if (n > 16) n = 16;
  memcpy(tmp, b, n);
  tmp[n] = 0;
  JSStringRef js = JSStringCreateWithUTF8CString(tmp);
  JSValueRef r = JSValueMakeString(ctx, js);
  JSStringRelease(js);
  return r;
}

//  ron.decode(string) -> BigInt
static JABC_FN(JABCronDecode) {
  if (argc < 1 || !JSValueIsString(ctx, args[0])) JABC_THROW("ron.decode(string)");
  JSStringRef s = JSValueToStringCopy(ctx, args[0], exception);
  if (*exception || s == NULL) return JSValueMakeUndefined(ctx);
  u8 b[32];
  size_t got = JSStringGetUTF8CString(s, (char*)b, sizeof(b));
  JSStringRelease(s);
  size_t n = got ? got - 1 : 0;
  u8cs from = {b, b + n};
  ok64 v = 0;
  RONutf8sDrain(&v, from);
  return JSBigIntCreateWithUInt64(ctx, (uint64_t)v, exception);
}

ok64 JABCCodecInstall() {
  JABC_API_OBJECT(ron);
  JABC_API_FN(ron, "encode", JABCronEncode);
  JABC_API_FN(ron, "decode", JABCronDecode);
  JABC_API_OBJECT(hex);
  JABC_API_FN(hex, "encode", JABChexEncode);
  JABC_API_FN(hex, "encodeInto", JABChexEncodeInto);
  JABC_API_FN(hex, "decode", JABChexDecode);
  JABC_API_FN(JABC_GLOBAL_OBJECT, "sha1", JABCsha1);
  JABC_API_FN(JABC_GLOBAL_OBJECT, "sha256", JABCsha256);
  return OK;
}
