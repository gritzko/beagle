#include "JABC.hpp"
extern "C" {
#include "abc/PRO.h"  //  HASHx uses sane/done; provided by the .cpp, not the header (§6)
}
#include "hash.hpp"
#include "heap.hpp"
#include "hit.hpp"
#include "hunk.hpp"
#include "pack.hpp"
#include "ulog.hpp"

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
      p.lane = lane;                                   // type tag for sort/merge
      const sort = abc["_sort_" + lane];               // QSORTx leaf, by lane Z
      p.sort = function () { sort(this, this.buffer.watermark); return this; };
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

  //  HUNK: a u8-backed log of TLV 'H' records (not a lane).  The object IS the
  //  cursor — `next()` advances the read pos and the field getters reflect the
  //  current record (zero-copy views into the one buffer).
  {
    const EMPTY8 = new Uint8Array(0), EMPTY32 = new Uint32Array(0);
    const P = Object.create(Uint8Array.prototype);
    P.feed = function (uri, text, toks) {
      this.buffer.watermark = abc._hunk_feed(this, this.buffer.watermark,
        uri, text || EMPTY8, toks || EMPTY32);
      return this;
    };
    P.dogenize = function (src, ext, uri) {     // tokenize source -> append a hunk
      this.buffer.watermark = abc._hunk_dogenize(this, this.buffer.watermark,
        src, ext, uri || "");
      return this;
    };
    P.rewind = function () { this._read = 0; this._rec = -1; return this; };
    P.next = function () {
      const e = abc._hunk_next(this, this._read | 0, this.buffer.watermark | 0);
      if (e < 0) return false;
      this._rec = this._read | 0; this._read = e; return true;
    };
    Object.defineProperty(P, "uri",  { get() { return abc._hunk_uri(this, this._rec); } });
    Object.defineProperty(P, "text", { get() { return abc._hunk_text(this, this._rec); } });
    Object.defineProperty(P, "toks", { get() { return abc._hunk_toks(this, this._rec); } });
    Object.defineProperty(P, "verb", { get() { return abc._hunk_verb(this, this._rec); } });
    Object.defineProperty(P, "time", { get() { return abc._hunk_time(this, this._rec); } });
    //  `out` is an io.buf Buf (the buf.cpp cursor): render into its IDLE, then
    //  commit with fed().
    const render = (mode) => function (out) {
      out.fed(abc._hunk_render(this, this._rec, out.idle(), 0, mode));
      return out;
    };
    P.color = render(1);
    P.plain = render(2);
    P.html = render(3);
    PROTO["HUNK"] = P;
  }

  //  PACK: an offset-addressed git pack log (u8).  feed returns the object's
  //  byte offset; seek/next address by offset only (a sha throws); delta
  //  records are surfaced (baseOffset/ref), not resolved.
  {
    const T2N = { commit: 1, tree: 2, blob: 3, tag: 4, "ofs-delta": 6, "ref-delta": 7 };
    const N2T = { 1: "commit", 2: "tree", 3: "blob", 4: "tag", 6: "ofs-delta", 7: "ref-delta" };
    const EMPTY8 = new Uint8Array(0);
    const P = Object.create(Uint8Array.prototype);
    //  Lazily size caller-owned scratch (base + delta) off the log's own
    //  byte length — an upper bound for any single object/delta in it.  The
    //  dog/git feed/resolve own all format logic; the binding only marshals.
    P._scratch = function () {
      if (!this._bsc) {
        const n = this.byteLength;
        this._bsc = new Uint8Array(n);   // base scratch (resolved bytes)
        this._dsc = new Uint8Array(2 * n); // delta scratch (split internally)
      }
    };
    P.header = function () {
      this.buffer.watermark = abc._pack_header(this, 0, 0);
      this._count = 0; this._read = 12; this._rec = -1;
      return this;
    };
    P.feed = function (type, content, prevOff) {
      const off = this.buffer.watermark;
      const t = (typeof type === "number") ? type : T2N[type];
      //  Resolve the base to its full bytes by offset (the read path); the
      //  dog/git writer decides OFS_DELTA vs raw from base+content.
      let base = EMPTY8, bo = -1;
      if (prevOff != null && prevOff >= 0) {
        this._scratch();
        base = abc._pack_resolve(this, prevOff, this._bsc, this._dsc);
        bo = prevOff;
      }
      this._scratch();
      this.buffer.watermark = abc._pack_feed(this, off, t, content, base, bo, this._dsc);
      this._count = (this._count | 0) + 1;
      return off;
    };
    //  resolve(out): chase this record's delta chain to full bytes via the
    //  dog/git resolver, append them to the `out` Buf.
    P.resolve = function (out) {
      this._scratch();
      const b = abc._pack_resolve(this, this._rec, this._bsc, this._dsc);
      out.feed(b);
      return out;
    };
    P.finish = function () { abc._pack_header(this, 0, this._count | 0); return this; };
    P.rewind = function () { this._read = 12; this._rec = -1; return this; };
    P.next = function () {
      const e = abc._pack_next(this, this._read | 0, this.buffer.watermark | 0);
      if (e < 0) return false;
      this._rec = this._read | 0; this._read = e; return true;
    };
    P.seek = function (off) {
      if (typeof off !== "number") throw "PACK addresses by offset, not sha";
      const e = abc._pack_next(this, off, this.buffer.watermark | 0);
      if (e < 0) return false;
      this._rec = off; this._read = e; return true;
    };
    Object.defineProperty(P, "count", { get() { return abc._pack_count(this); } });
    Object.defineProperty(P, "offset", { get() { return this._rec; } });
    Object.defineProperty(P, "type", { get() { return N2T[abc._pack_type(this, this._rec)]; } });
    Object.defineProperty(P, "size", { get() { return abc._pack_size(this, this._rec); } });
    Object.defineProperty(P, "baseOffset", {
      get() { const b = abc._pack_baseoff(this, this._rec); return b < 0 ? undefined : b; }
    });
    Object.defineProperty(P, "ref", { get() { return abc._pack_ref(this, this._rec); } });
    P.inflate = function (out) {
      out.fed(abc._pack_inflate(this, this._rec, out.idle(), 0));
      return out;
    };
    PROTO["PACK"] = P;
  }

  //  ULOG: append-only (ts,verb,uri) text log, no index.  feed moves the
  //  watermark; seek* are pure offset->offset scans (fwd, or rev from .end).
  {
    const P = Object.create(Uint8Array.prototype);
    P.feed = function (verb, uri, ts) {
      let t = (ts == null) ? abc._ulog_now() : BigInt(ts);
      if (this._lastTs != null && t <= this._lastTs) t = this._lastTs + 1n;  // monotonic
      this._lastTs = t;
      const off = this.buffer.watermark;
      this.buffer.watermark = abc._ulog_feed(this, off, verb, uri, t);
      return off;
    };
    P.rewind = function () { this._read = 0; this._rec = -1; return this; };
    P.next = function () {
      const e = abc._ulog_next(this, this._read | 0, this.buffer.watermark | 0);
      if (e < 0) return false;
      this._rec = this._read | 0; this._read = e; return true;
    };
    P.seek = function (off) {
      if (typeof off !== "number") throw "ULOG addresses by offset";
      const e = abc._ulog_next(this, off, this.buffer.watermark | 0);
      if (e < 0) return false;
      this._rec = off; this._read = e; return true;
    };
    Object.defineProperty(P, "end", { get() { return this.buffer.watermark; } });
    Object.defineProperty(P, "after", { get() { return this._read; } });
    Object.defineProperty(P, "offset", { get() { return this._rec; } });
    Object.defineProperty(P, "time", { get() { return abc._ulog_time(this, this._rec); } });
    Object.defineProperty(P, "verb", { get() { return abc._ulog_verb(this, this._rec); } });
    Object.defineProperty(P, "uri", { get() { return abc._ulog_uri(this, this._rec); } });
    const mkseek = (leaf, rev) => function (off, arg) {
      const o = abc[leaf](this, off, this.buffer.watermark | 0, arg, rev);
      if (o >= 0) this.seek(o);            // position cursor on a hit
      return o;
    };
    P.seekVerb = mkseek("_ulog_seekVerb", false);
    P.seekVerbRev = mkseek("_ulog_seekVerb", true);
    P.seekTime = mkseek("_ulog_seekTime", false);
    P.seekTimeRev = mkseek("_ulog_seekTime", true);
    P.seekURI = mkseek("_ulog_seekURI", false);
    P.seekURIRev = mkseek("_ulog_seekURI", true);
    PROTO["ULOG"] = P;
  }

  //  delt.apply(base, delta, out): reconstruct a delta target into out (a Buf)
  g.delt = {
    apply: (base, delta, out) => {
      const n = abc._delt_apply(base, delta, out.idle(), 0);
      out.fed(n);
      return n;
    },
  };

  const isLog = (f) => f === "HUNK" || f === "PACK" || f === "ULOG";   // u8-backed cursor logs

  function build(family, u8) {
    const proto = PROTO[family];
    if (!proto) throw "abc: unknown family " + family;
    let v;
    if (isLog(family)) {                // u8-backed log: use the byte view directly
      v = u8;
      Object.setPrototypeOf(v, proto);
      v._read = (family === "PACK") ? 12 : 0;   // PACK skips the 12-byte header
      v._rec = -1;
    } else {
      const M = LANE[laneOf(family)];
      if (!M) throw "abc: unknown family " + family;
      v = new M.A(u8.buffer, u8.byteOffset, (u8.byteLength / M.A.BYTES_PER_ELEMENT) | 0);
      Object.setPrototypeOf(v, proto);   // real typed array + the family's verbs
    }
    v.buffer._map = u8;                // pin the mapping (munmap on GC)
    v.buffer.watermark = 0;
    return v;
  }
  const bytes = (family, slots) => {
    if (isLog(family)) return slots;              // u8 bytes
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

  //  book: a file-backed output sized to an upper bound (cheap — sparse), to
  //  be filled (e.g. by merge) and trimmed to the live size on close.
  abc.book = (family, path, slots) => {
    const c = build(family, io._mmap(path, "c", bytes(family, slots)));
    c.buffer._path = path;
    return c;
  };
  abc.close = (c) => {
    try { io._msync(c); } catch (e) {}
    const b = c.buffer;
    if (b._path) {                                  // booked: trim file to live size
      const M = LANE[c.lane];
      io._truncate(b._path, (c.size | 0) * M.w * M.A.BYTES_PER_ELEMENT);
    }
    b._map = null;
  };

  //  HIT bulk ops over SORTED runs.  Inputs are containers (each carries its
  //  .lane and .size); the lane is read off the operands (no type arg), all
  //  must agree.  Without `out` a fresh lane-typed run is returned; with `out`
  //  (a container sized to >= Sum of inputs) the result is written in place and
  //  out.size is set, so it can be trimmed on close (abc.book) and re-merged.
  const kway = (pfx, op, inputs, out) => {
    if (!inputs.length) throw "abc." + op + ": no inputs";
    const lane = inputs[0].lane;
    if (!lane) throw "abc." + op + ": inputs need a lane (use abc.ram/over)";
    const M = LANE[lane];
    const runs = inputs.map((c) => {
      if (c.lane !== lane) throw "abc." + op + ": lane mismatch";
      return c.subarray(0, (c.size | 0) * M.w);     // live region only
    });
    if (out) {
      if (out.lane !== lane) throw "abc." + op + ": out lane mismatch";
      out.buffer.watermark = abc[pfx + lane](runs, out);
      return out;
    }
    const u8 = abc[pfx + lane](runs);
    return new M.A(u8.buffer, u8.byteOffset, (u8.byteLength / M.A.BYTES_PER_ELEMENT) | 0);
  };
  abc.merge = (inputs, out) => kway("_merge_", "merge", inputs, out);
  abc.intersect = (inputs, out) => kway("_isect_", "intersect", inputs, out);
})(this);
)JS";

ok64 JABCContInstall() {
  JABC_API_OBJECT(abc);
  JABCHeapInstall(abc);
  JABCHashInstall(abc);
  JABCHitInstall(abc);
  JABCHunkInstall(abc);
  JABCPackInstall(abc);
  JABCUlogInstall(abc);
  JABCExecute(JABC_CONT_JS);
  return OK;
}
