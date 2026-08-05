//  verbs/done/done.js — BE-040: `done KEY…` closes a ticket.  TODO-005 (ruling
//  2026-08-03) makes that TWO acts, both of them: (1) the head's `Now:` PAIR
//  becomes DONE — added when absent, changed in place when present, since the
//  pair is the state TODO-004 boards from (the legacy [DONE] mark still flips
//  alongside it) + delist its READMEs bullets; and (2) the ticket's WORKTREE,
//  if it owns one, moves to the `work/done/` discard root.
//  WORK-001 (the BE-044 reshape): `done .` / `done //KEY` is the WORKTREE form —
//  mv the work/ wt into `work/done/` (the r2 discard root: same device, no
//  EXDEV, and the work view ignores it; a name collision bumps `.2`, `.3`, … —
//  never clobbers), and a TICKET-named wt also flips its page header.
//  verbs/dont/dont.js is the [DONT] twin riding _run() here.
"use strict";

const pathlib = require("../../shared/util/path.js");
const join = pathlib.join;
const hunkrows = require("../../shared/hunkrows.js");
//  BE-040: reuse the landed BE-038 board helpers — the key SHAPE test and the
//  KEY→page probe (thin TOPIC/KEY.<ext>, fat TOPIC/KEY/README.<ext>).
const todoView = require("../../views/todo/todo.js");
const workView = require("../../views/work/work.js");

const EXTS = ["mkd", "md", "txt"];

