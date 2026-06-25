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
const be       = require("../../core/discover.js");
const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const classify = require("../../shared/classify.js");
const dag      = require("../../shared/dag.js");
const subs     = require("../../shared/subs.js");
//  JAB-004: render.js's dateCol/verbCol/writeStdout/shQuote are no longer
//  used here — the emit sink (core/emit.js) owns all column formatting at the
//  flush edge, and the fork machinery (shQuote) is gone.

//  Render order (status_step / status_emit_summary): ok first (count
//  only), then staged, then unstaged, then untracked.  `adv` follows
//  `mod` (SUBS-030: an advanced-sub gitlink-bump row, dumped/summarised
//  immediately after the content-`mod` block — see SNIFF.exe.c
//  status_dump_verb + STATUS_BUCKET order).
const ROW_ORDER = ["put", "new", "mov", "mod", "adv", "del", "mis", "unk"];
const SUMMARY_ORDER = ["ok", "put", "new", "mov", "pat", "mod", "adv", "del", "mis", "unk"];

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
module.exports = function handle(row, ctx) {
  //  Recursion (relaying each mounted sub's status as a path-prefixed
  //  `status:<subpath>` hunk) is now DEFAULT-ON (JAB-024): a bare `jab status`
  //  recurses into mounted subs, byte-matching native bare `be --plain`'s
  //  BEDefault relay (be_relay_subs) — the recursing producer.  `--nosub`
  //  SUPPRESSES the walk → only the parent hunk, byte-matching native
  //  `be status --plain` (the flat verb).  `--sub` is still accepted but is a
  //  no-op now (recursion is the default); it stays for symmetry / explicitness.
  //  Flags are seed-pinned (resolution-at-entry, JSQUE-004) — read from ctx,
  //  not the row (the queue round-trip carries only ts/verb/uri).
  const flags = (ctx && ctx.flags) || [];
  const recurse = flags.indexOf("--nosub") < 0;
  const out = ctx && ctx.out;

  //  The seed (top) row carries the "." cwd placeholder (loop.cli) — its wt is
  //  the pinned ctx.repo.  Any other uri is a sub wt root (in-process recursion
  //  passes the absolute sub dir), so re-discover that repo explicitly.
  const repo = (row && row.uri && row.uri !== ".")
        ? be.find(row.uri)
        : ((ctx && ctx.repo) || be.find((row && row.uri) || undefined));

  //  The display-path prefix for this hunk: "" at the top, else this sub's
  //  path RELATIVE to the top wt (so a grandchild reads `sub/grandchild`).
  //  Taken EXPLICITLY from the wt roots (JAB-004) — never io.cwd(), which is
  //  the wrong origin for an in-process sub.
  const topWt = (ctx && ctx.repo && ctx.repo.wt) || repo.wt;
  const prefix = relUnder(topWt, repo.wt);

  //  DEPTH-FIRST walk: emit this repo's hunk, then recurse each mounted sub.
  emitRepo(repo, prefix, out, recurse);

  //  Read-only leaf: no fan-out, nothing to enqueue.
};

