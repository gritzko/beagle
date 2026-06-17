#ifndef JABC_ULOG_HPP
#define JABC_ULOG_HPP
//  ULOG — append-only (ts, verb, uri) text log, NO index.  The third u8
//  cursor-log family (sibling of HUNK/PACK), built on the stateless
//  ULOGu8sFeed/ULOGu8sDrain row codec.  feed moves the append watermark;
//  the seek* primitives are pure (offset -> offset), scanning the log
//  forward or backward without touching any watermark.  No header (rows
//  start at byte 0); reverse scans start from .end (the watermark).
//
//  hunk.hpp must precede this header (shared JABCArgU8).
#include "cont.hpp"

extern "C" {
#include "abc/RON.h"
//  dog/ULOG.h pulls abc/ANSI.h, which uses `private` as a struct field name
//  (valid C, a reserved word in C++).  Rename it across the include — we
//  never reference that field.
#define private private_
#include "dog/ULOG.h"
#undef private
}

//  ULOGu8sDrain leaves rec.uri.data empty (only the parsed components are
//  set), so reconstruct the URI text from the components via URIMake — the
//  inverse of what feed serialized.  Returns the byte length written to out.
static size_t JABCulogUriText(u8* out, size_t cap, ulogrec* rec) {
  u8s into = {out, out + cap};
  if (URIMake(into, rec->uri.scheme, rec->uri.authority, rec->uri.path,
              rec->uri.query, rec->uri.fragment) != OK)
    return 0;
  return (size_t)(into[0] - out);
}

//  Start offset of the row ending just before `off` (off is a row boundary).
static size_t JABCulogPrevRow(u8* base, size_t off) {
  if (off == 0) return (size_t)-1;
  size_t j = off - 1;                  //  the '\n' that ends the previous row
  while (j > 0 && base[j - 1] != '\n') j--;
  return j;                            //  start of that row (after the prior '\n', or 0)
}

//  _ulog_feed(buf, off, verb, uri, ts) -> new write head
static JABC_FN(JABCulogFeed) {
  if (argc < 4) JABC_THROW("ulog._feed(buf, off, verb, uri, ts)");
  u8s c = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  u8 vb[16], ub[FILE_PATH_MAX_LEN];
  u8s vs = {}, us = {};
  if (!JABCArgU8(vs, ctx, args[2], vb, sizeof(vb), exception)) return JSValueMakeUndefined(ctx);
  if (!JABCArgU8(us, ctx, args[3], ub, sizeof(ub), exception)) return JSValueMakeUndefined(ctx);
  ron60 verb = 0;
  u8cs vsc = {vs[0], vs[1]};
  RONutf8sDrain(&verb, vsc);
  u64 ts = (argc > 4 && !JSValueIsUndefined(ctx, args[4]))
               ? JSValueToUInt64(ctx, args[4], exception)
               : (u64)RONNow();
  ulogrec rec = {};
  rec.ts = (ron60)ts;
  rec.verb = verb;
  rec.uri.data[0] = us[0];
  rec.uri.data[1] = us[1];
  URILexer(&rec.uri);
  u8s into = {c[0] + off, c[1]};
  if (ULOGu8sFeed(into, &rec) != OK) JABC_THROW("ulog: feed (full?)");
  return JSValueMakeNumber(ctx, (double)(size_t)(into[0] - c[0]));
}

//  _ulog_now() -> BigInt ron60 (a fresh monotonic stamp)
static JABC_FN(JABCulogNow) {
  return JSBigIntCreateWithUInt64(ctx, (uint64_t)RONNow(), exception);
}

//  _ulog_next(buf, off, dataLen) -> end of the row at off | -1
static JABC_FN(JABCulogNext) {
  if (argc < 3) JABC_THROW("ulog._next(buf, off, dataLen)");
  u8s c = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  size_t dl = (size_t)JSValueToNumber(ctx, args[2], exception);
  if (off >= dl) return JSValueMakeNumber(ctx, -1);
  u8cs scan = {c[0] + off, c[0] + dl};
  ulogrec rec = {};
  if (ULOGu8sDrain(scan, &rec) != OK) return JSValueMakeNumber(ctx, -1);
  return JSValueMakeNumber(ctx, (double)(size_t)((u8c*)scan[0] - c[0]));
}

static b8 JABCulogAt(ulogrec* rec, u8s c, JSContextRef ctx, JSValueRef bufv,
                     JSValueRef offv, JSValueRef* ex) {
  if (!JABCBytesOf(c, ctx, bufv, ex)) return NO;
  size_t off = (size_t)JSValueToNumber(ctx, offv, ex);
  u8cs scan = {c[0] + off, c[1]};
  return ULOGu8sDrain(scan, rec) == OK;
}