//  BE-040: ONE uniform miss line + throw (BE-003 spirit, todo.js's miss twin).
//  WORK-001: the verb name rides in (dont shares this file's machinery).
function missV(vname, arg, code) { io.log(vname + ": " + arg + ": " + code + "\n"); throw code; }
function miss(arg, code) { missV("done", arg, code); }

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
//  WORK-001: the closing MARK is a parameter — done → DONE, dont → DONT.
function flipHeader(key, h, mark) {
  const m = mark || "DONE";
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
  if (marks > 0 && rest[i] === ":") return p + " [" + m + "]:" + rest.slice(i + 1);
  if (rest.slice(0, 2) === ": " && rest[2] === "[") {
    const rb = rest.indexOf("] ");
    if (rb >= 3 && ucOnly(rest.slice(3, rb))) return p + ": [" + m + "] " + rest.slice(rb + 2);
  }
  if (rest.slice(0, 2) === ": " && rest.length > 2 && rest[2] !== "[")
    return p + " [" + m + "]: " + rest.slice(2);
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

//  TODO-005 (ruling 2026-08-03): the `Now:` PAIR is the ticket's STATE — the
//  [/meta/todo] pair TODO-004 boards from — so closing ADDS it, or CHANGES it in
//  place when the head already carries one.  The pair block is the run of
//  `Key: value` lines DIRECTLY under the header (todo.js pageHead's rule), so a
//  fresh pair is inserted as the FIRST line of that run.  Returns the new text.
const NOWRE = /^Now:(?: |$)/;
const PAIRRE = /^[A-Z][a-z][a-z]: /;
function setNow(text, mark) {
  const lines = text.split("\n");
  let i = 1;
  while (i < lines.length && (PAIRRE.test(lines[i]) || NOWRE.test(lines[i]))) {
    if (NOWRE.test(lines[i])) { lines[i] = "Now: " + mark; return lines.join("\n"); }
    i++;
  }
  lines.splice(1, 0, "Now: " + mark);
  return lines.join("\n");
}

//  A header line → its title (the `#` markers + padding stripped, todo.js style).
function titleOf(h) {
  let i = 0;
  while (i < h.length && h[i] === "#") i++;
  while (i < h.length && h[i] === " ") i++;
  return h.slice(i);
}

//  Flip ONE existing page's header to [mark] + delist its README bullets.
//  Extracted from doneOne (WORK-001) so the wt form shares the exact flip.
function flipOne(w, file, mark, vname, row) {
  const text = readText(file);
  if (text == null) missV(vname, w, "TODONONE");
  const nl = text.indexOf("\n");
  const head = nl < 0 ? text : text.slice(0, nl);
  //  TODO-005: the PAIR is the state, so it is set first and UNCONDITIONALLY —
  //  that is what makes DONE↔DONT a real change and closes a mark-less page.
  //  The legacy [MARK] flip rides along when the header is one of the three
  //  shapes it knows; an odd header still gets its pair, and says so.
  const flipped = flipHeader(w, head, mark);
  if (flipped === null) {
    //  BE-040 r2: the skip must be a VISIBLE row — a rowless run answered the
    //  pager's `:done KEY` with "no hunks" (io.log never reaches the pager).
    //  A header this verb cannot parse is left ENTIRELY alone, pair included:
    //  no ruling overturns that guarantee, so an odd page is never rewritten.
    io.log(vname + ": " + w + ": odd header, skipped: " + head + "\n");
    row(w + " odd header, skipped: " + head);
    return;
  }
  //  TODO-005: the PAIR is the state, so it is set alongside the legacy mark —
  //  added when the head carries none, changed in place when it does.  That is
  //  what makes DONE↔DONT a real change and closes a pair-less page.
  let out = setNow(text, mark);
  const n2 = out.indexOf("\n");
  out = n2 < 0 ? flipped : flipped + out.slice(n2);
  if (out === text) { row(titleOf(head) + " (already closed)"); return; }
  writeText(file, out);
  //  BE-038 curated-open: closing must ALSO delist the key's bullet from the
  //  topic README and the board README (meta/todo "delist from parents").
  const dir = boardDir();
  const tR = dir ? readmeFile(join(dir, w.slice(0, w.indexOf("-")))) : null;
  if (tR) delist(tR, w);
  const bR = dir ? readmeFile(dir) : null;
  if (bR) delist(bR, w);
  row(titleOf(flipped));
}

//  BE-040: close ONE key — flip the header, delist the bullet from the topic
//  README AND the board README, emit one confirmation row (key + title).
//  TODO-005: …and DISCARD the worktree it owns, when it owns one (no wt → the
//  page edit alone).  Both halves of the closing contract, one spell.
function doneOne(w, dir, row, mark, vname) {
  if (todoView.shape(w) !== "key") missV(vname, w, "TODONONE");
  const file = todoView.pageFile(dir, w);
  if (!file) missV(vname, w, "TODONONE");
  flipOne(w, file, mark, vname, row);
  const wt = wtForKey(w);
  if (wt) moveWt(wt.dir, wt.root, vname, row);
}

function exists(p) { try { io.lstat(p); return true; } catch (e) { return false; } }

//  TODO-005: MOVE one work/ worktree into the `work/done/` discard root (made on
//  demand; a name collision bumps `.2`, `.3`, … — never clobbers).  Same device,
//  so no EXDEV, and the work view ignores `done/` entirely.
function moveWt(wt, workR, vname, row) {
  const name = pathlib.basename(wt);
  const root = join(workR, "done");
  try { io.mkdir(root); } catch (e) {}          // FILEMakeDirP: idempotent
  let target = join(root, name);
  for (let n = 2; exists(target); n++) target = join(root, name + "." + n);
  try { io.rename(wt, target); }
  catch (e) {
    io.log(vname + ": cannot move " + wt + " to " + target + ": " + String(e) + "\n");
    throw vname + ": move failed";
  }
  row("mov " + name + " -> " + target, "mov");
  return name;
}

//  TODO-005: the worktree a ticket KEY owns — WORK-010's match run BACKWARDS
//  (the base key a `work/` name carries, suffix-tolerant), through work.js's own
//  gated listing; null when the ticket has none.  Closing a ticket discards its
//  wt, so `done KEY` and `done .` end in the same place.
function wtForKey(key) {
  const workR = (typeof be !== "undefined" && be.workRoot) ? be.workRoot() : null;
  if (!workR) return null;
  let names;
  try { names = workView.listWork(workR); }
  catch (e) { return null; }
  for (const nm of names)
    if (todoView.ticketKey(nm) === key) return { dir: join(workR, nm), root: workR };
  return null;
}

//  WORK-001: the WORKTREE form — `done .` (the O invite runs in the row's own
//  `//KEY` context) or an explicit `//KEY`.  ONLY a direct work/ child ever
//  moves; refusal is plain words.  R2: the discard root is `work/done/` (made
//  on demand); a name collision bumps `.2`, `.3`, … — never clobbers.
function wtOne(w, mark, vname, row) {
  let wt = null;
  if (w === "" || w === ".") {
    let repo = (typeof be !== "undefined" && be.repo) || null;
    if (!repo && typeof be !== "undefined" && be.treeAt) {
      try { repo = be.treeAt(); } catch (e) { repo = null; }
    }
    wt = repo && repo.wt;
  } else {
    try { wt = be.wtdir(w); } catch (e) { wt = null; }
  }
  const workR = (typeof be !== "undefined" && be.workRoot) ? be.workRoot() : null;
  const name = wt ? pathlib.basename(wt) : "";
  if (!wt || !workR || name === "done" || wt !== join(workR, name)) {
    io.log(vname + ": " + (wt || w || ".") + " is not a work/ worktree — nothing moved\n");
    throw vname + ": not a work/ worktree";
  }
  moveWt(wt, workR, vname, row);
  //  A TICKET-named wt also flips its page; a page-less / non-ticket wt just moves.
  if (todoView.shape(name) === "key") {
    const dir = boardDir();
    const file = dir ? todoView.pageFile(dir, name) : null;
    if (file) flipOne(name, file, mark, vname, row);
  }
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its args reading `be`.  File edits
//  + the wt move ONLY — never a wtlog row, never a post; the user reviews and
//  lands the tree.  Shared by done ([DONE]) and dont ([DONT]) via _run.
function run(argv, mark, vname) {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  const out = sink ? hunkrows(sink, null) : null;
  let opened = false;
  //  BE-040: rows open the `todo` banner LAZILY (a pure-miss run stays rowless);
  //  the banner is the board spell, so a pager click lands back on the board.
  function row(text, verb) {
    if (out) { if (!opened) { opened = true; out.open("todo"); } out.row(text, verb || "done", 0n); }
  }
  try {
    if (!argv.length) missV(vname, "", "TODONONE");
    for (let i = 0; i < argv.length; i++) {
      const w = String(argv[i] == null ? "" : argv[i]);
      //  WORK-001: "."/""/"//KEY" = the wt form; a TICKET key = the page form.
      if (w === "" || w === "." || w.slice(0, 2) === "//") {
        wtOne(w, mark, vname, row);
      } else {
        const dir = boardDir();
        if (!dir) missV(vname, "todo/", "TODONONE");
        doneOne(w, dir, row, mark, vname);
      }
    }
  } finally { if (out) out.done(); }
}

function done() { return run(arguments, "DONE", "done"); }
done.jab = "args";
module.exports = done;
//  WORK-001: dont.js rides the same machinery with the [DONT] mark.
module.exports._run = run;
