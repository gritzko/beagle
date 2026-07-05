//  shared/ticket.js — BRO-012: the ONE ticket-code resolver both click paths
//  converge on.  An issue key `ABC-123` (the tokenizers' `F` token) names a
//  ticket file at `todo/<TOPIC>/<KEY>.{md,txt,mkd}` under a worktree root; this
//  maps a key → a `cat://<name>/todo/<TOPIC>/<KEY>.<ext>` nav URI.  The RESOLVER
//  owns root order (be.todoRoot()): explicit $TODO_ROOT, then the CURRENT wt
//  root, then the OPEN/launch wt root; the first whose file exists wins.
//  Key detection uses the SHARED tokenizer (`tok.parse` with the mkd grammar,
//  the same `uc ucnum* "-" dgt+` rule that mints the body `F`), so the log
//  view's `F` can never drift from the pager's.  Resolves ONLY through
//  be.todoRoot/be.navCwd + the URI class — no hand-composed path, no URI regex.
"use strict";

const join = require("./util/path.js").join;

//  BRO-012: `todo/<TOPIC>/<KEY>.<ext>` layout + the extension probe order.
const TODO = "todo";
const EXTS = ["md", "txt", "mkd"];

//  BRO-012: tok32 tag/end accessors — same layout as view/bro.js:20 (a shared/
//  module can't import a view/, so mirror the two one-liners, not hand bit-math).
const TOK_TAG = (w) => String.fromCharCode(65 + ((w >>> 27) & 0x1f));
const TOK_END = (w) => w & 0xffffff;

//  A key `ABC-123` lives under `todo/ABC/` — the TOPIC is the run of chars
//  before the `-`.  Returns the repo-relative dir, or null for a malformed key.
function ticketDir(key) {
  const i = key.indexOf("-");
  if (i <= 0) return null;
  return TODO + "/" + key.slice(0, i);
}

//  BRO-012: scan `str` for issue keys via the SHARED tokenizer — tok.parse with
//  the `mkd` grammar fuses `uc ucnum* "-" dgt+` into ONE `F` token (verified;
//  the same rule the body FREE/MKDT/MDT tokenizers use).  Returns each `F`
//  token's { key, lo, hi } byte span (into utf8.Encode(str)) in order, so the
//  log view can split its summary span exactly where the tokenizer would.
function scanKeys(str) {
  const bytes = utf8.Encode(str);
  let toks;
  try { toks = tok.parse(bytes, "mkd"); } catch (e) { return []; }
  const out = [];
  let prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = TOK_END(toks[i]);
    if (TOK_TAG(toks[i]) === "F") out.push({ key: utf8.Decode(bytes.slice(prev, end)), lo: prev, hi: end });
    prev = end;
  }
  return out;
}

//  BRO-012: does `dir/todo/<TOPIC>/<KEY>.<ext>` exist for some ext?  `dir` is an
//  absolute wt root (from be.wtdir).  Returns the repo-relative path of the
//  first hit (md → txt → mkd), or null.  No path is ever handed to a URI raw —
//  the caller composes the nav URI through the URI class.
function findFile(dir, rel) {
  for (const ext of EXTS) {
    const p = rel + "." + ext;
    try { io.stat(join(dir, p)); return p; } catch (e) { /* absent → next ext */ }
  }
  return null;
}

//  BRO-012: resolve an issue key → a `cat://<name>/todo/<TOPIC>/<KEY>.<ext>` nav
//  URI, or null (a missing ticket = a quiet no-op).  The RESOLVER owns root
//  order via be.todoRoot() — $TODO_ROOT env, current wt root, open/launch wt
//  root — probing `<root>/todo/<TOPIC>/<KEY>.<ext>` under each; first hit wins.
//  The open URI is composed via be.navCwd(root) (the root's `//name` context)
//  + the URI class (scheme=cat, authority=`//name`, rooted path).
function ticketUri(key) {
  if (typeof be === "undefined" || !be.todoRoot || !be.navCwd) return null;
  const rel = ticketDir(key);
  if (!rel) return null;
  const base = rel + "/" + key;                       // todo/TOPIC/KEY (no ext)
  for (const root of be.todoRoot()) {
    const hit = findFile(root, base);
    if (!hit) continue;
    //  navCwd(root) → the root's `//name` context (authority carries its own
    //  `//`); a present authority roots the path, else a plain `cat:<path>`.
    const ctx = be.navCwd(root);
    let a; try { a = ctx ? uri._parse(ctx).authority : undefined; } catch (e) { a = undefined; }
    const p = a !== undefined ? "/" + hit : hit;
    return URI.make("cat", a, p) || ("cat:" + hit);
  }
  return null;
}

module.exports = { scanKeys: scanKeys, ticketUri: ticketUri, ticketDir: ticketDir };
