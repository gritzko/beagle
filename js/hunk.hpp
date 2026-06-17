#ifndef JABC_HUNK_HPP
#define JABC_HUNK_HPP
//  HUNK bindings — a u8-backed LOG of TLV 'H' records (one buffer, many
//  hunks) with a write head and a read cursor.  The JS HUNK object owns both
//  cursors; these native leaves are stateless — each reconstructs the slice/
//  record it needs from (buffer, offset) per call and holds nothing.
//
//  Append:  _hunk_feed(buf, off, uri, text, toks) -> newoff   (HUNKu8sFeed)
//           _hunk_dogenize(buf, off, src, ext, uri) -> newoff (tokenize + feed)
//  Walk:    _hunk_next(buf, readOff, dataLen) -> recEnd | -1   (HUNKu8sDrain)
//  Current: _hunk_uri/_text/_toks/_verb/_time(buf, recOff)     (zero-copy / BigInt)
//  Render:  _hunk_render(buf, recOff, out, outOff, mode) -> n
#include "cont.hpp"

extern "C" {
#include "dog/HUNK.h"
}

//  A u8 slice from a JS string (copied into `tmp`, NUL dropped) or a typed
//  array (zero-copy view).
static inline bool JABCArgU8(u8s out, JSContextRef ctx, JSValueRef v, u8* tmp,
                             size_t cap, JSValueRef* ex) {
  if (JSValueIsString(ctx, v)) {
    JSStringRef s = JSValueToStringCopy(ctx, v, ex);
    if (*ex || s == NULL) return false;
    size_t n = JSStringGetUTF8CString(s, (char*)tmp, cap);
    JSStringRelease(s);
    out[0] = tmp;
    out[1] = tmp + (n ? n - 1 : 0);
    return true;
  }
  return JABCBytesOf(out, ctx, v, ex);
}

//  A no-copy Uint8Array view over `ta`'s backing at [off, off+len).
static inline JSValueRef JABCSubU8(JSContextRef ctx, JSValueRef ta, size_t off,
                                   size_t len, JSValueRef* ex) {
  JSObjectRef o = JSValueToObject(ctx, ta, ex);
  if (*ex) return JSValueMakeUndefined(ctx);
  size_t bo = JSObjectGetTypedArrayByteOffset(ctx, o, ex);
  JSObjectRef buf = JSObjectGetTypedArrayBuffer(ctx, o, ex);
  if (*ex) return JSValueMakeUndefined(ctx);
  return JSObjectMakeTypedArrayWithArrayBufferAndOffset(
      ctx, kJSTypedArrayTypeUint8Array, buf, bo + off, len, ex);
}

