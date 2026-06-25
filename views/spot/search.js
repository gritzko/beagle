//  verbs/spot/search.js — the SHARED search VIEW scaffold (JAB-021) that the
//  `spot:` (structural), `grep:` (literal substring) and `regex:` (native JS
//  RegExp) modes ride.  ONE handler, the VERB selects the mode + matcher.
//  Pure JS over libabc+libdog: tok.parse/TokStream (lexer), shared/classify.js
//  (live-tip file walk), shared/store.js + git.tree (?ref historic walk), the
//  abc.index u64 lane (the .spot.idx trigram pre-filter), core/emit.js (hunk
//  framing).  NO dog binary spawned, NO /proc — portable.
//
//  Mirrors spot/CAPO.exe.c (URI parse + dispatch), spot/CAPO.c (CAPOScan walk,
//  CAPOGrepCtx context window, capo_spot_file coalescing, CAPOBuildHunk),
//  spot/GREP.c (substring/regex) and dog/HUNK.c (HUNKu8sMakeURI / FeedText
//  plain framing).
//
//  PARITY CAVEAT: HUNKu8sMakeURI's `#func` segment needs tok's DEF pass (the
//  S->N retag), which has NO JS binding (tok.parse == TOKLexer only).  So the
//  func segment is OMITTED here — byte-parity holds for the `path#Lnn` form
//  (matches whose context window starts before any in-scope DEF).  Building the
//  func face is a MUST-ASK new binding, out of scope (see the ticket).
//
//  FACTORING (so grep/regex are one flag + one matcher fn each):
//    parseURI(verb, uri)        -> { mode, body, ext, path, ref, remote }
//    matchers[mode](source, ...) -> [{lo, hi}]  per-byte match spans
//    walk + ctx-window + coalesce + emit are mode-AGNOSTIC.
"use strict";

const be       = require("../../core/discover.js");
const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const classify = require("../../shared/classify.js");
const join     = require("../../shared/util/path.js").join;
const match    = require("./match.js");
const ext2lang = require("./ext.js");
const EMPTY32  = new Uint32Array(0);   // JAB-029: hunks feed ctx.sink; cli renders

//  --- URI parse: scheme->mode, #body (strip '…'), .ext, ./path, ?ref -------
//  Mirrors CAPO.exe.c:174-272.  The seed lowers `<scheme>:<rest>` into the
//  row uri (scheme stripped by the loop); we get the verb (= mode) + the raw
//  uri tail.  `new URI` does the structured split (NEVER hand-rolled).
//  parseURI(verb, rawArgs): rawArgs is the raw positional arg list (ctx.args)
//  — the FULL `<scheme>:<uri>` first arg plus any trailing `.ext`/path/file
//  args (CAPO.exe.c's trail[] loop).  We re-parse from the raw arg(s) because
//  the seed lowers a fragment-only URI (`spot:#body`) to a "." placeholder
//  (no path slot) — the raw arg always carries the whole projector URI.
function parseURI(verb, rawArgs) {
  const mode = verb;                       // spot | grep | regex
  const args = (rawArgs && rawArgs.length) ? rawArgs.slice() : [""];
  //  First arg = the projector URI; strip the `<scheme>:` so URI sees the body.
  let first = String(args[0] || "");
  const pfx = mode + ":";
  if (first.indexOf(pfx) === 0) first = first.slice(pfx.length);
  const u = new URI(first);
  let body = u.fragment || "";
  let ext  = "";
  let path = u.path || "";
  let ref  = "";
  const remote = !!(u.authority && u.authority.length);

  //  Path-side `.ext` (`spot:.c#sym`): when the whole path is a `.ext`, it is
  //  the extension filter, not a file-narrowing constraint.
  if (path && path[0] === "." && ext2lang.known(path)) { ext = path; path = ""; }

  //  Strip ONE surrounding pair of '…' from the body (shell-quote leak).
  if (body.length >= 2 && body[0] === "'" && body[body.length - 1] === "'")
    body = body.slice(1, -1);

  //  ?ref / ?branch historic search (consumed by the keeper tree walk).
  if (u.query && u.query.length) ref = u.query;

  //  Trailing args (CAPO.exe.c trail[] loop): a `.ext` trail arg (`spot:#body
  //  .c`) sets the ext; a `?ref` trail arg (`grep:.c#body ?<sha>`) is the
  //  historic-search ref; a plain path narrows (skipped — single-tree).
  for (let i = 1; i < args.length; i++) {
    const a = String(args[i] || "");
    if (a[0] === "." && ext2lang.known(a)) { if (!ext) ext = a; continue; }
    if (a[0] === "?" && !ref) { const tu = new URI(a); ref = tu.query || a.slice(1); }
  }

  return { mode: mode, body: body, ext: ext, path: path, ref: ref, remote: remote };
}

