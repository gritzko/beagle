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
//  JAB-003: repo/worktree discovery (io.cwd walk-up) for the session context.
const discover = require("core/discover.js");
//  URI-011: the shared `word(context_uri, …rest)` spell composer — one classifier
//  for BOTH the address bar and the CLI (core/loop.js), so they compose alike.
const SPELL = require("shared/spell.js");
//  BRO-012: the shared ticket-code resolver — an `F` issue-key token with no
//  adjacent `U` derives its target (todo/<TOPIC>/<KEY>.{md,txt,mkd}) from here.
const TICKET = require("shared/ticket.js");

//  BRO-007: the ONE source of truth for the scroll-mode key bindings — the
//  `help:` view (views/help/help.js) imports this so its SHORTCUTS section can
//  never silently drift from `_keyScroll`.  KEEP IN SYNC with `_keyScroll`.
const SHORTCUTS = [
  ["q", "quit the pager"],
  ["j / k", "scroll down / up one line"],
  ["space / b", "page down / up"],
  ["g / G", "jump to top / bottom"],
  ["m", "toggle mouse tracking (wheel + click-to-follow)"],
  [": ", "open the address bar (type any URI spell)"],
  [". # ? /", "open the address bar pre-filled with that char"],
  ["Enter", "follow the URI of the row at the cursor"],
  ["- / BS", "back — pop to the previous view (refreshed)"],
  ["R / r", "refresh — re-run the current view (keep the scroll pos)"],
  ["h", "this help screen"],
];

//  ---- terminal write helpers (raw escapes; no OPOST, so we emit CRLF) -------
const ESC = "\x1b";
const CLEAR = ESC + "[2J" + ESC + "[H";       // clear + home
const HIDE_CUR = ESC + "[?25l", SHOW_CUR = ESC + "[?25h";
//  JAB-030: SGR mouse reporting (1000 = button events, 1006 = SGR extended
//  encoding `ESC[<b;col;rowM/m`).  Pure terminal protocol over the existing tty
//  fd — enable on enter, disable on exit; NO new binding (parsed in _feed).
const MOUSE_ON = ESC + "[?1000h" + ESC + "[?1006h";
const MOUSE_OFF = ESC + "[?1000l" + ESC + "[?1006l";
//  Bracketed paste (DEC ?2004): ask the terminal to WRAP a paste in ESC[200~ …
//  ESC[201~ so _feed can capture the payload verbatim instead of a pasted ESC
//  cancelling / a pasted newline submitting the address bar (paste was dropped).
const PASTE_ON = ESC + "[?2004h", PASTE_OFF = ESC + "[?2004l";
const PASTE_BEG = [0x1b, 0x5b, 0x32, 0x30, 0x30, 0x7e];   // ESC [ 2 0 0 ~
const PASTE_END = [0x1b, 0x5b, 0x32, 0x30, 0x31, 0x7e];   // ESC [ 2 0 1 ~
//  Match `seq` (bytes) against `data` at i: >0 = matched length, 0 = a definite
//  mismatch, -1 = a prefix match that ran off the buffer end (caller carries the
//  tail to the next read, exactly like the straddling mouse escape).
function _matchSeq(data, i, seq) {
  for (let k = 0; k < seq.length; k++) {
    if (i + k >= data.length) return -1;
    if (data[i + k] !== seq[k]) return 0;
  }
  return seq.length;
}

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
    for (const r of sub) rows.push({ hunk: h, off: r.off, end: r.end, pass: r.pass });
  }
  return rows;
}

