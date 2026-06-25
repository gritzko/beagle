//  views/commit/commit.js — the `commit:` read-only VIEW as a resident-loop
//  handler (JAB-009).  Renders ONE keeper-metadata hunk for a commit object:
//      commit <sha40>\n
//      <field> <value>\n   (object order, verbatim — tree/parent/author/…/
//                           gpgsig/encoding/mergetag preserved)
//      \n
//      <message body>       (forced trailing \n)
//  byte-matching the C keeper KEEPProjCommit metadata hunk (PROJ.c:384).  The
//  COMMIT-002 `git show` inline-diff relay (graf's `diff:?<sha>` after this
//  hunk) is OUT OF SCOPE — a sibling `diff:` port owns it; this view emits ONLY
//  the keeper metadata bytes.  Pure JS over the libabc/libdog bindings via
//  shared/store.js (getObject/resolveRef) + a RAW ordered header walk — NOT
//  git.parseCommit, which DROPS header order and non-canonical headers
//  (pack.hpp:350).  No dog binary, no /proc, no new binding.
//
//  KEY FINDING (verified vs `be commit:` --plain): the C plain output is
//  EXACTLY `"commit <sha40>\n"` + the RAW commit object bytes, verbatim — the
//  object already carries the ordered headers + blank line + message with its
//  own trailing \n.  So the hunk body is just that concatenation; the per-field
//  COLOUR spans (field=R/blue, sha=L/cyan, author|committer=G/green, subject=
//  N/bold) are layered on top for --color.
//
//  handle(row, ctx): the seed lowered `commit:<uri>` to a "." placeholder row
//  (classifyArg can't model a fragment-only URI — see cat.js/log.js); the
//  handler re-parses the WHOLE `commit:<uri>` off ctx.args[0], never row.uri.
//  Output is ONE content HUNK fed to ctx.sink (EMPTY uri → no banner line, so
//  --plain matches the C keeper which elides the `commit:?<sha>` URI as a
//  U-span; the loop edge renders plain/color/tlv via view/bro.js renderHunkLog
//  — the view never writes fd 1).

"use strict";

const store   = require("../../shared/store.js");
const wtlog   = require("../../shared/wtlog.js");
const shalib  = require("../../shared/util/sha.js");
const isFullSha = shalib.isFullSha;
const frameSha  = shalib.frameSha;

//  tok32 tag indices (A=0 … Z=25) — the dog/THEME palette the HUNK .color sink
//  paints (dog/THEME.c THEME16TBL): R=keyword=FG16(94)/blue (field names),
//  L=number=FG16(96)/cyan (sha values), G=string=FG16(32)/green (author/
//  committer values), N=defname=BOLD (subject), S=default (none).  VERIFIED
//  byte-for-byte vs `be commit:#<sha> --color` (modulo the line-terminator
//  39-vs-0m reset, a residual binding-level delta shared by every content
//  view — the C keeper resets fully to ESC[0m per line, the HUNK .color sink
//  emits the fg-only default delta 39 — see log.js's same note).
//  TAG_W (new=22) renders FG16(32)/green — the keeper tags the non-subject
//  message remainder 'W' (PROJ.c:458; its THEME16 colour == 'G').  We use 'G'
//  (also green) for header VALUES and 'W' for the message body so the byte
//  output matches either way (both → green); 'S' (default) for line `\n`
//  terminators so a span's colour does not bleed across the newline (the
//  log.js pattern — the C keeper's own per-line ESC[0m reset is a separate
//  proj_emit_hunk renderer, a documented binding-level delta vs the HUNK sink).
const TAG_R = 17, TAG_L = 11, TAG_G = 6, TAG_N = 13, TAG_S = 18, TAG_W = 22;
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }

//  --- a 1..40 hex hashlet test (the `#<hex>` / `?<hex>` slot). ---
function isHexish(s) { return !!s && /^[0-9a-f]{1,40}$/.test(s); }

//  --- raw ordered header walk (GITu8sDrainCommit twin, dog/git/GIT.c:119) ---
//  Split a raw git object's bytes into ORDERED `{name, value}` headers + the
//  message body.  One `field SP value\n` per header, RFC-822 continuation
//  folding (a line beginning with a SPACE continues the previous value), a
//  blank line ends the headers (the rest, verbatim, is the body).  Returns
//  { headers:[{name,value}], body:"<bytes-after-the-blank-line>" } over the
//  decoded text.  Re-rolled in PURE JS (a literal `\n` scan + first-space
//  split) because git.parseCommit drops header order + non-canonical headers.
function parseHeaders(text) {
  const headers = [];
  let i = 0;
  const n = text.length;
  while (i < n) {
    //  A blank line (immediate '\n') ends the header block; body = the rest.
    if (text[i] === "\n") { return { headers: headers, body: text.slice(i + 1) }; }
    //  Take this header line.
    let j = i;
    while (j < n && text[j] !== "\n") j++;
    let line = text.slice(i, j);
    i = (j < n) ? j + 1 : j;
    //  Fold RFC-822 continuation lines (next line starts with a space).
    while (i < n && text[i] === " ") {
      let k = i;
      while (k < n && text[k] !== "\n") k++;
      line += "\n" + text.slice(i, k);
      i = (k < n) ? k + 1 : k;
    }
    const sp = line.indexOf(" ");
    if (sp < 0) headers.push({ name: line, value: "" });
    else headers.push({ name: line.slice(0, sp), value: line.slice(sp + 1) });
  }
  //  No blank line at all (a header-only object): no body.
  return { headers: headers, body: "" };
}

