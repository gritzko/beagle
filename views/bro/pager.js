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
//  BRO-005: the hunk-header band SGR (pale-yellow bg) — single-sourced from the
//  theme (view/theme.js BANNER_SGR), the SAME band core/emit.js / C bro draw.
const theme = require("view/theme.js");

//  ---- terminal write helpers (raw escapes; no OPOST, so we emit CRLF) -------
const ESC = "\x1b";
const CLEAR = ESC + "[2J" + ESC + "[H";       // clear + home
const HIDE_CUR = ESC + "[?25l", SHOW_CUR = ESC + "[?25h";
//  JAB-030: SGR mouse reporting (1000 = button events, 1006 = SGR extended
//  encoding `ESC[<b;col;rowM/m`).  Pure terminal protocol over the existing tty
//  fd — enable on enter, disable on exit; NO new binding (parsed in _feed).
const MOUSE_ON = ESC + "[?1000h" + ESC + "[?1006h";
const MOUSE_OFF = ESC + "[?1000l" + ESC + "[?1006l";

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
  this.stack = [];                               // JAB-030: the view BACK-stack
  this.mode = "scroll";                          // "scroll" | "command"
  this.cmd = "";                                 // the address-bar edit buffer
  this.message = "";                             // a transient status note
  this.mouse = true;                             // BRO-005: mouse on (`m` toggles)
  this.quit = false;
}

//  Set the current view from a hunk array; (re)index against the current width.
Pager.prototype.setHunks = function (hunks) {
  this.view = { hunks: hunks, rows: null, scroll: 0, cols: 0 };
};

//  JAB-030: PUSH a fresh hunk view, stacking the current one (a spell / a
//  follow descends).  popView restores the previous view (the back key).
Pager.prototype.pushView = function (hunks) {
  if (this.view) this.stack.push(this.view);
  this.setHunks(hunks);
};
Pager.prototype.popView = function () {
  if (!this.stack.length) { this.message = "(no prev view)"; return; }
  this.view = this.stack.pop();
  this.view.rows = null;                         // re-index for the live width
};

//  JAB-030: the CURRENT view's base path — the first hunk's URI path — so a
//  bare/relative typed URI (a `#frag`, a `?ref`, or a sibling path) resolves
//  against what is on screen.  Empty when the view has no path-bearing hunk.
Pager.prototype._viewPath = function () {
  const v = this.view;
  if (!v || !v.hunks.length) return "";
  for (const h of v.hunks) {
    const u = uri._parse(h.uri || "");
    if (u.path) return u.path;
  }
  return "";
};

