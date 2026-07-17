//  status.js — `be status` reimplemented as a repo-local JS extension
//  (JS-027 / JS-031).  Pure JS over the JABC bindings + bin/lib/*: be.js
//  (repo discovery), wtlog.js (wtlog reader), store.js (object store),
//  classify.js (baseline ⊕ wt ⊕ put ⊕ del → buckets), ignore.js
//  (.gitignore).  No C, no dog — shares zero code with sniff.
//
//  Output mirrors native `be status --plain`:
//    status:
//     <date7> <verb3> <path>           (one per bucketed row)
//    <cwd-rel>?<branch>\t<n> ok, <m> mod, …   (summary line)
//
//  File rows only (JS-031): ok/put/new/mov/mod/del/mis/unk.  Submodule
//  rows + ahead/behind come in JS-032/033.  Rendered as plain text
//  matching the HUNK table layout (dog/HUNK.c::htbl_emit): a 7-col
//  centred date, a 3-col left-justified verb, then the path; the summary
//  packs `<rel>?<branch>` + a `\t` + per-bucket `<n> <verb>` segments.
//
//  Usage:  be status                       (be forks jabc on this script)
//          jab be/loop.js status [args]     (JSQUE-008 resident-loop handler)

"use strict";

//  JSQUE-008: sibling libs via relative require ("./lib/X.js"), resolved against
//  this module's own dir — robust under the resident loop (not argv[1]/__dirname).
//  JSQUE-016: by-verb reorg — core/discover + shared/ kernel + view/ via ../../ .
const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const subs     = require("../../shared/subs.js");
const branchlib = require("../../shared/branch.js");   // SUBS-050: the ONE branch codec
const ambient  = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const render   = require("../../view/render.js");
const navlib   = require("../../shared/nav.js");        // URI-011: full-URI nav helper
const quadlib  = require("../../shared/quad.js");       // BRO-030: the quad model
const quadrender = require("../../view/quadrender.js"); // BRO-030: quad row render
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const discover = require("../../core/discover.js");
const wtpath = discover.wtpath;
//  JAB-004: render.js's dateCol/verbCol/writeStdout/shQuote are no longer
//  used here — the emit sink (core/emit.js) owns all column formatting at the
//  flush edge, and the fork machinery (shQuote) is gone.