static JABC_FN(JABCulogTime) {
  u8s c = {};
  ulogrec rec = {};
  if (argc < 2 || !JABCulogAt(&rec, c, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  return JSBigIntCreateWithUInt64(ctx, (uint64_t)rec.ts, exception);
}
static JABC_FN(JABCulogVerb) {
  u8s c = {};
  ulogrec rec = {};
  if (argc < 2 || !JABCulogAt(&rec, c, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  u8 vb[16];
  u8s into = {vb, vb + sizeof(vb)};
  RONutf8sFeed(into, rec.verb);
  size_t n = (size_t)(into[0] - vb);
  char tmp[17];
  if (n > 16) n = 16;
  memcpy(tmp, vb, n);
  tmp[n] = 0;
  JSStringRef js = JSStringCreateWithUTF8CString(tmp);
  JSValueRef v = JSValueMakeString(ctx, js);
  JSStringRelease(js);
  return v;
}
static JABC_FN(JABCulogUri) {
  u8s c = {};
  ulogrec rec = {};
  if (argc < 2 || !JABCulogAt(&rec, c, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  u8 ub[2048];
  size_t n = JABCulogUriText(ub, sizeof(ub), &rec);
  char tmp[2048];
  if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
  if (n) memcpy(tmp, ub, n);
  tmp[n] = 0;
  JSStringRef js = JSStringCreateWithUTF8CString(tmp);
  JSValueRef v = JSValueMakeString(ctx, js);
  JSStringRelease(js);
  return v;
}

//  Shared scan for the seek* leaves.  kind: 0=verb(==v), 1=time(>=t fwd /
//  <=t rev), 2=uri(prefix pfx[0..plen)).  Returns the match offset, or
//  SIZE_MAX.  Pure — touches no watermark.  Forward scans to dl; reverse
//  walks row boundaries down from `off`.
static size_t JABCulogScan(u8* base, size_t off, size_t dl, b8 rev, int kind,
                           ron60 v, u64 t, u8c* pfx, size_t plen) {
  size_t cur = off;
  if (!rev) {
    while (cur < dl) {
      u8cs scan = {base + cur, base + dl};
      ulogrec r = {};
      ok64 o = ULOGu8sDrain(scan, &r);
      size_t end = (size_t)((u8c*)scan[0] - base);
      b8 hit = NO;
      if (o == OK) {
        if (kind == 0) hit = (r.verb == v);
        else if (kind == 1) hit = ((u64)r.ts >= t);
        else {
          u8 ub[2048];
          size_t ulen = JABCulogUriText(ub, sizeof(ub), &r);
          hit = (ulen >= plen && memcmp(ub, pfx, plen) == 0);
        }
      }
      if (hit) return cur;
      if (end <= cur) break;
      cur = end;
    }
  } else {
    while (cur > 0) {
      size_t rs = JABCulogPrevRow(base, cur);
      if (rs == (size_t)-1 || rs >= cur) break;
      u8cs scan = {base + rs, base + dl};
      ulogrec r = {};
      b8 hit = NO;
      if (ULOGu8sDrain(scan, &r) == OK) {
        if (kind == 0) hit = (r.verb == v);
        else if (kind == 1) hit = ((u64)r.ts <= t);
        else {
          u8 ub[2048];
          size_t ulen = JABCulogUriText(ub, sizeof(ub), &r);
          hit = (ulen >= plen && memcmp(ub, pfx, plen) == 0);
        }
      }
      if (hit) return rs;
      cur = rs;
    }
  }
  return (size_t)-1;
}

//  Common prologue for the seek leaves: (buf, off, dataLen, arg, rev).
#define ULOG_SEEK_HEAD()                                                     \
  if (argc < 4) JABC_THROW("ulog._seek(buf, off, dataLen, arg, rev)");       \
  u8s c = {};                                                                \
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx); \
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);             \
  size_t dl = (size_t)JSValueToNumber(ctx, args[2], exception);              \
  b8 rev = argc > 4 && JSValueToBoolean(ctx, args[4])
#define ULOG_SEEK_RET(M) \
  do { size_t m_ = (M); return JSValueMakeNumber(ctx, m_ == (size_t)-1 ? -1 : (double)m_); } while (0)

static JABC_FN(JABCulogSeekVerb) {
  ULOG_SEEK_HEAD();
  u8 vb[16];
  u8s vs = {};
  if (!JABCArgU8(vs, ctx, args[3], vb, sizeof(vb), exception))
    return JSValueMakeUndefined(ctx);
  ron60 target = 0;
  u8cs vsc = {vs[0], vs[1]};
  RONutf8sDrain(&target, vsc);
  ULOG_SEEK_RET(JABCulogScan(c[0], off, dl, rev, 0, target, 0, NULL, 0));
}
static JABC_FN(JABCulogSeekTime) {
  ULOG_SEEK_HEAD();
  u64 target = JSValueToUInt64(ctx, args[3], exception);
  ULOG_SEEK_RET(JABCulogScan(c[0], off, dl, rev, 1, 0, target, NULL, 0));
}
static JABC_FN(JABCulogSeekURI) {
  ULOG_SEEK_HEAD();
  u8 pb[2048];
  u8s ps = {};
  if (!JABCArgU8(ps, ctx, args[3], pb, sizeof(pb), exception))
    return JSValueMakeUndefined(ctx);
  ULOG_SEEK_RET(JABCulogScan(c[0], off, dl, rev, 2, 0, 0, ps[0], (size_t)$len(ps)));
}

static inline void JABCUlogInstall(JSObjectRef o) {
  JABC_API_FN(o, "_ulog_feed", JABCulogFeed);
  JABC_API_FN(o, "_ulog_now", JABCulogNow);
  JABC_API_FN(o, "_ulog_next", JABCulogNext);
  JABC_API_FN(o, "_ulog_time", JABCulogTime);
  JABC_API_FN(o, "_ulog_verb", JABCulogVerb);
  JABC_API_FN(o, "_ulog_uri", JABCulogUri);
  JABC_API_FN(o, "_ulog_seekVerb", JABCulogSeekVerb);
  JABC_API_FN(o, "_ulog_seekTime", JABCulogSeekTime);
  JABC_API_FN(o, "_ulog_seekURI", JABCulogSeekURI);
}

#endif