//  Emit ONE repo's status hunk into `out` (header + divergence + bucket rows +
//  summary), then — when recursing — walk its mounted subs DEPTH-FIRST, each
//  as a blank-line-separated `status:<subpath>` hunk under `prefix`.  `prefix`
//  is this repo's path relative to the top wt ("" for the top); a sub's own
//  rows + header are joined under it via a URI-aware path join, while the sub's
//  `?<branch>` summary token and the `?<sha>#<subject>` divergence rows are
//  NOT prefixed (they are not real path columns).
function emitRepo(repo, prefix, out, recurse) {
  const log = wtlog.open(repo);
  const k   = store.open(repo.storePath, repo.project);

  const res = classify.classify(repo, log, k);

  //  Cur tip (for the ahead/behind divergence: SNIFFAtCurTip, no patch).
  const cur = log.curTip();
  //  Summary branch label = the BASELINE tip's VERBATIM query (SNIFFAtBaseline
  //  → bu.query): a named branch (`master`), a detached full sha, a mounted
  //  sub's `/<project>[/<branch>]` anchor query (kept RAW, NOT project-
  //  stripped — JAB-004), or empty (trunk → `?`).
  const baseTip = log.baselineTip();
  const branch = (baseTip && baseTip.rawQuery) || "";

  //  --- JS-033: classify base-only gitlinks (SUBSDirty 3-axis) ---------
  //  Each deferred gitlink (classify.gitlinks) is pin-vs-tip compared on
  //  the sub's own shard; ADVANCED → an `adv` row, else → ok (count only).
  //  Folded into res.rows / res.counts so the render + summary below treat
  //  them like any other bucket.  classify.classify doesn't pre-seed an
  //  `adv` counter (it has no file-level adv), so seed it here.
  if (res.counts.adv === undefined) res.counts.adv = 0;
  const subList = [];           // [{ path, mounted, bucket, … }] for recursion
  for (const gl of res.gitlinks || []) {
    const mounted = isMount(repo.wt, gl.path);
    const cls = mounted ? subs.classifyMount(repo, gl.path, gl.pin)
                        : { bucket: "ok", stale: "", r4: "", ts: 0n };
    subList.push({ path: gl.path, mounted: mounted, bucket: cls.bucket,
                   stale: cls.stale, r4: cls.r4, ts: cls.ts });
    if (cls.bucket === "adv") {
      //  SUBS-030: an advanced sub (tip descends the gitlink pin, only a
      //  bump pending) reads the distinct `adv` verb, NOT `mod`.  The row
      //  carries the sub-tip commit ts (native status_push passes it).
      res.counts.adv++;
      res.rows.push({ bucket: "adv", path: gl.path, ts: cls.ts });
    } else {
      res.counts.ok++;
    }
  }

  //  --- JS-032: cur-vs-branch-tip commit divergence (ahead/behind) -----
  //  Resolve cur tip + the LOCAL ref tip of cur's branch, walk ancestry.
  //  ahead → `post` rows, behind → `miss` rows, both prepended above the
  //  file rows.  Counts feed the trailing `(behind N, ahead M)` note.
  const diverge = computeDivergence(k, log, cur);

  //  JSQUE-008: push every line through the emit sink (out) in final render
  //  order — the loop does ONE flush at the edge.  The columnar rows
  //  (divergence + buckets) go via out.row(text, verb, ts); the `status:`
  //  banner + the `?<branch>\t<counts>` summary are pre-formatted framing,
  //  pushed verbatim via out.raw.  JAB-004: the header carries this hunk's
  //  subpath (`status:` at the top, `status:<prefix>` for a sub).
  out.raw(prefix ? "status:" + prefix : "status:");

  //  Commit divergence block FIRST (ahead `post` rows, then behind `miss`
  //  rows), each `<date7> <verb3> ?<hashlet>#<subject>`.  These are NOT real
  //  path columns (a `?<sha>#<subject>` ref token) — NEVER path-prefixed.
  for (const c of diverge.ahead)
    out.row("?" + c.hashlet + (c.subject ? "#" + c.subject : ""), "post", c.ts);
  for (const c of diverge.behind)
    out.row("?" + c.hashlet + (c.subject ? "#" + c.subject : ""), "miss", c.ts);

  //  Rows in render order; within a bucket, sort lex-by-path so a gitlink
  //  `mod` row (appended above) interleaves with file `mod` rows at its
  //  lex position — the SNIFFClassify heap-merge order.  classify already
  //  emits each bucket lex-sorted, so this only re-orders the bucket that
  //  gained a gitlink row, and is a no-op for the rest.  JAB-004: each row's
  //  PATH column is joined under `prefix` at emit time (a URI-aware join,
  //  replacing relaySub's column-12 slicing).
  for (const bucket of ROW_ORDER) {
    const inBucket = [];
    for (const r of res.rows) if (r.bucket === bucket) inBucket.push(r);
    inBucket.sort(function (a, b) {
      return a.path < b.path ? -1 : a.path > b.path ? 1 : 0;
    });
    for (const r of inBucket) {
      let path = r.path;
      if (r.bucket === "mov" && r.dst) path = path + "#" + r.dst;
      out.row(joinPrefix(prefix, path), bucket, r.ts);
    }
  }

  //  Summary line: `<rel>?<branch>\t<counts>`.  At the top, `rel` is the
  //  cwd-relative prefix (status run from a subdir of the wt); for a sub the
  //  hunk header already carries the path, so the summary `rel` is empty and
  //  the `?<branch>` token is the sub's RAW anchor query, NOT path-prefixed.
  const rel = prefix ? "" : cwdRel(repo.wt);
  let summary = (rel ? rel : "") + "?" + branch + "\t";
  const segs = [];
  for (const b of SUMMARY_ORDER) {
    const n = res.counts[b] || 0;
    if (n > 0) segs.push(n + " " + b);
  }
  summary += segs.join(", ");
  //  Trailing `(behind N, ahead M)` note (GET-021): behind first, then
  //  ahead; omitted entirely when the wt is up-to-date.
  const aN = diverge.ahead.length, bN = diverge.behind.length;
  if (aN > 0 || bN > 0) {
    const parts = [];
    if (bN > 0) parts.push("behind " + bN);
    if (aN > 0) parts.push("ahead " + aN);
    summary += "  (" + parts.join(", ") + ")";
  }
  out.raw(summary);

  //  Native terminates EACH relayed sub block with a blank line (the HUNK
  //  inter-hunk separator) right after its summary, BEFORE that sub's own
  //  children — so a deep tree reads header/summary/blank, then the
  //  grandchild's header/summary/blank, depth-first.  The TOP hunk (prefix
  //  "") carries NO trailing blank (the first sub follows it directly).
  if (prefix) out.raw("");

  //  --- JAB-004 recursion (--sub): relay each MOUNTED sub's status as a
  //  SEPARATE `status:<subpath>` hunk AFTER this hunk's separator, IN-PROCESS
  //  (no fork) — matching bare `be --plain`'s BEDefault relay (be_relay_subs).
  //  DEPTH-FIRST in `.gitmodules` DECLARATION order (native KEEPSubsAt parses
  //  the `.gitmodules` blob top-to-bottom; the tree is authoritative for which
  //  declared path is a live gitlink — see keeper/SUBS.c::keep_subs_step), NOT
  //  the lex `res.gitlinks` order.  The recursing child carries the JOINED
  //  prefix so a grandchild reads `status:<sub>/<grandchild>`.  A sub whose
  //  mount shard can't be opened is skipped (native's clean-sub *NONE no-op).
  if (recurse) {
    //  Index the tree gitlinks (subList) by path for the mount/gitlink gate;
    //  drive the ORDER off `.gitmodules`.
    const byPath = {};
    for (const s of subList) byPath[s.path] = s;
    for (const subPath of gitmodulesOrder(repo.wt)) {
      const s = byPath[subPath];
      if (!s || !s.mounted) continue;        // declared but not a live mount
      const subWt = subs.mountWtDir(repo, s.path);
      let subRepo;
      try { subRepo = be.find(subWt); } catch (e) { continue; }
      emitRepo(subRepo, joinPrefix(prefix, s.path), out, recurse);
    }
  }
}