//  Codepoint-decode one display row's visible cells, painting each by its tok
//  tag through the SHARED view/bro.js THEME (cellAnsi → deltaSGR), the SAME
//  source the DIRECT path (renderHunkLog → the C THEME .color() sink) and the
//  diff renderer use — so the pager's per-cell colour can never drift from `be`
//  / `jab status --color | cat` (BRO-010: the legacy cellSGR/THEME16 table the
//  pager used had no status-verb slots, so the verb token rendered PLAIN).
//  'U'-tagged bytes are hidden (click-targets), matching rowEnd's column
//  accounting.  A non-diff (syntax/columnar) row is PASS_NORMAL / side EQ.
//  Emit one row's visible cells to a BYTE sink: `enc(str)` appends ASCII/SGR,
//  `raw(lo,hi)` appends the VERBATIM text byte slice — text is NEVER re-encoded,
//  so multibyte UTF-8 survives (BRO-011: the old string frame + a whole-frame
//  utf8.Encode double-encoded it, — → â…).  A diff row rides the shared two-pass
//  view/bro.js paintDiffRow; a non-diff row is one PASS_NORMAL/side-EQ THEME-fg
//  pass, batching same-SGR cells into one raw run (the C speller).  'U' hidden.
function emitBody(hunk, off, end, color, pass, enc, raw) {
  if (color && hunk.toks && bro.hasDiffSides(hunk.toks))
    return bro.paintDiffRow(hunk.text, hunk.toks, off, end, pass | 0, enc, raw);
  const text = hunk.text, toks = hunk.toks;
  let ti = 0;
  while (ti < toks.length && (toks[ti] & 0xffffff) <= off) ti++;
  let cur = bro.A0, pos = off, runLo = -1;
  while (pos < end) {
    while (ti < toks.length && (toks[ti] & 0xffffff) <= pos) ti++;
    const w = ti < toks.length ? toks[ti] : 0;
    const tag = ti < toks.length ? String.fromCharCode(65 + ((w >>> 27) & 0x1f)) : "S";
    let clen = [1,1,1,1,1,1,1,1,0,0,0,0,2,2,3,4][text[pos] >> 4];
    if (clen === 0 || pos + clen > end) clen = 1;
    if (tag === "U") { if (runLo >= 0) { raw(runLo, pos); runLo = -1; } pos += clen; continue; }
    if (color) {
      const want = bro.cellAnsi(tag, 0, 0);       // PASS_NORMAL, SIDE_EQ
      if (!bro.aEq(want, cur)) {
        if (runLo >= 0) { raw(runLo, pos); runLo = -1; }
        enc(bro.deltaSGR(want, cur)); cur = want;
      }
    }
    if (runLo < 0) runLo = pos;
    pos += clen;
  }
  if (runLo >= 0) raw(runLo, pos);
  if (color) enc(bro.resetSGR(cur));
}

//  STRING form of a painted row (driver/pty tests + any string consumer): the
//  SAME cell walk as the byte render, text DECODED to real codepoints so it is
//  mojibake-free even if a caller re-encodes it (BRO-011).
function paintRow(hunk, off, end, color, pass) {
  let out = "";
  emitBody(hunk, off, end, color, pass,
           function (s) { out += s; },
           function (lo, hi) { if (hi > lo) out += utf8.Decode(hunk.text.subarray(lo, hi)); });
  return out;
}

//  JAB-003: the session context — cwd + worktree root + repo, from discover.js
//  (io.cwd walk-up).  Repo-less (bro file viewer, no .be) → wt_root undefined.
function sessionBe() {
  const cwd = io.cwd();
  let repo = null;
  try { repo = discover.find(cwd); } catch (e) { repo = null; }
  return { cwd: cwd, wt_root: repo ? repo.wt : undefined, repo: repo };
}

//  ---- the pager state machine ----------------------------------------------
//  A View = { hunks, rows(cached per width), scroll }.  The pager holds the
//  CURRENT view; a spell from the address bar REPLACES it (the back-stack is
//  DEFERRED — JAB-028 slice stops here).
function Pager(fd, opts) {
  this.fd = fd;
  this.color = opts && opts.color !== undefined ? opts.color : true;
  this.driveSpell = opts && opts.driveSpell;     // (spell) -> hunks | null
  this.be = (opts && opts.be) || sessionBe();    // JAB-003: {cwd, wt_root, repo}
  this.view = null;                              // { hunks, rows, scroll, cols }
  this.stack = [];                               // JAB-030: the view BACK-stack
  this.mode = "scroll";                          // "scroll" | "command"
  this.cmd = "";                                 // the address-bar edit buffer
  this.pasting = false;                          // inside a bracketed paste burst
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
  if (!this.stack.length) { this.message = "(no prev view)"; return false; }
  this.view = this.stack.pop();
  this.view.rows = null;                         // re-index for the live width
  return true;                                   // DIS-060: back caller refreshes
};

//  JAB-030: the CURRENT view's base path — the first hunk's URI path — so a
//  bare/relative typed URI (a `#frag`, a `?ref`, or a sibling path) resolves
//  against what is on screen.  Empty when the view has no path-bearing hunk.
Pager.prototype._viewPath = function () {
  const v = this.view;
  if (!v || !v.hunks.length) return "";
  for (const h of v.hunks) {
    //  URI-014: strip a leading `<verb> ` off the banner word spell before the
    //  URI parse (a raw space would mis-parse); _splitSpell.uri is the address.
    const u = uri._parse(this._splitSpell(h.uri || "").uri);
    if (u.path) return u.path;
  }
  return "";
};

