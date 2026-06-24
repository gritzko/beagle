//  JSQUE-002: the verb->handler registry.  build() resolves each distinct
//  verb to its handler ONCE via require(verb) (warm process-resident cache,
//  keyed by abspath), so a thousand-row run pays the require/eval cost per
//  DISTINCT verb, never per row.  A handler is `(row, ctx) -> {enqueue?}`.
"use strict";

//  HANDLER CONTRACT (JSQUE-002):
//    module.exports = function handle(row, ctx) { ...; return result; }
//      row : { ts, verb, uri, offset }   (a queue row; uri is the args)
//      ctx : { repo, T0, out, queue }    (see be/loop.js for the fields)
//      result (optional): { enqueue: [{verb, uri}, ...] }  -> child rows
//        fanned out onto ctx.queue (consume-while-append).  Any other return
//        (undefined/null) enqueues nothing.  A THROW propagates to the top
//        (the loop never catches per-row) so it becomes the process exit code.
//    A handler module must NOT also run `main();` — the cache evals the body
//    ONCE at require time, so a tail call would fire at load, not per row.

//  build(verbs, requireFn): map each distinct verb name to its handler.
//  `requireFn` is the be-relative require of the CALLING module (so the
//  upward be/-scan finds the shard nearest loop.js, not cwd); default the
//  global require.  A verb whose module does not export a function is left
//  ABSENT from the table — loop.js falls back to the old one-shot script for
//  any verb the table does not resolve (incremental migration, JSQUE-001).
function build(verbs, requireFn) {
  const req = requireFn || require;
  const table = {};
  for (const verb of verbs) {
    if (table[verb] !== undefined) continue;   // distinct verbs only
    let mod;
    try { mod = req(verb); } catch (e) { table[verb] = null; continue; }
    table[verb] = (typeof mod === "function") ? mod : null;
  }
  return table;
}

module.exports = { build: build };
