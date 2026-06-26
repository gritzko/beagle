//  bro.js (lib) — the JS port of bro's RENDER pipeline (JS-053 TODO#2).
//  Pure JS over the JABC bindings: io.mmap/stat/readdir, tok.parse (→tok32),
//  uri, tty.size.  Mirrors bro/BRO.c — buildHunk (a URI → one hunk: file
//  text+tok32 or a 'F'-tagged dir listing), indexRows (the row index +
//  codepoint soft-wrap, BROAppendLines), cellSGR (the tag→SGR painter,
//  bro_cell_ansi + dog/THEME), statusURI (the live status-bar URI), and
//  plain (the non-tty BROPlain sink, byte-exact with `bro --plain`).
//
//  This increment ships the NON-interactive `plain` sink (the only output
//  path); the indexRows/cellSGR scaffolding is the renderer the colour TUI
//  (TODO#3) builds on.  Syntax/file-view only — diff in/rm passes (the
//  bro_classify split) are out of scope (a JS diff command feeds them later),
//  so every token here is side EQ and the row index is one row per line.

"use strict";

//  tok32 bit layout (dog/tok/TOK.h, mirrored by tok.TokStream):
//    [31..27] tag (A+n)  [26] custom  [25..24] side  [23..0] end offset
//  token i's start = token i-1's end (0 for i==0).
const TOK_TAG = (w) => String.fromCharCode(65 + ((w >>> 27) & 0x1f));
const TOK_END = (w) => w & 0xffffff;

//  UTF8_LEN[b>>4]: bytes in the codepoint a lead byte starts (abc UTF8_LEN).
const UTF8_LEN = [1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 3, 4];

//  ---- THEME: tok32 tag → SGR (README D/G/L/H/R/P/S/N/C/F/U) ---------------
//  The default "16" terminal-adaptive palette (dog/THEME.h THEME_16).  A cell's
//  foreground SGR for the colour/TUI sink; the plain sink below never paints,
//  so this table is exercised only by the future colour path (TODO#3).
const THEME16 = {
  D: "90",       // comment   — gray
  G: "32",       // string    — green
  L: "36",       // number    — cyan
  H: "95",       // preproc   — pink
  R: "34",       // keyword   — blue
  P: "90",       // punct     — gray
  S: "",         // default   — none
  N: "1",        // defined   — bold
  C: "1",        // call      — bold
  F: "35",       // filename  — violet
  U: "",         // uri       — invisible (cell skipped)
};

//  cellSGR(tag) -> the SGR parameter string for a cell's foreground (no diff
//  pass/side here — syntax view only; in/rm wash is a diff concern, TODO/later).
//  '' = default (no SGR).  Mirrors bro_cell_ansi's THEMEAt(fg_tag) for PASS_NORMAL
//  + side EQ.
function cellSGR(tag) {
  const s = THEME16[tag];
  return s === undefined ? "" : s;
}

//  ---- hunk build ----------------------------------------------------------
//  A hunk: { uri, verb:"hunk", text:Uint8Array, toks:Uint32Array, kind }.
//  text/toks are the raw bytes + packed tok32 the renderer indexes & paints.

//  Strip a single trailing '/' for FS ops (stat/mmap/readdir take the bare
//  path); the banner keeps the arg verbatim, so we never mutate `arg`.
function fsPath(path) {
  return path.length > 1 && path.endsWith("/") ? path.slice(0, -1) : path;
}

//  Build a FILE hunk: mmap the bytes, tok.parse by extension (best-effort —
//  an unknown ext yields no toks, exactly like BROTokenize's KnownExt gate).
//  Mirrors BROExec's file branch + BROTokenize.
function buildFileHunk(arg, path) {
  const bytes = io.mmap(path, "r").data();
  const ext = pathExt(path);
  let toks;
  try { toks = ext ? tok.parse(bytes, ext) : new Uint32Array(0); }
  catch (e) { toks = new Uint32Array(0); }   // lex miss → no highlight, still cat
  return { uri: arg, verb: "hunk", text: bytes, toks: toks, kind: "file" };
}

//  Build a DIR hunk: one line per entry (basename, dirs get a trailing '/'),
//  tagged 'F' (filename) + 'P' for the slash, in FILEScanDir order.  Mirrors
//  BROListDir / listdir_emit (FILE_SCAN_ALL = include dotfiles).  An empty dir
//  yields NULL — BROListDir emits no hunk (no banner) for it.
function buildDirHunk(arg, path) {
  const entries = io.readdir(path, { hidden: true });
  if (entries.length === 0) return null;
  let text = "";
  const tagAt = [];                          // [{tag, end}] over the text bytes
  for (const e of entries) {
    const isDir = e.endsWith("/");
    const name = isDir ? e.slice(0, -1) : e;
    text += name;
    tagAt.push(["F", utf8.Encode(text).length]);
    if (isDir) { text += "/"; tagAt.push(["P", utf8.Encode(text).length]); }
    text += "\n";
    tagAt.push(["W", utf8.Encode(text).length]);
  }
  const bytes = utf8.Encode(text);
  const toks = new Uint32Array(tagAt.length);
  for (let i = 0; i < tagAt.length; i++) {
    const tagCode = tagAt[i][0].charCodeAt(0) - 65;
    toks[i] = ((tagCode & 0x1f) << 27) | (tagAt[i][1] & 0xffffff);
  }
  return { uri: arg, verb: "hunk", text: bytes, toks: toks, kind: "dir" };
}

