//  quadrender.js — BRO-030: render a quad model (shared/quad.js) as rows.
//  The quad REPLACES the 3-char verb column: `<date7> <quad4> <path>`.
//  The model speaks the greppable ASCII canon `.xov` (same/removed/created/
//  advanced); PLAIN output keeps it, carrying staged as UPPERCASE on the wt
//  char (`X`/`O`/`V`) and conflict as `!`.  A tty substitutes the fancy
//  glyphs (x→✗, o→+, v→↑) and carries staged as bold, conflict as red
//  (theme.QUAD_SGR; red replaces the orange wt color).  '.' is unpainted,
//  so position stays authoritative and a `| cat` line is bare `.xov`.
//  A move dst row spells its pairing `src#dst` in the path column
//  (wiki/Status.mkd: the pair must not read as delete+create).

"use strict";

const theme  = require("./theme.js");
const render = require("./render.js");

const ESC = "\x1b[";
const COLS = ["track", "base", "patch", "wt"];
//  BRO-030 tty glyphs (gritzko 2026-07-17): ● created, ∅ removed, ↑ advanced.
//  A COMMIT row's `o` means "present in this column's line", not "created" —
//  it renders ✔ (ruling 2026-07-17), so it never reads as a file state.
const TTY_GLYPH = { ".": ".", "x": "∅", "o": "●", "v": "↑" };
const COMMIT_GLYPH = { ".": ".", "x": "∅", "o": "✔", "v": "↑" };

//  Paint ONE quad string as colored CELLS (gritzko 2026-07-17): black glyph on
//  a PASTEL per-column bg unstaged, white on the DARK hue staged (wt char),
//  conflict white on dark red.  Plain mode stays the pure ASCII canon
//  (case = staged, '!' = conflict).  Cells close with ESC[0m (bg is set).
function paintQuad(quad, row, colored, glyphs) {
  const g = glyphs || TTY_GLYPH;
  const q = Array.from(quad);
  let out = "";
  for (let i = 0; i < 4; i++) {
    let ch = q[i] == null ? "." : q[i];
    if (!colored) {
      if (i === 3 && row && row.con) ch = "!";
      else if (i === 3 && row && row.staged) ch = ch.toUpperCase();
      out += ch;
      continue;
    }
    if (ch === ".") { out += ch; continue; }         // same: unpainted
    let sgr = theme.QUAD_SGR[COLS[i]];
    if (i === 3) {
      if (row && row.con) sgr = theme.QUAD_SGR.con;
      else if (row && row.staged) sgr = theme.QUAD_SGR.staged;
    }
    out += ESC + sgr + "m" + (g[ch] || ch) + ESC + "0m";
  }
  return out;
}

//  One file row: `<date7> <quad4> <path>`; a move dst row (src recorded)
//  spells the pair `src#dst` in the path column.
function fileRow(row, colored) {
  const path = (row.src && row.src !== row.path)
        ? row.src + "#" + row.path : row.path;
  return render.dateCol(row.ts) + " " + paintQuad(row.quad, row, colored)
       + " " + path;
}

//  One commit row: `<date7> <quad4> ?<hashlet>#<subject>` — the same quad
//  vocabulary one level up (presence 'o' per column whose tip reaches it).
function commitRow(c, colored) {
  return render.dateCol(c.ts) + " " + paintQuad(c.quad, null, colored, COMMIT_GLYPH)
       + " ?" + c.hashlet + (c.subject ? "#" + c.subject : "");
}

//  renderModel(model, opts) → [line, …]: commit rows first (newest-first,
//  as the model orders them), then the file rows lex by path.
function renderModel(model, opts) {
  const colored = !!(opts && opts.color);
  const lines = [];
  for (const c of model.commits) lines.push(commitRow(c, colored));
  for (const r of model.rows) lines.push(fileRow(r, colored));
  return lines;
}

//  BRO-030: the pager tok tag for quad char `ch` in column `i` (0..3) — the
//  extension codes 26..31 ('['..'`', view/bro.js THEME); '.' stays 'S'.
function charTag(i, ch, staged, con) {
  if (ch === ".") return "S";
  if (i === 0) return "[";
  if (i === 1) return "\\";
  if (i === 2) return "]";
  return con ? "`" : staged ? "_" : "^";
}

module.exports = { renderModel: renderModel, fileRow: fileRow,
                   commitRow: commitRow, paintQuad: paintQuad,
                   TTY_GLYPH: TTY_GLYPH, COMMIT_GLYPH: COMMIT_GLYPH,
                   charTag: charTag };
