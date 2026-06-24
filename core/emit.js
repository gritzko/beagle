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
const theme = require("view/theme.js");      // JAB-025: the static pluggable theme

//  A row sink.  `banner` is the ONE header line (`put put:`, `get ?#sha`);
//  `rows` is the collected `{uri, verb, ts, ...tag}` effect stream rendered
//  at the flush.  The optional row tag (e.g. `{pass}`) rides through so the
//  caller's sort comparator can use it (put move/dir-before-file).
//
//  JAB-025: `opts.color` (default false) selects the TTY colour path at the
//  flush.  When OFF (a pipe, `--plain`, or no opts) the render is the EXISTING
//  JS plain columniser below, byte-for-byte unchanged — that is the path the
//  SUT=loop parity harnesses redirect through, so it MUST stay byte-identical.
//  When ON (stdout is a tty, or `--color`) each columnar row is painted
//  PER-COLUMN by the static view/theme.js theme — date column in the date SGR,
//  the verb in its per-verb palette SGR, the path plain — over the SAME column
//  layout the plain `line()` produces (so the DATE COLUMN STAYS, including for
//  ts=0 rows; NO banner band).  The `raw` framing lines (status `status:`
//  header, `?…` summary) are lightly themed to match native: the header gets
//  the one pale-yellow banner band, the summary stays plain.  `opts.theme`
//  (optional) swaps the palette — the theme object is the single SGR source.
function create(opts) {
  const color = !!(opts && opts.color);
  const thm = (opts && opts.theme) || theme.DEFAULT;
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

  //  JAB-025 colour render: the SAME row stream and the SAME column layout as
  //  render_, but each columnar row is painted PER-COLUMN by the static theme
  //  (view/theme.js) instead of running through the C banner.  A row renders as
  //    <date-SGR><7-date><reset> <verb-SGR><3-verb><reset> <path>\n
  //  — identical bytes to the plain `line()` once the SGR is stripped, so the
  //  date column STAYS (7 blanks for a ts==0 row) and there is NO banner band.
  //  Single-sources the SGR through `thm` — this code never spells an escape.
  //  A `raw` framing line is lightly themed to match native `be status`: the
  //  `status:` header is wrapped in the one pale-yellow banner band (the only
  //  band native draws); any other raw line (the `?…` summary, relayed sub
  //  hunks) stays verbatim PLAIN.
  function lineColor(verb, text, ts) {
    const date = render.dateCol(ts);
    const dp = thm.paint(thm.dateSlot);
    const datePainted = dp ? dp + date + thm.reset(thm.dateSlot) : date;
    const vcol = render.verbCol(verb);
    const vp = thm.verbPaint(verb);
    const verbPainted = vp ? vp + vcol + thm.verbReset(verb) : vcol;
    return datePainted + " " + verbPainted + " " + text + "\n";
  }
  //  The header `status:` band: native pale-yellow background, space-filled by
  //  the C drawer to the terminal width.  We band just the header text (no
  //  width fill — the row content below it is what the band visually heads, and
  //  a width fill would need the tty cols; the SGR wrap is the load-bearing
  //  parity with native's banner colour).
  function bannerLine(text) {
    return thm.bannerOpen() + text + thm.bannerClose() + "\n";
  }
  function renderColor_(sort) {
    const ordered = sort ? sort(rows.slice()) : rows;
    let body = "";
    let seenHeader = false;                   // first raw `status:` → the band
    if (header) body += lineColor(header.verb, header.uri, header.ts);
    for (const r of ordered) {
      if (r.raw != null) {
        if (!seenHeader && r.raw.slice(0, 7) === "status:") {
          body += bannerLine(r.raw); seenHeader = true;
        } else body += r.raw + "\n";
      } else body += lineColor(r.verb, r.uri, r.ts);
    }
    return utf8.Encode(body);
  }

  //  Edge flush: render then write to stdout (fd 1) via render.writeStdout.
  //  JAB-025: the colour path only diverges here — the collected rows and the
  //  ONE edge write are unchanged; `color` just swaps the per-line formatter.
  function flush(sort) {
    const bytes = color ? renderColor_(sort) : render_(sort);
    render.writeStdout(utf8.Decode(bytes));
  }

  return { banner: banner, row: row, raw: raw, render: render_, flush: flush };
}

module.exports = { create: create };
