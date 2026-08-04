//  views/ci/ci.js — CI-004 (ruling 2026-08-04): `ci` IS A VIEW.  Like status/
//  log/todo it is a read-only views/-tree verb ([Views] is the open category;
//  only the mutation basis is closed).  It FORKS whatever command the detection
//  ladder finds — or ATTACHES to the one already running in that tree — and
//  TRACKS THE TAIL of the file that run redirects its output into.  Nothing
//  here talks to the pager: the board's ` ∞` mints the ordinary spell
//  `//<wt>/: ci` and bro pushes this view through the ordinary machinery.
//
//  The screen:
//    BANNER  the COMMAND LINE (it tells what is running); the bare `ci` spell
//            when the tree names no command.
//    BODY    the last 4 KB of the log, cut at a line start, shown at the END.
//    FOOTER  `⋯ running` / `── PASS rc=0 ──` / `── FAIL rc=3 ──` in the verdict
//            tone — RENDER-ONLY: never a byte of the log file (shared/ci.js).
//
//  LIVE: while the job runs the view sets the GENERIC `tick` mark
//  (shared/viewmark.js) and bro re-runs it every ~1s, so the tail grows with no
//  keypress; the mark is gone the moment the run settles and the pager falls
//  quiet again.  `r` re-reads and re-tails through the very same path.
//
//  WHEN IT FORKS: only when nothing runs in the tree AND no FRESH verdict
//  stands (CI.ensure) — so the ~1s tick replays a settled screen instead of
//  starting a second build, while `r` (which bumps the rev root, staling every
//  memo) reads as "run it again".
"use strict";

const CI      = require("../../shared/ci.js");
const MARK    = require("../../shared/viewmark.js");   // TODO 8: the pager marks
const THEME   = require("../../view/theme.js");
const navlib  = require("../../shared/nav.js");
const ambient = require("../../shared/ambient.js");    // JAB-004: ctx→be bridge

//  tok32 (dog/tok/TOK.h): [31..27] tag (A+n)  [23..0] end byte offset.
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }
const TAG_S = tagCode("S"), TAG_O = tagCode("O");
const TICKMS = 1000;                      // the ruling's "refresh each ~1 s"

//  CI-004: log bytes are somebody's build output — a raw C0 control (a colour
//  escape, a CR, a tab) would scramble the pager's frame, so every one but '\n'
//  reads as a space.  Codepoint-for-codepoint, so the click column map holds.
function sane(s) {
  let out = "";
  for (const ch of s) {
    const c = ch.codePointAt(0);
    out += (c === 10 || (c >= 32 && c !== 127)) ? ch : " ";
  }
  return out;
}

//  CI-004: the tail text as display LINES (the trailing '\n' opens no last row).
function lines(text) {
  const ls = sane(text).split("\n");
  if (ls.length && ls[ls.length - 1] === "") ls.pop();
  return ls;
}

//  CI-004: ONE content hunk — a plain 'S' row per tail line, then the footer row
//  carrying its tone on a hidden `O` (`##rrggbb `, WHY-001 fg slot: the O sits
//  BEFORE the row's '\n', with an empty spell so a click falls through).
function emitHunk(sink, banner, rows, foot, tone) {
  const parts = [], spans = [];
  let off = 0;
  function span(text, tag) {
    const b = utf8.Encode(text);
    parts.push(b); off += b.length; spans.push([tag, off]);
  }
  for (const r of rows) span(r + "\n", TAG_S);
  if (foot) {
    span(foot, TAG_S);
    span("#" + tone + " ", TAG_O);
    span("\n", TAG_S);
  }
  const body = new Uint8Array(off);
  let p = 0;
  for (const b of parts) { body.set(b, p); p += b.length; }
  const toks = new Uint32Array(spans.length);
  for (let i = 0; i < spans.length; i++) toks[i] = tok(spans[i][0], spans[i][1]);
  sink.feed(banner, body, toks, "", 0n);
}

//  JAB-004: the plain-args verb.  Arg-blind by design (the runner takes no
//  per-invocation configuration): a bare `ci` acts on the tree the context is
//  in, an arg URI names another tree (`ci //ABC-123/`).
function ci(arg) {
  const _be  = (typeof be !== "undefined") ? be : null;
  const sink = (_be && _be.sink) || null;
  const out  = (_be && _be.out)  || null;
  const wantU = sink && ambient.format() !== "plain";
  //  the subject tree: an explicit arg, else the nav CONTEXT the click carried,
  //  else the run's own anchored tree (BE-039).
  const wt = CI.contextWt(arg ? String(arg) : (_be && _be.context) || "",
                          _be && _be.repo);

  const st = CI.ensure(wt);               // fork, attach, or replay the verdict
  const r  = wt ? CI.row(wt) : null;
  const t  = wt ? CI.tail(wt) : null;

  const banner = st.cmd || (r && r.cmd) || navlib.navLink("ci", "");
  let rows = t && t.text ? lines(t.text) : [];
  //  Nothing on disk yet: the runner's own plain words (the log is still empty,
  //  the tree names no build command, …) stand in for the bytes.
  if (!rows.length) rows = [st.message || "ci: no run and nothing to attach"];
  const foot = CI.footer(r);
  const tone = r ? (THEME.BTN_VERDICT[r.state] || THEME.BTN.run) : THEME.BTN.run;

  //  TODO 8: a LIVE run re-renders itself once a second; a settled one marks
  //  nothing and the pager goes back to waiting on the key alone.
  MARK.end();
  if (r && r.state === "run") MARK.tick(TICKMS);

  if (wantU) emitHunk(sink, banner, rows, foot, tone);
  else if (out) {
    out.raw(banner);
    for (const row of rows) out.raw(row);
    if (foot) out.raw(foot);
  }
}
ci.jab = "args";
module.exports = ci;

//  CI-004: exposed for the repro test (the ls.js/log.js model).
module.exports.lines = lines;
module.exports.emitHunk = emitHunk;
