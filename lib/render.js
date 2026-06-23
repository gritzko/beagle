//  render.js — HUNK-table render + shell-quote helpers shared by bin/*.js
//  (JS-043).  Pure JS over the JABC `ron`/`utf8`/`io` globals.  Consolidates
//  the date/verb column formatters and the stdout writer (status.js & get.js)
//  plus the POSIX single-quote helper (status.js's `shQuote` ≡ wire.js's
//  `shq`).
//
//    dateCol(ts)    7-col centred date: ron.date for a real ts, 7 spaces for
//                   ts==0 (matches htbl_emit's empty-ts branch, NOT
//                   ron.date's `   ?   ` placeholder).
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

function writeStdout(str) {
  const bytes = utf8.Encode(str);
  const b = io.buf(bytes.length + 8);
  b.feed(bytes);
  io.writeAll(1, b);
}

function shQuote(s) { return "'" + String(s).split("'").join("'\\''") + "'"; }

module.exports = { dateCol: dateCol, verbCol: verbCol,
                   writeStdout: writeStdout, shQuote: shQuote };
