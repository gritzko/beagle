//  views/bro/pager.js — JAB-028: bro's interactive raw-mode hunk pager.
//  Pure JS over the `tty` binding (raw/cook + winsize) + `io.read` from
//  /dev/tty.  Renders the current hunk stream in a scrollable viewport via the
//  shared render lib (view/bro.js indexRows soft-wrap + cellSGR paint), a
//  bottom status line (statusURI/statusPos), and a vim-`:` address bar that
//  drives ANY spell in-process (run it in --tlv mode, reparse the 'H' records,
//  swap the view).  NO dog spawn, no /proc, no `less` — the INTERNAL TUI.
//
//  tty.raw sets VMIN=0 VTIME=1 (a 100ms poll, not a hard block), so the key
//  loop re-polls io.read until a byte arrives.  cook-on-exit rides try/finally.
"use strict";

const bro = require("view/bro.js");

//  ---- terminal write helpers (raw escapes; no OPOST, so we emit CRLF) -------
const ESC = "\x1b";
const CLEAR = ESC + "[2J" + ESC + "[H";       // clear + home
const HIDE_CUR = ESC + "[?25l", SHOW_CUR = ESC + "[?25h";

//  Write a string to a tty fd (raw mode: explicit CRLF, no cooked translation).
function ttyWrite(fd, str) {
  const bytes = utf8.Encode(str);
  const b = io.buf(bytes.length + 8);
  b.feed(bytes);
  io.writeAll(fd, b);
}

//  ---- hunk-log -> a flat row index spanning EVERY hunk ----------------------
//  A view is a stream of hunks; the viewport is one continuous scroll over all
//  of them.  Each entry: {hunk, off, end} — a display row (soft-wrapped) plus
//  its owning hunk, so cellSGR/statusURI have the hunk in hand per row.  A
//  banner row (the `<verb> <uri>` header) precedes each hunk's body rows.
function indexAll(hunks, cols) {
  const rows = [];
  for (const h of hunks) {
    rows.push({ hunk: h, banner: true });      // the hunk header line
    const sub = bro.indexRows(h, cols);
    for (const r of sub) rows.push({ hunk: h, off: r.off, end: r.end });
  }
  return rows;
}

//  Codepoint-decode one display row's visible cells, painting each by its tok
//  tag via cellSGR (color) or leaving it plain.  'U'-tagged bytes are hidden
//  (click-targets), matching rowEnd's column accounting.  Mirrors the C
//  bro_cell_ansi loop: walk bytes, look up the covering tok, emit the cell.
function paintRow(hunk, off, end, color) {
  const text = hunk.text, toks = hunk.toks;
  let ti = 0;
  while (ti < toks.length && (toks[ti] & 0xffffff) <= off) ti++;
  let out = "", curSGR = "";
  let pos = off;
  while (pos < end) {
    while (ti < toks.length && (toks[ti] & 0xffffff) <= pos) ti++;
    const w = ti < toks.length ? toks[ti] : 0;
    const tag = ti < toks.length ? String.fromCharCode(65 + ((w >>> 27) & 0x1f)) : "S";
    let clen = [1,1,1,1,1,1,1,1,0,0,0,0,2,2,3,4][text[pos] >> 4];
    if (clen === 0 || pos + clen > end) clen = 1;
    if (tag === "U") { pos += clen; continue; }   // hidden cell, no column
    if (color) {
      const sgr = bro.cellSGR(tag);
      if (sgr !== curSGR) { out += ESC + "[0m" + (sgr ? ESC + "[" + sgr + "m" : ""); curSGR = sgr; }
    }
    for (let i = 0; i < clen; i++) out += String.fromCharCode(text[pos + i]);
    pos += clen;
  }
  if (color && curSGR) out += ESC + "[0m";
  return out;
}

//  ---- the pager state machine ----------------------------------------------
//  A View = { hunks, rows(cached per width), scroll }.  The pager holds the
//  CURRENT view; a spell from the address bar REPLACES it (the back-stack is
//  DEFERRED — JAB-028 slice stops here).
function Pager(fd, opts) {
  this.fd = fd;
  this.color = opts && opts.color !== undefined ? opts.color : true;
  this.driveSpell = opts && opts.driveSpell;     // (spell) -> hunks | null
  this.view = null;                              // { hunks, rows, scroll, cols }
  this.mode = "scroll";                          // "scroll" | "command"
  this.cmd = "";                                 // the address-bar edit buffer
  this.message = "";                             // a transient status note
  this.quit = false;
}

