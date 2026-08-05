//  views/ticket/ticket.js — TODO-011: THE one-ticket page view.
//
//  RULING (gritzko, 2026-08-04): `todo` is the TABULAR side — the open-ticket
//  board, one topic's list, the meta-pair filter listings.  The ONE-TICKET PAGE
//  is its own view, and this is it.  `ticket GET-001` renders the ticket's own
//  markdown: the thin `todo/GET/GET-001.mkd` or the fat
//  `todo/GET/GET-001/README.mkd`, off be.todoRoot() (URI-016: the project root
//  is DETECTED by a climb, never declared by an env var).  Direct addressing
//  ALWAYS works — a page is not a listing, so a CLOSED ticket (`Now: DONE`, a
//  legacy `[DONE]` header mark, or one parked in `todo/done/`) renders exactly
//  like an open one; only LISTINGS filter, and listings live in `todo`.
//
//  ONE arg, LEXICALLY classed (the `uc ucnum* "-" dgt+` key rule — no path
//  resolution in arg classing, BRO-023):
//    `ticket ABC-123`   the page
//    `ticket ABC`       REFUSED — a topic is a LIST: `todo ABC`
//    `ticket Now:OPEN`  REFUSED — a filter narrows a LISTING: `todo Now:OPEN`
//    `ticket`           REFUSED — the board with no arg is `todo`
//  Every refusal is plain words naming the view that does serve the arg.
//
//  NOTHING here is a second implementation.  CODE-030: shared/ticketpage.js is
//  the ONE home of the page family and this module reads it DIRECTLY: the
//  key/topic LEXER (shape, keyTopic), the board root (boardDir), the page-file
//  ladder (pageFile), the byte read (readBytes), the extension order (EXTS).
//  views/todo/todo.js keeps the rest of the shared ground: the arg
//  LINE grammar (parseArgs — grammatical only; each view refuses its own
//  non-classes), the whole-arg-line filter spell (spellWith/argLineWith) and
//  the two click classifiers (navSpell, filterVal).  The meta-pair block is
//  read through the ONE shared matcher, shared/metaidx.js metaBlock (TODO-003 /
//  TODO-004), never a local grammar.  What lives HERE is only what the board
//  has no use for: the mkd PAGE render and its link splicing.
//
//  CLICK SPELLS (BE-054: context-less `O` spells, the pager stays arg-blind and
//  the VERB resolves the arg — the Nav design law):
//    * a bare ticket key in the body (an `F` token) and a `[KEY]` reflink →
//      `ticket <KEY>` (TODO-011: a KEY click lands on a PAGE, always);
//    * TODO-008: a meta VALUE that lexes as a ticket id (`See: BE-050`,
//      `On1: ABC-020`, and an UNREGISTERED key's ticket-shaped value too — the
//      value's LEXICAL class alone decides) → `ticket <KEY>`, through todo.js's
//      one navSpell(), so the two render sites can never drift;
//    * the meta KEY half → `Key:*` and a non-ticket meta VALUE → `Key:value`,
//      both as WHOLE-ARG-LINE `todo` spells (BRO-025) — filters are TABLE
//      business and stay in `todo`.  The page's arg line is its ticket id,
//      which carries its TOPIC, so argLineWith yields `todo ABC Now:OPEN`;
//    * a `[ref]` / `[/pocket/Page]` reflink resolving to a non-ticket in-tree
//      page → the context-less `cat <meta-root-relative-path>` spell.
//  The page's OWN key never self-links.
"use strict";

const pathlib = require("../../shared/util/path.js");
const join    = pathlib.join;
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const SPELL   = require("../../shared/spell.js");      // BE-054: O-spell codec
const metaidx = require("../../shared/metaidx.js");    // TODO-003: the meta index
//  TODO-011: the ONE home of the shared ground (lexer, roots, arg grammar,
//  click classifiers).  todo.js does NOT require this module, so the edge is
//  one-way and needs no lazy dance.
const todo    = require("../todo/todo.js");
//  CODE-030: the page family (lexer, root, file ladder, byte read) is the
//  shared library now — read it DIRECTLY, not through the todo view.
const page    = require("../../shared/ticketpage.js");

const EMPTY32 = new Uint32Array(0);
const EXTS = page.EXTS;                    // the board's own .mkd-first order

//  tok32 (dog/tok/TOK.h): [31..27] tag (A+n)  [23..0] end byte offset.
function tokPack(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }
const TAG_O = tagCode("O");
function tokTagL(w) { return String.fromCharCode(65 + ((w >>> 27) & 0x1f)); }
function tokEnd(w) { return w & 0xffffff; }

