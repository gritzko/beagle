//  shared/hunkrows.js — GENERIC columnar→HUNK adapter (JAB-003).  A HUNK-
//  collector with the SAME `raw`/`row` surface a columnar view (core/emit.js's
//  sink) uses, but it builds a content HUNK (text + tok32) and feeds ctx.sink —
//  retiring ctx.out for a view that wants a TRUE hunk (plain/color/tlv all flow
//  through core/loop.js's renderHunkLog edge).  Generalized from status.js's
//  BRO-006 `sinkOut`: the banner/hunk URI is PASSED IN (or scheme-detected) for
//  ANY scheme, not hardcoded "status:".
//
//  Usage (a small verb):
//    const hunkrows = require("../../shared/hunkrows.js");
//    const out = hunkrows(ctx.sink, "type:" + path);   // open the canonical uri
//    out.raw(word);                                     // a verbatim text line
//    out.done();                                        // flush the hunk
//  Usage (a multi-hunk view, status-style): pass no uri and let a scheme-
//  prefixed `raw(scheme + ":...")` OPEN each hunk (raw with no scheme = a text
//  line, "" = dropped separator), matching status.js's sinkOut byte-for-byte.
"use strict";

const render = require("../view/render.js");
const theme  = require("../view/theme.js");

function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }

//  Concatenate Uint8Array chunks into one buffer of length `total`.
function concatBytes(chunks, total) {
  const all = new Uint8Array(total);
  let o = 0;
  for (const c of chunks) { all.set(c, o); o += c.length; }
  return all;
}

//  hunkrows(sink, uri, scheme): the adapter.  `uri` (optional) pre-opens the
//  canonical hunk (a single-hunk view, e.g. `type:<path>`).  `scheme`
//  (optional, e.g. "status:") makes a `raw()` line that STARTS with it OPEN a
//  new hunk (a multi-hunk view); with no `scheme` a scheme-prefixed line is
//  just text.  Defaults to `uri`'s own `<scheme>:` when only `uri` is given.
module.exports = function hunkrows(sink, uri, scheme) {
  let cur  = (uri != null) ? uri : null;   // current hunk URI
  let parts = [];                          // Uint8Array text chunks
  let spans = [];                          // [tagLetter, byteEnd]
  let off   = 0;                           // running byte offset
  if (scheme == null && uri != null) {
    //  URI-013: read the scheme off the URI class instead of hand-slicing on ':'.
    //  NOTE `uri` is the STRING param (shadows the `uri` global), so parse via the
    //  `URI` class; guard the native throw to keep the old never-throw behavior.
    let p; try { p = new URI(uri); } catch (e) { p = null; }
    scheme = (p && p.scheme) ? p.scheme + ":" : null;
  }

  function feedText(bytes) { parts.push(bytes); off += bytes.length; }

  function flush() {
    if (cur === null) return;              // nothing opened yet
    const body = concatBytes(parts, off);
    const toks = new Uint32Array(spans.length);
    for (let i = 0; i < spans.length; i++) toks[i] = tok(tagCode(spans[i][0]), spans[i][1]);
    sink.feed(cur, body, toks, "", 0n);
    cur = null; parts = []; spans = []; off = 0;
  }

  return {
    //  DIS-060: open a new hunk with an EXPLICIT (possibly schemeless) URI, so a
    //  verb carries a ref-only banner URI with no `<verb>:` (the put: replacement).
    open: function (uri) { flush(); cur = uri; },
    //  A scheme-prefixed line → new hunk; "" separator → drop; else → a text
    //  line (default 'S' tag over the whole line).
    raw: function (text) {
      if (scheme && text.slice(0, scheme.length) === scheme) { flush(); cur = text; return; }
      if (text === "") return;
      const b = utf8.Encode(text + "\n");
      feedText(b);
      spans.push(["S", off]);
    },
    //  One columnar row `<date7> <verb3> <path>\n`; per-row hidden `U`-tag nav
    //  target (`nav`) appended after the "\n" (byte-hidden from the visible row).
    row: function (text, verb, ts, _tag, nav) {
      const date = render.dateCol(ts == null ? 0n : ts);
      const vcol = render.verbCol(verb);
      const line = date + " " + vcol + " " + text + "\n";
      const lineB = utf8.Encode(line);
      feedText(lineB);
      const eDate = off - lineB.length + utf8.Encode(date).length;
      const eSep1 = eDate + 1;
      const eVerb = eSep1 + utf8.Encode(vcol).length;
      const eSep2 = eVerb + 1;
      const eNL   = off;
      const vtag  = theme.VERB_SLOT[verb] || "S";
      spans.push(["L", eDate]);
      spans.push(["S", eSep1]);
      spans.push([vtag, eVerb]);
      spans.push(["S", eSep2]);
      spans.push(["S", eNL]);
      if (nav) { feedText(utf8.Encode(nav)); spans.push(["U", off]); }
    },
    done: flush,
  };
};