//  Set the current view from a hunk array; (re)index against the current width.
Pager.prototype.setHunks = function (hunks) {
  this.view = { hunks: hunks, rows: null, scroll: 0, cols: 0 };
};

//  (Re)index the current view's rows for `cols`; cache by width so a resize
//  re-wraps but a scroll does not.  The status line steals the last screen row.
Pager.prototype.rows = function (cols) {
  const v = this.view;
  if (v.rows === null || v.cols !== cols) {
    v.rows = indexAll(v.hunks, cols);
    v.cols = cols;
  }
  return v.rows;
};

//  Paint one frame: the viewport (rows[scroll .. scroll+viewRows]) + the
//  bottom status/address line.  ONE write (the whole frame) to avoid flicker.
Pager.prototype.render = function () {
  const sz = tty.size(this.fd);
  const rowsN = sz.rows > 1 ? sz.rows : 24;
  const cols = sz.cols > 0 ? sz.cols : 80;
  const viewRows = rowsN - 1;                    // last row = status/address bar
  const rows = this.rows(cols);
  const v = this.view;
  if (v.scroll > rows.length - 1) v.scroll = Math.max(0, rows.length - 1);
  if (v.scroll < 0) v.scroll = 0;

  let frame = CLEAR;
  for (let i = 0; i < viewRows; i++) {
    const ri = v.scroll + i;
    if (ri < rows.length) {
      const r = rows[ri];
      if (r.banner) frame += this._banner(r.hunk, cols);
      else frame += paintRow(r.hunk, r.off, r.end, this.color);
    }
    frame += "\r\n";
  }
  frame += this._statusLine(rows, v.scroll, viewRows, cols);
  ttyWrite(this.fd, frame);
};

//  A hunk's header line: `<verb> <uri>` (the C HUNK banner), bolded on a tty.
Pager.prototype._banner = function (hunk, cols) {
  const verb = hunk.verb && hunk.verb !== "hunk" ? hunk.verb + " " : "";
  let line = verb + hunk.uri;
  if (line.length > cols) line = line.slice(0, cols);
  return this.color ? ESC + "[1m" + line + ESC + "[0m" : line;
};

//  The bottom line: in scroll mode the live status (statusURI#L + TOP/%/BOT);
//  in command mode the `:`-prefixed edit buffer (a vim address bar).
Pager.prototype._statusLine = function (rows, scroll, viewRows, cols) {
  if (this.mode === "command") {
    let line = ":" + this.cmd;
    return (this.color ? ESC + "[7m" : "") + this._fit(line, cols) +
           (this.color ? ESC + "[0m" : "");
  }
  //  Map the top visible row to its hunk + source line for statusURI.
  const r = rows.length ? rows[Math.min(scroll, rows.length - 1)] : null;
  let left = "";
  if (r) {
    const line = this._srcLine(r.hunk, r.banner ? 0 : r.off);
    left = bro.statusURI(r.hunk, line);
  }
  const pos = bro.statusPos(scroll, rows.length, viewRows);
  let line = (this.message ? this.message + "  " : "") + left + "  " + pos +
             "   (j/k space g/G : q)";
  return (this.color ? ESC + "[7m" : "") + this._fit(line, cols) +
         (this.color ? ESC + "[0m" : "");
};

//  The 1-based source line number a byte offset falls on within a hunk's text
//  (count the '\n' before it) — feeds statusURI's `<path>#L<n>`.
Pager.prototype._srcLine = function (hunk, off) {
  let n = 1;
  const t = hunk.text;
  for (let i = 0; i < off && i < t.length; i++) if (t[i] === 0x0a) n++;
  return n;
};

//  Pad/truncate to exactly `cols` so the inverse-video bar fills the row.
Pager.prototype._fit = function (s, cols) {
  if (s.length >= cols) return s.slice(0, cols);
  return s + " ".repeat(cols - s.length);
};

//  ---- key handling ----------------------------------------------------------
//  One keypress (a byte / a short escape sequence) drives the state machine.
//  Returns nothing; sets this.quit to exit.  Scroll keys: j/k line, space/b
//  page, g/G top/bottom.  ':' opens the address bar; Enter runs the spell.
Pager.prototype.key = function (b) {
  if (this.mode === "command") return this._keyCommand(b);
  return this._keyScroll(b);
};

Pager.prototype._page = function () {
  const sz = tty.size(this.fd);
  return (sz.rows > 2 ? sz.rows : 24) - 2;       // a near-full page (keep 1 row)
};

