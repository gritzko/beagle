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

//  GIT-016: verbs register by FILE — a bareword resolves to verbs/<verb>/<verb>.js
//  here (no explicit list); `head` (verbs/head/head.js) is picked up automatically.
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
    //  A name resolves from one of TWO trees: the mutating VERBS
    //  (verbs/<verb>/) — get/put/post/delete/patch — and the verbless VIEWS
    //  (views/<view>/) — the read-only projectors (ls/cat/diff/spot/…), which
    //  the loop dispatches by URI scheme exactly like a verb.  Try views/ then
    //  verbs/ (the names are disjoint); the be-relative scan (require.cpp)
    //  finds the shard nearest the requirer.
    try { mod = req("views/" + verb + "/" + verb + ".js"); }
    catch (e) {
      try { mod = req("verbs/" + verb + "/" + verb + ".js"); }
      catch (e2) { table[verb] = null; continue; }
    }
    table[verb] = (typeof mod === "function") ? mod : null;
  }
  return table;
}

module.exports = { build: build };