function isReg(p) { try { return io.stat(p).kind === "reg"; } catch (e) { return false; } }

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
//  A link TARGET (refdef path, or an inline `/pocket/Page` shortcut) → its
//  click spell (BE-054: minted O at the splice): a ticket file (`KEY.<ext>`
//  basename) re-enters `ticket KEY` (TODO-011 — it names a PAGE); any
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
  if (ext && EXTS.indexOf(ext) >= 0 && page.shape(stem) === "key" &&
      page.pageFile(board.dir, stem))
    return "ticket " + stem;
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
//  grammar and splice a hidden context-less `O` after every RESOLVABLE link
//  token (BE-054, cat.js withLinks model, board-scoped): a bare/`[KEY]` ticket
//  key → `ticket KEY`; a `[ref]`/`[/pocket/Page]` reflink → its refdef target's
//  spell (ticket/cat).  The page's OWN key gets no self-link.
function emitPage(sink, board, key, file, mode, a) {
  const bytes = page.readBytes(file);
  if (bytes == null) return false;
  let body = bytes, toks = EMPTY32;
  if (mode !== "plain") {
    try { toks = tok.parse(bytes, "mkd"); } catch (e) { toks = EMPTY32; }
    if (toks.length) {
      const pfx = board.root + "/";
      const rel = file.indexOf(pfx) === 0 ? file.slice(pfx.length) : "";
      const linked = pageLinks(board, key, pathlib.dirname(rel), bytes, toks, a);
      body = linked.body; toks = linked.toks;
    }
  }
  sink.feed("ticket " + key, body, toks, "", 0n);
  return true;
}
function pageLinks(board, selfKey, pageDirRel, body, toks, a) {
  const defs = refdefs(utf8.Decode(body));
  const us = new Array(toks.length);
  const sta = new Array(toks.length);
  let extra = 0, nlinks = 0, prev = 0;
  for (let i = 0; i < toks.length; i++) { us[i] = null; sta[i] = prev; prev = tokEnd(toks[i]); }
  const word = (i) => utf8.Decode(body.slice(sta[i], tokEnd(toks[i])));
  //  BE-054: the verb click is a context-less O (empty ctx = "here").
  const mint = (spell) => (spell && spell !== "ticket " + selfKey)
      ? utf8.Encode(SPELL.mintOspell("", spell)) : null;
  const keySpell = (w) => (page.shape(w) === "key" && w !== selfKey &&
                           page.pageFile(board.dir, w)) ? "ticket " + w : null;
  //  A bare ticket key is an `F` token — it links on its own.
  for (let i = 0; i < toks.length; i++)
    if (tokTagL(toks[i]) === "F" && tokEnd(toks[i]) > sta[i]) us[i] = mint(keySpell(word(i)));
  //  TODO-004: the META-PAIR block, read through the ONE shared matcher
  //  (metaidx.metaBlock — the mkd tokenizer's `T` = the line-opening `Key:`,
  //  plus the indent + "directly under the header" rulings of 2026-08-03).
  //  The earlier claim — that a `T` never fires on an INDENTED word, so the
  //  view needed no second grammar — was FALSE: `T` is indent-tolerant,
  //  while the index's line regex was anchored at column 0, so TODO-003's
  //  four-space `Now:`/`Sev:` rendered and clicked but matched nothing.  One
  //  matcher now answers both, so they cannot drift again.  Each half gets its
  //  own whole-ARG-LINE spell: the key → `Key:*` (every ticket carrying it),
  //  the value → `Key:value`.  The page's own id is the arg line here, so both
  //  resolve against its TOPIC (argLineWith) — and both stay `todo` spells,
  //  because a filter is a LISTING and listings are the board's business.
  for (const p of metaidx.metaBlock(body, toks)) {
    if (metaidx.codeOf(p.key) === null) continue;  // not a registered pair shape
    us[p.ki] = mint(todo.spellWith(a, p.key, "*"));
    if (p.vi >= toks.length) continue;
    //  TODO-008: a ticket-shaped VALUE is a LINK, not a filter — it jumps, and
    //  TODO-011 sends it to the PAGE view (todo.js navSpell, the one home).
    const nav = todo.navSpell(p.value);
    if (nav) { us[p.vi] = mint(nav); continue; }
    const fv = todo.filterVal(p.value);
    if (fv !== null) us[p.vi] = mint(todo.spellWith(a, p.key, fv));
  }
  //  DOG-024: a span is markup + text, so a reflink is the token RUN `[` … `]`,
  //  not one `G` token — read the label from between the brackets and give every
  //  token of the run the spell (a click anywhere on the link re-enters).
  for (let i = 0; i < toks.length; i++) {
    if (tokTagL(toks[i]) !== "G" || word(i) !== "[") continue;
    let j = i + 1;
    while (j < toks.length && !(tokTagL(toks[j]) === "G" && word(j) === "]")
           && word(j).indexOf("\n") < 0) j++;
    if (j >= toks.length || tokTagL(toks[j]) !== "G") continue;   // unclosed
    const label = utf8.Decode(body.slice(tokEnd(toks[i]), sta[j]));
    let spell = keySpell(label);
    if (!spell && defs[label] !== undefined) spell = targetSpell(board, pageDirRel, defs[label]);
    else if (!spell && label[0] === "/") spell = targetSpell(board, pageDirRel, label);
    const u = mint(spell);
    if (u) for (let k = i; k <= j; k++) if (!us[k]) us[k] = u;
    i = j;
  }
  for (let i = 0; i < toks.length; i++) if (us[i]) { extra += us[i].length; nlinks++; }
  if (!nlinks) return { body: body, toks: toks };
  const out = new Uint8Array(body.length + extra);
  const ntoks = new Uint32Array(toks.length + nlinks);
  let op = 0, oi = 0;
  prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = tokEnd(toks[i]);
    for (let p = prev; p < end; p++) out[op++] = body[p];
    ntoks[oi++] = tokPack((toks[i] >>> 27) & 0x1f, op);
    if (us[i]) { out.set(us[i], op); op += us[i].length; ntoks[oi++] = tokPack(TAG_O, op); }
    prev = end;
  }
  return { body: out, toks: ntoks };
}