//  _hunk_feed(buf, off, uri, text, toks) -> newoff
static JABC_FN(JABChunkFeed) {
  if (argc < 5) JABC_THROW("hunk.feed(buf, off, uri, text, toks)");
  u8s buf = {};
  if (!JABCBytesOf(buf, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  u8 uritmp[FILE_PATH_MAX_LEN];
  u8s uri = {}, text = {}, toks = {};
  if (!JABCArgU8(uri, ctx, args[2], uritmp, sizeof(uritmp), exception))
    return JSValueMakeUndefined(ctx);
  if (!JABCBytesOf(text, ctx, args[3], exception)) return JSValueMakeUndefined(ctx);
  if (!JABCBytesOf(toks, ctx, args[4], exception)) return JSValueMakeUndefined(ctx);
  hunk hk = {};
  hk.uri[0] = uri[0];   hk.uri[1] = uri[1];
  hk.text[0] = text[0]; hk.text[1] = text[1];
  hk.toks[0] = (tok32c*)toks[0];
  hk.toks[1] = (tok32c*)(toks[0] + $len(toks) / sizeof(tok32));
  u8s into = {buf[0] + off, buf[1]};
  if (HUNKu8sFeed(into, &hk) != OK) JABC_THROW("hunk.feed: out full");
  return JSValueMakeNumber(ctx, (double)(size_t)(into[0] - buf[0]));
}

//  _hunk_dogenize(buf, off, source, ext, uri) -> newoff
//  Tokenize source via the ext-selected lexer into a transient toks buffer,
//  then serialize a hunk (text=source, toks, uri).  The toks are malloc'd for
//  the call and freed before return — nothing is held, nothing crosses to JS.
static JABC_FN(JABChunkDogenize) {
  if (argc < 4) JABC_THROW("hunk.dogenize(buf, off, source, ext, uri)");
  u8s buf = {}, source = {};
  if (!JABCBytesOf(buf, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  if (!JABCBytesOf(source, ctx, args[2], exception)) return JSValueMakeUndefined(ctx);
  u8 exttmp[64], uritmp[FILE_PATH_MAX_LEN];
  u8s ext = {}, uri = {};
  if (!JABCArgU8(ext, ctx, args[3], exttmp, sizeof(exttmp), exception))
    return JSValueMakeUndefined(ctx);
  if (argc > 4) {
    if (!JABCArgU8(uri, ctx, args[4], uritmp, sizeof(uritmp), exception))
      return JSValueMakeUndefined(ctx);
  }
  size_t srcn = $len(source);
  u32* tm = (u32*)malloc((srcn + 1) * sizeof(u32));
  if (tm == NULL) JABC_THROW("hunk.dogenize: oom");
  u32* tb[4] = {tm, tm, tm, tm + srcn + 1};
  u8cs srcc = {source[0], source[1]};
  u8cs extc = {ext[0], ext[1]};
  ok64 o = HUNKu32bTokenize(tb, srcc, extc);
  if (o != OK) { free(tm); JABC_THROW("hunk.dogenize: lex"); }
  hunk hk = {};
  hk.uri[0] = uri[0];   hk.uri[1] = uri[1];
  hk.text[0] = source[0]; hk.text[1] = source[1];
  hk.toks[0] = (tok32c*)tb[1];
  hk.toks[1] = (tok32c*)tb[2];
  u8s into = {buf[0] + off, buf[1]};
  o = HUNKu8sFeed(into, &hk);
  free(tm);
  if (o != OK) JABC_THROW("hunk.dogenize: out full");
  return JSValueMakeNumber(ctx, (double)(size_t)(into[0] - buf[0]));
}

//  _hunk_next(buf, readOff, dataLen) -> recEnd | -1
static JABC_FN(JABChunkNext) {
  if (argc < 3) JABC_THROW("hunk._next(buf, readOff, dataLen)");
  u8s buf = {};
  if (!JABCBytesOf(buf, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t r = (size_t)JSValueToNumber(ctx, args[1], exception);
  size_t dl = (size_t)JSValueToNumber(ctx, args[2], exception);
  if (r >= dl) return JSValueMakeNumber(ctx, -1);
  u8cs from = {buf[0] + r, buf[0] + dl};
  hunk hk = {};
  if (HUNKu8sDrain(from, &hk) != OK) return JSValueMakeNumber(ctx, -1);
  return JSValueMakeNumber(ctx, (double)(size_t)((u8c*)from[0] - buf[0]));
}

//  Drain the record at recOff; the field accessors below re-drain (cheap TLV
//  walk) so no hunk state is held between calls.
static bool JABChunkAt(hunk* hk, u8s buf, JSContextRef ctx, JSValueRef bufv,
                       JSValueRef offv, JSValueRef* ex) {
  if (!JABCBytesOf(buf, ctx, bufv, ex)) return false;
  size_t rec = (size_t)JSValueToNumber(ctx, offv, ex);
  u8cs from = {buf[0] + rec, buf[1]};
  return HUNKu8sDrain(from, hk) == OK;
}

static JABC_FN(JABChunkUri) {
  if (argc < 2) JABC_THROW("hunk._uri(buf, recOff)");
  u8s buf = {};
  hunk hk = {};
  if (!JABChunkAt(&hk, buf, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  return JABCSubU8(ctx, args[0], (size_t)((u8c*)hk.uri[0] - buf[0]),
                   $len(hk.uri), exception);
}
static JABC_FN(JABChunkText) {
  if (argc < 2) JABC_THROW("hunk._text(buf, recOff)");
  u8s buf = {};
  hunk hk = {};
  if (!JABChunkAt(&hk, buf, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  return JABCSubU8(ctx, args[0], (size_t)((u8c*)hk.text[0] - buf[0]),
                   $len(hk.text), exception);
}
//  toks may sit at an unaligned offset in the record, so return a fresh
//  (aligned) Uint32Array copy rather than an alias.
static JABC_FN(JABChunkToks) {
  if (argc < 2) JABC_THROW("hunk._toks(buf, recOff)");
  u8s buf = {};
  hunk hk = {};
  if (!JABChunkAt(&hk, buf, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  size_t n = $len(hk.toks);
  JSObjectRef ta =
      JSObjectMakeTypedArray(ctx, kJSTypedArrayTypeUint32Array, n, exception);
  if (*exception || ta == NULL) return JSValueMakeUndefined(ctx);
  void* p = JSObjectGetTypedArrayBytesPtr(ctx, ta, exception);
  if (p && n) memcpy(p, hk.toks[0], n * sizeof(tok32));
  return ta;
}
static JABC_FN(JABChunkVerb) {
  if (argc < 2) JABC_THROW("hunk._verb(buf, recOff)");
  u8s buf = {};
  hunk hk = {};
  if (!JABChunkAt(&hk, buf, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  return JSBigIntCreateWithUInt64(ctx, (uint64_t)hk.verb, exception);
}
static JABC_FN(JABChunkTime) {
  if (argc < 2) JABC_THROW("hunk._time(buf, recOff)");
  u8s buf = {};
  hunk hk = {};
  if (!JABChunkAt(&hk, buf, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  return JSBigIntCreateWithUInt64(ctx, (uint64_t)hk.ts, exception);
}

//  _hunk_render(buf, recOff, out, outOff, mode) -> bytes written
//  mode: 1 = color (ANSI), 2 = plain, 3 = html.
static JABC_FN(JABChunkRender) {
  if (argc < 5) JABC_THROW("hunk._render(buf, recOff, out, outOff, mode)");
  u8s buf = {};
  hunk hk = {};
  if (!JABChunkAt(&hk, buf, ctx, args[0], args[1], exception))
    JABC_THROW("hunk.render: bad record");
  u8s out = {};
  if (!JABCBytesOf(out, ctx, args[2], exception)) return JSValueMakeUndefined(ctx);
  size_t oo = (size_t)JSValueToNumber(ctx, args[3], exception);
  int mode = (int)JSValueToNumber(ctx, args[4], exception);
  u8s into = {out[0] + oo, out[1]};
  u8* base = into[0];
  ok64 o;
  if (mode == 3) o = HUNKu8sFeedHtml(into, &hk);
  else if (mode == 1) o = HUNKu8sFeedColor(into, &hk);
  else o = HUNKu8sFeedText(into, &hk);
  if (o != OK) JABC_THROW("hunk.render: out full");
  return JSValueMakeNumber(ctx, (double)(size_t)(into[0] - base));
}

static inline void JABCHunkInstall(JSObjectRef o) {
  JABC_API_FN(o, "_hunk_feed", JABChunkFeed);
  JABC_API_FN(o, "_hunk_dogenize", JABChunkDogenize);
  JABC_API_FN(o, "_hunk_next", JABChunkNext);
  JABC_API_FN(o, "_hunk_uri", JABChunkUri);
  JABC_API_FN(o, "_hunk_text", JABChunkText);
  JABC_API_FN(o, "_hunk_toks", JABChunkToks);
  JABC_API_FN(o, "_hunk_verb", JABChunkVerb);
  JABC_API_FN(o, "_hunk_time", JABChunkTime);
  JABC_API_FN(o, "_hunk_render", JABChunkRender);
}

#endif
