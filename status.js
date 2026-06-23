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
//  Usage:  be status        (be forks jabc on this script)
//          jabc bin/status.js

"use strict";

//  Derive this script's dir from argv[1] so the sibling libs resolve by
//  absolute path from any cwd (be forks jabc with the caller's cwd).
const self = process.argv[1];
const here = self.slice(0, self.lastIndexOf("/"));
const be       = require(here + "/lib/be.js");
const wtlog    = require(here + "/lib/wtlog.js");
const store    = require(here + "/lib/store.js");
const classify = require(here + "/lib/classify.js");
const dag      = require(here + "/lib/dag.js");
const subs     = require(here + "/lib/subs.js");
const render   = require(here + "/lib/render.js");
const dateCol = render.dateCol, verbCol = render.verbCol,
      writeStdout = render.writeStdout, shQuote = render.shQuote;

//  Render order (status_step / status_emit_summary): ok first (count
//  only), then staged, then unstaged, then untracked.  `adv` follows
//  `mod` (SUBS-030: an advanced-sub gitlink-bump row, dumped/summarised
//  immediately after the content-`mod` block — see SNIFF.exe.c
//  status_dump_verb + STATUS_BUCKET order).
const ROW_ORDER = ["put", "new", "mov", "mod", "adv", "del", "mis", "unk"];
const SUMMARY_ORDER = ["ok", "put", "new", "mov", "pat", "mod", "adv", "del", "mis", "unk"];

