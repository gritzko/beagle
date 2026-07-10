//  views/todo/todo.js — BE-038: the read-only ticket-board view.  `todo` shows
//  the open-ticket board (topics + one-liner titles), `todo GET` one topic's
//  list, `todo GET-001` the ticket page itself (thin `todo/GET/GET-001.mkd` or
//  fat `todo/GET/GET-001/README.mkd`).  Args route by SHAPE (bare / TOPIC /
//  TOPIC-123 — the `uc ucnum* "-" dgt+` key rule), never by path resolution;
//  a miss is ONE uniform line + throw (BE-003 spirit): `todo: <arg>: TODONONE`.
//
//  The ticket tree root comes from be.todoRoot() (BRO-012 order: $TODO_ROOT,
//  current wt, launch wt, the hive's meta root) — the first root owning a
//  `todo/` dir wins; NOTHING is hardcoded.  List rows and in-page ticket keys
//  carry hidden `U` word spells (`todo <KEY>`, URI-014) so a pager click
//  re-enters the view; `todo/done/` (closed tickets) never lists.
//
//  OPEN filter (ruling 2026-07-10, header-grep): the ticket's OWN header line
//  is the truth — `#   KEY [MARK]: title` (or `KEY: [MARK] title`).  A state
//  mark `[DONE]`/`[WONTFIX]` closes the ticket (hidden from board + topic
//  lists); priority marks `[CRIT]`/`[HIGH]`/`[MED]`/`[LOW]` sort a topic CRIT
//  → HIGH → MED/unmarked → LOW (then numeric); an unknown mark shows and reads
//  open/normal so the vocabulary can grow ([JS], [UMBRELLA] live already).
//  Topic READMEs are landing pages, NEVER an index (they go stale);
//  `todo KEY` renders any page regardless — direct addressing always works.
//  Page reflinks resolve via the page's OWN refdef footer: a ticket-file
//  target re-enters `todo <KEY>`, any other in-tree page becomes the word
//  spell `cat <meta-root-relative-path>` (right when the pager's context tree
//  IS the meta root; cross-tree authority is a pending ruling).
"use strict";

const pathlib = require("../../shared/util/path.js");
const join    = pathlib.join;
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const ticket  = require("../../shared/ticket.js");    // BRO-012: shared key scan

const EMPTY32 = new Uint32Array(0);
const EXTS = ["mkd", "md", "txt"];        // this board is .mkd-first
const CAP = 1 << 20;                       // 1 MiB page cap (tickets are small)

//  tok32 (dog/tok/TOK.h): [31..27] tag (A+n)  [23..0] end byte offset.
function tokPack(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }
const TAG_U = tagCode("U");
const TAG_F = tagCode("F");
const TAG_S = tagCode("S");
const TAG_N = tagCode("N");
//  BE-040 r3: the BE-041 house button pair — a visible 'Y' label + a hidden
//  'O' click spell (`_uriAt` follows the O verbatim; plain never emits them).
const TAG_Y = tagCode("Y");
const TAG_O = tagCode("O");

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
function keyTopic(key) { return key.slice(0, key.indexOf("-")); }

//  --- the board root --------------------------------------------------------
//  First be.todoRoot() root that OWNS a `todo/` dir → { root, dir }; null when
//  none does.  `root` is the META tree (todo/, wiki/, meta/ live under it) —
//  page links re-anchor there.
function boardDir() {
  if (typeof be === "undefined" || !be.todoRoot) return null;
  for (const root of be.todoRoot()) {
    const d = join(root, "todo");
    try { if (io.stat(d).kind === "dir") return { root: root, dir: d }; }
    catch (e) { /* next */ }
  }
  return null;
}

//  --- fs probes -------------------------------------------------------------
function isDir(p)  { try { return io.stat(p).kind === "dir"; } catch (e) { return false; } }
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
//  A page's TITLE = its first line, `#` markers + padding stripped.
function pageTitle(file) {
  const b = readBytes(file);
  if (!b || !b.length) return "";
  let nl = 0; while (nl < b.length && b[nl] !== 10) nl++;
  let s = utf8.Decode(b.slice(0, nl));
  let i = 0; while (i < s.length && s[i] === "#") i++;
  while (i < s.length && s[i] === " ") i++;
  return s.slice(i);
}