//  --- the verb ---------------------------------------------------------------
//  Errors in PLAIN WORDS, never a bare code — and every one of them names the
//  view that DOES serve the arg, so a refusal is a pointer, not a dead end.
function bad(why) {
  io.log("ticket: " + why + "\n");
  throw "ticket: unknown argument";
}

//  JAB-004: PLAIN verb (`.jab="args"`) reads its args off `be`.  ONE hunk comes
//  out and its banner IS the invocation the address bar shows (`ticket KEY`).
function ticket() {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  if (!sink) return;
  const board = page.boardDir();
  if (!board)
    bad("there is no todo/ ticket tree above here — a ticket page needs one");
  const mode = ambient.format();
  const argv = [];
  for (let i = 0; i < arguments.length; i++) argv.push(String(arguments[i]));
  //  TODO-011: the SHARED grammar classes the line (lexically); this view then
  //  refuses every class but the one it serves, pointing at `todo` for the rest.
  const a = todo.parseArgs(argv, "ticket");
  if (a.err) bad(a.err);
  //  A filter narrows a LISTING and a page is not one.  With a ticket id in the
  //  line the pointer keeps the id's TOPIC (that is the listing the reader
  //  meant); without one it is the arg line verbatim.
  if (a.filters.length) {
    const rest = a.toks.map(function (t) {
      return page.shape(t) === "key" ? page.keyTopic(t) : t; }).join(" ");
    bad("'" + a.filters[0].key + ":" + a.filters[0].val + "' is a filter, and a" +
        " filter narrows a LISTING, never a page — write 'todo " + rest + "'" +
        " for that list; 'ticket' takes one ticket code and nothing else");
  }
  if (!a.subject)
    bad("which ticket? — write 'ticket ABC-123' for one ticket's page, or" +
        " 'todo' for the open board");
  if (a.subject.kind === "topic")
    bad("'" + a.subject.w + "' names a topic, and a topic is a LIST — write" +
        " 'todo " + a.subject.w + "' for its open tickets, or 'ticket " +
        a.subject.w + "-123' for one ticket's page");
  //  Direct addressing ALWAYS works — open or closed, the page renders.
  const key = a.subject.w;
  const file = page.pageFile(board.dir, key);
  if (!file || !emitPage(sink, board, key, file, mode, a))
    bad("there is no ticket " + key + " under todo/" + page.keyTopic(key) +
        "/ — 'todo " + page.keyTopic(key) + "' lists that topic's open tickets");
}
ticket.jab = "args";
module.exports = ticket;
//  TODO-011: the page layer, exposed for the repro tests (the ls.js/log.js
//  model) — the link splicer and its two spell resolvers.
module.exports.pageLinks = pageLinks;
module.exports.targetSpell = targetSpell;
module.exports.refdefs = refdefs;