//  Parse `<wt>/.gitmodules` and return the declared submodule `path` values in
//  FILE (declaration) order — the order native recurses subs (KEEPSubsAt drives
//  SUBSu8sParse over the `.gitmodules` blob top-to-bottom).  A minimal git-
//  config reader: `[submodule "<name>"]` opens a section, `path = <p>` records
//  it; only sections with a path are kept, deduped first-wins.  Absent/unreadable
//  `.gitmodules` → [] (no declared subs).  Native reads the committed blob from
//  the baseline tree; for a checked-out mount the wt copy is the same bytes — and
//  the per-path gitlink/mount gate above filters any stale declaration.
function gitmodulesOrder(wtRoot) {
  const p = (wtRoot.endsWith("/") ? wtRoot : wtRoot + "/") + ".gitmodules";
  let text;
  try { text = utf8.Decode(io.mmap(p, "r").data()); } catch (e) { return []; }
  const order = [], seen = {};
  let inSubmod = false;
  for (let line of text.split("\n")) {
    line = line.replace(/[#;].*$/, "").trim();      // strip comments + ws
    if (!line) continue;
    if (line[0] === "[") {                          // section header
      inSubmod = /^\[\s*submodule\b/i.test(line);
      continue;
    }
    if (!inSubmod) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    const key = line.slice(0, eq).trim().toLowerCase();
    const val = line.slice(eq + 1).trim();
    if (key === "path" && val && !seen[val]) { seen[val] = true; order.push(val); }
  }
  return order;
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

//  Resolve cur tip + the local ref tip of cur's branch, compute the
//  ahead/behind commit divergence via dag.js.  Mirrors
//  SNIFF.exe.c::status_emit_commit_diff: silent no-op (empty lists) when
//  cur has no 40-hex tip, the branch ref is absent, or cur == tip.
function computeDivergence(k, log, cur) {
  const empty = { ahead: [], behind: [] };
  if (!cur || !cur.sha || !subs.isFullSha(cur.sha)) return empty;
  //  Branch = cur tip's RAW query (native uses `cu.query`): empty = trunk;
  //  a detached cur carries the full sha as its query, which resolveRef
  //  won't match → no divergence (a detached cur has no branch ref to
  //  diverge from — exactly native's behaviour).
  const tip = k.resolveRef(cur.query || "");
  if (!tip || !subs.isFullSha(tip)) return empty;
  if (tip === cur.sha) return empty;
  return dag.aheadBehind(k, cur.sha, tip);
}

//  YES iff `<wt>/<subpath>/.be` is a regular file (a live mount).
//  Mirrors SNIFFSubIsMount: only a mounted sub is classified/recursed.
function isMount(wtRoot, subpath) {
  const p = (wtRoot.endsWith("/") ? wtRoot : wtRoot + "/") + subpath + "/.be";
  try { return io.stat(p).kind === "reg"; } catch (e) { return false; }
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