//  The header MARK: an UPPERCASE `[…]` word right after the key, either side
//  of the colon — `#   KEY [MARK]: title` or `#   KEY: [MARK] title` (both
//  live on the board); "" when absent/malformed.
function headerMark(key, title) {
  if (title.indexOf(key) !== 0) return "";
  let i = key.length;
  while (title[i] === " ") i++;
  if (title[i] === ":") { i++; while (title[i] === " ") i++; }
  if (title[i] !== "[") return "";
  let j = i + 1;
  while (j < title.length) {
    const c = title.charCodeAt(j);
    if (c >= 65 && c <= 90) j++;
    else break;
  }
  return (j > i + 1 && title[j] === "]") ? title.slice(i + 1, j) : "";
}
const CLOSED = { DONE: true, WONTFIX: true };
const PRIO = { CRIT: 0, HIGH: 1, MED: 2, LOW: 3 };   // unmarked / unknown = 2

//  List one topic dir's tickets: `KEY.<ext>` files + fat `KEY/` dirs whose key
//  matches the topic, priority- then numeric-sorted.  Returns
//  [{ key, title, mark }] — ALL tickets, open and closed alike.
function listTopic(dir, topic) {
  const tdir = join(dir, topic);
  let names; try { names = io.readdir(tdir); } catch (e) { return []; }
  const out = [];
  for (let nm of names) {
    //  io.readdir marks a dir entry with a trailing "/" (a fat `KEY/` ticket).
    const dirEnt = nm.length && nm[nm.length - 1] === "/";
    if (dirEnt) nm = nm.slice(0, -1);
    let key = nm;
    const dot = nm.indexOf(".");
    if (dot > 0) {
      if (dirEnt || EXTS.indexOf(nm.slice(dot + 1)) < 0) continue;
      key = nm.slice(0, dot);
    }
    if (shape(key) !== "key" || keyTopic(key) !== topic) continue;   // README etc
    if (dot < 0 && !dirEnt && !isDir(join(tdir, key))) continue;
    const file = pageFile(dir, key);
    if (!file) continue;
    if (!out.some(function (e) { return e.key === key; })) {
      const title = pageTitle(file);
      out.push({ key: key, title: title, mark: headerMark(key, title) });
    }
  }
  out.sort(function (a, b) {
    const ap = PRIO[a.mark] !== undefined ? PRIO[a.mark] : 2;
    const bp = PRIO[b.mark] !== undefined ? PRIO[b.mark] : 2;
    if (ap !== bp) return ap - bp;
    const an = parseInt(a.key.slice(a.key.indexOf("-") + 1), 10);
    const bn = parseInt(b.key.slice(b.key.indexOf("-") + 1), 10);
    return an - bn;
  });
  return out;
}

//  RULING 2026-07-10 (header-grep): one topic's LISTING = its ticket files
//  whose OWN header lacks a closed mark ([DONE]/[WONTFIX]).  No README index.
function openTickets(dir, topic) {
  const files = listTopic(dir, topic).filter(function (t) { return !CLOSED[t.mark]; });
  return { topic: topic, tickets: files };
}
//  The board's topics: every UPPERCASE-shaped subdir with >=1 ticket ("done" —
//  the closed-ticket parking lot — and lowercase/mixed dirs never list).
function listTopics(dir) {
  let names; try { names = io.readdir(dir); } catch (e) { return []; }
  const out = [];
  for (let nm of names) {
    const dirEnt = nm.length && nm[nm.length - 1] === "/";
    if (dirEnt) nm = nm.slice(0, -1);
    if (nm === "done" || shape(nm) !== "topic") continue;
    if (!dirEnt && !isDir(join(dir, nm))) continue;
    const g = openTickets(dir, nm);
    if (g.tickets.length) out.push(g);
  }
  out.sort(function (a, b) { return a.topic < b.topic ? -1 : a.topic > b.topic ? 1 : 0; });
  return out;
}

//  --- hunk building (the ls.js emitHunk model) -------------------------------
//  Append one text span + tag; returns the new offset.
function span(parts, spans, off, text, tag) {
  const b = utf8.Encode(text);
  parts.push(b);
  spans.push([tag, off + b.length]);
  return off + b.length;
}
//  One list row: `<indent><KEY><rest>\n` with the KEY an `F` token followed by
//  the hidden `U` spell `todo <KEY>` (pager _uriAt: visible tok → next-U → go).
//  BE-040 r3: `btn` grows the BE-041 button tail — ` ` sep, visible Y `[done]`,
//  hidden O `done KEY` — AFTER the U nav so the title click still navigates.
function titleRow(parts, spans, off, indent, key, title, btn) {
  const rest = title.indexOf(key) === 0 ? title.slice(key.length) : " " + title;
  if (indent) off = span(parts, spans, off, indent, TAG_S);
  off = span(parts, spans, off, key, TAG_F);
  off = span(parts, spans, off, "todo " + key, TAG_U);
  if (!btn) return span(parts, spans, off, rest + "\n", TAG_S);
  off = span(parts, spans, off, rest + " ", TAG_S);
  off = span(parts, spans, off, "[done]", TAG_Y);
  off = span(parts, spans, off, "done " + key, TAG_O);
  off = span(parts, spans, off, "\n", TAG_S);
  return off;
}
function feed(sink, banner, parts, spans, off) {
  const body = new Uint8Array(off);
  let p = 0;
  for (const part of parts) { body.set(part, p); p += part.length; }
  const toks = new Uint32Array(spans.length);
  for (let i = 0; i < spans.length; i++) toks[i] = tokPack(spans[i][0], spans[i][1]);
  sink.feed(banner, body, toks, "", 0n);
}

