//  JSQUE-002: the resident dispatch loop.  ONE long-running process pulls
//  `<verb> <uri>` rows off the core/job.js queue and dispatches each to a
//  resident handler via O(1) registry lookup; a handler may enqueue child
//  rows (fan-out, consume-while-append).  Replaces fork-per-verb: the JSC
//  arena + require cache are paid ONCE for the whole run.  See JSQUE-001/003.
"use strict";

const job = require("core/job.js");
const registry = require("core/registry.js");
//  JSQUE-008: the integration seam — the real seed (resolution-at-entry) and
//  emit sink (output-as-ULog) replace the JSQUE-002 stubs in the CLI entry.
//  JSQUE-016: the entry shim (be/loop.js) requires this, so argv[1] is the
//  shim path; _here is the be/ ROOT (where core/ + shared/ live).
const _self = process.argv[1];
const _here = _self.slice(0, _self.lastIndexOf("/"));
const resolve = require("core/resolve.js");
const emit = require("core/emit.js");
const be = require(_here + "/core/discover.js");
const wtlog = require(_here + "/shared/wtlog.js");
const store = require(_here + "/shared/store.js");
//  JAB-029: the edge render — cli() turns the collected HUNK sink into fd-1
//  bytes via bro.renderHunkLog (the ONE place mode plain/color/tlv is applied).
const bro = require(_here + "/view/bro.js");

//  run(opts): seed -> build registry -> consume-while-append dispatch loop.
//    opts.seedRows : [{verb, uri}]   the seed job list (argv lowered; JSQUE-004
//                    delivers the real resolution-at-entry seed — here a stub
//                    just forwards the rows).
//    opts.queuePath: where the queue ULOG lives.  cli() ALWAYS passes a
//                    per-process /tmp path (see _tmpQueue); the bare ".be/queue"
//                    default here only bites a direct run() call with no path.
//    opts.repo     : the opened repo handle (forwarded in ctx; loop is agnostic).
//    opts.out      : the emit sink (JSQUE-005); a no-op stub is used if absent.
//    opts.require  : the be-relative require of the caller (so the registry's
//                    require(verb) scans the right be/ shard); default global.
//  Returns { dispatched, order } — dispatched count + the verb-dispatch order
//  (the proof the loop drove the queue; the real run cares only about effects).
function run(opts) {
  opts = opts || {};
  const seedRows = opts.seedRows || [];
  const queuePath = opts.queuePath || ".be/queue";
  const req = opts.require || require;

  //  Resolve every distinct seed verb to a handler ONCE (warm cache).  A child
  //  verb a handler enqueues is resolved lazily on first sight (same cache).
  const handlers = registry.build(seedRows.map(function (r) { return r.verb; }), req);

  const q = job.openOrResume(queuePath, seedRows);

  //  ctx: the per-run context every handler shares (re-entrant — handlers keep
  //  no module-global accumulators; per-row state rides `row`).  This is the
  //  interface JSQUE-004 (seed/resolve) and JSQUE-005 (emit) integrate against.
  const ctx = {
    repo: opts.repo || null,           // opened repo (JSQUE-004 resolves it)
    T0: opts.T0 != null ? opts.T0 : ron.now(),  // cohort timestamp (one per run)
    out: opts.out || _nullSink(),      // emit sink (JSQUE-005 supplies the real)
    queue: q,                          // the live queue (for direct enqueue)
    //  JSQUE-008: seed-pinned constants threaded to every handler — the flag
    //  set + the resolved ref ops (resolution-at-entry; the queue round-trip
    //  carries only ts/verb/uri, so these ride ctx, not the row).
    flags: opts.flags || [],
    refs: opts.refs || [],
    resolved: opts.resolved || null,   // seedCtx's pinned coordinates
    //  JSQUE-011: the whole seed-row batch (one entry per path arg) so a
    //  batch verb (delete) can fold all its path forms in one handler pass —
    //  the queue round-trip carries rows singly, but DELETE's `delete:` table,
    //  dir-preflight barrier, and batch dirty-abort span the full arg list.
    seedRows: seedRows,
    //  JSQUE-013: PATCH's (ours, theirs, fork) commit-triple, pinned ONCE at
    //  seed (resolve.seed); the per-file weave leaves read it off ctx.
    triple: opts.triple || null,
    //  JSQUE-010: real-path-arg seed count (0 ⇒ the "." row is a placeholder).
    seededRowCount: opts.seededRowCount != null ? opts.seededRowCount : null,
    //  JSQUE-012: the raw positional args (POST's commit message rides here,
    //  seed-pinned — a non-path arg the queue uri can't carry).
    args: opts.args || [],
    //  the shared output mode (color|tlv|plain) — a view renders its hunk
    //  stream through view/bro.js renderHunkLog in this mode.
    mode: opts.mode || "plain",
    //  JAB-029: the in-memory HUNK sink every content view feeds (no fd 1).
    //  cli() owns ONE renderHunkLog(sink.log, mode) -> fd 1 at the edge; an
    //  in-process caller (bro) passes its OWN sink and reads sink.log direct.
    sink: opts.sink || _hunkSink(),
  };

  const order = [];
  let dispatched = 0;
  let row;
  //  JSQUE-014: ONE loop-edge catch is the single source of truth for a refusal
  //  — a handler `throw` (DELDIRTY/PUTNONE/POSTNONE/GETOVRL) jumps past the clean
  //  edge-flush, so FLUSH the partial banner here (same outSort the clean edge
  //  uses) THEN re-propagate to the top (jab maps it to the non-zero exit +
  //  stderr diag; no process.exit in handlers — JS-026).  Handlers no longer
  //  flush-before-throw (delete.js/post.js); the loop owns it.
  try {
    while ((row = q.next())) {
      order.push(row.verb);
      const handle = handlers[row.verb];
      if (handle == null) {            // unconverted verb: resolve lazily once
        const lazy = registry.build([row.verb], req);
        handlers[row.verb] = lazy[row.verb];
        if (handlers[row.verb] == null)
          throw "loop: no handler for verb '" + row.verb + "' (one-shot fallback NYI)";
      }
      const result = handlers[row.verb](row, ctx);
      dispatched++;
      //  Fan-out: a handler returns { enqueue: [...] } to append child rows at
      //  the tail; the cursor re-reads the watermark so they are seen this loop.
      if (result && result.enqueue && result.enqueue.length)
        q.append(result.enqueue);
    }
  } catch (e) {
    if (ctx.out && ctx.out.flush) ctx.out.flush(ctx.outSort || null);
    throw e;                           // re-propagate: process exit + stderr
  }
  q.markDone();
  q.close(true);                       // clean exit: trim + unlink the queue
  //  A handler may set ctx.outSort (a flush comparator) to own its render
  //  order at the edge — GET: new+upd lex, then del lex (JSQUE-009).
  return { dispatched: dispatched, order: order, outSort: ctx.outSort };
}

