//  verbs/cat/cat.js — JAB-020: cat:<path>[?ref] — print a file's bytes with
//  SYNTAX highlighting, NO diff.  Ruling (gritzko): cat: shows the file's OWN
//  bytes; the C cat:'s baseline-diff is a misnomer — for a diff there is diff:.
//  So: --plain = verbatim bytes, --color = syntax-painted (dog/THEME via tok),
//  --tlv = HUNK records.  Reuses the SHARED binding render (view/bro.js
//  renderHunkLog) — the same sink grep/spot/regex feed.  Pure JS over
//  libabc+libdog; no dog binary, no /proc.  Banner names the verb: `cat
//  <path>#L<n>` (per the verb-titled-hunks convention).
"use strict";

const store = require("../../shared/store.js");
const join  = require("../../shared/util/path.js").join;
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const bro   = require("../../view/bro.js");
const navlib = require("../../shared/nav.js");   // URI-011: full-URI hunk helper
const EMPTY32 = new Uint32Array(0);

const CAP = 1 << 20;   // 1 MiB/hunk cap; a bigger file splits with a #L<n> rebanner

//  tok32 (dog/tok/TOK.h): [31..27] tag (A+n)  [23..0] end byte offset; token
//  i's start = token i-1's end.  tag 'U' (20) = the invisible click-target.
function tokTag(w) { return String.fromCharCode(65 + ((w >>> 27) & 0x1f)); }
function tokEnd(w) { return w & 0xffffff; }
function tokPack(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
const TAG_U = "U".charCodeAt(0) - 65;   // 20

//  BRO-006: emit `U` click-targets on name/symbol tokens (the producer half of
//  the pager's `_uriAt` left-click nav).  The C cat/file-view (bro/BRO.c) emits
//  NO per-token `U`; its only file-view symbol nav is the right-click
//  `grep:#<word>` over the token under the cursor (bro_word_around, BRO.c:2968).
//  This ports THAT to a left-click `U`: every GREPABLE token (one whose bytes
//  hold a word char [A-Za-z0-9_] or a >=0x80 byte — the exact bro_word_around
//  predicate, BRO.c:1108-1113) gets a following `U` token whose hidden TEXT
//  bytes are `grep:#<token>`, matching `_uriAt` (visible tok -> `U` tok -> URI
//  bytes).  Body + toks grow in lockstep (GRAF.c:517-535 model): the U bytes
//  follow the token's bytes so the U range [prevEnd..end) is exactly the
//  appended `grep:#<token>`.  Note: the JS `tok.parse` binding runs the base
//  lexer only (no DEFMark), so identifiers are tagged `S`, not `N`/`C` — the
//  predicate is byte-level, not tag-level.  A cross-file jump to a symbol's
//  DEFINITION needs a symbol index cat.js lacks — deferred (BRO-006).
function grepable(body, lo, hi) {
  for (let i = lo; i < hi; i++) {
    const c = body[i];
    if (c >= 0x80) return true;
    if ((c >= 0x41 && c <= 0x5a) || (c >= 0x61 && c <= 0x7a) ||
        (c >= 0x30 && c <= 0x39) || c === 0x5f) return true;   // A-Z a-z 0-9 _
  }
  return false;
}
function withLinks(body, toks) {
  if (toks.length === 0) return { body: body, toks: toks };
  //  URI-011: grep click-target `<grep-uri>#<token>` — navUri carries the //name
  //  authority; when unscoped navUri("grep","")="grep:" so PFX="grep:#" (parity).
  const PFX = utf8.Encode(navlib.navUri("grep", "") + "#");
  let extra = 0, nlinks = 0, prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = tokEnd(toks[i]);
    if (end > prev && tokTag(toks[i]) !== "U" && grepable(body, prev, end))
      { extra += PFX.length + (end - prev); nlinks++; }   // grep-uri + '#' + token
    prev = end;
  }
  if (nlinks === 0) return { body: body, toks: toks };
  const out = new Uint8Array(body.length + extra);
  const ntoks = new Uint32Array(toks.length + nlinks);
  let op = 0, oi = 0;
  prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = tokEnd(toks[i]);
    const link = end > prev && tokTag(toks[i]) !== "U" && grepable(body, prev, end);
    //  Copy this token's body slice, re-offset its end into `out`.
    for (let p = prev; p < end; p++) out[op++] = body[p];
    ntoks[oi++] = tokPack((toks[i] >>> 27) & 0x1f, op);
    if (link) {
      //  Append `grep:#<token>` (the token = its just-copied bytes), then a
      //  `U` token whose end is the new body length (the U bytes' end).
      out.set(PFX, op); op += PFX.length;
      for (let p = prev; p < end; p++) out[op++] = body[p];
      ntoks[oi++] = tokPack(TAG_U, op);
    }
    prev = end;
  }
  return { body: out, toks: ntoks };
}

