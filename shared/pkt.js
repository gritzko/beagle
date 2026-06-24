//  pkt.js — git pkt-line framing (protocol v0/v1), pure JS (JS-039).  A
//  pkt-line is `<4 lowercase-hex length><payload>`; the length COUNTS the
//  4 length bytes.  Specials: `0000` flush, `0001` delim, `0002` resp-end.
//  Mirrors `dog/git/PKT.c` (PKTu8sDrain/Feed/FeedFlush) — trivial byte math,
//  no native leaf (like status.js's text parsing).  No C, no dog.
//
//  Exports:
//    frame(payload)      -> Uint8Array   one framed pkt-line
//    flushPkt()          -> Uint8Array   the 4-byte `0000`
//    Reader(fd)          -> pkt-line pull cursor over a blocking fd
//      .next()           -> { kind, payload }  kind: "line"|"flush"|"delim"|"eof"
//      .rest()           -> Uint8Array   bytes already read past the last pkt
//                          (the raw pack tail begins here)

"use strict";

const FLUSH = "flush", DELIM = "delim", LINE = "line", EOF = "eof";

//  4 lowercase-hex of a u16, the pkt length prefix.
function hex4(n) {
  let s = (n & 0xffff).toString(16);
  while (s.length < 4) s = "0" + s;
  return s;
}

//  frame(payload): payload is a Uint8Array | string → one framed pkt-line.
function frame(payload) {
  const body = (typeof payload === "string") ? utf8.Encode(payload) : payload;
  const total = body.length + 4;
  if (total > 0xffff) throw "pkt.frame: payload too big (" + body.length + ")";
  const hdr = utf8.Encode(hex4(total));
  const out = new Uint8Array(total);
  out.set(hdr, 0);
  out.set(body, 4);
  return out;
}

function flushPkt() { return utf8.Encode("0000"); }

//  Parse 4 hex bytes at u8[off..off+4) → length, or -1 on a non-hex byte.
function readLen(u8, off) {
  let v = 0;
  for (let i = 0; i < 4; i++) {
    const c = u8[off + i];
    let d;
    if (c >= 0x30 && c <= 0x39) d = c - 0x30;            // 0-9
    else if (c >= 0x61 && c <= 0x66) d = c - 0x61 + 10;  // a-f
    else if (c >= 0x41 && c <= 0x46) d = c - 0x41 + 10;  // A-F
    else return -1;
    v = (v << 4) | d;
  }
  return v;
}

//  Reader(fd): a pull cursor draining pkt-lines from a blocking fd.  Owns a
//  growable byte buffer with a [pos,len) window; refills via io._read on
//  demand.  `next()` yields one event; at real EOF returns {kind:"eof"}.
function Reader(fd) {
  let buf = new Uint8Array(1 << 16);
  let pos = 0, len = 0, eofd = false;

  //  Ensure at least `need` bytes live in [pos,len); read more if short.
  //  Returns NO at EOF before `need` bytes arrive.
  function ensure(need) {
    while (len - pos < need) {
      if (eofd) return false;
      if (len === buf.length) {
        //  Compact consumed prefix, else grow.
        if (pos > 0) { buf.copyWithin(0, pos, len); len -= pos; pos = 0; }
        if (len === buf.length) {
          const bigger = new Uint8Array(buf.length * 2);
          bigger.set(buf.subarray(0, len)); buf = bigger;
        }
      }
      const n = io._read(fd, buf.subarray(len));
      if (n <= 0) { eofd = true; return len - pos >= need; }
      len += n;
    }
    return true;
  }

  return {
    next() {
      if (!ensure(4)) return { kind: EOF };
      const total = readLen(buf, pos);
      if (total < 0) throw "pkt: bad length hex at " + pos;
      if (total === 0) { pos += 4; return { kind: FLUSH }; }
      if (total === 1) { pos += 4; return { kind: DELIM }; }
      if (total === 2) { pos += 4; return { kind: "respend" }; }
      if (total < 4) throw "pkt: short length " + total;
      if (!ensure(total)) throw "pkt: truncated pkt-line (want " + total +
                                ", have " + (len - pos) + ")";
      const payload = buf.slice(pos + 4, pos + total);
      pos += total;
      return { kind: LINE, payload };
    },
    //  Bytes already buffered past the last consumed pkt-line — the start
    //  of any raw (non-pkt) stream that follows (e.g. the packfile).
    rest() { return buf.slice(pos, len); },
    eof() { return eofd && pos >= len; }
  };
}

module.exports = { frame, flushPkt, Reader, hex4, readLen,
                   FLUSH, DELIM, LINE, EOF };
