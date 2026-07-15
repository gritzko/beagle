//  views/list/list.js — LIST-001: the `list:<path>` read-only VIEW, a github-
//  style directory browser.  ONE row per entry (files AND dirs):
//    `<wt-marker> <name>  <pale-grey last-commit summary>  <short rel-age>`
//  It FUSES ls's classifyDir (name + wt change bucket → the marker) with each
//  entry's LAST COMMIT (message summary + age) from shared/lastcommit.js — the
//  bounded first-touch history walk.  A dir = the newest commit touching anything
//  under it.  `list:<path>?<rev>` walks from <rev>; bare = cur tip + wt overlay
//  (classifyDir reads the wt).  jab+libdog bindings + JS ONLY, NO dog binary.
//  Reuse, do not fork: ls classifyDir, log's summary slot, render.relAge, the
//  pager U-tag; ls/tree stay untouched and cheap — list owns the O(history) fuse.
"use strict";

const store      = require("../../shared/store.js");
const wtlog      = require("../../shared/wtlog.js");
const classify   = require("../../shared/classify.js");
const lastcommit = require("../../shared/lastcommit.js");
const shalib     = require("../../shared/util/sha.js");
const render     = require("../../view/render.js");
const theme      = require("../../view/theme.js");
const navlib     = require("../../shared/nav.js");
const pathlib    = require("../../shared/util/path.js");
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const wtpath = require("../../core/discover.js").wtpath;
const join       = pathlib.join;
const isFullSha  = shalib.isFullSha;

//  BRO-006/log.js tok32: [31..27] tag, [23..0] end byte offset.  V = the ls verb
//  slot (wt marker) resolved per-bucket from theme.VERB_SLOT; D = grey (the pale
//  summary, log.js TAG_D); L = the age (cyan, like a date); U = hidden click-
//  target (20); S = default sep/'\n' (no colour bleed).
const TAG_D = 3, TAG_L = 11, TAG_U = 20, TAG_S = 18;
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }

//  Left-justify the 3-col wt marker (ls verbCol twin), and pad the name column
//  so the summary/age align github-style.  NAME_W is the fixed name column width.
const NAME_W = 24;
function padName(name) { return name.length >= NAME_W ? name + " " : name + " ".repeat(NAME_W - name.length + 1); }

//  Append ONE entry row's bytes + tok32 spans.  Row: `<marker> <name-pad><nav>
//  <summary>  <age>\n`, the hidden nav URI riding under the name's U token (a
//  pager left-click on the name opens `list:<sub>/` for a dir, `cat:<file>` for a
//  file).  marker = the wt bucket verb; summary/age are the last-commit fuse
//  (blank when unattributed within the walk ceiling).
function appendRow(textParts, spans, off, marker, name, navUri, summary, age) {
  const vcol = render.verbCol(marker);
  const namePad = padName(name);
  const pre = utf8.Encode(vcol + " ");                      // marker + sep
  const nameB = utf8.Encode(namePad);                       // the visible name col
  const uriB  = utf8.Encode(navUri);                        // hidden nav (U)
  const summB = utf8.Encode(summary);                       // pale-grey summary
  const midB  = utf8.Encode(age ? "  " : "");               // sep before age
  const ageB  = utf8.Encode(age || "");                     // short rel-age
  const nlB   = utf8.Encode("\n");
  textParts.push(pre); textParts.push(nameB); textParts.push(uriB);
  textParts.push(summB); textParts.push(midB); textParts.push(ageB); textParts.push(nlB);
  const ePre  = pre.length;                                 // marker + sep
  const eName = ePre + nameB.length;                        // name column
  const eUri  = eName + uriB.length;                        // hidden nav URI
  const eSumm = eUri + summB.length;                        // summary
  const eMid  = eSumm + midB.length;                        // sep
  const eAge  = eMid + ageB.length;                         // age
  const eNL   = eAge + nlB.length;                          // '\n'
  const vtag  = theme.VERB_SLOT[marker] || "S";
  spans.push([tagCode(vtag), off + ePre]);                 // wt marker (palette slot)
  spans.push([tagCode("F"), off + eName]);                 // name (violet)
  spans.push([tagCode("U"), off + eUri]);                  // hidden nav URI
  spans.push([TAG_D, off + eSumm]);                        // pale-grey summary
  spans.push([TAG_S, off + eMid]);                         // sep
  spans.push([TAG_L, off + eAge]);                         // age
  spans.push([TAG_S, off + eNL]);                          // '\n' (no bleed)
  return eNL;
}

//  Strip trailing slashes from a dir (keep a lone "/").
function noSlash(p) { while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1); return p; }

//  `abs` relative to `base` in DIR form (trailing "/"); "" when equal / not under.
function relDir(base, abs) {
  base = noSlash(base); abs = noSlash(abs);
  if (abs === base) return "";
  const pfx = base + "/";
  return abs.indexOf(pfx) === 0 ? abs.slice(pfx.length) + "/" : "";
}

//  LIST-001: resolve `?<rev>` to a commit sha (why.js resolveCommit twin — reuse
//  the convention, no hand-parse): branch ref FIRST, then full-sha / hashlet.
//  Empty query → the cur tip (the wt-overlay base).
function resolveTip(k, wtl, query) {
  if (!query) return (wtl.curTip() || {}).sha || undefined;
  const byRef = k.resolveRef(query);
  if (byRef && isFullSha(byRef)) return byRef;
  if (isFullSha(query)) return k.getObject(query) ? query : undefined;
  if (/^[0-9a-f]{6,39}$/.test(query)) return k.resolveHexAny(query);
  return undefined;
}