//  Read a wt file's bytes (NUL-safe); absent/non-regular → null, empty → [].
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

//  ?ref: resolve ref/branch/sha → commit → tree → the path's blob bytes
//  (mirrors the search view's walkRef; KEEPGetByURI's descend).
function readRefBytes(k, ref, path) {
  let sha = k.resolveRef(ref);
  if (!sha) { try { sha = require("../../core/resolve.js").resolveHex(k, ref); } catch (e) {} }
  if (!sha) return null;
  const treeSha = k.commitTree(sha) || sha;
  let found = null;
  k.readTreeRecursive(treeSha, function (leaf) {
    if (found) return;
    if ((leaf.kind === "f" || leaf.kind === "x") && leaf.path === path) {
      const o = k.getObject(leaf.sha);
      if (o && o.type === "blob") found = o.bytes;
    }
  });
  return found;
}

//  JAB-004: cat ONE arg — self-parse cat:<path>[?ref], read be.repo/be.sink +
//  ambient.format(), feed the same sink.
function catOne(arg) {
  const _be = (typeof be !== "undefined") ? be : null;
  const mode = ambient.format();
  const repo = _be && _be.repo;
  const sink = _be && _be.sink;
  if (!repo || !sink) return;

  let first = String(arg || "");
  if (first.indexOf("cat:") === 0) first = first.slice(4);
  const u = new URI(first);
  const path = u.path || "";
  const ref  = (u.query && u.query.length) ? u.query : "";
  if (!path) { io.log("cat: needs a path\n  try: cat:<path>\n"); throw "CATNOPATH"; }

  //  Bytes: a `?ref` blob (historic) else the live wt file.  Absent/empty → no
  //  output (no banner), matching the empty-file case.
  const k = store.open(repo.storePath, repo.project);
  let bytes = ref ? readRefBytes(k, ref, path) : readFileBytes(join(repo.wt, path));
  if (bytes == null || bytes.length === 0) return;

  const ext = bro.pathExt(path);            // "js" / "" — drives tok.parse
  //  JAB-029: feed each hunk into the shared in-memory HUNK sink (be.sink) — NO
  //  fd 1 here; the loop edge (cli) renders sink.log to fd 1 in the mode.
  let off = 0, line = 1;
  while (off < bytes.length) {
    //  1 MiB hunk, backed up to the last line boundary so a line never splits.
    let end = off + CAP < bytes.length ? off + CAP : bytes.length;
    if (end < bytes.length) {
      let nl = end; while (nl > off && bytes[nl - 1] !== 10) nl--;
      if (nl > off) end = nl;
    }
    let body = bytes.slice(off, end);
    let toks = EMPTY32;
    if (mode !== "plain" && ext) {
      try { toks = tok.parse(body, ext); } catch (e) { toks = EMPTY32; }
      //  BRO-006: append `U` click-targets on name/symbol (N/C) tokens.
      const wl = withLinks(body, toks); body = wl.body; toks = wl.toks;
    }
    //  URI-011: banner carries the full nav address `cat://name/<path>#L<n>`;
    //  navUri prefixes the authority, the #L<n> fragment stays AFTER it (parity).
    const uri = navlib.navUri("cat", path) + "#L" + line;
    sink.feed(uri, body, toks, "", 0n);   // banner: `cat:<path>#L<n>`
    for (let i = off; i < end; i++) if (bytes[i] === 10) line++;
    off = end;
  }
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its args reading `be`; the legacy
//  `cat(row, ctx)` shape (row `.uri` + ctx `.sink`, no global be) still routes via catOne.
function cat() {
  for (let i = 0; i < arguments.length; i++) catOne(arguments[i]);
}
cat.jab = "args";
module.exports = cat;