//  JAB-029: the in-memory HUNK sink — a grow-on-full HUNK ram log a content
//  view feeds (feed(uri, body, toks, verb, ts)).  abc.ram is fixed-size, so on
//  the binding's `out full` throw we double the cap and replay the held records
//  into a fresh log (atomic feed: a failed record is not half-written).  `.log`
//  is the HUNK log renderHunkLog reads at the edge; `.empty` gates the flush.
function _hunkSink(initial) {
  let cap = initial || (1 << 16);
  let log = abc.ram("HUNK", cap);
  const recs = [];
  function replay() { log = abc.ram("HUNK", cap); for (const r of recs) log.feed(r.uri, r.body, r.toks, r.verb, r.ts); }
  return {
    feed: function (uri, body, toks, verb, ts) {
      const r = { uri: uri, body: body, toks: toks, verb: verb, ts: ts };
      for (let t = 0; t < 40; t++) {
        try { log.feed(uri, body, toks, verb, ts); recs.push(r); return; }
        catch (e) {
          if (!("" + e).includes("full")) throw e;   // only grow on `out full`
          cap = cap * 2 + uri.length + (body ? body.length : 0) +
                (toks ? toks.length * 4 : 0) + 256;
          replay();
        }
      }
      throw "loop: HUNK sink grow exhausted";
    },
    get log() { return log; },
    get empty() { return recs.length === 0; },
  };
}

