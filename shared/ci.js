//  shared/ci.js — CI-004: the LOCAL ci leg.  Detect the project's default
//  build+test ("the default stuff"), run it in the background, report a verdict.
//
//  Three parts, in the order the pager's `v` uses them:
//   1. detect(wt) — the first-hit LADDER probed in the WORKTREE ROOT.  ONE
//      detector, so [CI-001]'s qemu fleet can run the same thing in its VMs.
//   2. run(wt)    — one `/bin/sh -c` child, stdout+stderr into a log FILE, exit
//      code echoed into a sibling `.rc` marker.  The marker exists because
//      `io.reap` only BLOCKS: a resident pager cannot wait, so the child leaves
//      its verdict on disk and the parent reads it on the next frame.
//   3. row(wt)    — { state, code, log, cmd }, the verdict.  STATUS-019: this is
//      a CONSUMER MEMO keyed by the rev the run SETTLED at (the rev tree stores
//      no values — todo.js's `_wtRev` is the same shape).  The rev moves on the
//      first edit under the wt, so the verdict drops itself; with no live
//      watcher every rev is a fresh token, so there is no memo at all (ruling 3)
//      and nothing is ever shown as fresh that might not be.
//      The `.rc` marker is the CHILD→PARENT handoff ONLY: it is consumed ONCE,
//      into the memo.  Re-deriving from it would resurrect a staled verdict.
//
//   4. ensure/tail/footer — what the `ci` VIEW (views/ci/ci.js) rides: fork or
//      ATTACH, the last 4 KB of the log, and the render-only verdict line.
//   5. status(wt) — TODO 11: the REMEMBERED verdict, a `{wt: ok|fail}` map on
//      disk beside the logs.  The memo above is the live-session layer; THIS is
//      what the board button's colour reads, cold and across pager restarts.
//
//  Paths: the ladder probes ride discover.wtpath (resolve-backed, tree-confined)
//  and the context worktree comes from discover.wtdir + treeAt.  No hand-rolled
//  URI or path composition anywhere.
"use strict";

const discover = require("../core/discover.js");
const pathlib = require("./util/path.js");
const sha = require("./util/sha.js");
const CACHE = require("./cache.js");

//  CI-004: `jsrc` is a symlink to `.`, so this file can load as TWO module
//  instances — the in-flight registry lives in ONE globalThis slot (cache.js's).
const RUNS = globalThis.__BE_CI_RUNS__ || (globalThis.__BE_CI_RUNS__ = {});
//  The verdict memos, same one-slot rule: wt → { rev, row }.
const MEMO = globalThis.__BE_CI_MEMO__ || (globalThis.__BE_CI_MEMO__ = {});

//  CI-004: the detection LADDER, first hit in the worktree root wins.  `probe`
//  is a wt-relative regular file; `cmd` is the shell line it stands for.
const LADDER = [
  { probe: "ci.sh",          cmd: "./ci.sh" },
  { probe: "scripts/ci.sh",  cmd: "./scripts/ci.sh" },
  { probe: "CMakeLists.txt",
    cmd: "cmake -S . -B build -G Ninja && ninja -C build && ctest --test-dir build -j16" },
  { probe: "configure",      cmd: "./configure && make && make test" },
  { probe: "Makefile",       cmd: "make && make test" },
  { probe: "Cargo.toml",     cmd: "cargo test" },
  { probe: "go.mod",         cmd: "go test ./..." },
  { probe: "package.json",   cmd: "npm test" },
  { probe: "pyproject.toml", cmd: "pytest" },
  { probe: "setup.py",       cmd: "pytest" },
];

function isFile(p) {
  let st; try { st = io.stat(p); } catch (e) { return false; }
  return st.kind === "reg";
}

//  CI-004: the ladder walk — { probe, cmd } for the first rung whose file is
//  there, else null (THE "nothing detected" result, distinct from any command).
function detect(wt) {
  if (!wt) return null;
  for (const r of LADDER) {
    let full; try { full = discover.wtpath(wt, r.probe); } catch (e) { continue; }
    if (isFile(full)) return { probe: r.probe, cmd: r.cmd };
  }
  return null;
}

