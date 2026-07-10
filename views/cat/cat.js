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
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const discover = require("../../core/discover.js");
const wtpath = discover.wtpath;
const pathlib = require("../../shared/util/path.js");   // BE-011: wtJoin confinement
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const bro   = require("../../view/bro.js");
const navlib = require("../../shared/nav.js");   // URI-011: full-URI hunk helper
const ticket = require("../../shared/ticket.js");   // BRO-012: F key → ticket URI
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
//  BRO-012: the `U` bytes a token links to, or null.  An `F` issue-key links to
//  its TICKET file (ticketUri), NOT a grep of the word; every other grepable
//  token keeps the URI-014 `grep [//name]#<token>` spell (unresolved F falls to
//  grep too — it is grepable).  Returned bytes ARE the hidden U target verbatim.
function uBytes(body, toks, i, prev, end, PFX) {
  if (end <= prev || tokTag(toks[i]) === "U") return null;
  if (tokTag(toks[i]) === "F") {
    const t = ticket.ticketUri(utf8.Decode(body.slice(prev, end)));
    if (t) return utf8.Encode(t);
  }
  if (!grepable(body, prev, end)) return null;
  const u = new Uint8Array(PFX.length + (end - prev));
  u.set(PFX, 0);
  for (let p = prev, o = PFX.length; p < end; p++, o++) u[o] = body[p];
  return u;
}
function withLinks(body, toks) {
  if (toks.length === 0) return { body: body, toks: toks };
  //  URI-014: grep click-target as the `word URI` spell `grep [//name]#<token>`
  //  — navLink puts the verb OUT of the scheme; unscoped PFX="grep #", scoped
  //  "grep //name#", the per-token `<token>` append completing the spell.
  const PFX = utf8.Encode(navlib.navLink("grep", "", undefined, ""));
  //  BRO-012: precompute each token's U bytes (ticket for F, grep otherwise) so
  //  the size + fill passes agree on the variable-length ticket URIs.
  const us = new Array(toks.length);
  let extra = 0, nlinks = 0, prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = tokEnd(toks[i]);
    const u = uBytes(body, toks, i, prev, end, PFX);
    us[i] = u;
    if (u) { extra += u.length; nlinks++; }
    prev = end;
  }
  if (nlinks === 0) return { body: body, toks: toks };
  const out = new Uint8Array(body.length + extra);
  const ntoks = new Uint32Array(toks.length + nlinks);
  let op = 0, oi = 0;
  prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = tokEnd(toks[i]);
    //  Copy this token's body slice, re-offset its end into `out`.
    for (let p = prev; p < end; p++) out[op++] = body[p];
    ntoks[oi++] = tokPack((toks[i] >>> 27) & 0x1f, op);
    if (us[i]) {                                  // append the hidden U target
      out.set(us[i], op); op += us[i].length;
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

  //  URI-013: ONE structured parse of the whole `cat:<path>[?ref]` — the URI
  //  binding reads `.path`/`.query` off the scheme'd form (no strip-then-reparse).
  const first = String(arg || "");
  const u = uri._parse(first);
  //  BE-037/BE-032: canonical path slot, resolved against the CONTEXT dir (cwd /
  //  nav sub-dir), re-anchored at the wt root; NAVESCAPE on climb-out.
  const path = discover.argRel(repo, u.path || "");
  const ref  = (u.query && u.query.length) ? u.query : "";
  if (!path) { io.log("cat: needs a path\n  try: cat:<path>\n"); throw "CATNOPATH"; }

  //  Bytes: a `?ref` blob (historic) else the live wt file.  Absent/empty → no
  //  output (no banner), matching the empty-file case.
  const k = store.open(repo.storePath, repo.project);
  //  BE-011: compose the wt path via wtJoin — an untrusted `..` climb above the
  //  wt root throws NAVESCAPE; refuse cleanly (never a silent outside read).
  let full = null;
  if (!ref) {
    try { full = wtpath(repo.wt, path); }
    catch (e) { io.log("cat: " + e + "\n"); return; }
  }
  let bytes = ref ? readRefBytes(k, ref, path) : readFileBytes(full);
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
    //  URI-014: banner is the `word URI` spell `cat [//name/]<path>#L<n>` — the
    //  verb OUT of the scheme (navLink), the #L<n> fragment riding the addressing.
    const uri = navlib.navLink("cat", path, undefined, "L" + line);
    sink.feed(uri, body, toks, "", 0n);   // banner: `cat <path>#L<n>`
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