//  The board / one topic, as ONE hunk of title rows.  BE-040 r3: `btns` puts a
//  `[done]` button on every OPEN list row (pager-only; plain passes false).
function emitList(sink, banner, groups, headers, btns) {
  const parts = [], spans = [];
  let off = 0;
  for (const g of groups) {
    if (headers) {                       // topic header row, itself a target
      off = span(parts, spans, off, g.topic, TAG_N);
      off = span(parts, spans, off, "todo " + g.topic, TAG_U);
      off = span(parts, spans, off, "\n", TAG_S);
    }
    for (const t of g.tickets)
      off = titleRow(parts, spans, off, headers ? "  " : "", t.key, t.title, btns);
    if (!g.tickets.length)               // an explicit `todo TOPIC`, all closed
      off = span(parts, spans, off, (headers ? "  " : "") +
        "(no open tickets in todo/" + g.topic + "/)\n", TAG_S);
  }
  feed(sink, banner, parts, spans, off);
}

//  --- page links --------------------------------------------------------------
//  The page's reflink DEFINITIONS: `[name]: <target> …` footer lines → a
//  name→target map (char-scan, one line each; a URL target stays inert later).
function refdefs(text) {
  const map = {};
  for (const line of text.split("\n")) {
    if (line[0] !== "[") continue;
    const rb = line.indexOf("]");
    if (rb <= 1 || line[rb + 1] !== ":") continue;
    let i = rb + 2;
    while (i < line.length && (line[i] === " " || line[i] === "\t")) i++;
    let j = i;
    while (j < line.length && line[j] !== " " && line[j] !== "\t") j++;
    if (j > i) map[line.slice(1, rb)] = line.slice(i, j);
  }
  return map;
}
function isReg(p) { try { return io.stat(p).kind === "reg"; } catch (e) { return false; } }
//  A link TARGET (refdef path, or an inline `/pocket/Page` shortcut) → its U
//  word spell: a ticket file (`KEY.<ext>` basename) re-enters `todo KEY`; any
//  other page resolves against the page's dir, re-anchors META-ROOT-relative
//  and opens as `cat <rel>` (extensionless shortcuts probe `.mkd/.md/.txt`).
//  Scheme'd targets (http:, mailto:) and NAVESCAPE climbs stay inert.  No
//  absolute fs path ever reaches a token; the spell text is all we compose.
function targetSpell(board, pageDirRel, target) {
  if (!target || target.indexOf(":") >= 0) return null;
  const base = pathlib.basename(target);
  const dot = base.lastIndexOf(".");
  const stem = dot > 0 ? base.slice(0, dot) : base;
  const ext = dot > 0 ? base.slice(dot + 1) : "";
  if (ext && EXTS.indexOf(ext) >= 0 && shape(stem) === "key" && pageFile(board.dir, stem))
    return "todo " + stem;
  let rel;
  try { rel = pathlib.resolveInTree(target[0] === "/" ? "" : pageDirRel, target); }
  catch (e) { return null; }                            // NAVESCAPE → no link
  if (!rel) return null;
  const abs = join(board.root, rel);
  if (isReg(abs)) return "cat " + rel;
  if (!ext) for (const e2 of EXTS) if (isReg(abs + "." + e2)) return "cat " + rel + "." + e2;
  return null;
}

