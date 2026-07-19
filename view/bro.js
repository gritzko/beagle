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

//  ===========================================================================
//  DIFF colour render — the JS port of bro/BRO.c's two-pass side→bg renderer
//  (the ONE diff-colour impl; retires HUNKu8sFeedColor's diff branch + hd.color
//  for diff hunks).  A hunk's tok32 carry a 2-bit diff side (eq/in/rm); the
//  renderer reconstructs OLD lines then NEW lines (rm-pass / in-pass), washing
//  changed words with a background colour.  Byte-exact with `be <uri> --color`
//  (which pages through the C bro) for the `diff:` view.
//  ===========================================================================
const ESC = String.fromCharCode(27);

//  --- ansi64 model (abc/ANSI.h) -------------------------------------------
//  ansi64 as {fm,fg,bm,bg,fl}: fg mode/value, bg mode/value, attr flags.
//  OR-combine is field-wise — a fg-only and a bg-only state merge cleanly,
//  exactly like the C `want |= THEMEAt(...)` (the fields never overlap).
const A0 = { fm: 0, fg: 0, bm: 0, bg: 0, fl: 0 };
function aFgB(n)   { return { fm: 1, fg: n, bm: 0, bg: 0, fl: 0 }; }  // basic 30-37/90-97
function aFg256(n) { return { fm: 2, fg: n, bm: 0, bg: 0, fl: 0 }; }
function aBg256(n) { return { fm: 0, fg: 0, bm: 2, bg: n, fl: 0 }; }
//  WHY-001: a 24-bit truecolor bg (mode 3, `48;2;r;g;b`) — the `why:` wash the
//  VIEW bakes as a `#rrggbb` O prefix; bg packs r/g/b, feedColor mode 3 spells it.
function aBgRGB(r, g, b) { return { fm: 0, fg: 0, bm: 3,
  bg: ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff), fl: 0 }; }
//  BRO-030: 24-bit fg twin (`38;2;r;g;b`) — feedColor mode 3, kind "3".
function aFgRGB(r, g, b) { return { fm: 3,
  fg: ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff), bm: 0, bg: 0, fl: 0 }; }
function aFlag(f)  { return { fm: 0, fg: 0, bm: 0, bg: 0, fl: f }; }
function aOr(a, b) {
  return { fm: a.fm | b.fm, fg: a.fg | b.fg, bm: a.bm | b.bm,
           bg: a.bg | b.bg, fl: a.fl | b.fl };
}
function aEq(a, b) {
  return a.fm === b.fm && a.fg === b.fg && a.bm === b.bm &&
         a.bg === b.bg && a.fl === b.fl;
}
const A_BOLD = 0x01;

//  dog/THEME.h THEME16TBL — the default terminal-adaptive palette (NOTE: the
//  legacy THEME16 string table above is a 16-colour *approximation*; this is
//  the byte-exact C table the diff renderer needs).  Diff bg tags I/O/J/K are
//  256-colour pale tints bro_cell_ansi ORs onto the fg.
const THEME = {
  D: aFgB(90), G: aFg256(149), L: aFgB(96), H: aFgB(35), R: aFgB(94), P: aFgB(90),
  N: aFlag(A_BOLD), C: aFlag(A_BOLD), F: aFg256(56), T: aFg256(56),
  I: aBg256(194), O: aBg256(224), J: aBg256(157), K: aBg256(217),
  //  Status-verb / whitespace slots (THEME16TBL).  'W' (whitespace) = green is
  //  the one that shows inside diff bodies; the rest round out the table.
  U: aFgB(34), W: aFgB(32), V: aFgB(36), E: aFgB(33), X: aFg256(94),
  M: aFgB(91), Q: aFgB(90), Y: aFgB(34), Z: aFgB(35), B: aFgB(33),
  //  WORK-004 ahbeh button slots: 'G' (the green slot) refreshed 32→salad-256
  //  for ahead `[+N]`; 'A' the NEW dedicated salmon slot for behind `[-N]`.
  A: aFg256(209),
  //  BRO-030 quad chars, codes 26..31 past 'Z' (A-Z is full): the GLYPH takes
  //  the column color (fg); staged wt INVERTS (column color bg, white fg),
  //  conflict inverts to red.  Mirrors view/theme.js QUAD_SGR — keep in step.
  "[":  aFgRGB(30, 144, 255),                    // track  — blue glyph
  "\\": aFgRGB(0, 180, 70),                      // base   — green glyph
  "]":  aFgRGB(220, 160, 0),                     // patch  — amber glyph
  "^":  aFgRGB(255, 140, 0),                     // wt     — orange glyph
  //  RGB white fg, not basic 97 — terminal themes remap the palette's white.
  "_":  aOr(aFgRGB(255, 255, 255), aBgRGB(255, 140, 0)),  // staged wt — white on orange
  "`":  aOr(aFgRGB(255, 255, 255), aBgRGB(220, 40, 40)),  // con wt — white on red
};
function themeAt(tag) { return THEME[tag] || A0; }
const THEME_BANNER = { fm: 2, fg: 0, bm: 2, bg: 230, fl: 0 };

