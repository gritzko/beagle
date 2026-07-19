//  render.js — HUNK-table render + shell-quote helpers shared by bin/*.js
//  (JS-043).  Pure JS over the JABC `ron`/`utf8`/`io` globals.  Consolidates
//  the date/verb column formatters and the stdout writer (status.js & get.js)
//  plus the POSIX single-quote helper (status.js's `shQuote` ≡ wire.js's
//  `shq`).
//
//    dateCol(ts)    7-col centred date: ron.date for a real ts, 7 spaces for
//                   ts==0 (matches htbl_emit's empty-ts branch, NOT
//                   ron.date's `   ?   ` placeholder).
//    relAge(ts,now) LIST-001: short relative age (`3h`/`2d`/`1y`) of a ron60
//                   ts vs a ron60 now; "" for ts==0 (unattributed/blank).
//    verbCol(v)     3-col left-justified verb.
//    writeStdout(s) write a JS string to stdout (fd 1) via io.write over a Buf.
//    shQuote(s)     single-quote a path for POSIX sh (wrap, escaping quotes).

"use strict";

function dateCol(ts) {
  if (!ts || ts === 0n) return "       ";   // 7 spaces
  return ron.date(typeof ts === "bigint" ? ts : BigInt(ts));
}

function verbCol(v) {
  return v.length >= 3 ? v : v + "   ".slice(v.length);
}

//  LIST-001: decode a ron60 BigInt to absolute ms (ulog.ronToMs twin — the SAME
//  6-bit calendar layout ron.date reads).  The local-tz offset is CONSTANT so it
//  cancels in a delta; relAge only ever subtracts two decoded values.
function ronToMs(r) {
  r = BigInt(r);
  const d = (k) => Number((r >> BigInt(k * 6)) & 63n);
  const yy = d(9) * 10 + d(8), mon = d(7), day = d(6) * 10 + d(5);
  const hh = d(4), mm = d(3), ss = d(2), ms = d(1) * 64 + d(0);
  return Date.UTC(2000 + yy, mon - 1, day, hh, mm, ss, ms);
}

//  LIST-001: short relative age of `ts` vs `now` (both ron60) — the coarsest
//  single unit: `Ns`/`Nm`/`Nh`/`Nd`/`Ny` (sibling of dateCol).  ts==0 (or a
//  future/degenerate delta) → "".  Boundaries: 60s→1m, 60m→1h, 24h→1d, 365d→1y.
function relAge(ts, now) {
  if (!ts || ts === 0n) return "";
  let sec;
  try { sec = Math.floor((ronToMs(now) - ronToMs(ts)) / 1000); }
  catch (e) { return ""; }
  if (sec < 0) sec = 0;
  if (sec < 60)       return sec + "s";
  if (sec < 3600)     return ((sec / 60) | 0) + "m";
  if (sec < 86400)    return ((sec / 3600) | 0) + "h";
  if (sec < 31536000) return ((sec / 86400) | 0) + "d";
  return ((sec / 31536000) | 0) + "y";
}

function writeStdout(str) {
  const bytes = utf8.Encode(str);
  const b = io.buf(bytes.length + 8);
  b.feed(bytes);
  io.writeAll(1, b);
}

function shQuote(s) { return "'" + String(s).split("'").join("'\\''") + "'"; }

module.exports = { dateCol: dateCol, verbCol: verbCol, relAge: relAge,
                   ronToMs: ronToMs,
                   writeStdout: writeStdout, shQuote: shQuote };