function main() {
  //  Recursion (relaying each mounted sub's status as a path-prefixed
  //  `status:<subpath>` hunk) is OPT-IN via `--sub`, OFF by default —
  //  matching the dispatch: native `be status` routes to THIS extension
  //  and does NOT recurse (only bare `be` → BEDefault wraps the producer
  //  with be_relay_subs).  So default output == `be status --plain`
  //  byte-for-byte; `--sub` reproduces bare `be --plain`'s recursing form.
  //  `--nosub` is accepted (and forces recursion off) for symmetry with
  //  native's flag + the deep-recursion child call below.
  const argv = process.argv.slice(2);
  const recurse = argv.indexOf("--sub") >= 0 && argv.indexOf("--nosub") < 0;

  const repo = be.find();
  const log  = wtlog.open(repo);
  const k    = store.open(repo.storePath, repo.project);

  const res = classify.classify(repo, log, k);

  //  Cur tip (for the ahead/behind divergence: SNIFFAtCurTip, no patch).
  const cur = log.curTip();
  //  Summary branch label = the BASELINE tip's RAW query (SNIFFAtBaseline
  //  → bu.query): a named branch (`master`), a detached full sha, or empty
  //  (trunk → `?`).  NOT the parsed branch, which drops a detached sha.
  const baseTip = log.baselineTip();
  const branch = (baseTip && baseTip.query) || "";

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

  //  Build the plain text body.
  let body = "status:\n";

  //  Commit divergence block FIRST (ahead `post` rows, then behind `miss`
  //  rows), each `<date7> <verb3> ?<hashlet>#<subject>`.
  for (const c of diverge.ahead)
    body += dateCol(c.ts) + " " + verbCol("post") + " ?" + c.hashlet +
            (c.subject ? "#" + c.subject : "") + "\n";
  for (const c of diverge.behind)
    body += dateCol(c.ts) + " " + verbCol("miss") + " ?" + c.hashlet +
            (c.subject ? "#" + c.subject : "") + "\n";

  //  Rows in render order; within a bucket, sort lex-by-path so a gitlink
  //  `mod` row (appended above) interleaves with file `mod` rows at its
  //  lex position — the SNIFFClassify heap-merge order.  classify already
  //  emits each bucket lex-sorted, so this only re-orders the bucket that
  //  gained a gitlink row, and is a no-op for the rest.
  for (const bucket of ROW_ORDER) {
    const inBucket = [];
    for (const r of res.rows) if (r.bucket === bucket) inBucket.push(r);
    inBucket.sort(function (a, b) {
      return a.path < b.path ? -1 : a.path > b.path ? 1 : 0;
    });
    for (const r of inBucket) {
      let path = r.path;
      if (r.bucket === "mov" && r.dst) path = path + "#" + r.dst;
      body += dateCol(r.ts) + " " + verbCol(bucket) + " " + path + "\n";
    }
  }

  //  Summary line: `<rel>?<branch>\t<counts>`.
  const rel = cwdRel(repo.wt);
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
  body += summary + "\n";

  //  --- JS-033 recursion (--sub): relay each MOUNTED sub's status as a
  //  SEPARATE `status:<subpath>` hunk AFTER the parent summary,
  //  path-prefixed, with the sub's OWN summary line — matching bare
  //  `be --plain`'s BEDefault relay (be_relay_subs → HUNKu8sRelay).  The
  //  recursing child is run WITH --sub too (deep trees relay fully);
  //  relaySub rebases every returned hunk under this level's subpath.
  if (recurse) {
    for (const s of subList) {
      if (!s.mounted) continue;
      body += relaySub(repo, s.path);
    }
  }

  //  Native prints status to STDOUT in --plain mode; io.log is STDERR.
  //  Emit on stdout (fd 1) so a parity diff captures the same stream.
  writeStdout(body);
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

//  Recurse this script into a mounted sub and return its FULL output
//  rebased under `<subpath>` — a separate `status:<subpath>` hunk (header
//  + path-prefixed rows + the sub's OWN summary), matching bare
//  `be --plain`'s BEDefault relay (be_relay_subs → HUNKu8sRelay).  The
//  child runs recursively (no --nosub), so a deep tree's grandchild hunks
//  come back as `status:<grandchild>`; we rebase EACH hunk's header path
//  and EACH row's path under `<subpath>`.  A failed/empty/clean child
//  yields nothing (native `*NONE` clean-sub no-op).
function relaySub(repo, subpath) {
  const childWt = subs.mountWtDir(repo, subpath);
  let out;
  try { out = runStatusIn(childWt); } catch (e) { return ""; }
  if (!out) return "";
  const lines = out.split("\n");

  //  Split the child output into hunks (each starts at a `status:` line)
  //  and rebase each under `<subpath>`.  Native bare `be --plain` relays
  //  EVERY mounted sub (BEDefault → be_relay_subs), including a CLEAN sub
  //  — which comes back as a header + a pure-`ok` summary with no rows
  //  (e.g. `status:abc\n?<sha>\t237 ok`).  So every hunk is kept; the row
  //  set may legitimately be empty.
  const hunks = [];
  let cur = null;
  for (let i = 0; i < lines.length; i++) {
    const ln = lines[i];
    if (ln === "") continue;
    if (ln === "status:" || ln.indexOf("status:") === 0) {
      cur = { header: ln, rows: [], summary: "" };
      hunks.push(cur);
      continue;
    }
    if (!cur) continue;
    if (ln.indexOf("\t") >= 0) { cur.summary = ln; continue; }
    cur.rows.push(ln);
  }

  let res = "";
  let emitted = false;
  for (const h of hunks) {
    emitted = true;
    //  Rebase the hunk header.
    if (h.header === "status:") res += "status:" + subpath + "\n";
    else res += "status:" + subpath + "/" +
                h.header.slice("status:".length) + "\n";
    //  Rebase each row's path column (starts at column 12).
    for (const r of h.rows) {
      if (r.length <= 12) { res += r + "\n"; continue; }
      res += r.slice(0, 12) + subpath + "/" + r.slice(12) + "\n";
    }
    if (h.summary) res += h.summary + "\n";
  }
  //  Native's relay terminates the relayed block with a trailing blank
  //  line (the HUNK inter-hunk separator); mirror it so bare `be --plain`'s
  //  byte tail matches.
  if (emitted) res += "\n";
  return res;
}

//  Run THIS status.js inside `dir` via the same jabc and capture stdout.
//  Mirrors BERelaySub's fork+chdir+exec, but invokes the JS extension so
//  the whole recursion stays pure-JS (no native be).  Uses io.spawnFds
//  with a temp-file sink (no pipe drain), then reads it back.  The child
//  recurses on its own (no --nosub) so deep trees relay fully; relaySub
//  rebases every returned hunk under this level's subpath.
function runStatusIn(dir) {
  const self = process.argv[1];   // this script (absolute when be-dispatched)
  const tmp = "/tmp/.bestatus.sub." + Date.now() + "." +
              Math.floor(Math.random() * 1e6);
  let fd;
  try { fd = io.open(tmp, "c"); } catch (e) { return ""; }
  //  io.spawnFds has no cwd knob, so set the child's cwd via a POSIX
  //  `sh -c 'cd <dir> && <jabc> <self> --plain'`.  process.argv[0] is the
  //  bare `jabc` name (be dispatches `argv[0]="jabc"`), so resolve the
  //  RUNNING jabc binary's absolute path inside the shell via
  //  `readlink /proc/$PPID/exe` — $PPID is the jabc that spawned this sh.
  //  --plain keeps the child output text-stable for the line-based rebase.
  let pid;
  try {
    const sh = shBin();
    if (!sh) { io.close(fd); try { io.unlink(tmp); } catch (e) {} return ""; }
    const cmd = "cd " + shQuote(dir) +
                " && JABC=$(readlink /proc/$PPID/exe 2>/dev/null)" +
                ' && [ -n "$JABC" ] || JABC=jabc; "$JABC" ' +
                shQuote(self) + " --plain --sub";
    pid = io.spawnFds(sh, [sh, "-c", cmd], -1, fd);
    io.close(fd);
    io.reap(pid);
  } catch (e) { try { io.close(fd); } catch (e2) {} try { io.unlink(tmp); } catch (e3) {} return ""; }
  let text = "";
  try { text = utf8.Decode(io.mmap(tmp, "r").data()); } catch (e) { text = ""; }
  try { io.unlink(tmp); } catch (e) {}
  return text;
}

function shBin() {
  for (const c of ["/bin/sh", "/usr/bin/sh"]) {
    try { if (io.stat(c).kind === "reg") return c; } catch (e) {}
  }
  return undefined;
}

//  YES iff `<wt>/<subpath>/.be` is a regular file (a live mount).
//  Mirrors SNIFFSubIsMount: only a mounted sub is classified/recursed.
function isMount(wtRoot, subpath) {
  const p = (wtRoot.endsWith("/") ? wtRoot : wtRoot + "/") + subpath + "/.be";
  try { return io.stat(p).kind === "reg"; } catch (e) { return false; }
}

//  cwd-relative path under the wt root (empty when cwd == wt root).
function cwdRel(wtRoot) {
  let cwd;
  try { cwd = io.cwd(); } catch (e) { return ""; }
  if (cwd === wtRoot) return "";
  const pfx = wtRoot.endsWith("/") ? wtRoot : wtRoot + "/";
  if (cwd.indexOf(pfx) === 0) return cwd.slice(pfx.length);
  return "";
}

main();
