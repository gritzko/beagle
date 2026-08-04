//  view/theme.js — the STATIC, PLUGGABLE colour theme for the columnar
//  status/banner render (JAB-025, gritzko's ruling: "palette and theme as a
//  static JS object, pluggable").  ONE place the SGR lives — the renderer
//  (core/emit.js renderColor_) paints each output column with the SGR this
//  module hands back; it never re-rolls an escape inline.
//
//  This SUPERSEDES the inline THEME16 that used to live in view/bro.js (the
//  banner-band C path).  Where bro's TODO#3 colour TUI needs a tok-tag → SGR
//  table it can import { tags } from here too.
//
//  --- the model (mirrors dog/THEME.h + abc/ANSI.{h,c}) --------------------
//  Native single-sources every colour as an `ansi64` slot indexed by a tag
//  letter (dog/THEME.c THEME16TBL/…), then spells it per cell via
//  ANSIu8sFeedDelta(want, prev).  We mirror exactly what that emitter spells,
//  as ready-made SGR PARAMETER strings:
//    * a basic fg N (30-37/90-97)  → "<N>"          → ESC[<N>m  (e.g. "34")
//    * a 256 fg N                  → "38;5;<N>"      → ESC[38;5;<N>m
//    * bold flag                   → "1"            → ESC[1m
//  The reset BACK to default fg that ANSIu8sFeedDelta emits between a painted
//  cell and the next default cell is ESC[39m (default-fg, NOT ESC[0m) — so a
//  painted column is `ESC[<sgr>m` + bytes + `ESC[39m`.  A bold-only slot resets
//  with ESC[22m.  The banner band (header) is the one full-width pale band and
//  closes with ESC[0m (it sets bg, so a plain `39` would not clear it).
"use strict";

const ESC = "\x1b[";

//  --- slot palettes (dog/THEME.c) -----------------------------------------
//  Each map: tag letter → SGR parameter string (what ANSIu8sFeedDelta spells
//  going DEFAULT → slot).  Only the populated letters appear; an absent tag
//  paints nothing (slot 'S'/default).  The status-verb slots (Y/V/W/E/X/M/Q/Z)
//  + the columns' own tags (L date, S default) are all that the row render
//  exercises; the tok-syntax tags (D/G/H/R/P/N/C/F/T) ride along for bro.
const SLOTS_16 = {
  //  tok-syntax (bro TODO#3) — kept for completeness / pluggability.
  //  BRO-036: 'B' is the ELASTIC-field tag — neutral, no entry (like 'S').
  D: "90", G: "32", L: "96", H: "35", R: "94", P: "90",
  N: "1",  C: "1",  F: "38;5;56", T: "38;5;56",
  //  status verbs (the row render).  dog/THEME.c THEME16TBL IDX('…').
  U: "34",        // put-tok slot (unused for status; status `put` uses Y)
  Y: "34",        // put / upd / adv  — blue
  V: "36",        // post / mov       — cyan
  W: "32",        // new / add        — green
  E: "33",        // mod              — yellow
  X: "38;5;94",   // del              — 256 brown
  M: "91",        // mis/miss/cnf/modl/conflict — bright red
  Q: "90",        // unk / dirty / dir — grey
  Z: "35",        // mrg / merged     — magenta
  //  BRO-030: quad wt rides the EXISTING X slot (orange-94); staged wt is the
  //  code-26 '[' tag, painted by the pager palette (view/bro.js) — I/J are
  //  diff-side BACKGROUNDS there and must not be repurposed.
  //  'S' default + 'A' sentinel: no entry → no paint.
};

const SLOTS_DARK = {
  D: "38;5;240", G: "38;5;37", L: "38;5;33", H: "38;5;166", R: "38;5;64",
  P: "38;5;240", N: "38;5;33;1", C: "38;5;33;1", F: "38;5;61", T: "38;5;61",
  U: "38;5;33", Y: "38;5;33", V: "38;5;37", W: "38;5;64", E: "38;5;136",
  X: "38;5;166", M: "38;5;196", Q: "38;5;240", Z: "38;5;125",
};

const SLOTS_LIGHT = {
  D: "38;5;245", G: "38;5;37", L: "38;5;33", H: "38;5;166", R: "38;5;64",
  P: "38;5;245", N: "38;5;33;1", C: "38;5;33;1", F: "38;5;61", T: "38;5;61",
  U: "38;5;33", Y: "38;5;33", V: "38;5;37", W: "38;5;64", E: "38;5;136",
  X: "38;5;166", M: "38;5;196", Q: "38;5;245", Z: "38;5;125",
};

