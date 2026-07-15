//  shared/ticket.js — BRO-012: the ONE ticket-code resolver both click paths
//  converge on.  An issue key `ABC-123` (the tokenizers' `F` token) names a
//  ticket file at `todo/<TOPIC>/<KEY>.{md,txt,mkd}` (thin) or
//  `todo/<TOPIC>/<KEY>/README.<ext>` (fat) under the PROJECT root; this
//  maps a key → a `cat://<name>/todo/<TOPIC>/<KEY>.<ext>` nav URI.  URI-016:
//  the ticket tree is be.todoRoot() — `projectRoot()+"/todo"`, ONE dir, no env
//  var and no candidate list; a key that is not there simply has no ticket.
//  Key detection uses the SHARED tokenizer (`tok.parse` with the mkd grammar,
//  the same `uc ucnum* "-" dgt+` rule that mints the body `F`), so the log
//  view's `F` can never drift from the pager's.  Resolves ONLY through
//  be.todoRoot/be.navCwd + the URI class — no hand-composed path, no URI regex.
"use strict";

const pathlib = require("./util/path.js");
const join = pathlib.join, dirname = pathlib.dirname;

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

//  BRO-012: does `dir/todo/<TOPIC>/<KEY>.<ext>` (thin) or the fat
//  `dir/todo/<TOPIC>/<KEY>/README.<ext>` exist for some ext?  `dir` is an
//  absolute wt root (from be.wtdir).  Returns the repo-relative path of the
//  first hit (per ext: thin then fat, md → txt → mkd), or null.  No path is
//  ever handed to a URI raw — the caller composes through the URI class.
function findFile(dir, rel) {
  for (const ext of EXTS) {
    for (const p of [rel + "." + ext, rel + "/README." + ext]) {
      try { io.stat(join(dir, p)); return p; } catch (e) { /* absent → next */ }
    }
  }
  return null;
}

//  BRO-012: resolve an issue key → a `cat://<name>/todo/<TOPIC>/<KEY>.<ext>` nav
//  URI, or null (a missing ticket = a quiet no-op).  URI-016: there is no root
//  ORDER any more — be.todoRoot() is the ONE ticket tree, `projectRoot()+"/todo"`,
//  so this probes `<todoRoot>/<TOPIC>/<KEY>.<ext>` and that is the whole search.
//  The paths stay PROJECT-ROOT-relative (`todo/TOPIC/KEY.ext`, as the nav URI
//  wants them), so the probe root is todoRoot()'s parent — the project root, by
//  the rule.  The open URI is composed via be.navCwd(root) (the root's `//name`
//  context) + the URI class (scheme=cat, authority=`//name`, rooted path).
function ticketUri(key) {
  if (typeof be === "undefined" || !be.todoRoot || !be.navCwd) return null;
  const rel = ticketDir(key);
  if (!rel) return null;
  const dir = be.todoRoot();                          // <projectRoot>/todo
  if (!dir) return null;                              // repo-less → no tickets
  const root = dirname(dir);                          // == projectRoot()
  const hit = findFile(root, rel + "/" + key);        // todo/TOPIC/KEY (no ext)
  if (!hit) return null;
  //  navCwd(root) → the root's `//name` context (authority carries its own
  //  `//`); a present authority roots the path, else a plain `cat:<path>`.
  const ctx = be.navCwd(root);
  let a; try { a = ctx ? uri._parse(ctx).authority : undefined; } catch (e) { a = undefined; }
  const p = a !== undefined ? "/" + hit : hit;
  return URI.make("cat", a, p) || ("cat:" + hit);
}

module.exports = { scanKeys: scanKeys, ticketUri: ticketUri, ticketDir: ticketDir };