//  CI-004: the run artefacts live OUTSIDE the tree — writing them inside would
//  dirty `be status` and drop the very cache bucket the verdict rides in.
function stateDir() { return pathlib.join(io.getenv("TMP") || "/tmp", "be-ci"); }

function paths(wt) {
  const key = sha.frameSha("blob", utf8.Encode(wt)).slice(0, 16);
  const d = stateDir();
  return { dir: d, log: pathlib.join(d, key + ".log"), rc: pathlib.join(d, key + ".rc") };
}

//  The exit code the child echoed, or null while it has not finished.
function readRc(p) {
  let st; try { st = io.stat(p); } catch (e) { return null; }
  if (st.kind !== "reg" || st.size === 0) return null;
  let fd; try { fd = io.open(p, "r"); } catch (e) { return null; }
  const b = io.buf(64);
  try { io.readAll(fd, b); } catch (e) { return null; } finally { io.close(fd); }
  const n = parseInt(utf8.Decode(b.data().slice()).replace(/[^0-9-]/g, ""), 10);
  return isNaN(n) ? null : n;
}

//  POSIX single-quoting: the wt path and the artefact paths reach /bin/sh as
//  literals, never as words the shell may split or expand.
function sq(s) { return "'" + String(s).split("'").join("'\\''") + "'"; }

//  The child finished → reap it, mark the record done (so a re-press starts a
//  fresh run instead of reporting "in progress" forever) and CONSUME the marker
//  into the memo, stamped with the rev the tree stands at NOW — the run's own
//  build droppings are already in, so the NEXT edit is what stales the verdict.
function settle(wt) {
  const r = RUNS[wt];
  if (!r || r.done) return;
  const code = readRc(paths(wt).rc);
  if (code === null) return;
  //  CI-004: an ATTACHED run is another process's child (pid -1) — only its own
  //  parent can reap it; the marker is still the handoff.
  if (r.pid >= 0) try { io.reap(r.pid); } catch (e) {}
  r.done = true;
  const p = paths(wt);
  MEMO[wt] = { rev: CACHE.rev(wt),
               row: { state: code === 0 ? "green" : "red", code: code,
                      log: p.log, cmd: r.cmd } };
  //  TODO 11: ...and REMEMBER it on disk — the memo is the live-session layer,
  //  the map is what a cold board (a fresh pager, no watcher) colours off.
  writeStatus(wt, code === 0 ? "ok" : "fail");
}

function running(wt) { const r = RUNS[wt]; return !!(r && !r.done); }

//  CI-004 (TODO 11, ruling 2026-08-04): the REMEMBERED verdict — a `{wt: status}`
//  map BESIDE THE LOGS, written at settle and read COLD.  It lives outside every
//  watched tree, so writing it bumps no wt's rev and self-stales nothing; and the
//  board shows the last verdict with no live watcher and across pager restarts.
//  One `<status>\t<wt>` row per worktree, status `ok` or `fail` — nothing else.
function statusPath() { return pathlib.join(stateDir(), "verdicts"); }
const SMEMO = globalThis.__BE_CI_SMEMO__ ||
              (globalThis.__BE_CI_SMEMO__ = { key: "", map: {} });

//  The whole map, re-parsed only when the file's (mtime, size) moved — a board
//  asks once per wt row.
function readStatus() {
  let st; try { st = io.stat(statusPath()); } catch (e) { SMEMO.key = ""; return {}; }
  const key = String(st.mtime) + ":" + st.size;
  if (SMEMO.key === key) return SMEMO.map;
  const map = {};
  let fd; try { fd = io.open(statusPath(), "r"); } catch (e) { return map; }
  const b = io.buf(st.size + 64);
  try { io.readAll(fd, b); } catch (e) {} finally { io.close(fd); }
  for (const line of utf8.Decode(b.data().slice()).split("\n")) {
    const t = line.indexOf("\t");
    if (t > 0) map[line.slice(t + 1)] = line.slice(0, t);
  }
  SMEMO.key = key; SMEMO.map = map;
  return map;
}