//  --- context window (CAPOGrepCtx, GREP.c:16): [lo, hi) covering nctx lines
//  before + the own line + nctx trailing lines around byte `pos`.  src is a
//  Uint8Array; offsets are byte positions.  Default nctx = 3 (CAPOGrepCtx).
function grepCtx(src, pos, nctx) {
  const slen = src.length;
  if (pos > slen) pos = slen;
  //  Backward: walk right-to-left over [0, pos), after `nctx` newlines stand
  //  on the '\n' ending the line above the window; window starts at that+1.
  let lo = 0, seen = 0;
  for (let i = pos - 1; i >= 0; i--) {
    if (src[i] !== 10) continue;           // '\n'
    if (seen === nctx) { lo = i + 1; break; }
    seen++;
  }
  //  Forward: from pos, drain up to nctx+1 lines (own + nctx trailing).
  let hi = pos;
  for (let i = 0; i <= nctx; i++) {
    while (hi < slen && src[hi] !== 10) hi++;
    if (hi < slen) hi++;                    // consume the '\n'
    else break;
  }
  return { lo: lo, hi: hi };
}

//  --- HUNKu8sMakeURI (dog/HUNK.c:1383), func-less form ---------------------
//  `path` + (`#L<lineno>` when lineno>0).  The 1-based line of byte ctx_lo.
//  (Func segment omitted — DEF has no JS binding; see header.)
function makeURI(path, src, ctxLo) {
  let ln = 1;
  for (let i = 0; i < ctxLo && i < src.length; i++) if (src[i] === 10) ln++;
  return path + "#L" + ln;
}

//  --- per-file search + hunk emit (capo_spot_file / capo_grep_file_cb) -----
//  Run the mode matcher over `source` (Uint8Array), coalesce hits sharing a
//  context window into one hunk, emit each via out.raw in the plain framing:
//    banner "hunk <uri>\n" + body verbatim + (trailing '\n' if missing)
//  and the emit sink appends the ONE blank-line separator.
function searchFile(em, ctxState, source, htoks, relpath, m, ext) {
  const nctx = ctxState.nctx;
  const spans = m.run(source, htoks);      // [{lo, hi}] ascending, per matcher
  if (!spans.length) return;

  let prevHi = 0;
  for (let i = 0; i < spans.length; ) {
    const s0 = spans[i];
    let win = grepCtx(source, s0.lo, nctx);
    if (s0.hi > s0.lo) {
      const w2 = grepCtx(source, s0.hi - 1, nctx);
      if (w2.hi > win.hi) win.hi = w2.hi;
    }
    let ctxLo = win.lo, ctxHi = win.hi;
    //  Absorb following hits whose match falls inside the growing window.
    let j = i + 1;
    for (; j < spans.length; j++) {
      const s2 = spans[j];
      if (s2.lo >= ctxHi) break;            // outside window → next hunk
      const w2 = grepCtx(source, s2.lo, nctx);
      if (w2.hi > ctxHi) ctxHi = w2.hi;
    }
    //  Contiguous-hit coalescing: a window overlapping the previous one is
    //  glued (no re-emitted overlap, no banner).  capo_spot_file:801.
    if (ctxLo < prevHi) ctxLo = prevHi;
    if (ctxLo < ctxHi) {
      const uri = makeURI(relpath, source, win.lo < ctxLo ? ctxLo : win.lo);
      emitHunk(em, uri, source, ctxLo, ctxHi, ext);
    }
    prevHi = ctxHi;
    i = j;
  }
}

