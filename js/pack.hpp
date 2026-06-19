#ifndef JABC_PACK_HPP
#define JABC_PACK_HPP
//  PACK — an OFFSET-ADDRESSED git pack log (header + [obj-hdr][zlib] records)
//  in a u8 buffer.  Reads/writes are byte offsets only; sha addressing is the
//  index's job (a wh128 layer above), so this binding never takes a sha.
//
//  GIT-007: PURE marshalling over the dog/git pack-log core.  Every entry
//  resolves a typed array to a u8s (JABCBytesOf) and calls ONE dog/git PACK
//  function — zero delta/encode/inflate decisions live here.  The writer is
//  PACKu8sFeedObj (GIT-002), the reader PACKResolveOfs (GIT-004); both take
//  caller-provided scratch, so this binding never malloc()s.
//
//  write: _pack_header(buf,off,count)->end ; _pack_feed(buf,off,type,content,
//         base,baseOff,delta)->end  (base non-empty + baseOff>=0 → OFS_DELTA,
//         else raw — the dog/git writer decides)
//  read:  _pack_next(buf,off,dataLen)->objEnd|-1 ;
//         _pack_resolve(buf,recOff,base,delta) -> resolved bytes (Uint8Array) ;
//         _pack_type/_size/_baseoff/_ref/_inflate(buf,recOff,...)
#include "cont.hpp"

extern "C" {
#include "dog/git/DELT.h"
#include "dog/git/PACK.h"
}

//  _pack_header(buf, off, count) -> off+12
static JABC_FN(JABCpackHeader) {
  if (argc < 3) JABC_THROW("pack._header(buf, off, count)");
  u8s c = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  u32 count = (u32)JSValueToNumber(ctx, args[2], exception);
  u8s into = {c[0] + off, c[1]};
  if (PACKu8sFeedHdr(into, count) != OK) JABC_THROW("pack: header");
  return JSValueMakeNumber(ctx, (double)(size_t)(into[0] - c[0]));
}

//  _pack_count(buf) -> object count (from the 12-byte header)
static JABC_FN(JABCpackCount) {
  if (argc < 1) JABC_THROW("pack._count(buf)");
  u8s c = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  u8cs from = {c[0], c[1]};
  pack_hdr hdr = {};
  if (PACKDrainHdr(from, &hdr) != OK) JABC_THROW("pack: bad header");
  return JSValueMakeNumber(ctx, (double)hdr.count);
}

//  _pack_feed(buf, off, type, content, base, baseOff, delta) -> new write head
//  GIT-007: pure marshalling — resolve the typed arrays and hand the
//  raw|OFS_DELTA decision + emit to the dog/git writer PACKu8sFeedObj, which
//  checks every header/varint/deflate return (no ignored-return double-header).
static JABC_FN(JABCpackFeed) {
  if (argc < 4) JABC_THROW("pack._feed(buf, off, type, content, base, baseOff, delta)");
  u8s c = {}, content = {}, base = {}, delta = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  u8 type = (u8)JSValueToNumber(ctx, args[2], exception);
  if (!JABCBytesOf(content, ctx, args[3], exception)) return JSValueMakeUndefined(ctx);
  //  base (resolved bytes) + baseOff are optional: absent/empty → raw record.
  if (argc > 4) JABCBytesOf(base, ctx, args[4], exception);
  double bod = argc > 5 ? JSValueToNumber(ctx, args[5], exception) : -1;
  if (argc > 6) JABCBytesOf(delta, ctx, args[6], exception);

  u8* b[4] = {c[0], c[0] + off, c[0] + off, c[1]};  //  log: DATA ends at off
  u8csc bc = {base[0], base[1]};
  u8csc cc = {content[0], content[1]};
  u8* d[4] = {delta[0], delta[0], delta[0], delta[1]};  //  delta encode scratch
  u64 base_off = bod >= 0 ? (u64)bod : (u64)off;  //  off when no base (no delta)
  if (PACKu8sFeedObj((u8bp)b, type, cc, bc, (u64)off, base_off, (u8bp)d, NULL) != OK)
    JABC_THROW("pack: feed (full?)");
  return JSValueMakeNumber(ctx, (double)(size_t)(b[2] - c[0]));
}

//  Drain the object header at recOff (advancing nothing the caller sees).
static b8 JABCpackAt(pack_obj* obj, u8s c, JSContextRef ctx, JSValueRef bufv,
                     JSValueRef offv, JSValueRef* ex) {
  if (!JABCBytesOf(c, ctx, bufv, ex)) return NO;
  size_t rec = (size_t)JSValueToNumber(ctx, offv, ex);
  u8cs from = {c[0] + rec, c[1]};
  return PACKDrainObjHdr(from, obj) == OK;
}

//  _pack_next(buf, off, dataLen) -> end of the object at off | -1
//  GIT-007: a zlib stream isn't length-delimited, so ask the dog/git
//  resolver where the record ends — no JS-side re-inflate-to-measure.
static JABC_FN(JABCpackNext) {
  if (argc < 3) JABC_THROW("pack._next(buf, off, dataLen)");
  u8s c = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  size_t dl = (size_t)JSValueToNumber(ctx, args[2], exception);
  if (off >= dl) return JSValueMakeNumber(ctx, -1);
  u8cs pack = {c[0], c[0] + dl};
  u64 end = 0;
  if (PACKRecordEnd(pack, (u64)off, &end) != OK) return JSValueMakeNumber(ctx, -1);
  return JSValueMakeNumber(ctx, (double)(size_t)end);
}