Pager.prototype._keyScroll = function (b) {
  const v = this.view;
  this.message = "";
  switch (b) {
    case 0x71: this.quit = true; break;                 // q
    case 0x6a: v.scroll += 1; break;                    // j  down
    case 0x6b: v.scroll -= 1; break;                    // k  up
    case 0x20: v.scroll += this._page(); break;         // space  page down
    case 0x62: v.scroll -= this._page(); break;         // b      page up
    case 0x67: v.scroll = 0; break;                     // g  top
    case 0x47: v.scroll = 1 << 30; break;               // G  bottom (clamped)
    case 0x3a: this.mode = "command"; this.cmd = ""; break;   // :  address bar
    default: break;
  }
};

Pager.prototype._keyCommand = function (b) {
  if (b === 0x0d || b === 0x0a) {                        // Enter: run the spell
    const spell = this.cmd;
    this.mode = "scroll";
    this.cmd = "";
    this._runSpell(spell);
    return;
  }
  if (b === 0x1b) { this.mode = "scroll"; this.cmd = ""; return; }   // Esc: cancel
  if (b === 0x7f || b === 0x08) {                        // Backspace
    if (this.cmd.length) this.cmd = this.cmd.slice(0, -1);
    else this.mode = "scroll";
    return;
  }
  if (b >= 0x20 && b < 0x7f) this.cmd += String.fromCharCode(b);   // printable
};

//  Drive a typed spell in-process: hand it to driveSpell (the bro handler wires
//  the --tlv capture + reparse); on success swap the view, else show the error.
Pager.prototype._runSpell = function (spell) {
  const s = spell.trim();
  if (!s) return;
  let hunks = null, err = null;
  try { hunks = this.driveSpell ? this.driveSpell(s) : null; }
  catch (e) { err = String(e); }
  if (err) { this.message = "err: " + err; return; }
  if (!hunks || hunks.length === 0) { this.message = "no hunks: " + s; return; }
  this.setHunks(hunks);
};

//  ---- the run loop ----------------------------------------------------------
//  Enter raw mode, paint, block-poll a key, repaint — until q.  cook + restore
//  the cursor on EVERY exit path (try/finally) so a throw never wedges the tty.
Pager.prototype.run = function () {
  const saved = tty.raw(this.fd);
  ttyWrite(this.fd, HIDE_CUR);
  try {
    const rb = io.buf(16);
    while (!this.quit) {
      this.render();
      //  Block on a key: VMIN=0 VTIME=1 means io.read returns 0 on a 100ms
      //  timeout, so spin until a byte arrives (portable, no platform poll).
      let n = 0;
      while (n === 0 && !this.quit) n = io.read(this.fd, rb);
      const data = rb.data();
      //  Consume EVERY buffered byte (a paste / an escape seq arrives at once);
      //  unknown escape sequences fall through harmlessly as their final byte.
      for (let i = 0; i < data.length && !this.quit; i++) this.key(data[i]);
      rb.reset();
    }
  } finally {
    ttyWrite(this.fd, ESC + "[0m" + SHOW_CUR + CLEAR);
    tty.cook(this.fd, saved);
  }
};

//  ---- tlv reparse: bytes -> hunks (the address-bar drive's other half) ------
//  Load a captured --tlv 'H'-record stream into a HUNK ram log and walk it into
//  {uri, verb, text, toks} hunks the renderer indexes — the SAME shape
//  buildFileHunk yields, so the viewport treats a spell result and a file alike.
function hunksFromTlv(tlv) {
  if (!tlv || tlv.length === 0) return [];
  const log = abc.ram("HUNK", tlv.length + 64);
  log.set(tlv, 0);
  log.buffer.watermark = tlv.length;
  log.rewind();
  return hunksFromLog(log);
}

//  JAB-030: walk a LIVE HUNK ram log (ctx.sink.log) into hunk objects — the
//  universal-pager edge hands the loop's collected sink straight here (no tlv
//  serialize round-trip).  Same {uri,verb,text,toks} shape buildFileHunk yields,
//  so the viewport treats every source alike.  verb stays "hunk" (the banner
//  shows the URI): log.verb is a RON60 bigint, not the view's text name.
function hunksFromLog(log) {
  if (!log) return [];
  log.rewind();
  const hunks = [];
  while (log.next()) {
    hunks.push({
      uri: utf8.Decode(log.uri),
      verb: "hunk",
      text: log.text.slice(),
      toks: log.toks.slice(),
      kind: "file",
    });
  }
  return hunks;
}

module.exports = {
  Pager: Pager,
  indexAll: indexAll,
  paintRow: paintRow,
  hunksFromTlv: hunksFromTlv,
  hunksFromLog: hunksFromLog,
};