//  --- SGR delta speller (abc/ANSI.c ANSIu8sFeedDelta / ANSIu8sFeedReset) ---
//  Emit only the attributes that transitioned from `prev` to `want`, in the
//  C order: flags-off, flags-on, fg, bg.  Byte-identical to the C speller so
//  a run of identical cells shares one open SGR and a row closes with `\033[0m`.
const FLAG_ON  = { 0x01: 1, 0x02: 2, 0x04: 3, 0x08: 4, 0x10: 5, 0x20: 7, 0x40: 9 };
const FLAG_OFF = { 0x01: 22, 0x02: 22, 0x04: 23, 0x08: 24, 0x10: 25, 0x20: 27, 0x40: 29 };
function feedColor(kind, mode, val) {
  if (mode === 1) return String(val);                       // BASIC: code verbatim
  if (mode === 2) return kind + "8;5;" + (val & 0xff);      // 256
  if (mode === 3) return kind + "8;2;" + ((val >> 16) & 0xff) + ";" +
                         ((val >> 8) & 0xff) + ";" + (val & 0xff);
  return kind === "3" ? "39" : "49";                        // DEFAULT
}
function deltaSGR(want, prev) {
  if (aEq(want, prev)) return "";
  const parts = [];
  const off = prev.fl & ~want.fl, on = want.fl & ~prev.fl;
  for (let b = 1; b <= 0x40; b <<= 1) if (off & b) parts.push(String(FLAG_OFF[b]));
  for (let b = 1; b <= 0x40; b <<= 1) if (on & b) parts.push(String(FLAG_ON[b]));
  if (want.fg !== prev.fg || want.fm !== prev.fm)
    parts.push(feedColor("3", want.fm, want.fg));
  if (want.bg !== prev.bg || want.bm !== prev.bm)
    parts.push(feedColor("4", want.bm, want.bg));
  if (parts.length === 0) parts.push("0");
  return ESC + "[" + parts.join(";") + "m";
}
function resetSGR(cur) { return aEq(cur, A0) ? "" : ESC + "[0m"; }

//  --- tok32 side accessor + diff-hunk probe -------------------------------
const TOK_SIDE = (w) => (w >>> 24) & 0x3;                   // 0=eq 1=in 2=rm
const SIDE_EQ = 0, SIDE_IN = 1, SIDE_RM = 2;
const PASS_NORMAL = 0, PASS_RM = 1, PASS_IN = 2;

//  A hunk is a diff hunk iff any non-'U' tok carries a side != EQ (the C
//  hunk_has_diff twin).  Drives the renderHunkLog/pager routing.
function hasDiffSides(toks) {
  for (let i = 0; i < toks.length; i++) {
    if (TOK_TAG(toks[i]) === "U") continue;
    if (TOK_SIDE(toks[i]) !== SIDE_EQ) return true;
  }
  return false;
}

//  WHY-001: a `why:` hunk marks each washed token with a hidden `O` (origin) token
//  right after it, bytes = `#rrggbb commit ?<hashlet>` — a LEADING baked bg the VIEW
//  resolved (hue f(sha) + age paleness).  hasWhyRuns → route to the why renderer;
//  `O`/`U` bytes are hidden.  The renderer is colour-AGNOSTIC: it just applies #rgb.
function hasWhyRuns(toks) {
  for (let i = 0; i < toks.length; i++)
    if (TOK_TAG(toks[i]) === "O") return true;
  return false;
}