//  ---- row index (BROAppendLines, NORMAL pass) -----------------------------
//  One row per logical line, codepoint soft-wrapped at `cols` (default 80 when
//  not a tty).  A row = { off, end } byte span over the hunk text (the '\n' is
//  the row terminator and excluded).  Diff in/rm passes are out of scope, so a
//  syntax hunk is a single NORMAL pass.  Mirrors bro_row_end_pass: 'U' (URI
//  click-target) bytes are hidden and don't count toward the column budget.

//  Codepoint end of one display row starting at byte `off` (BROAppendLines/
//  bro_row_end_pass for PASS_NORMAL): advance until a visible '\n' or `cols`
//  columns consumed; 'U'-tagged bytes are skipped (invisible, no column).
function rowEnd(hunk, off, cols) {
  const text = hunk.text, tlen = text.length, toks = hunk.toks;
  let ti = 0;
  while (ti < toks.length && (toks[ti] & 0xffffff) <= off) ti++;
  let cp = 0, pos = off;
  while (pos < tlen && cp < cols) {
    while (ti < toks.length && (toks[ti] & 0xffffff) <= pos) ti++;
    const tag = ti < toks.length ? TOK_TAG(toks[ti]) : "S";
    const ch = text[pos];
    const hidden = tag === "U";
    if (ch === 0x0a && !hidden) break;       // visible '\n' ends the row
    let clen = UTF8_LEN[ch >> 4];
    if (clen === 0 || pos + clen > tlen) clen = 1;
    pos += clen;
    if (!hidden) cp++;
  }
  return pos;
}

//  Walk one hunk's text into display rows (one per soft-wrap segment).
function indexRows(hunk, cols) {
  const rows = [];
  const tlen = hunk.text.length;
  let off = 0;
  while (off < tlen) {
    const end = rowEnd(hunk, off, cols);
    rows.push({ off: off, end: end });
    //  Next row starts past the terminating '\n' (rowEnd stops AT it), else at
    //  the wrap point.  Guard against a zero-width row (cols 0) stalling.
    const next = end < tlen && hunk.text[end] === 0x0a ? end + 1 : end;
    off = next > off ? next : off + 1;
  }
  return rows;
}

//  ---- status bar (BROStatusURI / BROStatusBar) ----------------------------
//  The live re-typeable URI of the current view position: `<path>#L<line>`.
//  A pathless hunk shows its URI verbatim.  Position: TOP / BOT / NN% / ALL.
//  Listing/query views keep the scheme (`ls:be/#L1`); file-content views
//  (cat/diff/blob) keep the bare `<path>#L<n>`.  KEEP IN LOCKSTEP with C
//  bro/BRO.c BRO_KEEP_SCHEME (BRO-008 parity).
const BRO_KEEP_SCHEME = new Set(
  ["ls", "lsr", "tree", "status", "grep", "regex", "spot", "log", "refs"]);
function statusURI(hunk, line) {
  const u = uri._parse(hunk.uri);
  if (!u.path) return hunk.uri;
  if (u.scheme && BRO_KEEP_SCHEME.has(u.scheme))
    return u.scheme + ":" + u.path + "#L" + line;
  return u.path + "#L" + line;
}

function statusPos(scroll, nrows, viewRows) {
  if (nrows <= viewRows) return "ALL";
  if (scroll === 0) return "TOP";
  if (scroll + viewRows >= nrows) return "BOT";
  return Math.floor((scroll * 100) / (nrows - viewRows)) + "%";
}

//  ---- plain sink (BROPlain, !BRO_COLOR branch) ----------------------------
//  The non-interactive `--plain` rendering, byte-exact with `bro --plain`:
//  per hunk emit the ONE banner header `hunk <uri>\n` (HUNKu8sFeedBanner plain:
//  no ts/verb-date here, just `[verb ]<uri>`) then the text verbatim, with a
//  trailing '\n' appended iff the text doesn't already end in one.  No tok
//  paint, no soft-wrap — that is exactly the C `!BRO_COLOR` path.
function plainHunk(hunk) {
  let head = "hunk " + hunk.uri + "\n";       // verb "hunk" + uri (banner)
  let out = utf8.Encode(head);
  const text = hunk.text;
  if (text.length === 0) return out;
  const needNL = text[text.length - 1] !== 0x0a;
  const buf = new Uint8Array(out.length + text.length + (needNL ? 1 : 0));
  buf.set(out, 0);
  buf.set(text, out.length);
  if (needNL) buf[buf.length - 1] = 0x0a;
  return buf;
}