//  LIST-001: build + feed ONE fused hunk for the scope dir.  Exposed for the
//  repro test (the ls/why hunk-builder pattern).  `entries` = the sorted merge
//  of classifyDir files+dirs; `commits` = name → { summary, ts, sha } (or {}).
function emitHunk(sink, banner, navPfx, entries, commits, now) {
  const textParts = [], spans = [];
  let off = 0;
  for (const e of entries) {
    const c = commits[e.name] || null;
    const summary = c ? c.summary : "";
    const age = c ? render.relAge(c.ts, now) : "";
    //  A dir click opens `list:<sub>/` (stay in the browser); a file → `cat:`.
    const nav = e.dir ? navlib.navLink("list", navPfx + e.name + "/")
                      : navlib.navLink("cat", navPfx + e.name);
    const label = e.dir ? e.name + "/" : e.name;
    off += appendRow(textParts, spans, off, e.marker, label, nav, summary, age);
  }
  const body = new Uint8Array(off);
  let p = 0;
  for (const part of textParts) { body.set(part, p); p += part.length; }
  const toks = new Uint32Array(spans.length);
  for (let i = 0; i < spans.length; i++) toks[i] = tok(spans[i][0], spans[i][1]);
  sink.feed(banner, body, toks, "", 0n);
}

//  LIST-001: list ONE `list:<path>?<rev>` scope — self-parse the URI, fuse the wt
//  listing (classifyDir) with the last-commit walk, feed ONE content hunk.
function listOne(arg) {
  const _be  = (typeof be !== "undefined") ? be : null;
  const sink = (_be && _be.sink) || null;
  if (!sink) return;
  const repo = (_be && _be.repo) || (_be ? _be.treeAt() : null);
  if (!repo) return;

  //  URI-013: ONE structured parse of `list:<path>?<rev>` (no hand-slice).
  let first = String(arg || "");
  if (first.indexOf("list:") !== 0) first = "list:" + first;
  const u = uri._parse(first);
  const path  = u.path || "";
  const query = u.query || "";

  //  Resolve the scope to an ABSOLUTE dir (top wt, a wt-relative path, or absolute).
  //  BE-028: confine a wt-relative scope via wtJoin — a `..` climbing above the
  //  wt THROWS NAVESCAPE (never a lexical escape); an absolute path is left as-is.
  const topWt = repo.wt;
  let absScope;
  if (path && path[0] === "/") absScope = path;
  else                         absScope = wtpath(topWt, path || "");
  absScope = noSlash(absScope);

  const scopePfx = relDir(repo.wt, absScope);            // rel to the owning wt
  const navPfx   = relDir(topWt, absScope);              // rel to the top wt
  const banner   = navlib.navLink("list", navPfx, query || undefined);

  const k   = store.open(repo.storePath, repo.project);
  const wtl = wtlog.open(repo);

  //  1. wt listing: classifyDir → immediate files (with wt bucket) + dirs.  A dir
  //  gets no wt bucket; its marker is "dir" unless it has changes beneath it.
  const res = classify.classifyDir(repo, wtl, k, scopePfx);
  //  LIST-001: ROLL UP wt-dirtiness into dir markers — the recursive classifier
  //  gives every non-eq path; a dir with any dirty descendant shows 'mod' (its
  //  status), coloured like status, instead of a plain grey 'dir'.
  const dirtyDirs = {};
  for (const r of (classify.classify(repo, wtl, k, {}).rows || [])) {
    if (scopePfx && r.path.indexOf(scopePfx) !== 0) continue;
    const rel = r.path.slice(scopePfx.length), slash = rel.indexOf("/");
    if (slash > 0) dirtyDirs[rel.slice(0, slash)] = 1;   // dirty path under a dir
  }
  const entries = [];
  for (const f of res.files) entries.push({ name: f.name, dir: false, marker: f.bucket });
  for (const name of res.dirs)
    entries.push({ name: name, dir: true, marker: dirtyDirs[name] ? "mod" : "dir" });
  entries.sort(function (a, b) {
    const ka = a.name + (a.dir ? "/" : ""), kb = b.name + (b.dir ? "/" : "");
    return ka < kb ? -1 : ka > kb ? 1 : 0;
  });

  //  2. the last-commit fuse: attribute each entry name its newest-touching
  //  commit via the bounded walk from the resolved tip (bare → cur tip).
  const tip = resolveTip(k, wtl, query);
  let commits = {};
  if (tip && isFullSha(tip))
    commits = lastcommit.lastCommits(k, tip, scopePfx, entries.map(function (e) { return e.name; }));

  //  3. fuse + feed ONE hunk; the age is relative to NOW.
  emitHunk(sink, banner, navPfx, entries, commits, ron.now());
}

//  Registry contract (JAB-004): a PLAIN-args handler looping its URI args off `be`.
function list() {
  const argv = arguments.length ? arguments : ["list:"];
  for (let i = 0; i < argv.length; i++) listOne(argv[i]);
}
list.jab = "args";
module.exports = list;

//  LIST-001: expose the hunk builders + tip resolver for the repro test (the
//  ls/why/log exported-internals pattern).
module.exports.tok = tok;
module.exports.appendRow = appendRow;
module.exports.emitHunk = emitHunk;
module.exports.resolveTip = resolveTip;
