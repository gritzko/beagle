//  conflict.js — the WEAVE conflict-marker triple scan (JS-051).  Pure JS,
//  no C, no dog.  A direct port of sniff/PATCH.c::SNIFFHasConflictMarker:
//  POST's pre-flight (POST-017) refuses a tracked `add` whose bytes carry a
//  complete merge-conflict triple, so a half-resolved WEAVE merge can't be
//  committed.  Both marker shapes WEAVEEmitMerged produces are matched:
//
//    inline:      `<<<<theirs||||ours>>>>`
//    line-block:  `<<<<\n…\n||||\n…\n>>>>\n`
//
//  Order is fixed: open `<<<<`, then ≥1 mid `||||`, then close `>>>>`.  A
//  bare `<<<<` in prose (e.g. wiki docs) is NOT a conflict.  A nested `<<<<`
//  before the close aborts the candidate and the scan resumes at the inner
//  open.  `--force` skips this scan (post.js), the documented escape for
//  marker-string false positives.

"use strict";

//  hasConflictMarker(bytes:Uint8Array) → bool.  `bytes` is the raw file
//  content (a blob's bytes, or a symlink target — POST only scans `add`
//  blobs).  Byte-for-byte the C run: a 4-byte run of one marker char.
function hasConflictMarker(bytes) {
  const e = bytes.length;
  function run4(i, ch) {
    return bytes[i] === ch && bytes[i + 1] === ch &&
           bytes[i + 2] === ch && bytes[i + 3] === ch;
  }
  const LT = 0x3c, BAR = 0x7c, GT = 0x3e;   // '<' '|' '>'
  let p = 0;
  while (p + 12 <= e) {              // need `<x4 |x4 >x4`
    if (!run4(p, LT)) { p++; continue; }
    //  Candidate open at p.  Scan forward for `||||` (≥1) then `>>>>`,
    //  with no intervening `<<<<` (nested) before the close.
    let q = p + 4;
    let sawMid = false, sawClose = false, nestedOpen = false;
    while (q + 4 <= e) {
      if (run4(q, LT)) { nestedOpen = true; break; }
      if (run4(q, GT)) { if (sawMid) sawClose = true; break; }
      if (run4(q, BAR)) { sawMid = true; q += 4; continue; }
      q++;
    }
    if (sawClose) return true;
    p = nestedOpen ? q : p + 4;     // resume at the inner open on nesting
  }
  return false;
}

module.exports = { hasConflictMarker: hasConflictMarker };