//  ---- TABLE-record ingest + colour sink (JAB-003 over JAB-002) ------------
//  A TABLE hunk is the dog/HUNK 'H'-record stream the binding's feedRow writes:
//  one record per {uri, verb, ts} row, no text/toks.  The C renderer formats
//  each as `<date> <verb> <uri>` (an absent ts → 0 drops the date column,
//  leaving `<verb> <uri>`).  We single-source the dog/THEME SGR through the
//  binding's C `.plain` (mode 2) / `.color` (mode 1) — JS never re-rolls an SGR.

//  Build a HUNK log from {uri, verb, ts?} rows: one feedRow per row, ts absent
//  → 0n (the binding's banner then omits the date column).  Sized to the rows
//  (uri + verb + the TLV framing per record, well over-provisioned) so the feed
//  never overruns the ram log.  Returns the HUNK log object (a read cursor).
function buildTableHunk(rows) {
  let size = 256;                            // header + slack
  for (const r of rows) size += r.uri.length + 64;
  const log = abc.ram("HUNK", size);
  for (const r of rows) {
    const ts = r.ts === undefined || r.ts === null ? 0n : r.ts;
    log.feedRow(r.uri, r.verb, ts);
  }
  return log;
}

//  Render a TABLE hunk to bytes: walk every record from the start and render it
//  through the binding's C sink — plain (mode 2) when !color, colour (mode 1,
//  SGR-painted via the C THEME) when color — concatenating the per-record bytes.
//  The plain block is one `<verb> <uri>\n` line per row; colour is the same
//  content wrapped in SGR escapes (so it carries ESC 27 and differs in length).
function tableHunk(log, color) {
  log.rewind();
  const chunks = [];
  let total = 0;
  while (log.next()) {
    const out = io.buf(256);
    if (color) log.color(out); else log.plain(out);
    const bytes = Uint8Array.from(out.data());   // copy: the view is reused
    chunks.push(bytes);
    total += bytes.length;
  }
  const all = new Uint8Array(total);
  let off = 0;
  for (const c of chunks) { all.set(c, off); off += c.length; }
  return all;
}

//  ---- content-hunk render in an output mode (the SHARED sink) -------------
//  Render a CONTENT-hunk HUNK log (the binding's 'H' records: uri + text + the
//  packed tok32) in the selected output mode, SINGLE-SOURCED through the C
//  binding — NEVER a JS reimplementation of HUNKu8sFeedText / the SGR painter /
//  the TLV framing:
//    "color" → .color (mode 1, dog/THEME SGR)
//    "plain" → .plain (mode 2, HUNKu8sFeedText — byte-identical to native)
//    "tlv"   → the log's own raw 'H'-record bytes (the on-wire TLV stream)
//  Per-record render + concat (like tableHunk) so a buf never overruns; tlv is
//  the raw DATA [0, watermark).  This is the one place a view turns hunks into
//  bytes — grep/spot/cat/bro all funnel here.
function renderHunkLog(log, mode) {
  if (mode === "tlv") return log.subarray(0, log.buffer.watermark | 0).slice();
  log.rewind();
  const chunks = [];
  let total = 0;
  while (log.next()) {
    const tlen = log.text ? log.text.length : 0;
    //  COMMIT-003: an empty-URI hunk (commit:) makes the log.uri getter throw
    //  RangeError; guard it so a banner-less record still renders.
    let ulen = 0; try { ulen = log.uri.length; } catch (e) { ulen = 0; }
    const cap = (tlen + ulen + 64) * (mode === "color" ? 10 : 2) + 256;
    const o = io.buf(cap);
    if (mode === "color") log.color(o); else log.plain(o);
    const b = o.data().slice();
    chunks.push(b);
    total += b.length;
  }
  const all = new Uint8Array(total);
  let off = 0;
  for (const c of chunks) { all.set(c, off); off += c.length; }
  return all;
}

//  ---- path ext (PATHu8sExt) ----------------------------------------------
//  The extension after the last '.' in the basename, or "" (no dot, or a
//  dotfile whose only dot is leading).  Drives the tok.parse language.
function pathExt(path) {
  const base = path.slice(path.lastIndexOf("/") + 1);
  const dot = base.lastIndexOf(".");
  return dot > 0 ? base.slice(dot + 1) : "";
}

module.exports = {
  buildFileHunk: buildFileHunk,
  buildDirHunk: buildDirHunk,
  fsPath: fsPath,
  pathExt: pathExt,
  cellSGR: cellSGR,
  indexRows: indexRows,
  rowEnd: rowEnd,
  statusURI: statusURI,
  statusPos: statusPos,
  plainHunk: plainHunk,
  buildTableHunk: buildTableHunk,
  tableHunk: tableHunk,
  renderHunkLog: renderHunkLog,
  THEME16: THEME16,
};
