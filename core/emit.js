//  core/emit.js — output-as-ULog row sink (JSQUE-005).  Handlers push one
//  `{uri, verb, ts}` row per effect via `out.row(uri, verb, ts)`; ONE edge
//  `flush`/`render` drains the collected rows into the banner bytes.  This
//  replaces the per-verb emitBanner string-building (put.js/get.js) with a
//  single sink — output becomes the fourth ULog role.
//
//  BYTE-PARITY (the [jobqueue] HUNK blocker): each line is built with
//  render.js's dateCol/verbCol exactly as the native banners are — a ts==0
//  row gets the 7-space blank-date column, NOT ron.date(0n)'s `   ?   `
//  placeholder, and NEVER the C per-record HUNKu8sFeedBanner (which drops
//  that column).  Sorting is collect-and-sort AT THE FLUSH, never live.
"use strict";

const render = require("view/render.js");   // JSQUE-016: lib/ -> view/

//  A row sink.  `banner` is the ONE header line (`put put:`, `get ?#sha`);
//  `rows` is the collected `{uri, verb, ts, ...tag}` effect stream rendered
//  at the flush.  The optional row tag (e.g. `{pass}`) rides through so the
//  caller's sort comparator can use it (put move/dir-before-file).
function create() {
  let header = null;                        // { verb, uri, ts } or null
  const rows = [];

  function banner(verb, uri, ts) { header = { verb: verb, uri: uri, ts: ts }; }

  //  Push one effect row.  `ts` 0n → blank-date column (put leaves);
  //  a real ts → dated column (get/status leaves).  `tag` is merged in.
  function row(uri, verb, ts, tag) {
    const r = { uri: uri, verb: verb, ts: ts == null ? 0n : ts };
    if (tag) for (const k in tag) r[k] = tag[k];
    rows.push(r);
  }

  //  JSQUE-008: push a PRE-FORMATTED line verbatim into the row stream — for
  //  framing the columnar `row()`s can't model (status's `status:` banner,
  //  its `?<branch>\t<counts>` summary, relayed sub hunks).  `raw` carries the
  //  exact bytes (sans trailing "\n"); render emits it as-is, never columnised.
  function raw(text) { rows.push({ raw: text }); }

  //  Render ONE line the way every native banner does: dateCol + " " +
  //  verbCol + " " + text.  `text` is the uri/path column.
  function line(verb, text, ts) {
    return render.dateCol(ts) + " " + render.verbCol(verb) + " " + text + "\n";
  }

  //  Drain to bytes.  `sort` (optional) is applied to the row list at the
  //  flush — get: new+upd lex then del lex; put: move/dir before file.
  function render_(sort) {
    let body = "";
    if (header) body += line(header.verb, header.uri, header.ts);
    const ordered = sort ? sort(rows.slice()) : rows;
    //  JSQUE-008: a `raw` row is verbatim (its own framing); else columnise.
    for (const r of ordered)
      body += r.raw != null ? r.raw + "\n" : line(r.verb, r.uri, r.ts);
    return utf8.Encode(body);
  }

  //  Edge flush: render then write to stdout (fd 1) via render.writeStdout.
  function flush(sort) { render.writeStdout(utf8.Decode(render_(sort))); }

  return { banner: banner, row: row, raw: raw, render: render_, flush: flush };
}

module.exports = { create: create };