//  Emit ONE hunk into the caller-owned in-memory HUNK sink (JAB-029): feed
//  {uri, body, toks, verb} into ctx.sink — NO per-hunk render, NO fd 1.  The
//  loop edge (cli) renders the collected sink in the mode (plain == native
//  HUNKu8sFeedText, colour == dog/THEME SGR, tlv == the raw 'H' records).  toks
//  are lexed only for the PAINTED modes; plain ignores them, so --plain stays
//  byte-identical.  An in-process caller (bro) reads the same sink directly.
function emitHunk(em, uri, source, lo, hi, ext) {
  const body = source.slice(lo, hi);
  let toks = EMPTY32;
  if (em.mode !== "plain" && ext) {
    try { toks = tok.parse(body, ext.replace(/^\./, "")); } catch (e) { toks = EMPTY32; }
  }
  em.sink.feed(uri, body, toks, em.verb, 0n);
}

//  --- file walk: live tip via classify; ?ref historic via store + git.tree -
//  Visit every candidate file (known ext, ext gate via same-lexer), handing
//  its bytes to `visit(relpath, bytes)`.  Mirrors CAPOScan (tracked+wt) and
//  CAPOScanRef (keeper tree walk + blob pull).
function walkLive(repo, log, k, want, visit) {
  const res = classify.classify(repo, log, k, { listing: true });
  //  classify's listing rows cover tracked (eq/mod/put/new/mov) + untracked
  //  (unk).  A row's path is wt-relative; read the wt bytes (the user's copy,
  //  what search hits reflect — CAPO.c:963).  Deleted (mis/del) rows skip.
  const seen = {};
  for (const r of res.rows) {
    if (r.bucket === "mis" || r.bucket === "del") continue;
    const rel = r.path;
    if (seen[rel]) continue; seen[rel] = 1;
    if (!extGate(rel, want)) continue;
    const full = join(repo.wt, rel);
    const bytes = readFileBytes(full);
    if (bytes) visit(rel, bytes);
  }
}

function walkRef(repo, k, refQuery, want, visit) {
  //  Resolve ?ref/?branch (or #sha) to a commit, then its tree, then walk
  //  leaves and pull each blob.  Mirrors CAPOScanRef → KEEPLsFiles tree walk
  //  + KEEPGetExact blob pull.
  let sha = k.resolveRef(refQuery);
  if (!sha) { const r = require("../../core/resolve.js"); try { sha = r.resolveHex(k, refQuery); } catch (e) {} }
  if (!sha) return;
  const treeSha = k.commitTree(sha) || sha;   // accept a tree sha directly
  k.readTreeRecursive(treeSha, function (leaf) {
    if (leaf.kind !== "f" && leaf.kind !== "x") return;   // reg/exec only
    if (!extGate(leaf.path, want)) return;
    const obj = k.getObject(leaf.sha);
    if (!obj || obj.type !== "blob") return;
    visit(leaf.path, obj.bytes);
  });
}

//  ext gate: the file's ext must be known AND (when a target ext is set) share
//  a lexer with it (CAPOKnownExt + TOKSameLexer).  `want` is the .ext filter.
function extGate(relpath, want) {
  const fe = ext2lang.extOf(relpath);
  if (!fe || !ext2lang.known(fe)) return false;
  if (want && !ext2lang.sameLexer(fe, want)) return false;
  return true;
}

function readFileBytes(full) {
  let st;
  try { st = io.lstat(full); } catch (e) { return null; }
  if (st.kind !== "reg") return null;
  if (st.size === 0) return new Uint8Array(0);
  let fd;
  try { fd = io.open(full, "r"); } catch (e) { return null; }
  try {
    const b = io.buf(st.size + 16);
    io.readAll(fd, b, st.size);
    return b.data().slice();
  } catch (e) { return null; }
  finally { try { io.close(fd); } catch (e) {} }
}

