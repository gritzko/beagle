//  verbs/done/done.js — BE-040: `done KEY…` closes a ticket: flip the page
//  header [DONE] (the 3 mark-done.sh forms) + delist its READMEs bullets.
"use strict";

const pathlib = require("../../shared/util/path.js");
const join = pathlib.join;
const hunkrows = require("../../shared/hunkrows.js");
//  BE-040: reuse the landed BE-038 board helpers — the key SHAPE test and the
//  KEY→page probe (thin TOPIC/KEY.<ext>, fat TOPIC/KEY/README.<ext>).
const todoView = require("../../views/todo/todo.js");

const EXTS = ["mkd", "md", "txt"];

//  BE-040: ONE uniform miss line + throw (BE-003 spirit, todo.js's miss twin).
function miss(arg, code) { io.log("done: " + arg + ": " + code + "\n"); throw code; }

//  BE-040/URI-016: the board dir — be.todoRoot() itself (`projectRoot()+"/todo"`),
//  the same ONE dir views/todo/todo.js boards from.  No probe order, and no
//  join(root, "todo"): todoRoot() already ends in `todo`.
function boardDir() {
  if (typeof be === "undefined" || !be.todoRoot) return null;
  const dir = be.todoRoot();
  if (!dir) return null;
  try { return io.stat(dir).kind === "dir" ? dir : null; } catch (e) { return null; }
}

function readText(full) {
  let st; try { st = io.lstat(full); } catch (e) { return null; }
  if (st.kind !== "reg") return null;
  const size = Number(st.size);
  if (size === 0) return "";
  let fd; try { fd = io.open(full, "r"); } catch (e) { return null; }
  try { const b = io.buf(size + 16); io.readAll(fd, b, size); return utf8.Decode(b.data().slice()); }
  finally { try { io.close(fd); } catch (e) {} }
}

function writeText(full, text) {
  const fd = io.open(full, "c");
  try {
    try { io.resize(fd, 0); } catch (e) {}
    const bytes = utf8.Encode(text);
    const b = io.buf(bytes.length + 8);
    b.feed(bytes);
    io.writeAll(fd, b);
  } finally { try { io.close(fd); } catch (e) {} }
}

function ucOnly(s) {
  for (let i = 0; i < s.length; i++) { const c = s.charCodeAt(i); if (c < 65 || c > 90) return false; }
  return true;
}

//  BE-040: the THREE mark-done.sh header forms — `KEY [MARK]:` / `KEY: [MARK] `
//  replace the mark, `KEY: title` inserts it; null = odd header (report, skip).
function flipHeader(key, h) {
  const p = "#   " + key;
  if (h.indexOf(p) !== 0) return null;
  const rest = h.slice(p.length);
  //  BE-040 RULING 2026-07-10: ONE mark is canonical, but a wild leading run of
  //  ` [MARK]`s (`[OPEN] [MED]:`) still parses — the WHOLE run collapses to [DONE].
  let i = 0, marks = 0;
  while (rest[i] === " " && rest[i + 1] === "[") {
    const rb = rest.indexOf("]", i + 2);
    if (rb < 0 || !ucOnly(rest.slice(i + 2, rb))) break;
    i = rb + 1; marks++;
  }
  if (marks > 0 && rest[i] === ":") return p + " [DONE]:" + rest.slice(i + 1);
  if (rest.slice(0, 2) === ": " && rest[2] === "[") {
    const rb = rest.indexOf("] ");
    if (rb >= 3 && ucOnly(rest.slice(3, rb))) return p + ": [DONE] " + rest.slice(rb + 2);
  }
  if (rest.slice(0, 2) === ": " && rest.length > 2 && rest[2] !== "[")
    return p + " [DONE]: " + rest.slice(2);
  return null;
}

//  BE-040: drop the key's OWN bullet lines (^\s*-\s+\[?KEY\b) from one README;
//  footer refdefs and mid-bullet mentions never match the anchor.
function delist(file, key) {
  const text = readText(file);
  if (text == null) return false;
  const lines = text.split("\n");
  const re = new RegExp("^\\s*-\\s+\\[?" + key + "\\b");
  const kept = lines.filter(function (l) { return !re.test(l); });
  if (kept.length === lines.length) return false;
  writeText(file, kept.join("\n"));
  return true;
}

function readmeFile(dir) {
  for (const ext of EXTS) {
    const p = join(dir, "README." + ext);
    try { if (io.stat(p).kind === "reg") return p; } catch (e) {}
  }
  return null;
}

//  A header line → its title (the `#` markers + padding stripped, todo.js style).
function titleOf(h) {
  let i = 0;
  while (i < h.length && h[i] === "#") i++;
  while (i < h.length && h[i] === " ") i++;
  return h.slice(i);
}

//  BE-040: close ONE key — flip the header, delist the bullet from the topic
//  README AND the board README, emit one confirmation row (key + title).
function doneOne(w, dir, row) {
  if (todoView.shape(w) !== "key") miss(w, "TODONONE");
  const file = todoView.pageFile(dir, w);
  if (!file) miss(w, "TODONONE");
  const text = readText(file);
  if (text == null) miss(w, "TODONONE");
  const nl = text.indexOf("\n");
  const head = nl < 0 ? text : text.slice(0, nl);
  if (head.indexOf("[DONE]") >= 0 || head.indexOf("[WONTFIX]") >= 0) {
    row(titleOf(head) + " (already closed)");
    return;
  }
  const flipped = flipHeader(w, head);
  if (flipped === null) {
    //  BE-040 r2: the skip must be a VISIBLE row — a rowless run answered the
    //  pager's `:done KEY` with "no hunks" (io.log never reaches the pager).
    io.log("done: " + w + ": odd header, skipped: " + head + "\n");
    row(w + " odd header, skipped: " + head);
    return;
  }
  writeText(file, nl < 0 ? flipped : flipped + text.slice(nl));
  //  BE-038 curated-open: closing must ALSO delist the key's bullet from the
  //  topic README and the board README (meta/todo "delist from parents").
  const tR = readmeFile(join(dir, w.slice(0, w.indexOf("-"))));
  if (tR) delist(tR, w);
  const bR = readmeFile(dir);
  if (bR) delist(bR, w);
  row(titleOf(flipped));
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its args reading `be`.  File edits
//  ONLY — never a wtlog row, never a post; the user reviews and lands the tree.
function done() {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  const out = sink ? hunkrows(sink, null) : null;
  let opened = false;
  //  BE-040: rows open the `todo` banner LAZILY (a pure-miss run stays rowless);
  //  the banner is the board spell, so a pager click lands back on the board.
  function row(text) {
    if (out) { if (!opened) { opened = true; out.open("todo"); } out.row(text, "done", 0n); }
  }
  const dir = boardDir();
  if (!dir) miss("todo/", "TODONONE");
  try {
    if (!arguments.length) miss("", "TODONONE");
    for (let i = 0; i < arguments.length; i++) doneOne(String(arguments[i]), dir, row);
  } finally { if (out) out.done(); }
}
done.jab = "args";
module.exports = done;