//  JAB-003: join a relative spell (`.` / `./x` / `..` / `../y`) onto a URI dir
//  path as a DIRECTORY — `..` pops a segment, a name pushes one.
function joinPath(base, rel) {
  const segs = base ? base.split("/") : [];
  for (const seg of rel.split("/")) {
    if (seg === "" || seg === ".") continue;
    if (seg === "..") { if (segs.length) segs.pop(); }
    else segs.push(seg);
  }
  return segs.join("/");
}

//  DIS-060/[Nav]: the TRANSPORT schemes (network/file) — addressing, NOT verbs.
//  A view uri whose scheme is OFF this set is a projector verb (`status:`/`sha1:`
//  → the verbs `status`/`sha1`); `keeper` is `be:`'s backend, not a URI scheme.
const TRANSPORT = { ssh:1, https:1, http:1, git:1, be:1, file:1 };

//  The directory of a URI path (drop the last segment); "" when path-less.
function dirOf(p) { p = p || ""; const i = p.lastIndexOf("/"); return i >= 0 ? p.slice(0, i) : ""; }


//  JAB-003: the CURRENT view's anchor URI — the tracked navigated spell, else
//  its first hunk's banner URI (a scheme verb self-labels its own hunk).
Pager.prototype._viewUri = function () {
  const v = this.view;
  if (!v) return "";
  if (v.uri) return v.uri;
  return v.hunks.length ? (v.hunks[0].uri || "") : "";
};