//  --- the handler ----------------------------------------------------------
module.exports = function handle(row, ctx) {
  const mode = (ctx && ctx.mode) || "plain";   // shared color/tlv/plain gate
  const repo = (ctx && ctx.repo) || null;
  if (!repo) return;

  //  Prefer the raw positional args (ctx.args) — they carry the FULL projector
  //  URI; the queue row.uri loses a fragment-only URI to a "." placeholder.
  const rawArgs = (ctx && ctx.args && ctx.args.length) ? ctx.args : [row.uri];
  const q = parseURI(row.verb, rawArgs);

  //  no-body hint (CAPO.exe.c:377-414): a search URI with no fragment body.
  if (!q.body) {
    //  Body in the WRONG slot — the path, not the `#fragment` (`regex:beta`):
    //  the specific native hint, distinct from the empty-URI case.
    if (q.path) {
      io.log("spot: search body goes in the URI fragment, not the path\n  try: " + q.mode + ":#" + q.path + "\n");
      throw "SPOTNOBODY";
    }
    io.log("spot: " + q.mode + ": needs a search body\n  try: " + q.mode + ":#<body>\n");
    throw "SPOTNOBODY";
  }
  //  spot REQUIRES a .ext (CAPO.exe.c:368-370).
  if (q.mode === "spot" && !q.ext) {
    io.log("spot: --spot requires a .ext argument\n");
    throw "SPOTNOEXT";
  }

  //  Build the mode matcher (one flag + one fn — grep/regex slot in here).
  const m = match.make(q.mode, q.body, q.ext);
  if (!m) { io.log("spot: bad " + q.mode + " pattern\n"); throw "SPOTBADPAT"; }

  const ctxState = { nctx: 3 };
  //  verb = the search mode (grep/spot/regex) — the hunk banner names it
  //  (`grep <uri>` / `spot <uri>` / `regex <uri>`), not the generic `hunk`.
  //  JAB-029: hunks feed the caller-owned ctx.sink (no fd 1); cli renders it.
  const sink = ctx && ctx.sink;
  if (!sink) return;
  const em  = { sink: sink, mode: mode, verb: q.mode };
  const log = wtlog.open(repo);
  const k   = store.open(repo.storePath, repo.project);

  function visit(rel, bytes) {
    //  Skip oversized sources (SPOTBIG: >24-bit tok offsets, 16 MiB).
    if (bytes.length >= (1 << 24)) return;
    const fe = ext2lang.extOf(rel);
    //  Lex once per file via tok.parse (the lexer SPOTTokenize uses); the
    //  spot matcher rides the TokStream cursor, grep/regex over raw bytes.
    let htoks = null;
    if (m.needsToks) {
      try { htoks = tok.parse(bytes, fe.replace(/^\./, "")); } catch (e) { return; }
    }
    searchFile(em, ctxState, bytes, htoks, rel, m, fe);
  }

  //  Historic `?ref`: prefer the seed-pinned sha (resolution-at-entry — the
  //  seed already resolved a `?<ref>` trail arg into ctx.refs as a `set` op),
  //  else the parsed ref query.  A `//remote` authority ⇒ path is the remote
  //  repo, not a local subtree — skip-as-filter (single-tree only; see report).
  let refSha = null;
  if (ctx.refs && ctx.refs.length)
    for (const r of ctx.refs) if (r.sha) { refSha = r.sha; break; }
  if (refSha)      walkRef(repo, k, refSha, q.ext, visit);
  else if (q.ref)  walkRef(repo, k, q.ref, q.ext, visit);
  else             walkLive(repo, log, k, q.ext, visit);
  //  JAB-029: no fd-1 write here — searchFile/emitHunk fed every hunk into
  //  ctx.sink; the loop edge (cli) renders the collected sink to fd 1 in `mode`.
};

//  exported for the parity harness / sibling verb modules
module.exports.parseURI = parseURI;
module.exports.grepCtx  = grepCtx;
module.exports.makeURI  = makeURI;
