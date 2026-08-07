//  shared/ticketpage.js — CODE-030: THE ticket-page family, extracted verbatim
//  out of views/todo/todo.js.  The board's LEXER (shape/ticketKey/keyTopic),
//  its ROOT (boardDir), its FILE LADDER (pageFile/readBytes) and the page HEAD
//  read (pageHead/pageTitle + the [MARK] span) are a library, not a view: the
//  `work` board and the `ticket` view read them too, and reaching into todo.js
//  for them is what put todo↔work in a cycle.  todo.js re-exports every name
//  unchanged, so no call site moved.  NOTHING here is a second implementation.
"use strict";

const pathlib = require("./util/path.js");
const join = pathlib.join;

const EXTS = ["mkd", "md", "txt"];        // this board is .mkd-first
const CAP = 1 << 20;                       // 1 MiB page cap (tickets are small)

//  --- arg SHAPE routing (BRO-023: a pure shape test, no fs probe) -----------
function ucnumRun(w, i) {
  while (i < w.length) {
    const c = w.charCodeAt(i);
    if ((c >= 65 && c <= 90) || (c >= 48 && c <= 57)) i++;
    else break;
  }
  return i;
}
//  "GET" → topic, "GET-001" → key, anything else → null.
function shape(w) {
  if (!w.length) return null;
  const c0 = w.charCodeAt(0);
  if (c0 < 65 || c0 > 90) return null;               // must open uppercase
  const run = ucnumRun(w, 0);
  if (run === w.length) return "topic";
  if (w[run] !== "-") return null;
  let j = run + 1;
  if (j === w.length) return null;
  while (j < w.length) {
    const c = w.charCodeAt(j);
    if (c >= 48 && c <= 57) j++;
    else return null;
  }
  return "key";
}
//  WORK-010: the BASE ticket key a name CARRIES — its leading `TOPIC-NNN`,
//  IGNORING any trailing suffix (a letter run or `-word`: `PIN-1b`, `URI-016-adv`,
//  `STATUS-008-f21` all → their base key).  "" when the name does not OPEN with a
//  key.  Same char rules as shape() (one parser), just tolerant of the tail.
function ticketKey(w) {
  if (!w.length) return "";
  const c0 = w.charCodeAt(0);
  if (c0 < 65 || c0 > 90) return "";                 // must open uppercase
  const run = ucnumRun(w, 0);
  if (run === w.length || w[run] !== "-") return "";
  let j = run + 1;
  if (j === w.length || w.charCodeAt(j) < 48 || w.charCodeAt(j) > 57) return "";
  while (j < w.length && w.charCodeAt(j) >= 48 && w.charCodeAt(j) <= 57) j++;
  return w.slice(0, j);                              // TOPIC-NNN, suffix dropped
}
function keyTopic(key) { return key.slice(0, key.indexOf("-")); }


//  --- the board root --------------------------------------------------------
//  URI-016: THE board dir is be.todoRoot() — `projectRoot()+"/todo"`, one dir,
//  no probe order.  → { root, dir } when it exists, null when it does not (no
//  repo, or a project with no ticket tree).  `dir` is todoRoot() itself; NEVER
//  join(root, "todo") again — todoRoot() already carries the `todo` segment.
//  `root` is the PROJECT root, the META tree (todo/, wiki/, meta/ live under
//  it) — page links re-anchor there.
function boardDir() {
  if (typeof be === "undefined" || !be.todoRoot) return null;
  const dir = be.todoRoot();
  if (!dir) return null;
  try { if (io.stat(dir).kind !== "dir") return null; } catch (e) { return null; }
  return { root: pathlib.dirname(dir), dir: dir };
}

