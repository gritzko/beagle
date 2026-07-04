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
//  JAB-004: a PLAIN-args verb — commit(...args) loops, commitOne(arg) self-parses
//  the WHOLE `commit:<uri>` string, reads be.repo/be.sink/ambient.format (ctx =
//  direct-handler fallback).  Output is ONE content HUNK fed to be.sink (EMPTY uri → no banner line, so
//  --plain matches the C keeper which elides the `commit:?<sha>` URI as a
//  U-span; the loop edge renders plain/color/tlv via view/bro.js renderHunkLog
//  — the view never writes fd 1).

"use strict";

const store   = require("../../shared/store.js");
const wtlog   = require("../../shared/wtlog.js");
const shalib  = require("../../shared/util/sha.js");
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const navlib  = require("../../shared/nav.js");        // URI-011: full-URI hunk helper
//  COMMIT-006: reuse the diff GENERATOR (graf weave + tree-vs-ref) directly —
//  call its handler with a pinned `views` spec; the inert diff VIEW routing
//  (ctx.views unset, JS-071) is bypassed.  See inlineDiff() below.
const diffView = require("../diff/diff.js");
const recurse = require("../../core/recurse.js");
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
//  BRO-006: TAG_U (20) is the invisible click-target — after a linky sha span we
//  splice its URI bytes + a `U` tok so the pager's `_uriAt` left-click navigates
//  (mirrors C KEEPProjCommit PROJ.c:489-493: `<scheme>:?<sha40>` + tok 'U').
const TAG_U = 20;
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
  //  JS-082 (co-landed COMMIT-004): a FULL 40-hex sha passes through verbatim
  //  (resolveHexAny's {1,39} prefix scanner rejects 40); a short prefix goes
  //  through the any-object resolver.  The commit/tag type gate below applies
  //  to both, so a blob/tree sha still resolves to NOTHING.
  const sha = isFullSha(hexish) ? hexish : k.resolveHexAny(hexish);
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
  const q = parsed.query, frag = parsed.fragment, hasQuery = parsed.hasQuery,
        path = parsed.path;

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
  //  COMMIT-004 (Defect, fix #2): a non-empty PATH slot (`commit:<sha>`) is a
  //  hashlet target — resolve it, NEVER silently fall through to the cur tip
  //  (`commit:0000…` must FAIL, not print the tip).  The cwd placeholder "." is
  //  the bare-`commit:` lowering (see the handler), so it falls to the tip.
  if (path && path !== ".") {
    const sha = resolveHashlet(k, path);
    return sha ? { sha: sha, bannerHex: sha } : undefined;
  }
  //  Bare `commit:` — the cur tip (SPEC: bare = cur tip).
  const cur = wtlog.open(repo).curTip();
  const sha = cur && cur.sha;
  return (sha && isFullSha(sha)) ? { sha: sha, bannerHex: sha } : undefined;
}

//  DIS-060: the sub commit PINNED at parsed.path by the BASE tree gitlink —
//  resolve the base ref (path IGNORED) in baseK, read the 160000 entry's sha.
function subGitlinkPin(baseK, repo, parsed) {
  const base = resolveSlot(baseK, repo, { query: parsed.query, fragment:
    parsed.fragment, path: "", emptyFrag: parsed.emptyFrag, hasQuery: parsed.hasQuery });
  if (!base) return undefined;
  const treeSha = baseK.commitTree(base.sha);
  const ent = treeSha ? baseK.descendPath(treeSha, parsed.path.split("/")) : undefined;
  return (ent && ent.kind === "commit") ? ent.sha : undefined;
}

//  DIS-060: a full-sha target as a slot record (a `#` fragment hashlet).
function pinFrag(sha) {
  return { query: "", fragment: sha, path: "", emptyFrag: false, hasQuery: false };
}