//  Remember `wt`'s verdict: read-modify-write the whole map (a handful of rows)
//  through a temp + rename, so a cold reader never sees a half-written file.
function writeStatus(wt, st) {
  const map = readStatus();
  const out = {};
  for (const k in map) out[k] = map[k];
  out[wt] = st;
  let text = "";
  for (const k in out) text += out[k] + "\t" + k + "\n";
  const tmp = statusPath() + ".tmp";
  try {
    try { io.mkdir(stateDir()); } catch (e) {}
    const fd = io.open(tmp, "c");
    const bytes = utf8.Encode(text);
    const b = io.buf(bytes.length + 8); b.feed(bytes);
    try { io.writeAll(fd, b); } finally { io.close(fd); }
    io.rename(tmp, statusPath());
  } catch (e) { return; }
  SMEMO.key = "";                 // our own write must not read back as stale
}

//  CI-004 (TODO 11): the LAST remembered verdict for `wt` — "ok" / "fail", else
//  null (never ran here).  COLD by construction: no watcher, no memo, no live
//  process.  This is what the board button's COLOUR reads.
function status(wt) { return (wt && readStatus()[wt]) || null; }

//  CI-004: spawn the detected command in `wt`.  Returns { started, message } —
//  plain words, no error codes; the pager puts `message` on the status line.
function run(wt) {
  if (!wt) return { started: false, message: "ci: no worktree in this context" };
  settle(wt);
  if (running(wt)) return { started: false, message: "ci: already running — " + RUNS[wt].cmd };
  const d = detect(wt);
  if (!d) return { started: false, message: "ci: no build or test command found in this tree" };
  const p = paths(wt);
  try { io.mkdir(p.dir); } catch (e) {}
  try { io.unlink(p.rc); } catch (e) {}      // a stale verdict must not read as fresh
  delete MEMO[wt];                           // ...and the old memo is not this run's
  //  Redirect FIRST (a `cd` complaint belongs in the log, not on the pager's
  //  screen), then run, then leave the exit code in the marker.
  const script = "exec >" + sq(p.log) + " 2>&1 </dev/null; cd " + sq(wt) +
                 " && { " + d.cmd + " ; }; echo $? >" + sq(p.rc);
  let pid;
  try { pid = io.spawnFds("/bin/sh", ["sh", "-c", script], -1, -1); }
  catch (e) { return { started: false, message: "ci: cannot start a shell (" + e + ")" }; }
  RUNS[wt] = { pid: pid, cmd: d.cmd, log: p.log, done: false };
  return { started: true, message: "ci: running " + d.cmd + " — log " + p.log };
}

//  CI-004 (TODO 7): a run in flight that is NOT ours — the log file is there and
//  the `.rc` marker is not, i.e. some other jab process's child is still writing.
//  ADOPT it (pid -1: we tail it, its own parent reaps it) rather than fork a
//  second build over the same tree.
function adopt(wt) {
  const p = paths(wt);
  if (!isFile(p.log) || readRc(p.rc) !== null) return false;
  const d = detect(wt);
  RUNS[wt] = { pid: -1, cmd: (d && d.cmd) || "", log: p.log, done: false };
  return true;
}