//  --- fs probes -------------------------------------------------------------
function readBytes(full) {
  let st;
  try { st = io.lstat(full); } catch (e) { return null; }
  if (st.kind !== "reg") return null;
  if (st.size === 0) return new Uint8Array(0);
  const size = Number(st.size) < CAP ? Number(st.size) : CAP;
  let fd;
  try { fd = io.open(full, "r"); } catch (e) { return null; }
  try { const b = io.buf(size + 16); io.readAll(fd, b, size); return b.data().slice(); }
  catch (e) { return null; }
  finally { try { io.close(fd); } catch (e) {} }
}
//  A key's page file under the board dir: thin `TOPIC/KEY.<ext>` first, then
//  fat `TOPIC/KEY/README.<ext>`; null when absent.
function pageFile(dir, key) {
  const base = join(dir, join(keyTopic(key), key));
  for (const ext of EXTS) { const p = base + "." + ext; try { io.stat(p); return p; } catch (e) {} }
  for (const ext of EXTS) { const p = join(base, "README." + ext); try { io.stat(p); return p; } catch (e) {} }
  return null;
}
//  TODO-005: ONE read per ticket yields BOTH halves of its head (the TODO-001
//  budget): the TITLE (first line, `#` markers + padding stripped) and the
//  `Key: value` META pairs DIRECTLY under it — the run ends at the first line
//  that is not a pair, so no body line can ever forge one.  The pairs the INDEX
//  answers (`Now:`/`Sev:`) come off metaOf, not this scan; the scan is for what
//  a packed row cannot give back — `Sub:`, a ticket CODE the render must read.
//  The indent is the ONE metaidx.js allows (`INDENT`): none, or four spaces —
//  real [/wiki/StrictMark] pages indent the block, and a `^Key:` anchor read
//  every ticket's meta as EMPTY (no [go] button, no `Sub:` nesting).
const META_PAIR = /^(?: {4})?([A-Z][a-z][a-z0-9]): (.*)$/;
function pageHead(file) {
  const out = { title: "", meta: {} };
  const b = readBytes(file);
  if (!b || !b.length) return out;
  let p = 0, n = 0;
  while (p < b.length) {
    let nl = p; while (nl < b.length && b[nl] !== 10) nl++;
    const line = utf8.Decode(b.slice(p, nl));
    if (n === 0) {
      let i = 0; while (i < line.length && line[i] === "#") i++;
      while (i < line.length && line[i] === " ") i++;
      out.title = line.slice(i);
    } else {
      const m = META_PAIR.exec(line);
      if (!m) break;
      if (out.meta[m[1]] === undefined) out.meta[m[1]] = m[2];
    }
    p = nl + 1; n++;
  }
  return out;
}
//  Is this ONE line a head meta pair?  The site renderer's intro scan asks —
//  the grammar stays here, never a second copy (verbs/mark/render.js pageMeta).
function isMetaPair(line) { return META_PAIR.test(line); }
//  A page's TITLE alone (the work view's post-message read).
function pageTitle(file) { return pageHead(file).title; }

//  The header MARK's [ … ] span (the `[` and `]` char indices) — an UPPERCASE
//  `[…]` word right after the key, either side of the colon (`KEY [MARK]:` or
//  `KEY: [MARK] `); null when absent/malformed.  headerMark/stripMark share it.
function markSpan(key, title) {
  if (title.indexOf(key) !== 0) return null;
  let i = key.length;
  while (title[i] === " ") i++;
  if (title[i] === ":") { i++; while (title[i] === " ") i++; }
  if (title[i] !== "[") return null;
  let j = i + 1;
  while (j < title.length) {
    const c = title.charCodeAt(j);
    if (c >= 65 && c <= 90) j++;
    else break;
  }
  return (j > i + 1 && title[j] === "]") ? { i: i, j: j } : null;
}
//  The header MARK text (`OPEN`/`HIGH`/… ); "" when absent (both placements).
function headerMark(key, title) {
  const s = markSpan(key, title);
  return s ? title.slice(s.i + 1, s.j) : "";
}
//  WORK-008: the title with its [MARK] token stripped (both placements), colon
//  spacing normalized to `KEY: title`; a markless title passes through as-is.
function stripMark(key, title) {
  const s = markSpan(key, title);
  if (!s) return title;
  const before = title.slice(0, s.i).replace(/\s+$/, "");
  const after = title.slice(s.j + 1).replace(/^\s+/, "");
  const sep = before[before.length - 1] === ":" && after && after[0] !== ":" ? " " : "";
  return before + sep + after;
}

module.exports = { EXTS: EXTS, CAP: CAP,
                   shape: shape, ticketKey: ticketKey, keyTopic: keyTopic,
                   boardDir: boardDir, pageFile: pageFile, readBytes: readBytes,
                   pageHead: pageHead, pageTitle: pageTitle, isMetaPair: isMetaPair,
                   META_PAIR: META_PAIR,
                   markSpan: markSpan, headerMark: headerMark,
                   stripMark: stripMark };
