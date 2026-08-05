//  verbs/fork/fork.js — the todo board's [go] button (TODO-005 goSlot): mint the
//  worktree a ticket does NOT own yet.  `fork KEY [REP]` makes `work/<KEY>` and
//  clones REP into it — the [/meta/work] wt life cycle (`mkdir work/<TICKET>`,
//  then `get` inside it), one spell.  REP defaults to the page's `Rep:` pair
//  ([/meta/todo]: the repo the ticket relates to, usually relative — `///be`).
//  The verb is context-LESS: the wt does not exist yet, so there is no `//<wt>/`
//  to run in, and the destination is arithmetic on be.workRoot(), never a walk.
//  The clone is THIS jab binary re-run as `get` in the new dir (the cross-
//  process shape) — get's own fresh-clone path, never a second checkout.
"use strict";

const pathlib = require("../../shared/util/path.js");
const join = pathlib.join;
const hunkrows = require("../../shared/hunkrows.js");
//  The board helpers stay in ONE place: the key SHAPE test, the KEY→page probe
//  and the head reader (title + `Key: value` pairs) are todo.js's, as done.js's.
const todoView = require("../../views/todo/todo.js");
//  POST-028's selfBin — /proc/self/exe, else argv[0]; the ONE "this binary".
const wire = require("../../shared/wire.js");

//  Plain words to the user, the code to the throw (vim.js's shape).
function miss(arg, msg, code) {
  io.log("fork: " + arg + ": " + msg + "\n");
  throw code;
}

function exists(p) { try { io.lstat(p); return true; } catch (e) { return false; } }

//  RULING 2026-08-05: a fork starts from the LIVE rev, so a scheme-less wt URI
//  gets its DIR-FORM slash — `///be` is the parent's stale gitlink PIN of `be`,
//  `///be/` is be's own tip ([/meta/work] clones `///be/`).  A schemed URI
//  (file:/be://ssh:) names a store, not a pin, and an explicit `?ref` is the
//  author's own base — both pass VERBATIM.  Composed through the URI class.
function liveRev(rep) {
  let u; try { u = uri._parse(rep); } catch (e) { return rep; }
  if (!u || u.scheme || u.authority === undefined) return rep;
  if (u.query !== undefined || u.fragment !== undefined) return rep;
  const p = u.path || "";
  if (p.length && p[p.length - 1] === "/") return rep;
  return URI.make(undefined, u.authority, p + "/", undefined, undefined) || rep;
}

//  The ticket's `Rep:` — pageHead's ONE read of the head (title + the pair run
//  directly under it), the same head goSlot renders the button off.
function repOf(key) {
  //  todo.js's boardDir is the {root, dir} PAIR; pageFile takes the dir half.
  const board = todoView.boardDir();
  const file = board ? todoView.pageFile(board.dir, key) : null;
  if (!file) miss(key, "no ticket page under todo/", "FORKNONE");
  const meta = todoView.pageHead(file).meta || {};
  const rep = meta.Rep === undefined ? "" : String(meta.Rep).trim();
  if (!rep) miss(key, "the ticket carries no Rep: — nothing to fork from", "FORKNONE");
  return rep;
}

//  Re-run THIS binary as `get <rep>` with the new wt as cwd: the child owns the
//  clone, so no ambient repo of ours (be.repo is the LAUNCH tree) can leak in.
function cloneInto(dir, rep, key) {
  const bin = wire.selfBin();
  const here = io.cwd();
  let p;
  io.chdir(dir);
  try { p = io.spawn(bin, [bin, "get", rep]); }
  catch (e) { io.chdir(here); miss(key, "cannot spawn '" + bin + "' (" + e + ")", "FORKSPAWN"); }
  io.chdir(here);
  try { io.close(p.stdin); } catch (e) {}
  const b = io.ram(1 << 16);
  while (io.read(p.stdout, b) > 0) {}            // drain, else the child blocks
  try { io.close(p.stdout); } catch (e) {}
  const r = io.reap(p.pid);
  if (r.signal !== undefined) miss(key, "the clone was killed by signal " + r.signal, "FORKDIED");
  if (r.code !== 0) miss(key, "the clone of " + rep + " failed (exit " + r.code + ")", "FORKFAIL");
}

//  Fork ONE ticket: work/<KEY> is made, then REP is cloned into it.  An existing
//  work/<KEY> is a REFUSAL, never a re-clone — the wt may hold live work.
function forkOne(key, rep, row) {
  if (todoView.shape(key) !== "key") miss(key, "not a ticket key", "FORKNONE");
  const workR = (typeof be !== "undefined" && be.workRoot) ? be.workRoot() : null;
  if (!workR) miss(key, "no project root above the cwd — no work/ dir", "FORKNONE");
  const dest = join(workR, key);
  if (exists(dest)) miss(key, "work/" + key + " already exists — not touched", "FORKHAVE");
  const src = liveRev(rep || repOf(key));
  try { io.mkdir(dest); }
  catch (e) { miss(key, "cannot make work/" + key + " (" + e + ")", "FORKMKDIR"); }
  cloneInto(dest, src, key);
  row("work/" + key + " <- " + src, "new");
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its args reading `be`.  `fork KEY`
//  reads the page's `Rep:`; `fork KEY REP` takes the button's own (goSlot passes
//  the rep it rendered).  Further keys fork off their own pages.
function run(argv) {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  const out = sink ? hunkrows(sink, null) : null;
  let opened = false;
  //  The rows open the `work` banner LAZILY, so a pager click off this run lands
  //  on the work forest — where the wt just minted now shows.
  function row(text, verb) {
    if (out) { if (!opened) { opened = true; out.open("work"); } out.row(text, verb || "new", 0n); }
  }
  try {
    if (!argv.length) miss("", "no ticket key (fork KEY [REP])", "FORKNONE");
    const key = String(argv[0] == null ? "" : argv[0]);
    //  A second arg that is NOT key-shaped is the REP (what goSlot passes);
    //  key-shaped args are all KEYS, each forked off its own page.
    if (argv.length === 2 && todoView.shape(String(argv[1] == null ? "" : argv[1])) !== "key") {
      forkOne(key, String(argv[1]), row); return;
    }
    for (let i = 0; i < argv.length; i++) forkOne(String(argv[i] == null ? "" : argv[i]), "", row);
  } finally { if (out) out.done(); }
}

function fork() { return run(Array.prototype.slice.call(arguments)); }
fork.jab = "args";
module.exports = fork;
//  Exposed for the repro test (the liveRev slash rule has no other witness).
module.exports.liveRev = liveRev;
