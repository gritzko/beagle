//  JSQUE-002: the resident dispatch loop.  ONE long-running process pulls
//  `<verb> <uri>` rows off the core/job.js queue and dispatches each to a
//  resident handler via O(1) registry lookup; a handler may enqueue child
//  rows (fan-out, consume-while-append).  Replaces fork-per-verb: the JSC
//  arena + require cache are paid ONCE for the whole run.  See JSQUE-001/003.
"use strict";

const job = require("core/job.js");
const registry = require("core/registry.js");

//  run(opts): seed -> build registry -> consume-while-append dispatch loop.
//    opts.seedRows : [{verb, uri}]   the seed job list (argv lowered; JSQUE-004
//                    delivers the real resolution-at-entry seed — here a stub
//                    just forwards the rows).
//    opts.queuePath: where the .be/queue ULOG lives (default ".be/queue").
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
  };

  const order = [];
  let dispatched = 0;
  let row;
  while ((row = q.next())) {
    order.push(row.verb);
    const handle = handlers[row.verb];
    if (handle == null) {              // unconverted verb: resolve lazily once
      const lazy = registry.build([row.verb], req);
      handlers[row.verb] = lazy[row.verb];
      if (handlers[row.verb] == null)
        throw "loop: no handler for verb '" + row.verb + "' (one-shot fallback NYI)";
    }
    //  A throw here is NOT caught — it propagates to the top so a bad row sets
    //  the process exit code (JS-026 intent: no process.exit in handlers).
    const result = handlers[row.verb](row, ctx);
    dispatched++;
    //  Fan-out: a handler returns { enqueue: [...] } to append child rows at
    //  the tail; the cursor re-reads the watermark so they are seen this loop.
    if (result && result.enqueue && result.enqueue.length)
      q.append(result.enqueue);
  }
  q.markDone();
  q.close(true);                       // clean exit: trim + unlink the queue
  return { dispatched: dispatched, order: order };
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

module.exports = { run: run };
