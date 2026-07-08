//  verbs/ls/ls.js — the `ls:` / `lsr:` worktree-listing view (JAB-018/019).
//  ONE handler, the VERB is the parameter: `ls` lists ONE directory as ONE
//  hunk; `lsr` is the SAME listing plus a fan-out — it enqueues an `lsr:<child>`
//  row per immediate subdir AND per mounted submodule, so the resident loop's
//  in-memory FIFO queue (JSQUE-020) drives the recursion BFS, ONE HUNK PER DIR,
//  crossing store boundaries into submodules.  verbs/lsr/lsr.js is a one-line
//  re-export of this module — same code, `row.verb` selects recurse.
//
//  libabc+libdog ONLY (be.find / wtlog / store / classify): NO dog spawned, NO
//  sniff, NO /proc.  Each per-directory hunk is built from classify.classifyDir
//  — the O(dir) listing of the scope's IMMEDIATE entries: each immediate file
//  is its status row, each immediate subdir / mount is ONE BLANK-DATED `dir
//  <name>/` row (native ls: dates no dir row, so nothing under it is scanned —
//  the whole reason ls: now costs O(dir), not O(repo)).  Entry names render
//  RELATIVE to the scope dir; the banner names the scope RELATIVE to the top wt
//  (`ls:`, `ls:sub/`, `lsr:chsub/lib/`).  A scope INSIDE a submodule
//  re-discovers that shard via be.find, so `jab ls <path-in-sub>/` lists it.
"use strict";

const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const classify = require("../../shared/classify.js");
const pathlib  = require("../../shared/util/path.js");
const join     = pathlib.join;
const ambient  = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const render   = require("../../view/render.js");
const theme    = require("../../view/theme.js");
const navlib   = require("../../shared/nav.js");   // URI-011: full-URI hunk helper

//  BRO-006: emit a content HUNK (text + tok32) per dir, mirroring sniff/LS.c
//  htbl_emit — each row carries a hidden `U`-tagged nav URI (`cat:<path>` for a
//  file/move-dst, `ls:<sub>` for a dir) so the bro pager's _uriAt makes a click
//  open the entry.  Row layout = `<7-date> <3-verb> <path>[ -> dst]<navuri>\n`,
//  toks L(date) S(sep) <verb> S(sep) F(path) U(navuri) W(\n).  The nav URI rides
//  BEFORE the '\n' (not after, as C) so the body ENDS in a visible '\n' — that
//  keeps HUNKu8sFeedText's plain render byte-identical to native ls (no extra
//  ensure-final '\n').  tag 'U' = 20; U bytes hidden in plain/color, click-only.
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }

//  Append ONE entry row's bytes + tok32 spans to the accumulators.  `verb` is
//  the bucket name (eq/mod/dir…); `nav` is the hidden click-target URI.  Plain
//  bytes (date/verb/path columns) are byte-identical to the columnar emit; the
//  nav URI bytes ride hidden under a 'U' tok (skipped by plain/color renderers).
function appendRow(textParts, spans, off, verb, path, navUri, ts) {
  const date = render.dateCol(ts);                 // 7-col date; ts 0n → 7 spaces
  const vcol = render.verbCol(verb);               // 3-col verb
  const cols = date + " " + vcol + " " + path;     // visible columns (no '\n' yet)
  const colsB = utf8.Encode(cols);
  const uriB  = utf8.Encode(navUri);
  const nlB   = utf8.Encode("\n");
  textParts.push(colsB); textParts.push(uriB); textParts.push(nlB);
  const eDate = utf8.Encode(date).length;          // [.,7)   date
  const eSep1 = eDate + 1;                          // sep " "
  const eVerb = eSep1 + utf8.Encode(vcol).length;   // 3-col verb
  const eSep2 = eVerb + 1;                          // sep " "
  const ePath = colsB.length;                      // end of visible path
  const eUri  = ePath + uriB.length;               // hidden nav URI
  const eNL   = eUri + nlB.length;                 // visible '\n'
  const vtag  = theme.VERB_SLOT[verb] || "S";
  spans.push([tagCode("L"), off + eDate]);          // date
  spans.push([tagCode("S"), off + eSep1]);          // sep
  spans.push([tagCode(vtag), off + eVerb]);         // verb (palette slot)
  spans.push([tagCode("S"), off + eSep2]);          // sep
  spans.push([tagCode("F"), off + ePath]);          // visible path
  spans.push([tagCode("U"), off + eUri]);           // hidden nav URI (click target)
  spans.push([tagCode("S"), off + eNL]);            // the visible '\n'
  return colsB.length + uriB.length + nlB.length;
}