//  --- verb → slot letter (dog/ULOG.c ULOG_VERB_TAGS) ----------------------
//  The status verbs the row render emits map to a palette slot exactly as
//  ulog_verb_tag does (dog/ULOG.c:1187-1232) — mirrored bucket-for-bucket so
//  JS `be status --color` paints each row the SAME hue as native.  An unlisted
//  verb falls back to 'S' (no paint), like ULOGVerbTag returning 'S'.  The
//  native `be status` SUMMARY line (sniff/SNIFF.exe.c:417-426 STATUS_BUCKET)
//  is the per-bucket authority for the buckets ULOG_VERB_TAGS omits (`pat`).
//
//  DIS-057 introduces three buckets native `be status` has NO equivalent for —
//  `rmv` (the removal half of a move pair) and `cnf` (a DIS-057 spelling of
//  C's `conf`).  Each is mapped to its closest C family:
//    rmv → 'X'  (del/brown — the removal family; analogy, no native verb)
//    cnf → 'M'  (== C `conf` bright red, ULOG.c:1230)
//    mrg → 'Z'  (== C `mrg` magenta, ULOG.c:1211)
//    pat → 'C'  (== native summary tag, SNIFF.exe.c:421 — bold; ULOG.c has no
//                `pat` row verb, so the summary slot is the only C authority)
const VERB_SLOT = {
  put: "Y", upd: "Y", adv: "Y",                          // C ULOG.c:1194/1205/1202 — blue
  post: "V", mov: "V",                                   // C ULOG.c:1195/1200 — cyan
  rmv: "X",                                              // DIS-057 analogy → del family (brown)
  "new": "W", add: "W", applied: "W",                    // C ULOG.c:1197/1198/1217 — green
  pat: "C",                                              // DIS-057: native summary tag (SNIFF.exe.c:421) — bold
  mod: "E",                                              // C ULOG.c:1201 — yellow
  del: "X",                                              // C ULOG.c:1206 — brown
  mis: "M", miss: "M", cnf: "M", con: "M", modl: "M", conflict: "M",  // C ULOG.c:1207-1231 — bright red (STATUS-005 con / DIS-057 cnf ≡ conf)
  unk: "Q", dir: "Q", dirty: "Q",                        // C ULOG.c:1210/1214/1220 — grey
  mrg: "Z", merged: "Z",                                 // C ULOG.c:1211/1218 — magenta
  //  BRO-036: `hunk` vacates 'B' (now the elastic-field tag, gritzko's ruling:
  //  the ONE free letter) → 'E' (the same "33" in the 16 palette).
  hunk: "E", eq: "D",                                    // C ULOG.c:1213/1212
};

//  --- quad status columns (BRO-030, wiki/Status.mkd) ----------------------
//  The GLYPH carries the column color (truecolor fg — the 256-cube orange
//  reads muddy on xterm, gritzko 2026-07-17); the staged wt char INVERTS
//  (column color as bg), conflict inverts to red.  Inverted fg is RGB white,
//  NOT the palette's 97 "bright white" (themes remap it — beige on solarized).
//  '.' (same) is unpainted.  The pager mirrors these in view/bro.js ('['..'`').
const QUAD_SGR = {
  track:  "38;2;30;144;255",                   // blue glyph
  base:   "38;2;0;180;70",                     // green glyph
  patch:  "38;2;220;160;0",                    // amber glyph
  wt:     "38;2;255;140;0",                    // orange glyph
  staged: "38;2;255;255;255;48;2;255;140;0",   // staged wt — white on orange
  con:    "38;2;255;255;255;48;2;220;40;40",   // conflicted wt — white on red
};

//  --- diff word-wash slots (DIFF-016, DIS-080) ----------------------------
//  The diff render washes a changed token with a 256-colour BACKGROUND; each
//  provenance gets a PALE tint (the token seen from the other pass) and a WASH
//  (the token on its own pass).  `local` is the historical I/O/J/K salad/salmon
//  pair (dog/THEME.h); `patched` (a patch-in's theirs token) is the DIS-080
//  addition.  DIFF-020: 4 washes, no conflict colour — an overlap is the two
//  families meeting.  view/bro.js THEME mirrors these — keep in lockstep.
//  Palette rule (gritzko 2026-08-03): the MAXED channel is the axis (G=insert,
//  R=remove), the BLUE channel is the provenance — ours 175, theirs 95.
const DIFF_WASH = {
  inPale: 194, inWash: 157,      // local insert   — salad green (215/175,255,215/175)
  rmPale: 224, rmWash: 217,      // local remove   — salmon      (255,215/175,215/175)
  pinPale: 192, pinWash: 155,    // patched-in insert — lime     (215/175,255,135/95)
  prmPale: 222, prmWash: 215,    // patched-in remove — orange   (255,215/175,135/95)
};