//  JSQUE-008: `be status` as a loop HANDLER.  Converted from a `main();`
//  one-shot to `module.exports = handle(row, ctx)` — the wt path rides the ROW
//  (row.uri), seed-pinned flags ride ctx.flags, output goes through `ctx.out`
//  (one flush at the loop edge), sibling libs via relative ./.  No process.argv
//  read, no self-run tail.  Read-only leaf: no fan-out, no store write/barrier.
//
//  JAB-004 (absorbs JSQUE-015): recurse mounted subs IN-PROCESS — NO forked
//  child `jab`, NO `/tmp` tmpfile, NO `readlink /proc`, NO `sh -c`.  The loop
//  already emits rows in pure JS (out.row → render.js); a sub is just MORE
//  rows on the same `out`, path-prefixed at EMIT time (a URI-aware join, not
//  the old column-12 string surgery).  Recursion is a synchronous DEPTH-FIRST
//  walk (emitRepo emits a hunk then immediately recurses each mounted sub),
//  matching native bare `be --plain`'s BEDefault relay ORDER (parent hunk
//  fully, then each sub's whole subtree in tree order, blank-line separated).
//  Depth-first is why we recurse in-process here rather than fanning breadth-
//  first `status <subWt>` rows onto the FIFO queue (which would interleave a
//  grandchild AFTER a later sibling — the wrong order).
//  JAB-004/DIS-060: PLAIN verb (`.jab="args"`) — reads ambient off global `be`.
//  A PATH arg SCOPES status to that subtree: parse the URI (never string-slice),
//  take the PATH slot, IGNORE a `?ref`/`#frag` (status describes the LIVE wt, cf.
//  ls) — be.treeAt on the abs path re-discovers a mounted sub's shard, so `status
//  test` shows the sub's own status.  No path arg → the top wt (columnar view).
function status() {
  const _be = (typeof be !== "undefined") ? be : null;
  const topWt = (_be && _be.repo && _be.repo.wt) || null;
  let scope = null;
  for (let i = 0; i < arguments.length; i++) {
    const a = String(arguments[i] || "");
    if (!a || a[0] === "-") continue;                     // skip flags
    let p; try { p = uri._parse(a); } catch (e) { p = {}; }
    if (p.path && p.path !== ".") { scope = p.path.replace(/^\.\//, ""); break; }
  }
  //  STATUS-006: no explicit path arg → scope to the run's CONTEXT DIR (be.ctxDir
  //  via discover.ctxSub, ROOTED so argRel skips the ctx re-resolve); a subdir cwd/
  //  nav scopes to that subtree, the wt root (ctxSub "") stays whole-wt.
  if (!scope && _be && _be.repo) {
    const c = discover.ctxSub(_be.repo);
    if (c) scope = "/" + c;
  }
  if (scope && topWt) {
    //  WHY-001/BE-032: a leading `/` is the wt ROOT (`/` alone → whole wt); a
    //  relative path resolves against the CONTEXT dir (cwd/nav sub-dir).
    const rel = discover.argRel(_be && _be.repo, scope);
    if (rel && rel !== "./") return statusOne({ uri: wtpath(topWt, rel) }, null);
  }
  return statusOne(null, null);
}

//  Emit the top wt's status (+ its mounted subs, depth-first). Ambient off `be`
//  (plain path), falling back to `ctx` (legacy direct-handler test). `row.uri`
//  (legacy) may pin a sub wt root; the plain path always starts at be.repo.
function statusOne(row, ctx) {
  const _be   = (typeof be !== "undefined") ? be : null;
  const emitOut = (_be && _be.out)  || (ctx && ctx.out)  || null;   // columnar (plain)
  const sink    = (_be && _be.sink) || (ctx && ctx.sink) || null;   // U-target hunk

  //  Recursion (relaying each mounted sub's status as a path-prefixed
  //  `status:<subpath>` hunk) is DEFAULT-ON (JAB-024): a bare `jab status`
  //  recurses into mounted subs, byte-matching native bare `be --plain`'s
  //  BEDefault relay. `--nosub` SUPPRESSES the walk (only the parent hunk);
  //  `--sub` is accepted but a no-op (recursion is the default).
  const flags = (_be && _be.flags) || (ctx && ctx.flags) || [];
  const recurse = flags.indexOf("--nosub") < 0;
  //  BRO-030: the quad vocabulary (wiki/Status.mkd) is THE default now; a stray
  //  `--quad` is tolerated as a no-op so old scripts don't error.

  //  BRO-006: color/tlv read `U` click-targets from the HUNK tok32 stream
  //  (be.sink), which the columnar emit sink can't carry — so those feed a real
  //  content HUNK (per-row toks + hidden `U` nav) via sinkOut; PLAIN keeps the
  //  columnar out (the cli edge owns the pager gate, so mode!=="plain" suffices).
  const mode = ambient.format();   // JAB-004
  const useSink = mode !== "plain" && sink;
  const out = useSink ? sinkOut(sink) : emitOut;

  //  The seed (top) row carries the "." cwd placeholder (loop.cli) — its wt is
  //  the pinned repo. A legacy row.uri may pin a sub wt root (re-discover it).
  const pinned = (_be && _be.repo) || (ctx && ctx.repo) || null;
  const reqAbs = (row && row.uri && row.uri !== ".") ? row.uri : null;
  const repo = reqAbs ? be.treeAt(reqAbs)
        : (pinned || be.treeAt((row && row.uri) || undefined));

  //  The display-path prefix for this hunk: "" at the top, else this sub's
  //  path RELATIVE to the top wt. Taken EXPLICITLY from the wt roots (JAB-004) —
  //  never io.cwd(), the wrong origin for an in-process sub.
  const topWt = (pinned && pinned.wt) || repo.wt;
  const prefix = relUnder(topWt, repo.wt);

  //  STATUS-006: a plain (non-sub) dir arg CLIMBS to its anchor repo; the residue
  //  below the found repo root is a subtree FILTER (a mounted sub redirected to its
  //  OWN shard has residue "" → whole sub, unchanged).
  const filter = reqAbs ? relUnder(repo.wt, reqAbs) : "";

  //  DEPTH-FIRST walk: emit this repo's hunk, then recurse each mounted sub.
  emitRepo(repo, prefix, out, recurse, filter, { colored: mode !== "plain" });

  //  Flush sinkOut's last buffered hunk (the columnar out flushes at the edge).
  if (useSink) out.done();

  //  Read-only leaf: no fan-out, nothing to enqueue.
}
status.jab = "args";
module.exports = status;

//  BRO-006/BRO-030: a HUNK-collector building a content HUNK (text + tok32) per
//  repo, fed to ctx.sink — `open` starts a hunk, "" is dropped (the sink owns
//  separators), a non-empty `raw` is a summary line; `quadRow`/`quadCommit` pack
//  the quad row with per-column tok spans + the hidden 'U' nav / 'O' button.
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }

//  BRO-030: the tok slot for quad char `ch` in column `i` (0..3) — the CELL
//  tags '['..'`' (codes 26..31, past 'Z'): pastel track/base/patch/wt bgs,
//  '_' = staged wt (white on dark), '`' = conflicted wt (white on dark red).
//  '.' stays default 'S'.  A-Z is full; I/J/O/K are diff-side BGs — hands off.
function quadCharTag(i, ch, staged, con) {
  if (ch === ".") return "S";
  if (i === 0) return "[";
  if (i === 1) return "\\";
  if (i === 2) return "]";
  return con ? "`" : staged ? "_" : "^";
}
const QUAD_TTY = quadrender.TTY_GLYPH;   // BRO-030: x→✗ o→+ v→↑, '.' stays '.'

function sinkOut(sink) {
  let uri = null;                 // current hunk URI (`status:` / `status:<sub>`)
  let parts = [];                 // Uint8Array text chunks
  let spans = [];                 // [tagLetter, byteEnd]
  let off = 0;                    // running byte offset

  function feedText(bytes) { parts.push(bytes); off += bytes.length; }

  function flush() {
    if (uri === null) return;     // nothing opened yet
    const body = concatBytes(parts, off);
    const toks = new Uint32Array(spans.length);
    for (let i = 0; i < spans.length; i++) toks[i] = tok(tagCode(spans[i][0]), spans[i][1]);
    sink.feed(uri, body, toks, "", 0n);
    uri = null; parts = []; spans = []; off = 0;
  }

  return {
    //  URI-014: open a hunk with the EXPLICIT banner spell (`status //name`); the
    //  old `status:`-prefix sniff is retired — banners arrive via open, not raw.
    open: function (u) { flush(); uri = u; },
    //  "" separator → drop; else → a summary text line.
    raw: function (text) {
      if (text === "") return;
      const b = utf8.Encode(text + "\n");
      feedText(b);
      spans.push(["S", off]);     // the whole summary line, default tag
    },
    //  BRO-030: one quad row `<date7> <quad4> <path>` — date 'L', the four quad
    //  chars as TTY glyphs with per-column tok spans (quadCharTag), path 'S'.  A
    //  file row appends its hidden `U` nav + BE-041 action button: path, U nav,
    //  sep, the visible label (its palette slot), the hidden `O` click spell.
    //  Multibyte-safe: byte ends come from utf8.Encode per fed chunk.
    quadRow: function (row, nav, act) {
      feedText(utf8.Encode(render.dateCol(row.ts == null ? 0n : row.ts)));
      spans.push(["L", off]);                                    // date column
      feedText(utf8.Encode(" ")); spans.push(["S", off]);        // sep
      const q = Array.from(row.quad);
      for (let i = 0; i < 4; i++) {
        const ch = q[i] == null ? "." : q[i];
        feedText(utf8.Encode(QUAD_TTY[ch] || ch));
        spans.push([quadCharTag(i, ch, row.staged, row.con), off]);
      }
      const path = (row.src && row.src !== row.path)
            ? row.src + "#" + row.path : row.path;
      feedText(utf8.Encode(" " + path)); spans.push(["S", off]); // sep + path
      if (nav) { feedText(utf8.Encode(nav)); spans.push(["U", off]); }  // hidden nav
      if (act) {
        feedText(utf8.Encode(" "));         spans.push(["S", off]);       // sep
        feedText(utf8.Encode(act.label));   spans.push([act.tag, off]);   // visible label
        feedText(utf8.Encode(act.spell));   spans.push(["O", off]);       // hidden click spell
      }
      feedText(utf8.Encode("\n")); spans.push(["S", off]);
    },
    //  BRO-030: one quad commit row `<date7> <quad4> ?<hashlet>#<subject>` — the
    //  same per-column spans, no nav/button (a commit row isn't clickable).
    quadCommit: function (c) {
      feedText(utf8.Encode(render.dateCol(c.ts == null ? 0n : c.ts)));
      spans.push(["L", off]);
      feedText(utf8.Encode(" ")); spans.push(["S", off]);
      const q = Array.from(c.quad);
      for (let i = 0; i < 4; i++) {
        const ch = q[i] == null ? "." : q[i];
        //  BRO-030: a commit row's `o` is "present in this line" → ✔ glyphs.
        feedText(utf8.Encode(quadrender.COMMIT_GLYPH[ch] || ch));
        spans.push([quadCharTag(i, ch, false, false), off]);
      }
      feedText(utf8.Encode(" ?" + c.hashlet + (c.subject ? "#" + c.subject : "")));
      spans.push(["S", off]);
      feedText(utf8.Encode("\n")); spans.push(["S", off]);
    },
    done: flush,
  };
}

//  Concatenate Uint8Array chunks into one buffer of length `total`.
function concatBytes(chunks, total) {
  const all = new Uint8Array(total);
  let o = 0;
  for (const c of chunks) { all.set(c, o); o += c.length; }
  return all;
}

//  BRO-030: the summary segments — per-COLUMN change tallies (model.counts,
//  zero segments omitted), the quad vocabulary replacing the per-bucket zoo.
const QUAD_SUMMARY = ["track", "base", "patch", "wt", "staged", "con"];

//  Emit ONE repo's status hunk into `out` (header + commit rows + file rows +
//  summary), then — when recursing — walk its mounted subs DEPTH-FIRST, each as
//  a blank-line-separated `status:<subpath>` hunk under `prefix` ("" for the
//  top).  BRO-030: THE unified quad view (wiki/Status.mkd) — the shared/quad.js
//  model, rendered as ASCII canon via quadrender for the columnar (plain) `out`
//  or as a real HUNK with per-column tok spans + nav/buttons when `out` is a
//  sinkOut (`out.quadRow`).  `quad.colored` only decorates the columnar render.
function emitRepo(repo, prefix, out, recurse, filter, quad) {
  const log = wtlog.open(repo);
  const k   = store.open(repo.storePath, repo.project);
  //  STATUS-006: the subtree filter narrows the quad gather (DIS-054 underNarrow)
  //  to the subtree — rows, counts AND gitlinks below `<filter>/` only.
  const narrow = filter
        ? function (p) { return p === filter || p.indexOf(filter + "/") === 0; }
        : null;
  const model = quadlib.quadOf(repo, log, k,
                               narrow ? { underNarrow: narrow } : {});

  //  BRO-030: an advanced MOUNTED gitlink (subs.classifyMount `adv`) is a
  //  file row whose wt column reads '↑' — a gitlink advance like any file.
  for (const gl of model.gitlinks || []) {
    if (!isMount(repo.wt, gl.path)) continue;
    const cls = subs.classifyMount(repo, gl.path, gl.pin);
    if (cls.bucket !== "adv") continue;
    let row = null;
    for (const r of model.rows) if (r.path === gl.path) { row = r; break; }
    if (!row) {
      row = { path: gl.path, quad: "....", staged: false, con: false, ts: cls.ts };
      model.rows.push(row);
    }
    //  BRO-030: '↑' is multibyte — rewrite the quad via Array, never slice.
    const q = Array.from(row.quad);
    if (q[3] === ".") model.counts.wt++;
    q[3] = quadlib.CH.advanced;
    row.quad = q.join("");
  }
  model.rows.sort(function (a, b) {
    return a.path < b.path ? -1 : a.path > b.path ? 1 : 0;
  });

  out.open(navlib.navLink("status", prefix || ""));

  //  STATUS-006: commit rows are repo-level — a subtree filter keeps the hunk
  //  path-only.  Through sinkOut they carry per-column tok spans; the columnar
  //  out gets the pre-formatted ASCII/ANSI line.
  const commits = filter ? [] : model.commits;
  for (const c of commits) {
    if (out.quadCommit) out.quadCommit(c);
    else out.raw(quadrender.commitRow(c, quad.colored));
  }
  for (const r of model.rows) {
    //  BRO-030: a sub's rows join under `prefix` at emit time (JAB-004).
    const navPath = joinPrefix(prefix, r.path);
    const row = { path: navPath,
                  src: r.src ? joinPrefix(prefix, r.src) : undefined,
                  quad: r.quad, staged: r.staged, con: r.con, ts: r.ts };
    //  BRO-030 (BE-006/041): the wt char routes the hidden `U` nav + button —
    //  a baseline exists to diff for v/x/! (diff:), else cat: (created 'o');
    //  an unstaged wt v/o → [put], x → [del] (con carries neither).  The button
    //  arg stays RAW wt-relative (BE-039); the nav rides navLink's authority.
    if (out.quadRow) {
      const wt = r.con ? "!" : r.quad[3];
      const nav = navlib.navLink(
            (wt === "v" || wt === "x" || wt === "!") ? "diff" : "cat", navPath);
      let act = null;
      if (!r.staged && !r.con) {
        if (wt === "v" || wt === "o") act = { label: "[put]", tag: "Y", spell: "put " + navPath };
        else if (wt === "x")          act = { label: "[del]", tag: "X", spell: "delete " + navPath };
      }
      out.quadRow(row, nav, act);
    } else {
      out.raw(quadrender.fileRow(row, quad.colored));
    }
  }

  //  BRO-030: the emitRepo summary frame (`<rel><label>\t…  (behind/ahead)`),
  //  bucket segments swapped for quad-column counts.
  const cur = log.curTip();
  const att = log.attachedBranch();
  const branch = (cur && cur.sha && att.detached)
        ? cur.sha : branchlib.format(att.br);
  const rel = prefix ? "" : cwdRel(repo.wt);
  const label = att.uriTrack
        ? att.track + (att.base ? "#" + att.base.slice(0, 8) : "")
        : "?" + branch;
  let summary = (rel ? rel : "") + label + "\t";
  const segs = [];
  for (const b of QUAD_SUMMARY) {
    const n = model.counts[b] || 0;
    if (n > 0) segs.push(n + " " + b);
  }
  summary += segs.join(", ");
  //  BRO-030: behind/ahead off the commit quads — a track-column 'o' (canon
  //  created) is a behind (track-side) commit, anything else is a local ahead one.
  let aN = 0, bN = 0;
  for (const c of commits) { if (c.quad[0] === quadlib.CH.created) bN++; else aN++; }
  if (aN > 0 || bN > 0) {
    const parts = [];
    if (bN > 0) parts.push("behind " + bN);
    if (aN > 0) parts.push("ahead " + aN);
    summary += "  (" + parts.join(", ") + ")";
  }
  out.raw(summary);
  if (prefix) out.raw("");

  //  JAB-004 recursion: relay each MOUNTED sub's status as a SEPARATE
  //  `status:<subpath>` hunk, DEPTH-FIRST in `.gitmodules` declaration order.
  if (recurse) {
    for (const subPath of gitmodulesOrder(repo.wt)) {
      if (narrow && !narrow(subPath)) continue;
      if (!isMount(repo.wt, subPath)) continue;
      const subWt = subs.mountWtDir(repo, subPath);
      let subRepo;
      try { subRepo = be.treeAt(subWt); } catch (e) { continue; }
      emitRepo(subRepo, joinPrefix(prefix, subPath), out, recurse, undefined, quad);
    }
  }
}

//  Parse `<wt>/.gitmodules` and return the declared submodule `path` values in
//  FILE (declaration) order — the order native recurses subs (KEEPSubsAt drives
//  SUBSu8sParse over the `.gitmodules` blob top-to-bottom).  A minimal git-
//  PUT-004: delegates to the shared reader (was a copy-pasted git-config parser).
function gitmodulesOrder(wtRoot) {
  return require("../../shared/gitmodules.js").paths(wtRoot);
}

//  URI-aware join of a path column under a sub prefix (JAB-004) — replaces
//  relaySub's column-12 string slicing.  An empty prefix is a no-op (top
//  level); else `<prefix>/<path>`.  `uri._parse` confirms the column is a real
//  path (it has a path component and no scheme/authority); a non-path token is
//  returned untouched (defensive — the bucket rows are always plain paths).
function joinPrefix(prefix, col) {
  if (!prefix) return col;
  const u = uri._parse(col);
  if (u.scheme || u.authority) return col;   // not a bare path — leave as-is
  return prefix + "/" + col;
}

//  YES iff `<wt>/<subpath>/.be` is a regular file (a live mount).
//  Mirrors SNIFFSubIsMount: only a mounted sub is classified/recursed.
function isMount(wtRoot, subpath) {
  const p = wtpath(wtRoot, subpath + "/.be");
  //  SUBS-049: the `.be` FILE form OR a PRIMARY nested wt (`.be` DIR + wtlog),
  //  mirroring be.treeAt's anchor — parity with the FILE-anchored sub in status.
  let k; try { k = io.stat(p).kind; } catch (e) { return false; }
  if (k === "reg") return true;
  if (k !== "dir") return false;
  try { return io.stat(p + "/wtlog").kind === "reg"; } catch (e) { return false; }
}

//  `childWt` relative to `topWt` — the sub's display-path prefix (JAB-004),
//  taken EXPLICITLY from the two wt roots (no io.cwd()).  "" when they are the
//  same dir (the top wt is its own origin); else the path tail under topWt
//  (`vendor/sub`, or `vendor/sub/vendor/leaf` for a grandchild).
function relUnder(topWt, childWt) {
  if (childWt === topWt) return "";
  const pfx = topWt.endsWith("/") ? topWt : topWt + "/";
  if (childWt.indexOf(pfx) === 0) return childWt.slice(pfx.length);
  return "";
}

//  cwd-relative path under the wt root (empty when cwd == wt root).  Used only
//  for the TOP hunk's summary `<rel>?<branch>` token (status run from a subdir
//  of the wt prints the cwd-relative prefix); a sub hunk passes prefix="" here
//  since its path already rides the `status:<subpath>` header.
function cwdRel(wtRoot) {
  let cwd;
  try { cwd = io.cwd(); } catch (e) { return ""; }
  if (cwd === wtRoot) return "";
  const pfx = wtRoot.endsWith("/") ? wtRoot : wtRoot + "/";
  if (cwd.indexOf(pfx) === 0) return cwd.slice(pfx.length);
  return "";
}
