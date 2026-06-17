#include "JABC.hpp"
extern "C" {
#include "abc/PRO.h"  //  HASHx uses sane/done; provided by the .cpp, not the header (§6)
}
#include "hash.hpp"
#include "heap.hpp"

//  The container framework: per-(family,lane) prototypes whose verbs are bound
//  once to the native _heap_*/_hash_* leaves, plus the all-mmap constructors
//  (abc.ram / abc.mmap / abc.over).  A container IS a lane-typed view over a
//  JS-owned mapping; the cursor (watermark) rides on its ArrayBuffer; close()
//  drops the pin and lets GC munmap.  Embedded as a raw string literal.
static const char* JABC_CONT_JS = R"JS(
(function (g) {
  "use strict";
  const abc = g.abc, io = g.io;

  //  lane -> { typed-array ctor, JS elems per slot, pair?, bigint? }
  const LANE = {
    u8:    { A: Uint8Array,     w: 1, pair: false, big: false },
    u16:   { A: Uint16Array,    w: 1, pair: false, big: false },
    u32:   { A: Uint32Array,    w: 1, pair: false, big: false },
    u64:   { A: BigUint64Array, w: 1, pair: false, big: true  },
    kv32:  { A: Uint32Array,    w: 2, pair: true,  big: false },
    kv64:  { A: BigUint64Array, w: 2, pair: true,  big: true  },
    wh64:  { A: BigUint64Array, w: 1, pair: false, big: true  },
    wh128: { A: BigUint64Array, w: 2, pair: true,  big: true, set: true },
    sha1:   { A: Uint8Array, w: 20, pair: false, big: false, blob: true },
    sha256: { A: Uint8Array, w: 32, pair: false, big: false, blob: true },
  };
  const PROTO = {};
  const laneOf = (fam) => fam.slice(4);   // strip "HEAP"/"HASH" (4 chars)

  for (const lane in LANE) {
    const M = LANE[lane];
    //  HEAP: push/pop/peek/size, cursor on buffer.watermark
    {
      const push = abc["_heap_" + lane + "_push"];
      const pop  = abc["_heap_" + lane + "_pop"];
      const p = Object.create(M.A.prototype);
      p.push = M.pair
        ? function (k, v) { this.buffer.watermark = push(this, this.buffer.watermark, k, v); return this; }
        : function (v)    { this.buffer.watermark = push(this, this.buffer.watermark, v);    return this; };
      p.pop = function () {
        if (this.buffer.watermark === 0) return undefined;
        const r = pop(this, this.buffer.watermark);
        this.buffer.watermark--;
        return r;
      };
      p.peek = M.blob
        ? function () { return this.buffer.watermark ? this.subarray(0, M.w) : undefined; }
        : M.pair
        ? function () { return this.buffer.watermark ? [this[0], this[1]] : undefined; }
        : function () { return this.buffer.watermark ? this[0] : undefined; };
      Object.defineProperty(p, "size", { get() { return this.buffer.watermark; } });
      PROTO["HEAP" + lane] = p;
    }
    //  HASH: open-addressed table over the whole (zeroed, pow2) region
    {
      const put = abc["_hash_" + lane + "_put"];
      const get = abc["_hash_" + lane + "_get"];
      const del = abc["_hash_" + lane + "_del"];
      const p = Object.create(M.A.prototype);
      if (M.blob) {
        //  set of fixed-size byte blobs (sha1/sha256): key == the whole blob
        p.put = function (b) { put(this, b); return this; };
        p.get = function (b) { return get(this, b); };
        p.has = function (b) { return this.get(b) !== undefined; };
        p.del = function (b) { del(this, b); return this; };
      } else if (M.set) {
        //  set of (key,val) pairs (wh128): lookups supply both fields
        p.put = function (k, v) { put(this, k, v); return this; };
        p.get = function (k, v) { return get(this, k, v); };
        p.has = function (k, v) { return this.get(k, v) !== undefined; };
        p.del = function (k, v) { del(this, k, v); return this; };
      } else if (M.pair) {
        //  key -> val map (kv32/kv64)
        p.put = function (k, v) { put(this, k, v); return this; };
        p.get = function (k) { return get(this, k); };
        p.has = function (k) { return this.get(k) !== undefined; };
        p.del = function (k) { del(this, k); return this; };
      } else {
        //  set of scalars (get returns the value iff present)
        p.put = function (v) { put(this, v); return this; };
        p.get = function (v) { return get(this, v); };
        p.has = function (v) { return this.get(v) !== undefined; };
        p.del = function (v) { del(this, v); return this; };
      }
      PROTO["HASH" + lane] = p;
    }
  }

  function build(family, u8) {
    const M = LANE[laneOf(family)], proto = PROTO[family];
    if (!M || !proto) throw "abc: unknown family " + family;
    const v = new M.A(u8.buffer, u8.byteOffset, (u8.byteLength / M.A.BYTES_PER_ELEMENT) | 0);
    Object.setPrototypeOf(v, proto);   // real typed array + the family's verbs
    v.buffer._map = u8;                // pin the mapping (munmap on GC)
    v.buffer.watermark = 0;
    return v;
  }
  const bytes = (family, slots) => {
    const M = LANE[laneOf(family)];
    return slots * M.w * M.A.BYTES_PER_ELEMENT;
  };

  abc.ram  = (family, slots) => build(family, io._ram(bytes(family, slots)));
  abc.mmap = (family, path, mode, slots) => {
    mode = mode || "rw";
    return build(family, io._mmap(path, mode, mode === "c" ? bytes(family, slots || 0) : 0));
  };
  abc.over = (family, ta) => build(family,
    (ta instanceof Uint8Array) ? ta : new Uint8Array(ta.buffer, ta.byteOffset, ta.byteLength));
  abc.close = (c) => { try { io._msync(c); } catch (e) {} c.buffer._map = null; };
})(this);
)JS";

ok64 JABCContInstall() {
  JABC_API_OBJECT(abc);
  JABCHeapInstall(abc);
  JABCHashInstall(abc);
  JABCExecute(JABC_CONT_JS);
  return OK;
}