//  WHY-001: the bg for a visible token — PEEK the next token; if it's `O`, read the
//  LEADING `#rrggbb` the view baked and apply it as a truecolor wash.  Else A0 (white);
//  a wash with no leading `#rrggbb` (an unattributed token) also renders white.
function whyBgAt(text, toks, ti) {
  const nx = ti + 1;
  if (nx >= toks.length || TOK_TAG(toks[nx]) !== "O") return A0;
  const s = utf8.Decode(text.slice(toks[nx - 1] & 0xffffff, toks[nx] & 0xffffff));
  const m = /^#([0-9a-fA-F]{6})/.exec(s);
  if (!m) return A0;
  const v = parseInt(m[1], 16);
  return aBgRGB((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
}

//  WORK-005: a row-leading bare `#rrggbb` O token sets the row's DEFAULT-fg (the
//  work-view age fade); null unless the FIRST token of the row is exactly that.
function rowFgAt(text, toks, ti, off) {
  if (ti >= toks.length || TOK_TAG(toks[ti]) !== "O") return null;
  const start = ti > 0 ? TOK_END(toks[ti - 1]) : 0;
  if (start !== off) return null;
  const s = utf8.Decode(text.slice(start, TOK_END(toks[ti])));
  const m = /^#([0-9a-fA-F]{6})$/.exec(s);
  if (!m) return null;
  const v = parseInt(m[1], 16);
  return aFgRGB((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
}

//  WHY-001: paint ONE display row [off, end) of a `why:` blame hunk (the twin of
//  paintDiffRow) — each cell's fg = themeAt(tag), OR'd with its commit pastel bg
//  (whyBgAt); `U` click-target bytes hidden.  Same batched-raw + minimal-SGR
//  contract: `enc(str)` appends SGR/ASCII, `raw(lo,hi)` the verbatim text slice.
function paintWhyRow(text, toks, off, end, enc, raw) {
  const ntoks = toks.length;
  let ti = 0;
  while (ti < ntoks && TOK_END(toks[ti]) <= off) ti++;
  const rowFg = rowFgAt(text, toks, ti, off);   // WORK-005: work-view age fade
  let cur = A0, runLo = -1, pos = off;
  while (pos < end) {
    while (ti < ntoks && TOK_END(toks[ti]) <= pos) ti++;
    const tag = ti < ntoks ? TOK_TAG(toks[ti]) : "S";
    const ch = text[pos];
    let clen = UTF8_LEN[ch >> 4];
    if (clen === 0 || pos + clen > end) clen = 1;
    //  WHY-001: `U` (underline/click) and `O` (origin bg+click) bytes are hidden.
    if (tag === "U" || tag === "O") { if (runLo >= 0) { raw(runLo, pos); runLo = -1; } pos += clen; continue; }
    let want = ti < ntoks ? aOr(themeAt(tag), whyBgAt(text, toks, ti)) : A0;
    //  WORK-005: the fade tints only THEME-default cells; coloured spans keep theirs.
    if (rowFg && want.fm === 0 && want.fg === 0) want = aOr(want, rowFg);
    if (!aEq(want, cur)) {
      if (runLo >= 0) { raw(runLo, pos); runLo = -1; }
      enc(deltaSGR(want, cur)); cur = want;
    }
    if (runLo < 0) runLo = pos;
    pos += clen;
  }
  if (runLo >= 0) raw(runLo, pos);
  const r = resetSGR(cur); if (r) enc(r);
}

//  WHY-001: STRING form of paintWhyRow (the pager frame is a JS string) — same
//  per-cell fg+commit-bg walk, text DECODED to real codepoints (BRO-011).
function paintWhyRowStr(text, toks, off, end) {
  let out = "";
  paintWhyRow(text, toks, off, end,
              function (s) { out += s; },
              function (lo, hi) { if (hi > lo) out += utf8.Decode(text.subarray(lo, hi)); });
  return out;
}

//  WHY-001: render a `why:` blame hunk in colour — banner then per-line body,
//  each row painted by paintWhyRow (commit pastel bg OR'd onto the syntax fg).
//  Returns utf8 bytes.  cols default 200 (the pipe-mode width; rarely wraps).
function colorWhyHunk(uriStr, text, toks, cols) {
  cols = cols || 200;
  const chunks = [];
  const enc = function (s) { if (s.length) chunks.push(utf8.Encode(s)); };
  const raw = function (lo, hi) { if (hi > lo) chunks.push(text.subarray(lo, hi)); };
  if (uriStr && uriStr.length) bannerColor(uriStr, cols, enc);
  const tlen = text.length;
  let off = 0;
  while (off < tlen) {
    const end = rowEnd({ text: text, toks: toks }, off, cols);
    const rowEndByte = end < tlen && text[end] === 0x0a ? end : end;
    paintWhyRow(text, toks, off, rowEndByte, enc, raw);
    if (rowEndByte < tlen && text[rowEndByte] === 0x0a) { raw(rowEndByte, rowEndByte + 1); off = rowEndByte + 1; }
    else off = rowEndByte > off ? rowEndByte : off + 1;
  }
  let total = 0; for (const c of chunks) total += c.length;
  const all = new Uint8Array(total); let o = 0;
  for (const c of chunks) { all.set(c, o); o += c.length; }
  return all;
}

//  WHY-001: --plain for a `why:` hunk — the visible bytes only.  The native HUNK
//  cursor hides `U` but NOT `O`, so a why hunk's --plain routes here: emit each
//  non-`U`/`O` token span verbatim (that IS the file content the wash sits over).
function whyPlain(text, toks) {
  const b = io.buf(text.length + 8);
  let pos = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = toks[i] & 0xffffff, tag = TOK_TAG(toks[i]);
    if (tag !== "U" && tag !== "O" && end > pos) b.feed(text.subarray(pos, end));
    pos = end;
  }
  return b.data().slice();
}

//  --- bro_cell_ansi: (fg tag, pass, side) -> ansi64 -----------------------
function cellAnsi(tag, pass, side) {
  let want = themeAt(tag);
  if (pass === PASS_NORMAL) {
    if (side === SIDE_IN) want = aOr(want, themeAt("I"));
    else if (side === SIDE_RM) want = aOr(want, themeAt("O"));
  } else if (pass === PASS_RM) {
    want = aOr(want, side === SIDE_RM ? themeAt("K") : themeAt("O"));
  } else {  // PASS_IN
    want = aOr(want, side === SIDE_IN ? themeAt("J") : themeAt("I"));
  }
  return want;
}

//  --- bro_row_end_pass: end byte of one display row in `pass` -------------
//  Advance at most `cols` VISIBLE codepoints, stop at a visible '\n'; bytes
//  hidden in this pass (the other side, or 'U') advance but don't count.
function rowEndPass(text, toks, tlen, off, cols, pass) {
  const ntoks = toks.length;
  let ti = 0;
  while (ti < ntoks && TOK_END(toks[ti]) <= off) ti++;
  let cp = 0, pos = off;
  while (pos < tlen && cp < cols) {
    while (ti < ntoks && TOK_END(toks[ti]) <= pos) ti++;
    const side = ti < ntoks ? TOK_SIDE(toks[ti]) : SIDE_EQ;
    const tag  = ti < ntoks ? TOK_TAG(toks[ti]) : "S";
    const ch = text[pos];
    const hidden = tag === "U" || tag === "O" ||
                   (pass === PASS_RM && side === SIDE_IN) ||
                   (pass === PASS_IN && side === SIDE_RM);
    if (ch === 0x0a && !hidden) break;
    let clen = UTF8_LEN[ch >> 4];
    if (clen === 0 || pos + clen > tlen) clen = 1;
    pos += clen;
    if (!hidden) cp++;
  }
  return pos;
}

//  --- bro_classify_lines: per-segment {lo,hi,in_b,rm_b,eq_b,bndSide} ------
//  A "segment" runs from after the previous '\n' to (incl.) the next '\n'.
//  The boundary '\n' carries a side determining which pass(es) see the break.
function classifyLines(text, toks) {
  const tlen = text.length, ntoks = toks.length;
  const out = [];
  let lineLo = 0, ti = 0, inB = 0, rmB = 0, eqB = 0;
  for (let off = 0; off < tlen; off++) {
    while (ti < ntoks && TOK_END(toks[ti]) <= off) ti++;
    const side = ti < ntoks ? TOK_SIDE(toks[ti]) : SIDE_EQ;
    const tag  = ti < ntoks ? TOK_TAG(toks[ti]) : "S";
    if (tag === "U" || tag === "O") continue;
    if (text[off] === 0x0a) {
      out.push({ lo: lineLo, hi: off, inB: inB, rmB: rmB, eqB: eqB, bnd: side });
      lineLo = off + 1; inB = rmB = eqB = 0;
    } else if (side === SIDE_IN) inB++;
    else if (side === SIDE_RM) rmB++;
    else eqB++;
  }
  if (lineLo < tlen)
    out.push({ lo: lineLo, hi: tlen, inB: inB, rmB: rmB, eqB: eqB, bnd: SIDE_EQ });
  return out;
}

//  Line kind (bro_classify) + the helper predicates.
const K_EQ = 0, K_PURE_IN = 1, K_PURE_RM = 2, K_MOD_INLINE = 3, K_MOD_SPLIT = 4;
function lineKind(li) {
  const changed = li.inB + li.rmB;
  if (changed === 0) return K_EQ;
  if (li.eqB === 0) {
    if (li.inB > 0 && li.rmB > 0) return K_MOD_SPLIT;
    return li.inB > 0 ? K_PURE_IN : K_PURE_RM;
  }
  const total = changed + li.eqB;
  if (changed * 4 < total) return K_MOD_INLINE;
  return K_MOD_SPLIT;
}
function lineContinues(li) {
  if (li.bnd === SIDE_IN) return li.rmB > 0;
  if (li.bnd === SIDE_RM) return li.inB > 0;
  return false;
}
function passSeesNL(pass, bnd) {
  if (pass === PASS_NORMAL) return true;
  if (pass === PASS_RM) return bnd !== SIDE_IN;
  return bnd !== SIDE_RM;  // PASS_IN
}

//  --- bro_walk_hunk: drive per-pass logical-row emission ------------------
//  Calls emit(lo, endNl, pass) for each logical render row (an eq context
//  line in NORMAL pass; a modified block's rm-pass rows then in-pass rows).
function walkHunk(text, toks, emit) {
  const info = classifyLines(text, toks);
  const nl = info.length;
  let i = 0;
  while (i < nl) {
    const k = lineKind(info[i]);
    if ((k === K_EQ || k === K_MOD_INLINE) && info[i].bnd === SIDE_EQ) {
      emit(info[i].lo, info[i].hi, PASS_NORMAL);
      i++;
      continue;
    }
    //  Find block end: the next eq/inline context line whose predecessor does
    //  not continue into it.
    let j = i;
    while (j < nl) {
      const kj = lineKind(info[j]);
      if (info[j].bnd === SIDE_EQ && (kj === K_EQ || kj === K_MOD_INLINE) &&
          (j === i || !lineContinues(info[j - 1]))) break;
      j++;
    }
    const blockHi = info[j - 1].hi;
    //  rm-pass: group across hidden IN '\n's.
    let rowStart = info[i].lo, pendIn = 0, pendRm = 0, pendEq = 0;
    for (let m = i; m < j; m++) {
      pendIn += info[m].inB; pendRm += info[m].rmB; pendEq += info[m].eqB;
      if (passSeesNL(PASS_RM, info[m].bnd)) {
        if (pendRm > 0 || pendEq > 0) emit(rowStart, info[m].hi, PASS_RM);
        rowStart = info[m].hi + 1; pendIn = pendRm = pendEq = 0;
      }
    }
    if (pendRm > 0 || pendEq > 0) emit(rowStart, blockHi, PASS_RM);
    //  in-pass: symmetric.
    rowStart = info[i].lo; pendIn = pendRm = pendEq = 0;
    for (let m = i; m < j; m++) {
      pendIn += info[m].inB; pendRm += info[m].rmB; pendEq += info[m].eqB;
      if (passSeesNL(PASS_IN, info[m].bnd)) {
        if (pendIn > 0 || pendEq > 0) emit(rowStart, info[m].hi, PASS_IN);
        rowStart = info[m].hi + 1; pendIn = pendRm = pendEq = 0;
      }
    }
    if (pendIn > 0 || pendEq > 0) emit(rowStart, blockHi, PASS_IN);
    i = j;
  }
}

//  Render one display row [off, lineEnd) in `pass` into a byte sink: walk
//  visible cells, skip pass-hidden + 'U' bytes, spell minimal SGR deltas
//  (carried `cur`), reset at row end.  `enc(str)` appends ASCII (SGR) bytes;
//  `raw(lo,hi)` appends the verbatim text byte slice (NEVER re-encoded — the
//  text is already utf8, so re-encoding would double-encode multibyte chars).
//  Contiguous same-SGR visible cells batch into one `raw` run.
function paintDiffRow(text, toks, off, lineEnd, pass, enc, raw) {
  const ntoks = toks.length;
  let ti = 0;
  while (ti < ntoks && TOK_END(toks[ti]) <= off) ti++;
  let cur = A0, runLo = -1;
  let pos = off;
  while (pos < lineEnd) {
    while (ti < ntoks && TOK_END(toks[ti]) <= pos) ti++;
    const tag  = ti < ntoks ? TOK_TAG(toks[ti]) : "S";
    const side = ti < ntoks ? TOK_SIDE(toks[ti]) : SIDE_EQ;
    const ch = text[pos];
    let clen = UTF8_LEN[ch >> 4];
    if (clen === 0 || pos + clen > lineEnd) clen = 1;
    const hidden = tag === "U" ||
                   (pass === PASS_RM && side === SIDE_IN) ||
                   (pass === PASS_IN && side === SIDE_RM);
    if (hidden) { if (runLo >= 0) { raw(runLo, pos); runLo = -1; } pos += clen; continue; }
    const want = cellAnsi(tag, pass, side);
    if (!aEq(want, cur)) {
      if (runLo >= 0) { raw(runLo, pos); runLo = -1; }
      enc(deltaSGR(want, cur)); cur = want;
    }
    if (runLo < 0) runLo = pos;
    pos += clen;
  }
  if (runLo >= 0) raw(runLo, pos);
  const r = resetSGR(cur); if (r) enc(r);
}

//  Pager variant: one diff row as an SGR-painted STRING (the pager's frame is a
//  JS string written via ttyWrite/utf8.Encode).  Same two-pass cell logic as
//  paintDiffRow; text bytes go through String.fromCharCode like the pager's own
//  syntax paintRow (the pager's one-byte-per-char frame convention).
function paintDiffRowStr(text, toks, off, lineEnd, pass) {
  const ntoks = toks.length;
  let ti = 0;
  while (ti < ntoks && TOK_END(toks[ti]) <= off) ti++;
  let out = "", cur = A0, pos = off;
  while (pos < lineEnd) {
    while (ti < ntoks && TOK_END(toks[ti]) <= pos) ti++;
    const tag  = ti < ntoks ? TOK_TAG(toks[ti]) : "S";
    const side = ti < ntoks ? TOK_SIDE(toks[ti]) : SIDE_EQ;
    const ch = text[pos];
    let clen = UTF8_LEN[ch >> 4];
    if (clen === 0 || pos + clen > lineEnd) clen = 1;
    if (tag === "U" ||
        (pass === PASS_RM && side === SIDE_IN) ||
        (pass === PASS_IN && side === SIDE_RM)) { pos += clen; continue; }
    const want = cellAnsi(tag, pass, side);
    if (!aEq(want, cur)) { out += deltaSGR(want, cur); cur = want; }
    for (let b = 0; b < clen; b++) out += String.fromCharCode(text[pos + b]);
    pos += clen;
  }
  out += resetSGR(cur);
  return out;
}

//  Render the THEME_BANNER colour band for a hunk URI, space-filled to `cols`
//  (HUNKu8sFeedBanner HUNKOutColor; ts/verb absent for a diff hunk).  `used`
//  is the URI BYTE length (the C u8csLen), matching the C fill exactly.
function bannerColor(uriStr, cols, enc) {
  const uriBytes = utf8.Encode(uriStr);
  enc(deltaSGR(THEME_BANNER, A0));
  enc(uriStr);
  let used = uriBytes.length, pad = "";
  while (used < cols) { pad += " "; used++; }
  enc(pad);
  enc(resetSGR(THEME_BANNER));
  enc("\n");
}

//  colorDiffHunk: banner + the two-pass body for ONE diff hunk record.  cols
//  defaults to 200 (the C bro pipe-mode width).  Returns utf8 bytes.
function colorDiffHunk(uriStr, text, toks, cols) {
  cols = cols || 200;
  const chunks = [];
  const enc = function (s) { if (s.length) chunks.push(utf8.Encode(s)); };
  const raw = function (lo, hi) { if (hi > lo) chunks.push(text.subarray(lo, hi)); };
  if (uriStr && uriStr.length) bannerColor(uriStr, cols, enc);
  const tlen = text.length;
  walkHunk(text, toks, function (lo, endNl, pass) {
    //  Soft-wrap [lo, endNl] into display rows (bro_append_rows); cols 200
    //  rarely wraps, but keep parity for long lines.
    let off = lo;
    while (off <= endNl) {
      const end = rowEndPass(text, toks, tlen, off, cols, pass);
      const rowEndByte = end < endNl ? end : endNl;
      paintDiffRow(text, toks, off, rowEndByte, pass, enc, raw);
      enc("\n");
      if (end >= endNl) break;
      off = end;
    }
  });
  let total = 0; for (const c of chunks) total += c.length;
  const all = new Uint8Array(total); let o = 0;
  for (const c of chunks) { all.set(c, o); o += c.length; }
  return all;
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
    const hidden = tag === "U" || tag === "O";   // WHY-001: O bytes take no column
    if (ch === 0x0a && !hidden) break;       // visible '\n' ends the row
    let clen = UTF8_LEN[ch >> 4];
    if (clen === 0 || pos + clen > tlen) clen = 1;
    pos += clen;
    if (!hidden) cp++;
  }
  return pos;
}

//  Walk one hunk's text into display rows (one per soft-wrap segment).  A diff
//  hunk (tok sides) walks the bro two-pass index (old rows then new rows, each
//  carrying its render `pass`); a syntax hunk is one NORMAL-pass row per line.
//  BRO-014: `wrap` boolean — false (no-wrap) emits ONE row per logical line,
//  clamped by rowEnd to `cols`, then skips the tail to the next '\n'; true (or
//  undefined, the default) soft-wraps.
function indexRows(hunk, cols, wrap) {
  if (hunk.toks && hasDiffSides(hunk.toks)) return indexDiffRows(hunk, cols, wrap);
  const rows = [];
  const text = hunk.text, tlen = text.length;
  let off = 0;
  while (off < tlen) {
    const end = rowEnd(hunk, off, cols);
    rows.push({ off: off, end: end, pass: PASS_NORMAL });
    let next;
    if (wrap === false) {
      //  BRO-014 no-wrap: the clamped tail is hidden — skip to past the logical
      //  line's '\n' so the next row is the next line, not a wrap of this one.
      let nl = end; while (nl < tlen && text[nl] !== 0x0a) nl++;
      next = nl < tlen ? nl + 1 : nl;
    } else {
      //  Next row starts past the terminating '\n' (rowEnd stops AT it), else at
      //  the wrap point.  Guard against a zero-width row (cols 0) stalling.
      next = end < tlen && text[end] === 0x0a ? end + 1 : end;
    }
    off = next > off ? next : off + 1;
  }
  return rows;
}

//  Diff hunk row index (bro_walk_hunk + bro_append_rows): each row carries its
//  render `pass` (rm/in/normal) so the pager paints + hides the right side.
//  BRO-014: `wrap` false emits ONE clamped row per logical (per-pass) line.
function indexDiffRows(hunk, cols, wrap) {
  const rows = [];
  const text = hunk.text, toks = hunk.toks, tlen = text.length;
  walkHunk(text, toks, function (lo, endNl, pass) {
    let off = lo;
    while (off <= endNl) {
      const end = rowEndPass(text, toks, tlen, off, cols, pass);
      const rowEndByte = end < endNl ? end : endNl;
      rows.push({ off: off, end: rowEndByte, pass: pass });
      if (end >= endNl || wrap === false) break;      // BRO-014: no-wrap = 1 row
      off = end;
    }
  });
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
  //  URI-013: rebuild via URI.make (path is repo-relative/rootless, no authority,
  //  no query; the `#L<n>` line anchor rides the fragment slot) — byte-identical to
  //  the old `scheme:path#L<n>` / `path#L<n>` concat, with that concat as a fallback.
  if (u.scheme && BRO_KEEP_SCHEME.has(u.scheme))
    return URI.make(u.scheme, undefined, u.path, undefined, "L" + line) ||
           (u.scheme + ":" + u.path + "#L" + line);
  return URI.make(undefined, undefined, u.path, undefined, "L" + line) ||
         (u.path + "#L" + line);
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
    //  DIFF colour: a hunk whose toks carry diff sides renders through the JS
    //  two-pass side→bg renderer (colorDiffHunk) — NOT the C .color() (whose
    //  diff branch is retired).  Plain/tlv and non-diff colour stay single-
    //  sourced through the C binding.
    if (mode === "color" && log.toks && hasDiffSides(log.toks)) {
      let uriStr = "";
      try { const u = log.uri; uriStr = typeof u === "string" ? u : utf8.Decode(u); }
      catch (e) { uriStr = ""; }
      const b = colorDiffHunk(uriStr, log.text, log.toks, 200);
      chunks.push(b); total += b.length;
      continue;
    }
    //  WHY-001: a `why:` blame hunk (hidden `O` runs) paints each token's commit
    //  pastel bg via colorWhyHunk (the diff renderer's why twin).
    if (mode === "color" && log.toks && hasWhyRuns(log.toks)) {
      let uriStr = "";
      try { const u = log.uri; uriStr = typeof u === "string" ? u : utf8.Decode(u); }
      catch (e) { uriStr = ""; }
      const b = colorWhyHunk(uriStr, log.text, log.toks, 200);
      chunks.push(b); total += b.length;
      continue;
    }
    //  WHY-001: --plain for a why hunk goes through whyPlain (the native cursor
    //  hides `U` but not `O`), else the O bytes leak into the plain text.
    if (mode !== "color" && log.toks && hasWhyRuns(log.toks)) {
      const b = whyPlain(log.text, log.toks);
      chunks.push(b); total += b.length;
      continue;
    }
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
  //  DIFF colour renderer (the ONE JS diff-colour impl) + its building blocks,
  //  exported for the pager (indexRows/paintRow pass-awareness) and tests.
  colorDiffHunk: colorDiffHunk,
  hasDiffSides: hasDiffSides,
  //  WHY-001: the blame-colour renderer.  The palette lives in the VIEW (why.js
  //  bakes each O token's `#rrggbb`); the renderer just applies whatever it reads.
  colorWhyHunk: colorWhyHunk,
  whyPlain: whyPlain,
  hasWhyRuns: hasWhyRuns,
  whyBgAt: whyBgAt,
  //  WHY-001: the per-ROW blame painter (twin of paintDiffRow) + its string form,
  //  so the pager routes a why hunk through the SAME wash the pipe --color uses.
  paintWhyRow: paintWhyRow,
  paintWhyRowStr: paintWhyRowStr,
  cellAnsi: cellAnsi,
  deltaSGR: deltaSGR,
  resetSGR: resetSGR,
  //  BRO-010: the pager paints non-diff (syntax/columnar) cells through the SAME
  //  THEME machinery (cellAnsi → deltaSGR), so it needs the ansi64 identity (A0)
  //  + equality (aEq) the speller carries state with.
  A0: A0,
  aEq: aEq,
  walkHunk: walkHunk,
  rowEndPass: rowEndPass,
  paintDiffRow: paintDiffRow,
  paintDiffRowStr: paintDiffRowStr,
};