//  --- short-hashlet -> full COMMIT|TAG sha (KEEPResolveHex twin) ------------
//  Backed by the ONE canonical any-object resolver `store.resolveHexAny` (the
//  KEEPLookup twin): a full sha passes through iff it exists, a short prefix
//  resolves to the unique object of ANY type (ambiguous -> undefined, matching
//  the C type-agnostic lookup).  commit: then keeps only a COMMIT or TAG — a
//  blob/tree prefix is not a commit object, so it resolves to NOTHING.
function resolveHashlet(k, hexish) {
  if (!isHexish(hexish)) return undefined;
  const sha = k.resolveHexAny(hexish);
  if (!sha) return undefined;
  const o = k.getObject(sha);
  return (o && (o.type === "commit" || o.type === "tag")) ? sha : undefined;
}

//  --- slot resolution (proj_resolve_object_sha + KEEPProjDispatch twin) -----
//  Parse `commit:<uri>` into a target sha + the banner-uri hex.  Slot rules
//  (PROJ.c:41/640, verified vs `be commit:`):
//    `#<hex>`            fragment hashlet — banner shows the LITERAL fragment.
//    `?<hex>`            a hex QUERY is PROMOTED to a `#` hashlet (KEEPProjDispatch
//                        :640) — but the banner shows the RESOLVED full sha.
//    `?<ref>`            a non-hex QUERY = a branch ref (REFSResolve) — banner =
//                        the resolved full sha.
//    bare `commit:`      no slot → the CUR tip (the --at HEAD wtlog curTip); the
//                        banner shows the resolved full sha.  [SPEC: the C
//                        oracle KEEPFAILs on a bare `commit:`; this view follows
//                        the ticket SPEC `bare = cur tip`.]
//  Returns { sha, bannerHex } | undefined (resolve fail → no output, nonzero).
function resolveSlot(k, repo, parsed) {
  const q = parsed.query, frag = parsed.fragment, hasQuery = parsed.hasQuery;

  //  An explicit empty `#` (`commit:#` / `commit:?#`) → resolve fail.
  if (parsed.emptyFrag) return undefined;

  //  Fragment slot `#<hex>` — banner shows the LITERAL fragment input.
  if (frag) {
    if (!isHexish(frag)) return undefined;
    const sha = resolveHashlet(k, frag);
    return sha ? { sha: sha, bannerHex: frag } : undefined;
  }
  //  Query slot `?<x>`.
  if (hasQuery) {
    if (q === "") return undefined;                   // empty `?` → resolve fail
    //  A hex query is promoted to a hashlet; the banner shows the resolved sha.
    if (isHexish(q)) {
      const sha = resolveHashlet(k, q);
      return sha ? { sha: sha, bannerHex: sha } : undefined;
    }
    //  A non-hex query = a branch ref (REFSResolve); banner = resolved sha.
    const sha = k.resolveRef(q);
    return sha ? { sha: sha, bannerHex: sha } : undefined;
  }
  //  Bare `commit:` — the cur tip (SPEC: bare = cur tip).
  const cur = wtlog.open(repo).curTip();
  const sha = cur && cur.sha;
  return (sha && isFullSha(sha)) ? { sha: sha, bannerHex: sha } : undefined;
}

