#include "JABC.hpp"
extern "C" {
#include "abc/URI.h"
}

//  abc/URI bindings: parse a URI string into its 8 components, compose one
//  from parts, percent-escape/unescape.  A URI is small text, so components
//  cross as JS strings (decoded), not zero-copy views.  The JS `URI` class
//  (embedded below) wraps these leaves.

//  A JS string from a u8 slice (NUL-terminated copy; URI bytes are ASCII-ish).
static JSValueRef JABCSliceStr(JSContextRef ctx, u8cs s) {
  char tmp[2048];
  size_t n = (size_t)$len(s);
  if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
  if (n) memcpy(tmp, s[0], n);
  tmp[n] = 0;
  JSStringRef js = JSStringCreateWithUTF8CString(tmp);
  JSValueRef v = JSValueMakeString(ctx, js);
  JSStringRelease(js);
  return v;
}

static void JABCSetStr(JSContextRef ctx, JSObjectRef obj, const char* name,
                       JSValueRef val) {
  JSStringRef k = JSStringCreateWithUTF8CString(name);
  JSObjectSetProperty(ctx, obj, k, val, kJSPropertyAttributeNone, NULL);
  JSStringRelease(k);
}

//  Copy a JS-string arg into `tmp`, fill `out` as a u8cs over it (NUL dropped).
static void JABCArgSlice(u8cs out, JSContextRef ctx, JSValueRef v, u8* tmp,
                         size_t cap) {
  out[0] = tmp;
  out[1] = tmp;
  if (!JSValueIsString(ctx, v)) return;
  JSStringRef js = JSValueToStringCopy(ctx, v, NULL);
  size_t n = JSStringGetUTF8CString(js, (char*)tmp, cap);
  JSStringRelease(js);
  out[1] = tmp + (n ? n - 1 : 0);
}

//  uri._parse(string) -> {scheme,authority,user,host,port,path,query,fragment}
static JABC_FN(JABCuriParse) {
  if (argc < 1 || !JSValueIsString(ctx, args[0])) JABC_THROW("uri.parse(string)");
  u8 buf[4096];
  JSStringRef s = JSValueToStringCopy(ctx, args[0], exception);
  if (*exception || s == NULL) return JSValueMakeUndefined(ctx);
  size_t len = JSStringGetUTF8CString(s, (char*)buf, sizeof(buf));
  JSStringRelease(s);
  len = len ? len - 1 : 0;  //  drop the NUL the API counts
  uri u = {};
  u.data[0] = buf;
  u.data[1] = buf + len;
  if (URILexer(&u) != OK) JABC_THROW("uri.parse: malformed");
  JSObjectRef o = JSObjectMake(ctx, NULL, NULL);
  JABCSetStr(ctx, o, "scheme", JABCSliceStr(ctx, u.scheme));
  JABCSetStr(ctx, o, "authority", JABCSliceStr(ctx, u.authority));
  JABCSetStr(ctx, o, "user", JABCSliceStr(ctx, u.user));
  JABCSetStr(ctx, o, "host", JABCSliceStr(ctx, u.host));
  JABCSetStr(ctx, o, "port", JABCSliceStr(ctx, u.port));
  JABCSetStr(ctx, o, "path", JABCSliceStr(ctx, u.path));
  JABCSetStr(ctx, o, "query", JABCSliceStr(ctx, u.query));
  JABCSetStr(ctx, o, "fragment", JABCSliceStr(ctx, u.fragment));
  return o;
}

//  uri._make(scheme, authority, path, query, fragment) -> string
static JABC_FN(JABCuriMake) {
  u8 sb[256], ab[256], pb[1024], qb[512], fb[512], out[4096];
  u8cs scheme, auth, path, query, frag;
  JABCArgSlice(scheme, ctx, argc > 0 ? args[0] : NULL, sb, sizeof(sb));
  JABCArgSlice(auth, ctx, argc > 1 ? args[1] : NULL, ab, sizeof(ab));
  JABCArgSlice(path, ctx, argc > 2 ? args[2] : NULL, pb, sizeof(pb));
  JABCArgSlice(query, ctx, argc > 3 ? args[3] : NULL, qb, sizeof(qb));
  JABCArgSlice(frag, ctx, argc > 4 ? args[4] : NULL, fb, sizeof(fb));
  u8s into = {out, out + sizeof(out)};
  u8* base = into[0];
  if (URIMake(into, scheme, auth, path, query, frag) != OK)
    JABC_THROW("uri.make: failed");
  u8cs res = {base, into[0]};
  return JABCSliceStr(ctx, res);
}

//  uri._esc(raw) -> percent-encoded ; uri._unesc(esc) -> decoded
static JABC_FN(JABCuriEsc) {
  if (argc < 1) JABC_THROW("uri.escape(string)");
  u8 in[2048], out[4096];
  u8cs raw;
  JABCArgSlice(raw, ctx, args[0], in, sizeof(in));
  u8s into = {out, out + sizeof(out)};
  u8* base = into[0];
  if (URIu8sEsc(into, raw) != OK) JABC_THROW("uri.escape: failed");
  u8cs res = {base, into[0]};
  return JABCSliceStr(ctx, res);
}
static JABC_FN(JABCuriUnesc) {
  if (argc < 1) JABC_THROW("uri.unescape(string)");
  u8 in[4096], out[4096];
  u8cs esc;
  JABCArgSlice(esc, ctx, args[0], in, sizeof(in));
  u8s into = {out, out + sizeof(out)};
  u8* base = into[0];
  if (URIu8sUnesc(into, esc) != OK) JABC_THROW("uri.unescape: failed");
  u8cs res = {base, into[0]};
  return JABCSliceStr(ctx, res);
}

//  The JS-facing URI class over the leaves above.
static const char* JABC_URI_JS = R"JS(
(function (g) {
  "use strict";
  const uri = g.uri;
  class URI {
    constructor(text) {
      this.href = String(text);
      const p = uri._parse(this.href);
      this.scheme = p.scheme;       this.authority = p.authority;
      this.user = p.user;           this.host = p.host;
      this.port = p.port;           this.path = p.path;
      this.query = p.query;         this.fragment = p.fragment;
    }
    static make(scheme, authority, path, query, fragment) {
      return uri._make(scheme || "", authority || "", path || "",
                       query || "", fragment || "");
    }
    static escape(s)   { return uri._esc(String(s)); }
    static unescape(s) { return uri._unesc(String(s)); }
    toString() { return this.href; }
  }
  uri.URI = URI;
  g.URI = URI;
})(this);
)JS";

ok64 JABCUriInstall() {
  JABC_API_OBJECT(uri);
  JABC_API_FN(uri, "_parse", JABCuriParse);
  JABC_API_FN(uri, "_make", JABCuriMake);
  JABC_API_FN(uri, "_esc", JABCuriEsc);
  JABC_API_FN(uri, "_unesc", JABCuriUnesc);
  JABCExecute(JABC_URI_JS);
  return OK;
}