//  --- the metadata hunk body (KEEPProjCommit bytes, PROJ.c:431-507) ---------
//  Build the hunk body bytes + the per-field tok32 spans for --color.  The body
//  is `commit <sha>\n` + the raw object's ordered headers + blank + message,
//  with a forced trailing `\n` (the object already carries one — but force it
//  so a truncated/odd object still ends clean).  Spans: each header's NAME (+
//  trailing space) = R/blue; a `tree`/`parent`/`commit` sha value = L/cyan; any
//  other value = G/green; the subject (1st body line) = N/bold; the rest plain.
function buildHunk(sha, headers, body) {
  const parts = [];   // [{ text, tag, uri? }] — concatenated; tag drives the span,
                      // uri (if set) splices an invisible `U` click-target after.
  function emit(text, tag, uri) {
    if (text.length) parts.push({ text: text, tag: tag, uri: uri || "" });
  }

  //  `commit <sha>\n` — "commit " R, the sha (the page, no link) L, "\n" S.  No
  //  `U` here: the synthetic commit header IS the page (mirrors PROJ.c:431-436;
  //  COMMIT-001's `diff:?<sha>` link was superseded by COMMIT-002's relay).
  emit("commit ", TAG_R);
  emit(sha, TAG_L);
  emit("\n", TAG_S);

  //  Each header verbatim, in object order.
  for (const h of headers) {
    emit(h.name + " ", TAG_R);
    //  BRO-006: tree/parent sha values are clickable — tree → `tree:?<sha40>`,
    //  parent → `commit:?<sha40>` (open the parent), mirroring PROJ.c:468-493.
    //  Every other field (author/committer/gpgsig/encoding/mergetag/…) is plain.
    const linkScheme = (h.name === "tree") ? "tree"
                     : (h.name === "parent") ? "commit" : null;
    const linky = linkScheme !== null && isFullSha(h.value.slice(0, 40));
    //  URI-011: full nav-authority URI — authority BEFORE the `?` so the hunk
    //  carries the FULL address (`<scheme>://name?<sha40>`; "" auth = byte-parity).
    const uri = linky ? navlib.navUri(linkScheme, "") + "?" + h.value.slice(0, 40) : "";
    emit(h.value, linky ? TAG_L : TAG_G, uri);
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

  //  Concatenate to bytes + pack the cumulative-offset tok32 spans.  The PLAIN
  //  variant (no `U`) is the COMMIT-003 bytes verbatim; the color/tlv variant
  //  splices each linky part's URI bytes + a `U` tok right after its span, so a
  //  pager left-click on the sha navigates (the `_uriAt` contract).  Plain feeds
  //  EMPTY toks (COMMIT-003), so its bytes must stay U-free — two bodies.
  let text = "";
  for (const p of parts) text += p.text;
  const bytes = utf8.Encode(text);
  const spans = new Uint32Array(parts.length);
  let off = 0;
  for (let i = 0; i < parts.length; i++) {
    off += utf8.Encode(parts[i].text).length;
    spans[i] = tok(parts[i].tag, off);
  }

  //  The U-bearing body/toks for color/tlv.  Walk parts in order; after a part
  //  carrying a `uri`, append the URI bytes and a `U` tok (end = new length).
  let textU = "";
  const tagsU = [];
  let offU = 0;
  for (const p of parts) {
    textU += p.text;
    offU += utf8.Encode(p.text).length;
    tagsU.push(tok(p.tag, offU));
    if (p.uri) {
      textU += p.uri;
      offU += utf8.Encode(p.uri).length;
      tagsU.push(tok(TAG_U, offU));
    }
  }
  return {
    bytes: bytes, toks: spans,
    bytesU: utf8.Encode(textU), toksU: Uint32Array.from(tagsU),
  };
}

//  --- COMMIT-006: inline the commit's full diff (tree vs parent) ------------
//  Mirror C `be commit:?<sha>` ([COMMIT-002], df596d0f): after the metadata,
//  relay graf's `diff:?<sha>` hunk stream (first-parent.tree → commit.tree).
//  Driven by calling the diff VIEW handler with a pinned `views` spec — NOT the
//  dead ctx.views routing (JS-071).  ALWAYS vs the FIRST parent (merges too,
//  user RULING; LOG-001 spine); only ROOT (0 parents) skips.  The handler folds
//  it into the ONE metadata record (plain) or feeds it after (color/tlv).
//
//  Two render shapes match native byte-for-byte (the HUNK plain feed adds ONE
//  trailing separator PER record, color/tlv add none):
//    plain → fold the diff's rendered TEXT into the metadata record so there is
//            no mid-stream separator (native has none between metadata + diff).
//    color/tlv → feed the diff's own HUNK records after the metadata record
//            (the diff machinery feeds them via out.feed; no separator added).
function diffSpecRow(sha, parentSha) {
  const row = { uri: "diff:?" + sha, verb: "diff" };
  const spec = { mode: "range", fromSha: parentSha, toSha: sha,
                 navver: parentSha + ".." + sha, path: "" };
  return { row: row, spec: spec };
}

//  JAB-004: diff is a PURE plain-args verb — it reads be.out/be.sink/be.views/
//  be.uri/be.flags/be.format off the GLOBAL only.  So SWAP those globals (incl.
//  be.views pinning ds.spec at ds.row.uri) around the call, call diffView(uri)
//  PLAIN with the diff-uri STRING, then RESTORE.  Commit always runs with a be.
function runDiff(repo, ds, mode, out, sink, flags) {
  const uri = ds.row.uri;
  const views = {}; views[uri] = ds.spec;
  //  JAB-004: swap be.repo too — a `commit:<sub>?<sha>` descend gives us the SUB
  //  repo; diff reads be.repo, so it must point at the sub, not the base.
  const s = { repo: be.repo, out: be.out, sink: be.sink, views: be.views,
              uri: be.uri, flags: be.flags, format: be.format };
  be.repo = repo; be.out = out; be.sink = sink; be.views = views; be.uri = uri;
  be.flags = flags; be.format = mode;
  try { diffView(uri); }
  finally { be.repo = s.repo; be.out = s.out; be.sink = s.sink;
            be.views = s.views; be.uri = s.uri; be.flags = s.flags;
            be.format = s.format; }
}

//  Capture the diff's PLAIN rendered text (out.chunk) for the metadata-fold.
function diffPlainText(repo, sha, parentSha) {
  const chunks = [];
  runDiff(repo, diffSpecRow(sha, parentSha), "plain",
          { chunk: function (t) { chunks.push(t); } },
          { feed: function () {} }, ["--plain"]);
  return chunks.join("");
}

//  Feed the diff's own HUNK records (color/tlv) into the commit sink, so they
//  follow the metadata record exactly like the C graf relay.
function diffFeedRecords(repo, sha, parentSha, mode, sink) {
  runDiff(repo, diffSpecRow(sha, parentSha), mode,
          { chunk: function () {} }, sink,
          mode === "color" ? ["--color"] : []);
}

//  --- commit ONE arg --------------------------------------------------------
//  JAB-004: self-parse `commit:<uri>` from the STRING arg, read be.repo/be.sink
//  + ambient.format(), feed the same sink; `ctx` = direct-handler fallback (no be).
function commitOne(arg, ctx) {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = (_be && _be.sink) || (ctx && ctx.sink) || null;
  if (!sink) return;
  //  SUBS-045: `let` so a `commit:<sub>?<sha>` link can swap to the sub repo.
  let repo = (_be && _be.repo) || (ctx && ctx.repo) || null;
  if (!repo) return;

  //  The whole `commit:<uri>` is the STRING arg (a fragment-only URI can't
  //  survive a queue row; cf. cat.js/log.js).  Never trust a row.uri.
  let first = String(arg || "");
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
    //  COMMIT-004 (fix #2): the PATH slot (`commit:<sha>`) — read as a hashlet
    //  target, not a silent cur-tip fall-through.
    path: u.path || "",
    //  An explicit `#` with no value (`commit:#` / `commit:?#`) is a resolve
    //  FAIL (an empty hashlet), distinct from a bare `commit:` (= cur tip).
    emptyFrag: hasHash && (u.fragment || "") === "",
    hasQuery: rest.indexOf("?") >= 0 && rest.indexOf("#") !== 0,
  };
  //  `?#<hex>` is a FRAGMENT slot (the `?` is empty), not an empty-query fail.
  if (parsed.fragment) parsed.hasQuery = false;

  //  SUBS-045/DIS-060: `commit:<sub>?<ref>` descends into the mounted sub.  A
  //  BASE ref (typed nav) names the sub commit PINNED by the base gitlink at
  //  <sub>; a SUB ref (descended-log `?<sub-sha>` click) resolves in the sub.
  //  Try the base pin first; on miss the sub-sha resolves in the sub (disjoint).
  if (parsed.path && parsed.path !== ".") {
    const baseK = store.open(repo.storePath, repo.project);
    const pin = (parsed.hasQuery || parsed.fragment)
              ? subGitlinkPin(baseK, repo, parsed) : undefined;
    const d = recurse.resolveRepoForPath(repo, parsed.path);
    if (d.prefix) {
      repo = d.repo;
      if (pin) Object.assign(parsed, pinFrag(pin));  // retarget to the pinned sub sha
      else parsed.path = d.rest;                 // else keep the ref for the sub
    }
  }

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

  //  COMMIT-003: colour spans ONLY for --color; plain feeds EMPTY toks (the
  //  cat/blob gate) — a hand-built toks table failed the HUNK drain → 0 bytes.
  //  BRO-006: color/tlv feed the U-bearing body+toks (tree/parent sha links);
  //  plain feeds the U-free body so the hidden URI bytes never leak (no toks to
  //  elide them in the empty-toks plain path).
  const mode = ambient.format();   // JAB-004
  const bytes = mode === "plain" ? hunk.bytes : hunk.bytesU;
  const toks  = mode === "plain" ? new Uint32Array(0) : hunk.toksU;

  //  COMMIT-006: inline the diff vs the FIRST parent for EVERY commit with a
  //  parent — merges included (user RULING 2026-06-26: git `--first-parent`,
  //  uniform with the non-merge path, LOG-001 spine).  ROOT (0 parents) SKIPS
  //  (no base; native's root diff is wt-driven — skip is simpler).
  const parents = k.commitParents(sha) || [];
  const inline = parents.length >= 1 ? parents[0] : null;

  //  PLAIN: fold the diff text INTO the metadata record (no mid separator, like
  //  the C keeper-then-graf relay) — the HUNK plain feed then appends the single
  //  trailing separator after the WHOLE record (the existing binding-level delta).
  if (mode === "plain" && inline) {
    const diffText = diffPlainText(repo, sha, inline);
    const all = utf8.Decode(bytes) + diffText;
    sink.feed("", utf8.Encode(all), new Uint32Array(0), "", 0n);
    return;
  }

  //  ONE feed: EMPTY uri (no banner line) so --plain matches the C keeper which
  //  elides the `commit:?<sha>` URI as a U-span.  The HUNK content render adds
  //  the single trailing blank-line separator (a binding-level constant shared
  //  by every content view).  verb "" → no verb word.
  sink.feed("", bytes, toks, "", 0n);

  //  COMMIT-006: color/tlv relay the diff's own HUNK records after the metadata
  //  (no separator added in those modes) — the C `diff:?<sha>` graf relay twin.
  if (inline) diffFeedRecords(repo, sha, inline, mode, sink);
}

//  JAB-004: PLAIN verb (`.jab="args"`) — loops its args reading `be` (own
//  `(row,ctx)` entry fallback removed; commit is now purely plain-args).
function commit() {
  for (let i = 0; i < arguments.length; i++) commitOne(arguments[i]);
}
commit.jab = "args";
module.exports = commit;