//  CI-004 (TODO 7): THE view's entry — FORK the detected command, or ATTACH to
//  the run already going in this tree.  ONE runner: the `v` key, the board's ` ∞`
//  (through views/ci/ci.js) and the CLI all arrive here or at run() below.
//  It forks ONLY when nothing runs AND no FRESH verdict stands, so a re-render
//  (the ~1s tick) replays the settled screen instead of starting a second build;
//  a `r`, which bumps the rev root and stales every memo, IS "run it again".
//  Returns { message (plain words, may be ""), cmd (the command line, or "") }.
function ensure(wt) {
  if (!wt) return { message: "ci: no worktree in this context", cmd: "" };
  settle(wt);
  if (running(wt) || adopt(wt))
    return { message: "ci: running " + (RUNS[wt].cmd || "the job in this tree"),
             cmd: RUNS[wt].cmd };
  const fresh = row(wt);
  if (fresh) return { message: "", cmd: fresh.cmd || "" };
  const r = run(wt);
  const d = RUNS[wt];
  return { message: r.message, cmd: (d && !d.done && d.cmd) || "" };
}

//  CI-004: THE verdict row.  null = nothing FRESH to show — never ran here, or
//  the tree moved under the last verdict, or there is no watcher to say either
//  way.  `run` = in flight (live process state, not a memo); else the memo's
//  green/red, valid exactly while the wt's rev stands where the run settled.
function row(wt) {
  if (!wt) return null;
  settle(wt);
  if (running(wt))
    return { state: "run", code: null, log: paths(wt).log, cmd: RUNS[wt].cmd };
  const m = MEMO[wt];
  if (!m) return null;
  if (m.rev !== CACHE.rev(wt)) { delete MEMO[wt]; return null; }
  return m.row;
}

//  CI-004 (TODO 7): THE TAIL — the last TAILBYTES of the run's log, cut at a
//  line start so the first row is never half a line.  The file `spawnFds` wrote
//  IS the view: this is a reader, never a second capture path.  Re-read per
//  refresh (a fresh map each time), so a growing log tails live.
const TAILBYTES = 4096;
function tail(wt) {
  if (!wt) return null;
  const p = paths(wt);
  let m = null;
  try { m = io.mmap(p.log, "r"); } catch (e) { return null; }
  let text = "";
  try {
    const all = m.data();
    let from = all.length > TAILBYTES ? all.length - TAILBYTES : 0;
    if (from > 0) {
      let i = from;                            // ...forward to the next line start
      while (i < all.length && all[i] !== 0x0a) i++;
      if (i < all.length) from = i + 1;
    }
    text = utf8.Decode(all.slice(from));
  } catch (e) { text = ""; } finally { try { m.close(); } catch (e) {} }
  return { text: text, log: p.log, cut: TAILBYTES };
}

//  CI-004 (TODO 9 ruling): the verdict FOOTER the log view renders UNDER the
//  tail — render-only, never a byte in the log file.  The banner keeps the
//  command line; the verdict lives here.
function footer(r) {
  if (!r) return "";
  if (r.state === "run") return "⋯ running";
  return "── " + (r.state === "green" ? "PASS" : "FAIL") + " rc=" + r.code + " ──";
}

//  CI-004: the one-line badge the pager's status bar carries; "" when nothing ran.
function badge(r) {
  if (!r) return "";
  if (r.state === "run") return "ci: running";
  if (r.state === "green") return "ci: ok";
  return "ci: failed (exit " + r.code + ") " + r.log;
}

//  CI-004: the CURRENT CONTEXT's worktree root — the nav URI maps to a dir
//  through discover.wtdir (confined), then treeAt anchors the tree it is in.
//  BRO-046: it is TWO `.be` climbs, so it is resolved AT NAVIGATION only — the
//  pager publishes its own (`pager.ctxWt`); one-shot callers still land here.
function contextWt(navStr, fallback) {
  const fb = (fallback && fallback.wt) || null;
  let d = null;
  if (navStr) { try { d = discover.wtdir(navStr); } catch (e) { d = null; } }
  if (!d) return fb;
  let repo; try { repo = discover.treeAt(d); } catch (e) { return fb; }
  return (repo && repo.wt) || fb;
}

module.exports = { detect: detect, run: run, row: row, badge: badge,
                   paths: paths, contextWt: contextWt, running: running,
                   ensure: ensure, tail: tail, footer: footer, LADDER: LADDER,
                   status: status, statusPath: statusPath };
