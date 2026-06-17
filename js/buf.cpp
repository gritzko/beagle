#include "JABC.hpp"

//  The Buf cursor class + the buffer constructors and the public fd<->buffer
//  helpers live in JS (the binding holds no memory): a Buf wraps a JS-owned
//  Uint8Array plus the PAST|DATA|IDLE boundaries, and its methods call the
//  native leaves (io._read/_write/_mmap/_ram, utf8.encodeInto) which only
//  touch the backing bytes.  Embedded as a raw string literal — no js2c/node
//  codegen, no escaping hazards.
static const char* JABC_BUF_JS = R"JS(
(function (g) {
  "use strict";
  const U8 = Uint8Array;

  //  Buf: a Uint8Array (the bytes, JS-owned) plus a cursor.
  //    PAST [0,_data)   already consumed (take/skip/write)
  //    DATA [_data,_idle) the live payload
  //    IDLE [_idle,cap)   free space (feed/read flow here)
  class Buf {
    constructor(bytes, data, idle) {
      this.bytes = bytes;
      this._data = data | 0;
      this._idle = (idle === undefined ? data : idle) | 0;
      this.cap = bytes.length;
    }
    static over(u8) { return new Buf(u8, 0, u8.length); } // bytes are DATA (read)
    static into(u8) { return new Buf(u8, 0, 0); }         // bytes are IDLE (fill)

    get size() { return this._idle - this._data; }   // live byte count
    get room() { return this.cap - this._idle; }      // free byte count
    get empty() { return this._idle === this._data; }

    data() { return this.bytes.subarray(this._data, this._idle); } // no-copy view
    idle() { return this.bytes.subarray(this._idle, this.cap); }
    past() { return this.bytes.subarray(0, this._data); }

    feed(src) {                                       // append (u8bFeed)
      const s = (src instanceof Buf) ? src.data() : src;
      if (s.length > this.room) throw "NOROOM";
      this.bytes.set(s, this._idle);
      this._idle += s.length;
      return s.length;
    }
    feed1(b) {                                        // append one byte
      if (this.room < 1) throw "NOROOM";
      this.bytes[this._idle++] = b & 0xff;
      return 1;
    }
    feedStr(str) {                                    // utf8-encode into IDLE
      const n = g.utf8.encodeInto(str, this.idle());
      this._idle += n;
      return n;
    }
    fed(n) {                                          // commit a manual idle write
      if (n < 0 || n > this.room) throw "BADARG";
      this._idle += n;
      return this;
    }

    take(n) {                                         // consume from DATA front (u8bUsed)
      if (n < 0 || n > this.size) throw "BADARG";
      const v = this.bytes.subarray(this._data, this._data + n);
      this._data += n;
      return v;
    }
    skip(n) {
      if (n < 0 || n > this.size) throw "BADARG";
      this._data += n;
      return this;
    }

    shed(n) {                                         // un-append (u8bShed)
      if (n < 0 || n > this.size) throw "BADARG";
      this._idle -= n;
      return this;
    }
    pop() {
      if (this.empty) throw "NODATA";
      return this.bytes[--this._idle];
    }
    reset() { this._data = 0; this._idle = 0; return this; }
    shift() {                                         // compact, drop PAST (u8bShift)
      if (this._data === 0) return this;
      this.bytes.copyWithin(0, this._data, this._idle);
      this._idle -= this._data;
      this._data = 0;
      return this;
    }
    splice(off, cut, paste) {                         // in-place edit within DATA
      const p = (paste instanceof Buf) ? paste.data() : (paste || new U8(0));
      if (off < 0 || cut < 0 || off + cut > this.size) throw "BADARG";
      const delta = p.length - cut;
      if (delta > this.room) throw "NOROOM";
      const d = this._data;
      this.bytes.copyWithin(d + off + p.length, d + off + cut, this._idle);
      this.bytes.set(p, d + off);
      this._idle += delta;
      return this;
    }

    grow(n) {                                         // enlarge capacity
      if (n <= this.cap) return this;
      if (this._grow) { this._grow(n); return this; } // mapped-backing hook
      const nb = new U8(n);
      nb.set(this.bytes.subarray(0, this._idle));
      this.bytes = nb;
      this.cap = n;
      return this;
    }
    msync() { g.io._msync(this.bytes); return this; } // flush a mapped backing
  }

  g.Buf = Buf;
  const io = g.io, utf8 = g.utf8;
  io.Buf = Buf;

  //  Constructors
  io.buf = (n) => Buf.into(new U8(n));               // engine ArrayBuffer
  io.ram = (n) => Buf.into(io._ram(n));              // anonymous mmap
  io.mmap = (path, mode, size) => {                  // file mapping
    mode = mode || "r";
    const u = io._mmap(path, mode, size || 0);
    return (mode === "c") ? Buf.into(u) : Buf.over(u);
  };
  io.book = () => {
    throw "io.book(): booked mmap not wired yet (use io.mmap + io.resize)";
  };

  //  text sugar over the native leaf
  utf8.Encode = (s) => {
    const b = new U8(s.length * 3 + 4);              // 3 bytes/UTF-16 unit upper bound
    return b.subarray(0, utf8.encodeInto(s, b));
  };

  //  fd <-> buffer.  A Buf advances its own cursor; a bare Uint8Array is
  //  single-shot (no cursor to advance).
  io.read = (fd, b) => {
    if (b instanceof Buf) { const n = io._read(fd, b.idle()); b._idle += n; return n; }
    return io._read(fd, b);
  };
  io.write = (fd, b) => {
    if (b instanceof Buf) { const n = io._write(fd, b.data()); b._data += n; return n; }
    return io._write(fd, b);
  };
  io.readAll = (fd, b) => {
    if (b instanceof Buf) { let t = 0, n; while (b.room > 0 && (n = io.read(fd, b)) > 0) t += n; return t; }
    let v = b, t = 0, n; while (v.length && (n = io._read(fd, v)) > 0) { t += n; v = v.subarray(n); } return t;
  };
  io.writeAll = (fd, b) => {
    if (b instanceof Buf) { let t = 0, n; while (b.size > 0) { n = io.write(fd, b); if (n <= 0) break; t += n; } return t; }
    let v = b, t = 0, n; while (v.length) { n = io._write(fd, v); if (n <= 0) break; t += n; v = v.subarray(n); } return t;
  };
  //  v1: readv/writev are loops, not a single readv(2)/writev(2) syscall.
  io.readv = (fd, list) => { let t = 0; for (const b of list) t += io.read(fd, b); return t; };
  io.writev = (fd, list) => { let t = 0; for (const b of list) t += io.writeAll(fd, b); return t; };

  io.stdin = 0; io.stdout = 1; io.stderr = 2;
})(this);
)JS";

ok64 JABCbufInstall() {
  JABCExecute(JABC_BUF_JS);
  return OK;
}