//  --- the BUTTON palette (TODO-005, gritzko's ruling) ---------------------
//  A clickable view BUTTON is TWO CELLS carrying its tone as FOREGROUND over a
//  VERY PALE wash of that same tone — enough background to read as a button,
//  never an inversion (ruled 2026-08-03).  Both colours are TRUECOLOR, so they
//  ride the WHY-001 mechanism rather than a tok tag: the view bakes
//  `#<pale><tone> ` (the bg slot then the fg slot; view/bro.js whyBgAt) onto the
//  button's own hidden `O`, and the 32-code tag space — which is full — needs no
//  new slot.  A DISABLED button is plain grey fg with NO background at all.
//  ONE block, one place to retune; every view button reads it.
const BTN = {
  //  the three NAMED brand tones for the GLYPH buttons (gritzko).
  status: "#0085ca",   // Pantone Process Blue  — `status //<wt>`, face " i"
  ci:     "#00a95c",   // Hexachrome Green      — `post '<msg>'`, face " ✓"
  log:    "#ffd02e",   // Pantone Dandelion     — `log //<wt>`,   face " ≡"
                       // (13-0758 TCX; replaces 437, which was near-invisible
                       //  as a foreground on a dark terminal)
  //  The COUNT buttons are two PANELS, each a blue/green/red triad (gritzko's
  //  ruling 2026-08-03 — explicit hexes, superseding the earlier rotated trios).
  //  Position still says which slot; the colour says which KIND of change.
  chg:    "#3647c9",   // blue   — changed,  bare `put`
  add:    "#47c936",   // green  — new,      `put +`
  del:    "#c7384d",   // red    — deleted,  bare `delete`
  patch:  "#8420df",   // violet — diverged, `patch`
  post:   "#1fe084",   // green  — ahead,    bare `post`
  get:    "#ef8310",   // orange — behind,   bare `get`
  //  TODO-005 [go]: MINT this ticket's worktree from its `Rep:` repo.  Pantone
  //  Shocking Orange — the one CREATE action on the board, so it owns a tone
  //  outside both trios.
  go:     "#ff6d2b",
  //  TODO-005: the trailing DONE/DONT panel (hexes ruled 2026-08-03).
  done:   "#3bc43d",   // green — ` ✓` closes
  dont:   "#c2803d",   // ochre — ` ✗` shelves
  //  CI-004 (ruling 2026-08-04): RUN this tree's default build+test, face ` ∞`.
  //  Its own slot — `ci` above is the commit ✓, a different act entirely.  A
  //  LANDED verdict tints the button instead: the add/del green and red, the
  //  board's existing pass/fail vocabulary, never two more near-duplicate hexes.
  run:    "#5883a7",   // steel blue — a run IN FLIGHT (the ` ⋯` face, `⋯ running`)
};
//  CI-004 (TODO 11, ruling 2026-08-04): the run button's THREE colours — the
//  REMEMBERED verdict (shared/ci.js status map), and no dim/stale variants: the
//  disabled grey's truecolor twin for never-ran, then failed, then ok.  While a
//  job is in flight the button wears BTN.run — that is live state, not a verdict.
const BTN_RUN = { none: "#808080", fail: "#e1351e", ok: "#1fe033" };
//  The SAME two tones under the live layer's spelling (shared/ci.js row().state)
//  — the ci view's PASS/FAIL footer reads these.
const BTN_VERDICT = { green: BTN_RUN.ok, red: BTN_RUN.fail };
//  The pale wash is DERIVED, never hand-picked: mix the tone toward white by
//  BTN_PALE, once, for every button in every view.  Retune the factor here and
//  all nine washes move together.  Memoized — a board asks per button per row.
const BTN_PALE = 0.88;
const _pale = Object.create(null);
function pale(hex) {
  const key = String(hex);
  if (_pale[key] !== undefined) return _pale[key];
  const v = parseInt(key.slice(1), 16);
  let out = "#";
  for (let sh = 16; sh >= 0; sh -= 8) {
    const c = (v >> sh) & 0xff;
    out += Math.round(c + (255 - c) * BTN_PALE).toString(16).padStart(2, "0");
  }
  return (_pale[key] = out);
}

//  The FALLBACK tok tag per button — the nearest LEGACY 16-palette slot to each
//  tone (view/bro.js THEME).  A button face is emitted on this tag and its `O`
//  OVERRIDES it with the truecolor pair, so a reader that never gets the O (an
//  old renderer, a stale require cache, a --plain-ish path) still shows the
//  button in its class colour — it can never degrade to grey, which is the
//  DISABLED signal and must stay unambiguous.
const BTN_TAG = { status: "V", ci: "W", log: "E", chg: "V", add: "W",
                  del: "M", patch: "V", post: "G", get: "A", go: "A",
                  done: "W", dont: "M", run: "V" };