//  JAB-030: resolve a typed spell RELATIVE to the current view.  A schemed
//  spell (`grep:x`, `ls:`) or an absolute path is absolute → returned as-is.
//  A fragment-only `#frag` / ref-only `?ref` re-anchors on the view's path; a
//  relative `./x`, `../x` or bare `name` resolves against the view's directory.
Pager.prototype._resolveSpell = function (spell) {
  const s = spell.trim();
  if (!s) return s;
  //  DIS-060: a bare word is a `module(args)` CALL ([Nav]), NOT a `word:` scheme
  //  — minting `word+":"` phantom-schemed a mutation verb (`post` -> `post:`).
  //  Return the bare word: driveSpell re-enters cli() which resolves it as a
  //  verb/view (shape-1) and ERASES the current URI, same as the old `word:`.
  if (/^[a-zA-Z][a-zA-Z0-9]*$/.test(s)) return s;
  //  URI-014: a `<verb> <rest>` target (a baked word-URI link/banner — leading
  //  bareword + SPACE) is a COMPLETE absolute spell; hand it STRAIGHT to
  //  driveSpell (spellCall→argline splits `verb arg`), no URI-relative mangling.
  if (/^[a-zA-Z][a-zA-Z0-9]*!? /.test(s)) return s;
  //  Not a URI (a `verb param` call — spaces/quotes) → ride to the dispatcher.
  let t; try { t = new URI(s); } catch (e) { return s; }
  //  URI-014: the context is the view's ADDRESS part (a word banner has a space,
  //  which `new URI` would mis-parse) — _verbUri().uri strips the verb word.
  const cur = new URI(this._verbUri().uri || "");
  //  Explicit URI form is RELATIVE to the current URI: a schemed spell INHERITS
  //  the //authority + path/?query it OMITS (`ls:` → `ls:test`); #fragment resets.
  //  URI-012: fill the OMITTED authority from the context so a relative click-
  //  target (`diff:<path>`) in a `//WT`-scoped view follows `diff://WT/<path>`,
  //  not the scope-less target that resolves against cwd → `no hunks`.  Idempotent
  //  (a present `//auth` passes through); composed via URI.make (setters are inert).
  if (t.scheme) {
    const auth = t.authority !== undefined ? t.authority : cur.authority;
    let path = t.path || cur.path;
    if (auth !== undefined && path && path[0] !== "/") path = "/" + path;
    //  URI-012/BRO-012: inherit the context ?ref ONLY for a relative click (the
    //  target OMITS its authority).  A target with its OWN //authority is
    //  absolute, maybe cross-repo (a ticket `cat://journal/…`) — the context hash
    //  is meaningless there → "no hunks"; keep its own (usually absent) ref.
    const query = t.query !== undefined ? t.query
                : (t.authority !== undefined ? undefined : cur.query);
    return URI.make(t.scheme, auth, path, query, t.fragment) || t.toString();
  }
  //  A scheme-less `./x` / `?x` / `#x` mutates one slot of the current URI.
  if (cur.scheme) {
    cur.fragment = undefined;                     // a fresh command drops #frag
    if (s[0] === ".") { cur.path = joinPath(cur.path || "", s); return cur.toString(); }
    if (s[0] === "?") { cur.query = s.slice(1); return cur.toString(); }
    if (s[0] === "#") { cur.fragment = s.slice(1); return cur.toString(); }
  }
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

  //  BRO-011: build the frame as UTF-8 BYTES in a buf — SGR/ASCII via `enc`
  //  (utf8.Encode), text bytes fed VERBATIM via `raw` — then ONE write.  A JS
  //  string + a whole-frame utf8.Encode double-encoded multibyte (— → â…).
  const chunks = [];
  const enc = function (s) { if (s.length) chunks.push(utf8.Encode(s)); };
  enc(CLEAR);
  for (let i = 0; i < viewRows; i++) {
    const ri = v.scroll + i;
    if (ri < rows.length) {
      const r = rows[ri];
      if (r.banner) enc(this._banner(r.hunk, cols));
      else {
        const text = r.hunk.text;
        emitBody(r.hunk, r.off, r.end, this.color, r.pass, enc,
                 function (lo, hi) { if (hi > lo) chunks.push(text.subarray(lo, hi)); });
      }
    }
    enc("\r\n");
  }
  enc(this._statusLine(rows, v.scroll, viewRows, cols));
  let total = 0; for (const c of chunks) total += c.length;
  const b = io.buf(total + 8);
  for (const c of chunks) b.feed(c);
  io.writeAll(this.fd, b);
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
  //  DIS-060/[Nav]: the address bar INDICATES the current (verb, URI).
  const vu = this._verbUri();
  let left = (vu.verb ? vu.verb + " " : "") + vu.uri;
  if (this.message) left = this.message + "  " + left;
  //  BRO-007: `<pos>  h: help` (help pointer + scroll position) is RIGHT-aligned;
  //  the URI stays left, the gap between them padded to the terminal width.
  const right = bro.statusPos(scroll, rows.length, viewRows) + "  h: help";
  const space = cols - right.length;
  let line;
  if (space < 1) line = right.slice(0, cols);
  else {
    if (left.length > space - 1) left = left.slice(0, space - 1);
    line = left + " ".repeat(space - left.length) + right;
  }
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
    //  DIS-060/[Nav]: the address bar opens on `:` (verb / anything, EMPTY) AND
    //  on a slot sigil `. / ? #` — the char is PRE-INSERTED so the buffer reads
    //  `./x`/`/x`/`?ref`/`#pos` (a slot edit relative to the current view).
    case 0x3a: this.mode = "command"; this.cmd = ""; break;          // :  empty
    case 0x2e: case 0x2f: case 0x23: case 0x3f:                      // . / # ? sigil
      this.mode = "command"; this.cmd = String.fromCharCode(b); break;
    //  JAB-030: Enter FOLLOWS the URI of the hunk at the cursor row (its banner
    //  URI is itself a spell) — a mouse click follows the same path (_followRow).
    case 0x0d: case 0x0a: this._followRow(v.scroll); break;          // Enter
    //  JAB-030/DIS-060: the BACK key POPS the view stack (a spell/follow pushed
    //  it) and REFRESHES the restored prev view (re-run its spell, keep its pos).
    case 0x2d: case 0x7f: case 0x08: if (this.popView()) this._refresh(); break;  // - / BS
    //  DIS-060: `R`/`r` REFRESH — re-run the current view's spell IN PLACE (no
    //  push), keeping the scroll pos, so a changed store/wt re-renders live.
    case 0x52: case 0x72: this._refresh(); break;                    // R/r refresh
    //  BRO-007: `h` runs the `help:` spell — pushes views/help/help.js as a
    //  normal view (scrollable, `-`/BS backs out).  SHORTCUTS (above) is the
    //  single source the help: view mirrors; keep both in sync.
    case 0x68: this._runSpell("help:"); break;                       // h  help
    default: break;
  }
};