//  A no-op emit sink so the loop is provable without JSQUE-005 wired in.  Same
//  surface as core/emit.js (create/banner/row/render) — the real sink drops in.
function _nullSink() {
  const rows = [];
  return {
    banner: function () {},
    row: function (path, verb, ts, extra) { rows.push({ path: path, verb: verb }); },
    render: function () { return new Uint8Array(0); },
    rows: rows,
  };
}

//  --- JSQUE-008: the canonical CLI entry (argv -> seed -> run -> flush) ---
//  The SHARED integrated entry every later verb reuses: argv lowers to a verb +
//  positional args + flags; the repo + its ambient coordinates are pinned ONCE
//  via resolve.seedCtx (resolution-at-entry); resolve.seed turns the positional
//  args into branch-free seed rows; run() drives the loop with the REAL emit
//  sink; ONE out.flush(sort) at the edge renders the collected rows.  Replaces
//  the 002 `_nullSink`/`opts.seedRows` stubs.
function cli(argv) {
  let verb = argv[2];
  if (!verb) throw "loop: no verb (usage: loop.js <verb> [args])";
  //  Split flags (a leading '-') from the positional args — flags are seed
  //  globals (pinned in ctx), positionals become seed rows.
  const flags = [], args = [];
  for (const a of argv.slice(3)) (a[0] === "-" ? flags : args).push(a);

  //  One-shot projector form `jab <scheme>:<uri>` (mirrors native `be
  //  scheme:uri`, e.g. `grep:#body`, `cat:path`): the VERB token itself carries
  //  the whole URI.  A legal verb is a bare word, so a ':' in the verb position
  //  marks a scheme:uri one-shot — lower it to verb=<scheme> and thread the FULL
  //  token as the leading positional arg, so a view handler reads body/path off
  //  ctx.args (a fragment-only URI can't survive a queue row, and the unsplit
  //  `grep:#body` is not a legal queue verb token — it round-trips to '0').
  const _sc = verb.indexOf(":");
  if (verb[0] !== "-" && _sc > 0) { args.unshift(verb); verb = verb.slice(0, _sc); }

  //  JAB-025: the colour gate for the emit sink.  `--color` forces SGR,
  //  `--plain` forces the plain bytes; otherwise default to whether stdout
  //  (fd 1) is a terminal.  The piped/`--plain` path is byte-parity plain (the
  //  SUT=loop harnesses redirect stdout, so they land here OFF); a tty (or
  //  `--color`) renders the columnar rows through the C THEME.
  //  The SHARED output-mode gate — ONE source for every view.  An explicit flag
  //  wins; with none, a tty defaults to colour.  color = dog/THEME SGR, tlv =
  //  the raw HUNK 'H' records (on-wire), plain = HUNKu8sFeedText bytes.
  //  --color/tty → color; --tlv → tlv; --plain/else → plain.
  const mode = flags.indexOf("--color") >= 0 ? "color"
             : flags.indexOf("--tlv")   >= 0 ? "tlv"
             : flags.indexOf("--plain") >= 0 ? "plain"
             : io.isatty(1)                  ? "color"
             : "plain";
  const color = mode === "color";

  //  Pin the repo + ambient coordinates ONCE at entry (JSQUE-004).  A fresh
  //  GET clone targets an EMPTY dir (no `.be` yet) — be.find throws there, so
  //  GET runs repo-less: the seed is the raw remote URI; the handler creates
  //  the anchor itself (JSQUE-009).  Other verbs require an existing wt.
  let repo = null, sctx = null, seeded;
  try {
    repo = be.find();
    const wtl = wtlog.open(repo);
    const k = store.open(repo.storePath, repo.project);
    sctx = resolve.seedCtx(repo, wtl, k, { skipIgnore: true });
    seeded = resolve.seed(verb, args, sctx, repo);
  } catch (e) {
    //  bro is a file viewer — runs with no/empty .be, like get.
    if (verb !== "get" && verb !== "bro") throw e;   // GET/bro run repo-less
    seeded = { rows: args.map(function (a) { return { path: a }; }), refs: [] };
  }

  //  argv -> branch-free seed rows.  No positional args (status) -> ONE self
  //  row so the verb fires once; path/ref verbs fan to one row per arg + the
  //  pinned ref ops.  resolve.seed never re-resolves a ref per row (JSQUE-004).
  //  A ULog row needs a non-empty uri (an empty one is not materialised), so a
  //  no-arg seed carries "." (cwd) — the handler prefers ctx.repo regardless.
  //  JSQUE-012: a `#msg` arg seeds an EMPTY-path row (the message rides ctx.args);
  //  filter those so a fold verb (post) fires once over its change-set, not per word.
  const realRows = seeded.rows.filter(function (r) { return r.path; });
  const seedRows = realRows.length
        ? realRows.map(function (r) {
            //  JSQUE-010: a move-form pin carries a dst (fragment) — emit the
            //  full `path#dst` uri so the handler re-parses both slots.
            return { verb: verb, uri: r.dst ? (r.path + "#" + r.dst) : r.path };
          })
        : [{ verb: verb, uri: "." }];

  //  JOBQ: the queue ALWAYS lives in /tmp keyed by PID — never in-repo.  A
  //  PID-keyed name means a fresh run (new PID) can never RESUME a DEAD
  //  process's leftover queue (the stale-dispatch / `verb '0'` bug); the path
  //  is per-process, so two concurrent runs get distinct files (no collision).
  //  A repo-less GET (fresh clone) keys off cwd; a repo run keys off its bePath.
  const queuePath = repo ? _queuePath(repo)
        : _tmpQueue(io.cwd());

  const out = emit.create({ color: color });   // JAB-025: tty/--color gate
  //  JAB-029: the content-view HUNK sink (cat/grep/spot/regex feed it, no fd 1);
  //  cli OWNS the one renderHunkLog(sink.log, mode) -> fd 1 edge write below.
  const sink = _hunkSink();
  const res = run({
    seedRows: seedRows, queuePath: queuePath, repo: repo, require: require,
    out: out, sink: sink, flags: flags, refs: seeded.refs, resolved: sctx,
    mode: mode,              // the shared color/tlv/plain output mode (ctx.mode)
    triple: seeded.triple,   // JSQUE-013: forward the seed-pinned PATCH triple
    //  JSQUE-010: count of REAL path-arg rows (vs the "." placeholder); a
    //  ref-only run has 0, so the handler applies ctx.refs without staging ".".
    seededRowCount: seeded.rows.length,
    args: args,   // JSQUE-012: raw positional args (POST commit message)
  });
  //  ONE flush at the edge.  status pre-orders its rows (divergence then
  //  buckets), so no global sort comparator — render in push order.
  out.flush(res.outSort || null);   // JSQUE-009: GET owns its edge sort order
  //  JAB-029: the content-view edge — render the collected HUNK sink to fd 1 in
  //  the mode (plain/color/tlv), ONE write, in the one place the mode is known.
  if (!sink.empty) {
    const bytes = bro.renderHunkLog(sink.log, mode);
    if (bytes.length) { const b = io.buf(bytes.length + 8); b.feed(bytes); io.writeAll(1, b); }
  }
  return res;
}

