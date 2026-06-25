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
//  ts=0 rows; NO per-row banner band).  HEADER lines match native bare `be`:
//  every `status:`/`status:<sub>` hunk header AND the `out.banner` verb banner
//  (put/get) get the one pale-yellow band (text-only); the `?…` summary stays
//  plain.  `opts.theme` (optional) swaps the palette — the single SGR source.
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
  //  A `raw` framing line is themed to match native bare `be` (the recursing
  //  relay the JAB-024 loop mirrors): EVERY hunk header — the parent `status:`
  //  AND each relayed `status:<subpath>` sub-header — is wrapped in the one
  //  pale-yellow banner band (text-only, no width fill — see bannerLine).  The
  //  `out.banner` verb banner (put/get) is banded too.  Any OTHER raw line (the
  //  `?<branch>\t<counts>` summary, which native paints per-segment) stays
  //  verbatim PLAIN — out of scope here.
  function lineColor(verb, text, ts) {
    const date = render.dateCol(ts);
    const dp = thm.paint(thm.dateSlot);
    const datePainted = dp ? dp + date + thm.reset(thm.dateSlot) : date;
    const vcol = render.verbCol(verb);
    const vp = thm.verbPaint(verb);
    const verbPainted = vp ? vp + vcol + thm.verbReset(verb) : vcol;
    return datePainted + " " + verbPainted + " " + text + "\n";
  }
  //  The header band: native pale-yellow background wrapping the header TEXT,
  //  NO width fill.  VERIFIED against the binary (`be --color` / `be put|get
  //  --color | cat -A`): the loop's `jab status` recurses (JAB-024), so its
  //  colour oracle is bare `be` — the relay/BEDefault path (htbl_stream_banner
  //  / BERelaySub) — NOT the flat `be status` verb.  That relay path, and the
  //  put/get verb banner, band the header TEXT ONLY (` 01:43  put put:` →
  //  `^[[…230m 01:43  put put:^[[0m`, visible len 16; `status:` → len 7;
  //  `status:html` → len 11; …).  ONLY the standalone flat `be status` verb
  //  space-fills its band to width 200 (HUNKu8sFeedBanner.s_h1) — a path the
  //  recursing loop never takes, so we do NOT width-fill here (that would make
  //  `jab status --color` DIFFER from its bare-`be` oracle).  Closes with
  //  ESC[0m (bannerClose) — the band set a bg, so a default-fg 39 won't clear.
  function bannerLine(text) {
    return thm.bannerOpen() + text + thm.bannerClose() + "\n";
  }
  //  The verb-banner header (`out.banner`: `put put:` / `get ?#sha`) bands the
  //  PLAIN columnar line — date + verb + text in the band, NO per-column SGR
  //  inside (native draws the whole banner line in the one band, not painted
  //  per cell).  We reuse the plain `line()` form (sans its trailing "\n") so
  //  the banded text byte-matches native's ` <date>  <verb> <text>`.
  function bannerColumnar(verb, text, ts) {
    const plain = line(verb, text, ts);                 // ends with "\n"
    return bannerLine(plain.slice(0, -1));
  }
  //  A raw framing line is a HUNK HEADER iff it starts with `status:` — the
  //  parent `status:` AND every recursive `status:<subpath>` sub-header (the
  //  JAB-024 in-process relay emits one per mounted sub).  Native bands EVERY
  //  one of them (bare `be --color`), so we band them all — dropping the old
  //  `seenHeader` first-only gate.  Any OTHER raw line (the `?<branch>\t<counts>`
  //  summary, which native paints per-segment — out of scope) stays verbatim.
  function isHunkHeader(raw) { return raw.slice(0, 7) === "status:"; }
  function renderColor_(sort) {
    const ordered = sort ? sort(rows.slice()) : rows;
    let body = "";
    //  The `out.banner` verb-banner header is banded full-line (put/get).
    if (header) body += bannerColumnar(header.verb, header.uri, header.ts);
    for (const r of ordered) {
      if (r.raw != null) {
        body += isHunkHeader(r.raw) ? bannerLine(r.raw) : r.raw + "\n";
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

  return { banner: banner, row: row, raw: raw, render: render_,
           renderColor: renderColor_, flush: flush };
}

module.exports = { create: create };
