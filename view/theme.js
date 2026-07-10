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
  D: "90", G: "32", L: "96", H: "35", R: "94", P: "90",
  N: "1",  C: "1",  F: "38;5;56", T: "38;5;56", B: "33",
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
  //  'S' default + 'A' sentinel: no entry → no paint.
};

const SLOTS_DARK = {
  D: "38;5;240", G: "38;5;37", L: "38;5;33", H: "38;5;166", R: "38;5;64",
  P: "38;5;240", N: "38;5;33;1", C: "38;5;33;1", F: "38;5;61", T: "38;5;61",
  B: "38;5;180",
  U: "38;5;33", Y: "38;5;33", V: "38;5;37", W: "38;5;64", E: "38;5;136",
  X: "38;5;166", M: "38;5;196", Q: "38;5;240", Z: "38;5;125",
};

const SLOTS_LIGHT = {
  D: "38;5;245", G: "38;5;37", L: "38;5;33", H: "38;5;166", R: "38;5;64",
  P: "38;5;245", N: "38;5;33;1", C: "38;5;33;1", F: "38;5;61", T: "38;5;61",
  B: "38;5;186",
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
  hunk: "B", eq: "D",                                    // C ULOG.c:1213/1212
};

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
  select: select,
  makeTheme: makeTheme,
};