Pager.prototype._keyCommand = function (b) {
  if (b === 0x0d || b === 0x0a) {                        // Enter: apply the spell
    const spell = this.cmd;
    this.mode = "scroll";
    this.cmd = "";
    this._applySpell(spell);
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
  //  JAB-003: ANY failure (resolve, drive, dispatch) shows in the addr bar —
  //  never let it escape the key loop and exit the pager.
  try {
    const s = this._resolveSpell(spell);
    if (!s) return;
    const hunks = this.driveSpell ? this.driveSpell(s) : null;
    if (!hunks || hunks.length === 0) { this.message = "no hunks: " + s; return; }
    this.pushView(hunks);
    //  DIS-060/URI-014: track the view as (verb, ADDRESS) split off the resolved
    //  spell ([Nav] click-targets are `verb args`), so a follow-up typed slot-edit
    //  (`#L`, `?ref`) resolves against the address, not the whole word spell.
    const sp = this._splitSpell(s);
    this.view.verb = sp.verb;
    this.view.uri  = sp.uri;
  } catch (e) { this.message = "err: " + String(e); }
};

//  DIS-060: total URI parse — never throws (an empty URI on malformed input).
Pager.prototype._parse = function (s) {
  try { return new URI(s || ""); } catch (e) { return new URI(""); }
};

//  URI-014: a spell/banner string → { verb, uri }.  A `<verb> <uri>` word spell
//  (baked link/banner) splits on the FIRST space — the verb is the leading
//  token, the rest the scheme-less address.  A residual `<scheme>:`-verb form
//  (C-baked diff:/cat:, out of scope) decodes the scheme as the verb (the compat
//  bridge, [URI-012]).  Else bare (verb "", uri = the string).
Pager.prototype._splitSpell = function (spell) {
  const m = /^([a-zA-Z][a-zA-Z0-9]*!?) (.*)$/.exec(spell || "");
  if (m) return { verb: m[1], uri: m[2] };
  //  URI-014: a lone bareword banner (`log`, `status` — empty addressing) IS the
  //  verb spell, NOT a path; else `new URI("log")` would mis-read it as a path.
  if (/^[a-zA-Z][a-zA-Z0-9]*!?$/.test(spell || "")) return { verb: spell, uri: "" };
  const u = this._parse(spell || "");
  if (u.scheme && !TRANSPORT[u.scheme])
    return { verb: u.scheme,
             uri: URI.make(undefined, u.authority, u.path, u.query, u.fragment) || "" };
  return { verb: "", uri: spell || "" };
};

//  DIS-060/[Nav]/URI-014: the current view's (verb, URI) — set explicitly by a
//  nav (_applySpell/_runSpell), else split off the hunk's banner word spell
//  (`<verb> <uri>`), else the scheme-decode bridge for a C-baked residue.
Pager.prototype._verbUri = function () {
  const v = this.view;
  if (v && v.verb !== undefined) return { verb: v.verb, uri: v.uri || "" };
  const spell = this._viewUri();
  const sp = this._splitSpell(spell);
  if (sp.verb) return sp;
  let verb = "";
  const h = v && v.hunks && v.hunks[0];
  if (h && h.verb && h.verb !== "hunk") verb = h.verb;
  return { verb: verb, uri: spell };
};

//  URI-011: the `word(context_uri, …rest)` spell composer.  Split the address-
//  bar spell, peel a leading bareword VERB, then shape arg 0 from the FIRST
//  URI-shaping token (`./x` path, `//WT` auth, `?x` ref, `#x` frag, `scheme:…`);
//  every OTHER token is REST — the verb's natural slot — handed through RAW.  A
//  non-URI first token keeps the context as arg 0.  Returns { verb, arg0, rest }.
//  URI-011: compose the address-bar spell into { verb, arg0, rest } via the
//  SHARED composer (shared/spell.js).  Context = the tracked view URI, else the
//  cwd (`//WT/path`).  Kept as a method so the driver test drives it directly.
Pager.prototype._composeCall = function (s) {
  const cur = this._verbUri();
  const ctxUri = cur.uri || (typeof be !== "undefined" && be.navCwd ? be.navCwd() : "");
  return SPELL.compose(ctxUri, cur.verb, s);
};
Pager.prototype._buildSpell = function (c) { return SPELL.buildSpell(c); };

//  DIS-060/[Nav]/URI-011: apply a typed address-bar spell through the composer —
//  build the `verb(context_uri, …rest)` call, drive it, and TRACK arg 0 as the
//  view URI so the next spell inherits it.  Replaces the per-slot inherit + the
//  message-shortcut that DROPPED the authority ([URI-011] `:post 'msg'` leak).
Pager.prototype._applySpell = function (cmd) {
  const s = (cmd || "").trim();
  if (!s) return;
  const c = this._composeCall(s);
  this._driveApply(this._buildSpell(c), c.verb, c.arg0);
};

//  DIS-060: drive a resolved spell + track the view's (verb, uri).  Shared by the
//  slot-edit path (a recomposed URI) and the message-call path (a raw spell).
Pager.prototype._driveApply = function (spell, verb, uri) {
  try {
    const hunks = this.driveSpell ? this.driveSpell(spell) : null;
    if (!hunks || hunks.length === 0) { this.message = "no hunks: " + spell; return; }
    this.pushView(hunks);
    this.view.verb = verb;
    this.view.uri  = uri;
  } catch (e) { this.message = "err: " + String(e); }
};

//  DIS-060: REFRESH the current view — re-run its (verb, uri) spell and swap the
//  hunks IN PLACE (no pushView, no back-stack entry), keeping the scroll pos so a
//  changed store/wt re-renders where the user was.  render() clamps a now-shorter
//  scroll.  Reconstructs the spell from _verbUri (tracked, else hunk-decoded).
Pager.prototype._refresh = function () {
  const v = this.view;
  if (!v) return;
  const vu = this._verbUri();
  const spell = (vu.verb ? vu.verb + " " : "") + vu.uri;
  if (!spell.trim()) { this.message = "(nothing to refresh)"; return; }
  const scroll = v.scroll;
  try {
    const hunks = this.driveSpell ? this.driveSpell(spell) : null;
    if (!hunks || hunks.length === 0) { this.message = "no hunks: " + spell; return; }
    v.hunks = hunks; v.rows = null; v.scroll = scroll;   // re-index, keep the pos
    this.message = "refreshed";
  } catch (e) { this.message = "err: " + String(e); }
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
    //  Inside a bracketed paste: swallow the payload into the address bar (in
    //  command mode) until the ESC[201~ end marker, so a pasted newline/ESC no
    //  longer submits/cancels the bar and non-ASCII is not silently dropped.
    if (this.pasting) {
      if (data[i] === 0x1b) {
        const e = _matchSeq(data, i, PASTE_END);
        if (e < 0) return i;                     // end marker straddles the read
        if (e > 0) { this.pasting = false; i += e; continue; }
        i++; continue;                           // a stray ESC in content: drop it
      }
      if (data[i] >= 0x20 && data[i] !== 0x7f && this.mode === "command")
        this.cmd += String.fromCharCode(data[i]);   // a printable/UTF-8 paste byte
      i++;
      continue;
    }
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
    //  Bracketed-paste BEGIN (ESC[200~).  Probe only once ESC '[' '2' are all
    //  present (the mouse gate's discipline) so a lone Esc keypress still falls
    //  through to key() and cancels the bar — real markers arrive whole.
    if (data[i] === 0x1b && i + 2 < data.length &&
        data[i + 1] === 0x5b && data[i + 2] === 0x32) {
      const b = _matchSeq(data, i, PASTE_BEG);
      if (b < 0) return i;                       // begin marker straddles the read
      if (b > 0) { this.pasting = true; i += b; continue; }
      //  ESC[2 but not ESC[200~ (e.g. Insert = ESC[2~): fall through to key().
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
  //  BRO-012: an `F` issue-key token has NO producer `U`; derive its ticket
  //  file URI from the token TEXT (todo/<TOPIC>/<KEY>.{md,txt,mkd}).  The
  //  resolver owns root order ($TODO_ROOT, current wt, open/launch wt).
  if (ti < toks.length &&
      String.fromCharCode(65 + ((toks[ti] >>> 27) & 0x1f)) === "F") {
    const lo = ti > 0 ? (toks[ti - 1] & 0xffffff) : 0;
    const hi = toks[ti] & 0xffffff;
    const key = hi > lo ? utf8.Decode(hunk.text.slice(lo, hi)) : "";
    const t = key ? TICKET.ticketUri(key) : null;
    if (t) return t;
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
  ttyWrite(this.fd, HIDE_CUR + MOUSE_ON + PASTE_ON);
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
    ttyWrite(this.fd, MOUSE_OFF + PASTE_OFF + ESC + "[0m" + SHOW_CUR + CLEAR);
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
  SHORTCUTS: SHORTCUTS,        // BRO-007: single-sourced for views/help/help.js
};