//  Build + feed ONE content hunk for a directory's `entries` (sorted) to the
//  HUNK `sink`.  navPfx is the scope relative to the top wt (so the hidden nav
//  URI is a full-path spell the pager can re-open).  A dir → `ls:<sub>/`, a
//  move → `cat:<dst>`, a plain file → `cat:<path>`.  Exposed for the repro test.
function emitHunk(sink, banner, navPfx, entries) {
  const textParts = [];
  const spans = [];
  let off = 0;
  for (const e of entries) {
    if (e.dir) {
      //  URI-014: nav click-target is the `word URI` spell `ls [//name/]<sub>/`.
      const nav = navlib.navLink("ls", navPfx + e.name + "/");
      off += appendRow(textParts, spans, off, "dir", e.name + "/", nav, 0n);
    } else {
      //  DIS-057 RULING 2026-06-29: a move is the `rmv`(src)+`mov`(dst) pair (no
      //  `-> dst` arrow), so the entry text is the bare name.  URI-014: the click
      //  target is the `word URI` spell `cat [//name/]<path>` (verb out of scheme).
      const nav = navlib.navLink("cat", navPfx + e.key);
      off += appendRow(textParts, spans, off, e.verb, e.text, nav, e.ts);
    }
  }
  const body = new Uint8Array(off);
  let p = 0;
  for (const part of textParts) { body.set(part, p); p += part.length; }
  const toks = new Uint32Array(spans.length);
  for (let i = 0; i < spans.length; i++) toks[i] = tok(spans[i][0], spans[i][1]);
  sink.feed(banner, body, toks, "", 0n);
}

//  Strip trailing slashes from an absolute dir (keep a lone "/").
function noSlash(p) { while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1); return p; }

//  `abs` relative to `base`, in DIR form (trailing "/"); "" when equal or not
//  under base.  Drives both the classifyDir scope prefix (base = owning repo
//  wt) and the banner prefix (base = top wt).
function relDir(base, abs) {
  base = noSlash(base); abs = noSlash(abs);
  if (abs === base) return "";
  const pfx = base + "/";
  return abs.indexOf(pfx) === 0 ? abs.slice(pfx.length) + "/" : "";
}

//  JAB-004: repo readers memoised in `cache` (was ctx._lsReaders; now a plain obj
//  the fn owns for the recursion) so an lsr run opens each shard's index ONCE.
function repoReaders(cache, repo) {
  if (cache[repo.wt]) return cache[repo.wt];
  return (cache[repo.wt] = { log: wtlog.open(repo),
                             k: store.open(repo.storePath, repo.project) });
}