//  A ticket page: raw .mkd bytes; non-plain modes tokenize with the mkd
//  grammar and splice a hidden `U` after every RESOLVABLE link token (the
//  cat.js withLinks model, board-scoped): a bare/`[KEY]` ticket key →
//  `todo KEY`; a `[ref]`/`[/pocket/Page]` reflink → its refdef target's
//  spell (todo/cat).  The page's OWN key gets no self-link.
function emitPage(sink, board, key, file, mode) {
  const bytes = readBytes(file);
  if (bytes == null) return false;
  let body = bytes, toks = EMPTY32;
  if (mode !== "plain") {
    try { toks = tok.parse(bytes, "mkd"); } catch (e) { toks = EMPTY32; }
    if (toks.length) {
      const pfx = board.root + "/";
      const rel = file.indexOf(pfx) === 0 ? file.slice(pfx.length) : "";
      const linked = pageLinks(board, key, pathlib.dirname(rel), bytes, toks);
      body = linked.body; toks = linked.toks;
    }
  }
  sink.feed("todo " + key, body, toks, "", 0n);
  return true;
}
function pageLinks(board, selfKey, pageDirRel, body, toks) {
  const defs = refdefs(utf8.Decode(body));
  const us = new Array(toks.length);
  let extra = 0, nlinks = 0, prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = tokEnd(toks[i]);
    us[i] = null;
    const tg = tokTagL(toks[i]);
    //  A bare key is an `F` token; a reflink `[X]` is one `G` token,
    //  brackets included — strip them before the shape/refdef lookup.
    if ((tg === "F" || tg === "G") && end > prev) {
      let word = utf8.Decode(body.slice(prev, end));
      if (tg === "G" && word.length > 2 && word[0] === "[" && word[word.length - 1] === "]")
        word = word.slice(1, -1);
      let spell = null;
      if (shape(word) === "key" && word !== selfKey && pageFile(board.dir, word))
        spell = "todo " + word;
      else if (tg === "G" && defs[word] !== undefined)
        spell = targetSpell(board, pageDirRel, defs[word]);
      else if (tg === "G" && word[0] === "/")            // inline [/pocket/Page]
        spell = targetSpell(board, pageDirRel, word);
      if (spell && spell !== "todo " + selfKey) us[i] = utf8.Encode(spell);
    }
    if (us[i]) { extra += us[i].length; nlinks++; }
    prev = end;
  }
  if (!nlinks) return { body: body, toks: toks };
  const out = new Uint8Array(body.length + extra);
  const ntoks = new Uint32Array(toks.length + nlinks);
  let op = 0, oi = 0;
  prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = tokEnd(toks[i]);
    for (let p = prev; p < end; p++) out[op++] = body[p];
    ntoks[oi++] = tokPack((toks[i] >>> 27) & 0x1f, op);
    if (us[i]) { out.set(us[i], op); op += us[i].length; ntoks[oi++] = tokPack(TAG_U, op); }
    prev = end;
  }
  return { body: out, toks: ntoks };
}
function tokTagL(w) { return String.fromCharCode(65 + ((w >>> 27) & 0x1f)); }
function tokEnd(w) { return w & 0xffffff; }

//  --- the verb ---------------------------------------------------------------
//  BE-003 spirit: ONE uniform miss line, then throw (jab maps it to exit!=0).
function miss(arg, code) { io.log("todo: " + arg + ": " + code + "\n"); throw code; }

function todoOne(arg, board, mode, sink) {
  //  DIS-060: tolerate the scheme'd `todo:GET-1` spell form via ONE parse.
  let w = String(arg == null ? "" : arg);
  if (w.indexOf(":") >= 0) {
    try { const p = uri._parse(w); if (p.scheme === "todo") w = p.path || ""; } catch (e) {}
  }
  if (w === "" || w === ".") {
    emitList(sink, "todo", listTopics(board.dir), true, mode !== "plain");
    return;
  }
  const s = shape(w);
  if (s === "topic") {
    if (!isDir(join(board.dir, w))) miss(w, "TODONONE");
    emitList(sink, "todo " + w, [openTickets(board.dir, w)], false, mode !== "plain");
    return;
  }
  if (s === "key") {
    //  Direct addressing ALWAYS works — open or closed, the page renders.
    const file = pageFile(board.dir, w);
    if (!file || !emitPage(sink, board, w, file, mode)) miss(w, "TODONONE");
    return;
  }
  miss(w, "TODONONE");
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its args reading `be`.
function todo() {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  if (!sink) return;
  const board = boardDir();
  if (!board) miss("todo/", "TODONONE");
  const mode = ambient.format();
  const argv = arguments.length ? arguments : [""];
  for (let i = 0; i < argv.length; i++) todoOne(argv[i], board, mode, sink);
}
todo.jab = "args";
module.exports = todo;
//  BE-038: expose the internals for the repro test (the ls.js/log.js model).
module.exports.shape = shape;
module.exports.listTopics = listTopics;
module.exports.pageFile = pageFile;