//  JAB-030: resolve a typed spell RELATIVE to the current view.  A schemed
//  spell (`grep:x`, `ls:`) or an absolute path is absolute → returned as-is.
//  A fragment-only `#frag` / ref-only `?ref` re-anchors on the view's path; a
//  relative `./x`, `../x` or bare `name` resolves against the view's directory.
Pager.prototype._resolveSpell = function (spell) {
  const s = spell.trim();
  if (!s) return s;
  const u = uri._parse(s);
  if (u.scheme) return s;                         // schemed → absolute spell
  const base = this._viewPath();
  if (!base) return s;
  if (s[0] === "#" || s[0] === "?") return base + s;   // re-anchor on the view
  if (s[0] === "/") return s;                     // absolute path
  if (s[0] === ".") {                             // relative to the view's dir
    const dir = base.indexOf("/") >= 0 ? base.slice(0, base.lastIndexOf("/")) : "";
    if (s === "." || s === "./") return dir || ".";
    if (s.slice(0, 2) === "./") return (dir ? dir + "/" : "") + s.slice(2);
    return (dir ? dir + "/" : "") + s;            // ../x etc. kept verbatim tail
  }
  return s;                                       // a bare name: a fresh spell
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

//  A hunk's header line: `<verb> <uri>` (the C HUNK banner).  On a tty render it
//  as the pale-yellow BAND (theme.bannerOpen → BANNER_SGR bg, space-FILL to the
//  terminal width so the band spans the row like core/emit.js / C bro's
//  HUNKu8sFeedBanner, theme.bannerClose → ESC[0m).  Plain (non-tty) stays text.
Pager.prototype._banner = function (hunk, cols) {
  const verb = hunk.verb && hunk.verb !== "hunk" ? hunk.verb + " " : "";
  let line = verb + hunk.uri;
  if (line.length > cols) line = line.slice(0, cols);
  if (!this.color) return line;
  const thm = theme.DEFAULT;
  return thm.bannerOpen() + this._fit(line, cols) + thm.bannerClose();
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
    //  BRO-005: `m` toggles SGR mouse tracking (wheel/click) on/off, writing
    //  the enable/disable bracket to the tty so the terminal stops reporting.
    case 0x6d: this._toggleMouse(); break;              // m  mouse on/off
    //  JAB-030: the address bar opens on `:` (vim, EMPTY) AND on a URI-special
    //  `. # ? /` — the typed char is PRE-INSERTED so the line reads `:<char>`
    //  with the cursor after it (the URI is then relative to the current view).
    case 0x3a: this.mode = "command"; this.cmd = ""; break;          // :  empty
    case 0x2e: case 0x23: case 0x3f: case 0x2f:                       // . # ? /
      this.mode = "command"; this.cmd = String.fromCharCode(b); break;
    //  JAB-030: Enter FOLLOWS the URI of the hunk at the cursor row (its banner
    //  URI is itself a spell) — a mouse click follows the same path (_followRow).
    case 0x0d: case 0x0a: this._followRow(v.scroll); break;          // Enter
    //  JAB-030: the BACK key POPS the view stack (a spell/follow pushed it).
    case 0x2d: case 0x7f: case 0x08: this.popView(); break;          // - / BS
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
//  the --tlv capture + reparse); on success PUSH the view (back-stack), else
//  show the error.  The typed spell is first resolved relative to the view.
Pager.prototype._runSpell = function (spell) {
  const s = this._resolveSpell(spell);
  if (!s) return;
  let hunks = null, err = null;
  try { hunks = this.driveSpell ? this.driveSpell(s) : null; }
  catch (e) { err = String(e); }
  if (err) { this.message = "err: " + err; return; }
  if (!hunks || hunks.length === 0) { this.message = "no hunks: " + s; return; }
  this.pushView(hunks);
};

//  JAB-030: FOLLOW the URI of the hunk at display-row `ri` — its banner URI is
//  itself a spell, so a key (Enter) or a mouse click on a URI row runs it via
//  driveSpell and PUSHES the result.  Reuses _runSpell (resolve + drive + push).
Pager.prototype._followRow = function (ri) {
  const rows = this.view.rows || this.rows(80);
  if (!rows.length) return;
  const r = rows[Math.max(0, Math.min(ri, rows.length - 1))];
  const target = r && r.hunk ? r.hunk.uri : "";
  if (!target) { this.message = "(no URI here)"; return; }
  this._runSpell(target);
};

//  JAB-030: feed a whole input buffer, splitting SGR mouse escapes (`ESC[<…M/m`)
//  from ordinary keys.  A click (button 0 PRESS = `M`, the low 2 btn bits 0,
//  no drag bit 32) follows the URI at the clicked screen row; every other byte
//  routes to key().  Returns the count consumed; an UNFINISHED tail escape is
//  left for the next read (one mouse seq can straddle two io.read chunks).
Pager.prototype._feed = function (data) {
  let i = 0;
  while (i < data.length && !this.quit) {
    //  A mouse report opens with ESC '[' '<' ; scan to its M|m terminator.
    if (data[i] === 0x1b && i + 2 < data.length &&
        data[i + 1] === 0x5b && data[i + 2] === 0x3c) {
      let j = i + 3;
      while (j < data.length && data[j] !== 0x4d && data[j] !== 0x6d) j++;
      if (j >= data.length) return i;            // incomplete: wait for more
      let seq = "";
      for (let k = i + 3; k < j; k++) seq += String.fromCharCode(data[k]);
      this._mouse(seq, data[j] === 0x4d);
      i = j + 1;
      continue;
    }
    this.key(data[i]);
    i++;
  }
  return i;
};

//  BRO-005: handle one decoded SGR mouse report `<b;col;row>` (press iff the
//  terminator was 'M').  Mirrors C bro's MAUS handler (BRO.c):
//    - WHEEL (button bit 64) scrolls 3 rows up (64) / down (65);
//    - a plain LEFT PRESS (low 2 btn bits 0, no drag bit 32) maps (col,row) to
//      the byte under the cursor; if that token is followed by a `U` click-
//      target, navigate to its hidden URI, else FOLLOW the row's hunk URI.
//  No-op when mouse tracking is toggled off (`m`).
Pager.prototype._mouse = function (seq, press) {
  if (!this.mouse) return;
  const f = seq.split(";");
  const b = f[0] | 0, col = f[1] | 0, row = f[2] | 0;
  if ((b & 64) !== 0) {                           // wheel: button 64 up / 65 dn
    if (!press) return;
    this.view.scroll += (b & 1) ? 3 : -3;         // 65 down, 64 up (C step = 3)
    return;
  }
  if (!press) return;
  if ((b & 0x23) !== 0) return;                  // not a plain left press (drag/btn)
  //  A click on a visible token followed by a `U` token opens that URI; else
  //  fall back to the clicked row's hunk URI (titles, dir entries, status rows).
  const hit = this._screenToByte(row, col);
  if (hit) {
    const target = this._uriAt(hit.hunk, hit.off);
    if (target) { this._runSpell(target); return; }
  }
  this._followRow(this.view.scroll + (row - 1));   // 1-based screen row → index
};

//  BRO-005: map a 1-based screen (row, col) to the {hunk, off} of the visible
//  character under that cell — the JS twin of C bro's `bro_screen_to_byte`.
//  Walks the display row the SAME way paintRow does, skipping `U`-hidden bytes
//  so `col` counts EMITTED codepoints.  Returns null for the banner row, a
//  blank tail row, or a click past end-of-line.
Pager.prototype._screenToByte = function (row, col) {
  if (row < 1 || col < 1) return null;
  const rows = this.view.rows || this.rows(80);
  const ri = this.view.scroll + (row - 1);
  if (ri < 0 || ri >= rows.length) return null;
  const r = rows[ri];
  if (r.banner) return null;                     // titles don't carry a U-target
  const hunk = r.hunk, text = hunk.text, toks = hunk.toks;
  let ti = 0;
  while (ti < toks.length && (toks[ti] & 0xffffff) <= r.off) ti++;
  let cp = 1, pos = r.off;
  while (pos < r.end) {
    while (ti < toks.length && (toks[ti] & 0xffffff) <= pos) ti++;
    const tag = ti < toks.length ? String.fromCharCode(65 + ((toks[ti] >>> 27) & 0x1f)) : "S";
    let clen = [1,1,1,1,1,1,1,1,0,0,0,0,2,2,3,4][text[pos] >> 4];
    if (clen === 0 || pos + clen > r.end) clen = 1;
    if (tag !== "U") {                            // hidden cells take no column
      if (cp === col) return { hunk: hunk, off: pos };
      cp++;
    }
    pos += clen;
  }
  return null;
};

//  BRO-005: the `U` click-target URI for a byte offset, or null.  Mirrors C
//  bro: find the token covering `off`, and if the NEXT token is tag `U`, its
//  hidden text bytes (prev-end .. its-end) ARE the nav URI (TOK.h tok32Val).
//  BRO-005 follow-up: that token-precise check only lights the ONE token before
//  the U (the sha8/filename), so a click on the row's date/summary/author —
//  the bulk of a log:/ls: row, AND its soft-wrap tail rows — missed and the row
//  read as dead.  Fall back to the U-target of the whole LOGICAL line iff that
//  line carries EXACTLY ONE U (log/ls/commit/diff: one per line); cat/diff WORD-
//  links keep ≥1 U per line, so the unique-U guard leaves them token-precise.
Pager.prototype._uriAt = function (hunk, off) {
  const toks = hunk.toks;
  let ti = 0;
  while (ti < toks.length && (toks[ti] & 0xffffff) <= off) ti++;
  const nxt = ti + 1;
  if (nxt < toks.length &&
      String.fromCharCode(65 + ((toks[nxt] >>> 27) & 0x1f)) === "U") {
    const lo = toks[nxt - 1] & 0xffffff;       // nxt>0 always (ti>=0)
    const hi = toks[nxt] & 0xffffff;
    if (hi > lo) return utf8.Decode(hunk.text.slice(lo, hi));
  }
  return this._lineUri(hunk, off);
};

//  The single U-target of the LOGICAL line (`\n`-delimited) containing byte
//  `off`, or null.  Scans the tokens whose span starts inside the line; returns
//  the lone U's hidden bytes ONLY when the line has exactly one U (so cat's
//  many-per-line word links never resolve to a wrong target).
Pager.prototype._lineUri = function (hunk, off) {
  const text = hunk.text, toks = hunk.toks;
  let lo = off; while (lo > 0 && text[lo - 1] !== 0x0a) lo--;       // line start
  let hi = off; while (hi < text.length && text[hi] !== 0x0a) hi++; // line end
  let uTok = -1, uCount = 0, prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = toks[i] & 0xffffff;
    if (prev >= lo && prev < hi &&
        String.fromCharCode(65 + ((toks[i] >>> 27) & 0x1f)) === "U") {
      uTok = i; uCount++;
    }
    prev = end;
  }
  if (uCount !== 1) return null;
  const a = uTok > 0 ? (toks[uTok - 1] & 0xffffff) : 0;
  const b = toks[uTok] & 0xffffff;
  return b > a ? utf8.Decode(text.slice(a, b)) : null;
};

//  BRO-005: flip mouse tracking, writing the SGR enable/disable bracket to the
//  tty so the terminal stops/starts reporting (the C `m` key path).
Pager.prototype._toggleMouse = function () {
  this.mouse = !this.mouse;
  if (this.fd >= 0) ttyWrite(this.fd, this.mouse ? MOUSE_ON : MOUSE_OFF);
  this.message = "mouse: " + (this.mouse ? "on" : "off");
};

//  ---- the run loop ----------------------------------------------------------
//  Enter raw mode, paint, block-poll a key, repaint — until q.  cook + restore
//  the cursor (and disable mouse) on EVERY exit path (try/finally) so a throw
//  never wedges the tty.
Pager.prototype.run = function () {
  const saved = tty.raw(this.fd);
  ttyWrite(this.fd, HIDE_CUR + MOUSE_ON);
  try {
    const rb = io.buf(64);
    let pend = null;                             // a straddling mouse-seq tail
    while (!this.quit) {
      this.render();
      //  Block on a key: VMIN=0 VTIME=1 means io.read returns 0 on a 100ms
      //  timeout, so spin until a byte arrives (portable, no platform poll).
      let n = 0;
      while (n === 0 && !this.quit) n = io.read(this.fd, rb);
      //  Prepend any unfinished tail from the previous read, then feed; carry a
      //  still-unfinished mouse escape forward (a click can straddle reads).
      let data = rb.data();
      if (pend) { const m = new Uint8Array(pend.length + data.length);
        m.set(pend, 0); m.set(data, pend.length); data = m; pend = null; }
      const used = this._feed(data);
      if (used < data.length) pend = data.slice(used);
      rb.reset();
    }
  } finally {
    ttyWrite(this.fd, MOUSE_OFF + ESC + "[0m" + SHOW_CUR + CLEAR);
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