//  JOBQ: this process's PID, for the per-process queue name (the portable POSIX
//  `io.getpid()` leaf — no /proc, no platform-specifics).  The PID makes the
//  queue path unique per run, so a dead process's queue is NEVER resumed and
//  two concurrent (forked-worker) runs never collide.
function _pid() {
  return String(io.getpid());
}

//  JOBQ: the queue ALWAYS lives in /tmp, keyed by USER + PID + a path key.  No
//  in-repo `.be/queue` (that file leaked into the working tree and, PID-less,
//  was resumed across runs).  PID-keyed ⇒ no stale resume, no fork collision.
function _tmpQueue(key) {
  return "/tmp/.bequeue." + (io.getenv("USER") || "x") + "." + _pid() +
         "." + String(key).split("/").join("_");
}

//  The per-process /tmp queue path keyed by the repo's bePath — replaces the
//  old in-repo `.be/queue` (primary) / un-PID'd /tmp scratch (secondary): both
//  now route through _tmpQueue so EVERY case is PID-keyed and unlinked on exit.
function _queuePath(repo) {
  return _tmpQueue(repo.bePath || "");
}

//  JSQUE-016: always required (via the be/loop.js entry shim), so export
//  run/cli; the shim self-runs cli() when invoked directly.
if (typeof module !== "undefined") module.exports = { run: run, cli: cli };
else cli(process.argv);