static JABC_FN(JABCpackType) {
  u8s c = {};
  pack_obj o = {};
  if (argc < 2 || !JABCpackAt(&o, c, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  return JSValueMakeNumber(ctx, (double)o.type);
}
static JABC_FN(JABCpackSize) {
  u8s c = {};
  pack_obj o = {};
  if (argc < 2 || !JABCpackAt(&o, c, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  return JSValueMakeNumber(ctx, (double)o.size);
}
static JABC_FN(JABCpackBaseOff) {
  u8s c = {};
  pack_obj o = {};
  if (argc < 2 || !JABCpackAt(&o, c, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  if (o.type != PACK_OBJ_OFS_DELTA) return JSValueMakeNumber(ctx, -1);
  size_t rec = (size_t)JSValueToNumber(ctx, args[1], exception);
  return JSValueMakeNumber(ctx, (double)(rec - o.ofs_delta));
}
static JABC_FN(JABCpackRef) {
  u8s c = {};
  pack_obj o = {};
  if (argc < 2 || !JABCpackAt(&o, c, ctx, args[0], args[1], exception))
    return JSValueMakeUndefined(ctx);
  if (o.type != PACK_OBJ_REF_DELTA) return JSValueMakeUndefined(ctx);
  return JABCBlob(ctx, o.ref_delta[0], 20);
}

//  _pack_inflate(buf, recOff, out, outOff) -> bytes inflated (= obj.size)
//  Raw single-record read: drain the header, inflate the one zlib stream.
static JABC_FN(JABCpackInflate) {
  if (argc < 4) JABC_THROW("pack._inflate(buf, recOff, out, outOff)");
  u8s c = {};
  pack_obj o = {};
  if (!JABCpackAt(&o, c, ctx, args[0], args[1], exception))
    JABC_THROW("pack: bad record");
  size_t rec = (size_t)JSValueToNumber(ctx, args[1], exception);
  u8s out = {};
  if (!JABCBytesOf(out, ctx, args[2], exception)) return JSValueMakeUndefined(ctx);
  size_t oo = (size_t)JSValueToNumber(ctx, args[3], exception);
  u8cs from = {c[0] + rec, c[1]};
  pack_obj tmp = {};
  PACKDrainObjHdr(from, &tmp);
  u8s into = {out[0] + oo, out[1]};
  if (PACKInflate(from, into, tmp.size) != OK) JABC_THROW("pack: inflate (out full?)");
  return JSValueMakeNumber(ctx, (double)(size_t)tmp.size);
}

//  _pack_resolve(buf, recOff, base, delta) -> resolved object bytes
//  GIT-007: the WHOLE delta chase is the dog/git resolver PACKResolveOfs;
//  base/delta are caller-owned scratch, the result is copied out to a fresh
//  Uint8Array (the chase aliasing stays inside the scratch, never leaks).
static JABC_FN(JABCpackResolve) {
  if (argc < 4) JABC_THROW("pack._resolve(buf, recOff, base, delta)");
  u8s c = {}, base = {}, delta = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t rec = (size_t)JSValueToNumber(ctx, args[1], exception);
  if (!JABCBytesOf(base, ctx, args[2], exception)) return JSValueMakeUndefined(ctx);
  if (!JABCBytesOf(delta, ctx, args[3], exception)) return JSValueMakeUndefined(ctx);
  u8cs pack = {c[0], c[1]};
  u8cs out = {};
  u8 type = 0;
  if (PACKResolveOfs(pack, (u64)rec, base, delta, out, &type) != OK)
    JABC_THROW("pack: resolve");
  return JABCBlob(ctx, out[0], (size_t)u8csLen(out));
}

//  _delt_apply(base, delta, out, outOff) -> reconstructed bytes
static JABC_FN(JABCdeltApply) {
  if (argc < 4) JABC_THROW("delt._apply(base, delta, out, outOff)");
  u8s base = {}, delta = {}, out = {};
  if (!JABCBytesOf(base, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  if (!JABCBytesOf(delta, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);
  if (!JABCBytesOf(out, ctx, args[2], exception)) return JSValueMakeUndefined(ctx);
  size_t oo = (size_t)JSValueToNumber(ctx, args[3], exception);
  u8cs dl = {delta[0], delta[1]};
  u8cs bl = {base[0], base[1]};
  u8* op = out[0] + oo;
  u8* og[3] = {op, op, out[1]};
  if (DELTApply(dl, bl, og) != OK) JABC_THROW("delt: apply (out full?)");
  return JSValueMakeNumber(ctx, (double)(size_t)(og[1] - op));
}

static inline void JABCPackInstall(JSObjectRef o) {
  JABC_API_FN(o, "_pack_header", JABCpackHeader);
  JABC_API_FN(o, "_pack_count", JABCpackCount);
  JABC_API_FN(o, "_pack_feed", JABCpackFeed);
  JABC_API_FN(o, "_pack_next", JABCpackNext);
  JABC_API_FN(o, "_pack_type", JABCpackType);
  JABC_API_FN(o, "_pack_size", JABCpackSize);
  JABC_API_FN(o, "_pack_baseoff", JABCpackBaseOff);
  JABC_API_FN(o, "_pack_ref", JABCpackRef);
  JABC_API_FN(o, "_pack_inflate", JABCpackInflate);
  JABC_API_FN(o, "_pack_resolve", JABCpackResolve);
  JABC_API_FN(o, "_delt_apply", JABCdeltApply);
}

#endif