//  --- the metadata hunk body (KEEPProjCommit bytes, PROJ.c:431-507) ---------
//  Build the hunk body bytes + the per-field tok32 spans for --color.  The body
//  is `commit <sha>\n` + the raw object's ordered headers + blank + message,
//  with a forced trailing `\n` (the object already carries one — but force it
//  so a truncated/odd object still ends clean).  Spans: each header's NAME (+
//  trailing space) = R/blue; a `tree`/`parent`/`commit` sha value = L/cyan; any
//  other value = G/green; the subject (1st body line) = N/bold; the rest plain.
function buildHunk(sha, headers, body) {
  const parts = [];   // [{ text, tag }]  — concatenated; tag drives the span
  function emit(text, tag) { if (text.length) parts.push({ text: text, tag: tag }); }

  //  `commit <sha>\n` — "commit " R, the sha (the page, no link) L, "\n" S.
  emit("commit ", TAG_R);
  emit(sha, TAG_L);
  emit("\n", TAG_S);

  //  Each header verbatim, in object order.
  for (const h of headers) {
    emit(h.name + " ", TAG_R);
    //  tree/parent (and the synthetic `commit`, already emitted) link the sha;
    //  every other field (author/committer/gpgsig/encoding/mergetag/…) plain.
    const linky = (h.name === "tree" || h.name === "parent");
    emit(h.value, linky ? TAG_L : TAG_G);
    emit("\n", TAG_S);
  }

  //  Blank line, then the message body.  Subject (1st line) BOLD ('N'), the
  //  rest GREEN ('W' — PROJ.c:458, C --color paints those body lines green).
  //  Emit per-line so a span's colour never bleeds across a '\n' (the log.js
  //  anti-bleed pattern): each line's text gets its colour tag, each '\n' gets
  //  'S'/default — the C keeper's per-line ESC[0m reset is its own
  //  proj_emit_hunk renderer (a documented binding-level delta vs the sink).
  emit("\n", TAG_S);                          // the blank separator line
  let msg = body;
  //  Force a trailing \n if absent (PROJ.c forces it).
  if (msg.length === 0 || msg[msg.length - 1] !== "\n") msg += "\n";
  if (msg.length) {
    let line = 0;
    let off = 0;
    while (off < msg.length) {
      let nl = msg.indexOf("\n", off);
      const hasNL = nl >= 0;
      if (!hasNL) nl = msg.length;
      emit(msg.slice(off, nl), line === 0 ? TAG_N : TAG_W);  // subject bold, rest green
      if (hasNL) emit("\n", TAG_S);            // terminator default → no bleed
      off = hasNL ? nl + 1 : nl;
      line++;
    }
  }

  //  Concatenate to bytes + pack the cumulative-offset tok32 spans.
  let text = "";
  for (const p of parts) text += p.text;
  const bytes = utf8.Encode(text);
  const spans = new Uint32Array(parts.length);
  let off = 0;
  for (let i = 0; i < parts.length; i++) {
    off += utf8.Encode(parts[i].text).length;
    spans[i] = tok(parts[i].tag, off);
  }
  return { bytes: bytes, toks: spans };
}

//  --- the handler -----------------------------------------------------------
module.exports = function handle(row, ctx) {
  const sink = ctx && ctx.sink;
  if (!sink) return;
  const repo = (ctx && ctx.repo) || null;
  if (!repo) return;

  //  The whole `commit:<uri>` rides ctx.args[0] (the seed lowered it to a "."
  //  placeholder — a fragment-only URI can't survive a queue row; cf.
  //  cat.js/log.js).  Never trust row.uri.
  const rawArgs = (ctx && ctx.args && ctx.args.length) ? ctx.args : [row.uri];
  let first = String(rawArgs[0] || "");
  if (first.indexOf("commit:") !== 0) first = "commit:" + first;
  const u = new URI(first);
  //  URI collapses `commit:?` (empty query) into query "" — recover the
  //  explicit-`?` distinction from the raw token (an empty `?` is a resolve
  //  FAIL, distinct from a bare `commit:` = cur tip).
  const rest = first.slice("commit:".length);
  const hasHash = rest.indexOf("#") >= 0;
  const parsed = {
    query: u.query || "",
    fragment: u.fragment || "",
    //  An explicit `#` with no value (`commit:#` / `commit:?#`) is a resolve
    //  FAIL (an empty hashlet), distinct from a bare `commit:` (= cur tip).
    emptyFrag: hasHash && (u.fragment || "") === "",
    hasQuery: rest.indexOf("?") >= 0 && rest.indexOf("#") !== 0,
  };
  //  `?#<hex>` is a FRAGMENT slot (the `?` is empty), not an empty-query fail.
  if (parsed.fragment) parsed.hasQuery = false;

  const k = store.open(repo.storePath, repo.project);
  const slot = resolveSlot(k, repo, parsed);
  if (!slot) throw "COMMITNONE";                 // resolve fail → no stdout, nonzero

  //  Read the object; one-shot tag deref (DOG_OBJ_TAG → its `object` header's
  //  target, which MUST then be a commit — PROJ.c:396).
  let sha = slot.sha;
  let obj = k.getObject(sha);
  if (!obj) throw "COMMITNONE";                   // missing object → fail
  if (obj.type === "tag") {
    const tagText = utf8.Decode(obj.bytes);
    const th = parseHeaders(tagText);
    let target;
    for (const h of th.headers) if (h.name === "object") { target = h.value; break; }
    if (!target || !isFullSha(target)) throw "COMMITFAIL";
    sha = target;
    obj = k.getObject(sha);
    if (!obj) throw "COMMITFAIL";
  }
  if (obj.type !== "commit") throw "COMMITFAIL";  // non-commit → fail

  //  Raw ordered header walk over the commit object's own bytes.
  const text = utf8.Decode(obj.bytes);
  const ph = parseHeaders(text);
  const hunk = buildHunk(sha, ph.headers, ph.body);

  //  ONE feed: EMPTY uri (no banner line) so --plain matches the C keeper which
  //  elides the `commit:?<sha>` URI as a U-span.  The HUNK content render adds
  //  the single trailing blank-line separator (a binding-level constant shared
  //  by every content view).  verb "" → no verb word.
  sink.feed("", hunk.bytes, hunk.toks, "", 0n);
};
