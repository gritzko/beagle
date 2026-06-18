#ifndef JABC_PACK_HPP
#define JABC_PACK_HPP
//  PACK — an OFFSET-ADDRESSED git pack log (header + [obj-hdr][zlib] records)
//  in a u8 buffer.  Reads/writes are byte offsets only; sha addressing is the
//  index's job (a wh128 layer above), so this binding never takes a sha.
//  Records are surfaced raw — delta resolution composes from seek + DELT.apply.
//
//  write: _pack_header(buf,off,count)->end ; _pack_feed(buf,off,type,content,
//         prevOff)->end  (prevOff>=0 tries in-pack OFS_DELTA, else raw)
//  read:  _pack_next(buf,off,dataLen)->objEnd|-1 ;
//         _pack_type/_size/_baseoff/_ref/_inflate(buf,recOff,...)
//  delt:  _delt_apply(base, delta, out, outOff) -> bytes written
#include "cont.hpp"

extern "C" {
#include "dog/git/DELT.h"
#include "dog/git/PACK.h"
#include "dog/git/ZINF.h"
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

//  _pack_feed(buf, off, type, content, prevOff) -> new write head
static JABC_FN(JABCpackFeed) {
  if (argc < 4) JABC_THROW("pack._feed(buf, off, type, content, prevOff)");
  u8s c = {}, content = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  u8 type = (u8)JSValueToNumber(ctx, args[2], exception);
  if (!JABCBytesOf(content, ctx, args[3], exception)) return JSValueMakeUndefined(ctx);
  double prevd = argc > 4 ? JSValueToNumber(ctx, args[4], exception) : -1;
  u8* base = c[0];
  u8* term = c[1];
  u8* b[4] = {base, base + off, base + off, term};  //  idle starts at off
  b8 emitted = NO;

  if (prevd >= 0) {  //  try an in-pack OFS_DELTA against the object at prevOff
    size_t prevOff = (size_t)prevd;
    u8cs bf = {base + prevOff, term};
    pack_obj bo = {};
    if (PACKDrainObjHdr(bf, &bo) == OK && bo.type >= 1 && bo.type <= 4) {
      u8* bb = (u8*)malloc(bo.size ? bo.size : 1);
      u8s binto = {bb, bb + bo.size};
      u8cs bz = {bf[0], term};
      if (ZINFInflate(binto, bz) == OK) {
        size_t clen = (size_t)$len(content);
        u8* db = (u8*)malloc(clen + 64);
        u8* dbuf[4] = {db, db, db, db + clen + 64};
        u8csc basec = {bb, bb + bo.size};
        u8csc tgt = {content[0], content[1]};
        //  DELTEncode returns DELTFAIL when the delta isn't smaller than the
        //  target — fall back to a raw record in that case.
        if (DELTEncode(basec, tgt, (u8bp)dbuf) == OK) {
          size_t dlen = (size_t)(dbuf[2] - dbuf[1]);
          if (dlen < clen) {
            PACKu8sFeedObjHdr((u8bp)b, PACK_OBJ_OFS_DELTA, dlen);
            PACKu8sFeedOfs((u8bp)b, (u64)(off - prevOff));
            u8cs dz = {dbuf[1], dbuf[2]};
            if (ZINFDeflate(u8bIdle(b), dz) == OK) emitted = YES;
          }
        }
        free(db);
      }
      free(bb);
    }
  }
  if (!emitted) {
    if (PACKu8sFeedObjHdr((u8bp)b, type, (u64)$len(content)) != OK)
      JABC_THROW("pack: header (full?)");
    u8cs cz = {content[0], content[1]};
    if (ZINFDeflate(u8bIdle(b), cz) != OK) JABC_THROW("pack: deflate (full?)");
  }
  return JSValueMakeNumber(ctx, (double)(size_t)(b[2] - base));
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
static JABC_FN(JABCpackNext) {
  if (argc < 3) JABC_THROW("pack._next(buf, off, dataLen)");
  u8s c = {};
  if (!JABCBytesOf(c, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  size_t off = (size_t)JSValueToNumber(ctx, args[1], exception);
  size_t dl = (size_t)JSValueToNumber(ctx, args[2], exception);
  if (off >= dl) return JSValueMakeNumber(ctx, -1);
  u8cs from = {c[0] + off, c[0] + dl};
  pack_obj obj = {};
  if (PACKDrainObjHdr(from, &obj) != OK) return JSValueMakeNumber(ctx, -1);
  //  zlib streams aren't self-delimiting by length: inflate (into scratch)
  //  to learn how many compressed bytes the object consumes.
  u8* sc = (u8*)malloc(obj.size ? obj.size : 1);
  u8s sin = {sc, sc + obj.size};
  u8cs z = {from[0], c[0] + dl};
  ok64 o = ZINFInflate(sin, z);
  size_t end = (size_t)((u8c*)z[0] - c[0]);
  free(sc);
  if (o != OK) return JSValueMakeNumber(ctx, -1);
  return JSValueMakeNumber(ctx, (double)end);
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
  //  re-drain to reach the zlib start
  u8cs from = {c[0] + rec, c[1]};
  pack_obj tmp = {};
  PACKDrainObjHdr(from, &tmp);
  u8s into = {out[0] + oo, out[1]};
  u8* obase = into[0];
  u8cs z = {from[0], c[1]};
  if (ZINFInflate(into, z) != OK) JABC_THROW("pack: inflate (out full?)");
  return JSValueMakeNumber(ctx, (double)(size_t)(into[0] - obase));
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
  JABC_API_FN(o, "_delt_apply", JABCdeltApply);
}

#endif