//  The button FACES.  A face is exactly two cells: a two-digit count, or a
//  space + an icon.  `ASCII` is the plain-terminal twin of each icon, kept
//  beside it so a future ascii mode is a table swap, not a code change.
//  `done` wears the HEAVY check (U+2714), not the light one the ci button
//  carries — the panel's closing act is the emphatic one (gritzko 2026-08-03).
//  CI-004: `run` has TWO faces — the idle ` ∞` (the endless loop of building and
//  testing) and the IN-PROGRESS twin a running job wears (`runb`).
const BTN_FACE = { status: " i", ci: " ✓", log: " ≡", go: "go",
                   done: " ✔", dont: " ✗", run: " ∞", runb: " ⋯" };
const BTN_FACE_ASCII = { status: " i", ci: " v", log: " =", go: "go",
                         done: " v", dont: " x", run: " 8", runb: " ." };

//  --- banner band (dog/THEME.h THEME_BANNER) ------------------------------
//  Status/header band: black fg (256:0) on pale-yellow bg (256:230); native
//  space-fills to the terminal width.  Closes with ESC[0m (it sets a bg, so a
//  default-fg `39` would leave the band open).
const BANNER_SGR = "38;5;0;48;5;230";

//  --- a theme object ------------------------------------------------------
//  paint(slotLetter)  → ESC[<sgr>m for that slot, or "" (default/no paint).
//  verbPaint(verb)    → ESC[<sgr>m for that verb's slot, or "".
//  reset(slotLetter)  → the closing SGR for a painted cell of that slot:
//                       ESC[22m for a bold-only slot (N/C — the on-code was
//                       the bold flag), else ESC[39m (default fg).  "" when the
//                       cell wasn't painted.  This mirrors ANSIu8sFeedDelta
//                       spelling the slot→DEFAULT delta.
//  bannerOpen()/bannerClose() → the header band wrap.
function makeTheme(name, slots) {
  function sgr(letter) {
    const s = slots[letter];
    return s ? ESC + s + "m" : "";
  }
  function paint(letter) { return sgr(letter); }
  function verbPaint(verb) { return sgr(VERB_SLOT[verb]); }
  function reset(letter) {
    const s = slots[letter];
    if (!s) return "";                       // not painted → no reset
    //  bold-only slot (no colour digits, just "1") resets with 22.
    return s === "1" ? ESC + "22m" : ESC + "39m";
  }
  function verbReset(verb) { return reset(VERB_SLOT[verb]); }
  return {
    name: name,
    slots: slots,
    verbSlot: VERB_SLOT,
    paint: paint,
    verbPaint: verbPaint,
    reset: reset,
    verbReset: verbReset,
    //  the columns the status row paints: date col = slot 'L', path col plain.
    dateSlot: "L",
    pathSlot: "S",
    bannerOpen: function () { return ESC + BANNER_SGR + "m"; },
    bannerClose: function () { return ESC + "0m"; },
  };
}

const THEME16 = makeTheme("16", SLOTS_16);
const THEMEDARK = makeTheme("dark", SLOTS_DARK);
const THEMELIGHT = makeTheme("light", SLOTS_LIGHT);

//  Named themes + a default.  PLUGGABLE: swap `DEFAULT` or pass a chosen
//  theme into the renderer to repaint without touching the render code.
const THEMES = { "16": THEME16, dark: THEMEDARK, light: THEMELIGHT };

//  Pick by name (env $BRO_THEME, else "16") — mirrors THEMESelect's fallback.
function select(name) {
  if (!name) name = (typeof io !== "undefined" && io.getenv && io.getenv("BRO_THEME")) || "16";
  return THEMES[name] || THEME16;
}

module.exports = {
  THEMES: THEMES,
  DEFAULT: THEME16,
  THEME16: THEME16,
  THEMEDARK: THEMEDARK,
  THEMELIGHT: THEMELIGHT,
  VERB_SLOT: VERB_SLOT,
  QUAD_SGR: QUAD_SGR,
  DIFF_WASH: DIFF_WASH,
  BTN: BTN,
  BTN_RUN: BTN_RUN,
  BTN_VERDICT: BTN_VERDICT,
  BTN_PALE: BTN_PALE,
  BTN_TAG: BTN_TAG,
  pale: pale,
  BTN_FACE: BTN_FACE,
  BTN_FACE_ASCII: BTN_FACE_ASCII,
  select: select,
  makeTheme: makeTheme,
};