//  JAB-004: list ONE scope dir as ONE hunk; `queue` self-drives the lsr BFS (the
//  plain path drops {enqueue}, so ls fans out itself). `be` else `ctx` (legacy).
function lsOne(uri, verb, ctx, queue, rdCache) {
  const _be   = (typeof be !== "undefined") ? be : null;
  const out   = (_be && _be.out)  || (ctx && ctx.out)  || null;
  const sink  = (_be && _be.sink) || (ctx && ctx.sink) || null;
  //  BRO-006: color/tlv feed the U-target hunk; PLAIN keeps the columnar `out`
  //  (the loop's plain dump lacks ls's htbl_trim → the hunk gains a trailing nl).
  const wantU = sink && ambient.format() !== "plain";
  const topWt = (_be && _be.repo && _be.repo.wt) || (ctx && ctx.repo && ctx.repo.wt) || ".";

  //  Resolve the scope to an ABSOLUTE dir: "." (cwd → top wt), a wt-relative
  //  path (`sub/`), or an absolute path (a self-driven recursion child).
  //  BE-028: confine a wt-relative scope via wtJoin (THROWS NAVESCAPE on any `..`
  //  climb above the wt); an absolute path is a trusted lsr recursion child.
  let absScope;
  if (uri && uri[0] === "/") absScope = uri;
  else                       absScope = pathlib.wtJoin(topWt, uri || "");
  absScope = noSlash(absScope);

  //  The OWNING repo of the scope — be.find re-discovers a submodule's shard
  //  when the scope is inside a mount (cross-store seam), else the top repo.
  let repo;
  try { repo = _be ? _be.find(absScope) : be.find(absScope); }
  catch (e) { repo = (_be && _be.repo) || (ctx && ctx.repo) || null; }
  if (!repo) return;

  const scopePfx = relDir(repo.wt, absScope);               // rel to OWNING wt
  const navPfx   = relDir(topWt, absScope);                 // rel to TOP wt
  const banner   = navlib.navLink(verb, navPfx);            // URI-014: `word URI` banner spell
  const rd        = repoReaders(rdCache, repo);
  const res       = classify.classifyDir(repo, rd.log, rd.k, scopePfx);

  //  Merge files + dirs into ONE lex-ordered entry list (a file `deep.txt` sorts
  //  before a dir `deep/`, as native ls: orders full paths).  A dir row's date
  //  is BLANK (ts 0n): native ls: dates no directory.
  const entries = [];
  for (const f of res.files)
    //  DIS-057: a rename lists as the `rmv`(src)+`mov`(dst) pair, no `-> dst`.
    entries.push({ key: f.name, dir: false, text: f.name, verb: f.bucket, ts: f.ts });
  for (const name of res.dirs)
    entries.push({ key: name + "/", dir: true, name: name });
  entries.sort(function (a, b) { return a.key < b.key ? -1 : a.key > b.key ? 1 : 0; });

  //  BRO-006: color/tlv → ONE U-target content HUNK; PLAIN → the columnar `out`.
  if (wantU) emitHunk(sink, banner, navPfx, entries);
  else if (out) {
    out.raw(banner);
    for (const e of entries)
      if (e.dir) out.row(e.name + "/", "dir", 0n);
      else       out.row(e.text, e.verb, e.ts);
  }

  //  lsr: push one `lsr:<child>` abspath per subdir/mount (lex) onto the FIFO ls
  //  drains itself (was a {enqueue} the plain path drops).  ls: is a leaf.
  if (queue)
    for (const e of entries) if (e.dir) queue.push(join(absScope, e.name));
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops args reading `be`.
function ls() {
  //  JAB-004: each arg is a URI token — its `lsr:`/`ls:` scheme (stripped) drives
  //  recursion; no positional lists "." (the legacy seed's "." row).
  const rdCache = {};
  const argv = arguments.length ? arguments : ["."];
  for (let i = 0; i < argv.length; i++) {
    //  DIS-060: PARSE the arg URI (never string-slice it) — ls is a FILESYSTEM
    //  lister: only the scheme (lsr → recurse) + the PATH slot drive it, a
    //  `?ref`/`#frag` is IGNORED (a committed-tree ref is tree:'s job).
    let p; try { p = uri._parse(String(argv[i] || "")); } catch (e) { p = {}; }
    const verb = p.scheme === "lsr" ? "lsr" : "ls";
    const path = p.path || ".";
    const queue = verb === "lsr" ? [] : null;
    lsOne(path, verb, null, queue, rdCache);
    while (queue && queue.length) lsOne(queue.shift(), "lsr", null, queue, rdCache);
  }
}
ls.jab = "args";
module.exports = ls;

//  BRO-006: expose the U-target hunk builders for the repro test (the dog/HUNK
//  tok-build model — same idea as log.js's exported tok()/appendRow).
module.exports.tok = tok;
module.exports.appendRow = appendRow;
module.exports.emitHunk = emitHunk;
