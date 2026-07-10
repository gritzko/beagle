//  core/registry.js — the verb->handler registry.  build() resolves each
//  distinct verb to its handler ONCE via require(verb) (warm process-resident
//  cache, keyed by abspath), so the require/eval cost is paid per DISTINCT verb.
"use strict";

//  VERB CONTRACT (JAB-004): a verb is a PLAIN-ARGS function.
//    module.exports = function verb(...args) { ... }; verb.jab = "args";
//      args : the tokenizer's plain JS values (strings / safe-scalars / evaled).
//      ambient repo/sink/out/format/force/verb ride the global `be` (mintBe).
//      the verb parses its own URIs, owns its own fan-out, feeds be.sink/be.out;
//      a THROW propagates to the loop edge (jab maps it to the non-zero exit).
//    (Object form `module.exports = { args:true, run:verb }` is also accepted.)
//    A verb module must NOT run `main();` — the cache evals the body ONCE.

//  JAB-004: opt-in marker — a verb exports a fn with `.jab==="args"` (or
//  `{args:true,run:fn}`).  An unmarked module has no plain-args handler.
function convention(mod) {
  if (mod && typeof mod === "object" && mod.args === true && typeof mod.run === "function")
    return { how: "args", fn: mod.run };
  if (typeof mod === "function")
    return { how: mod.jab === "args" ? "args" : "legacy", fn: mod };
  return null;                              // not a handler module
}

//  GIT-016: verbs register by FILE — a bareword resolves to verbs/<verb>/<verb>.js
//  here (no explicit list); `head` (verbs/head/head.js) is picked up automatically.
//  build(verbs, requireFn): map each distinct verb name to its handler.
//  `requireFn` is the be-relative require of the CALLING module (so the
//  upward be/-scan finds the shard nearest loop.js, not cwd); default the
//  global require.  A verb whose module does not export a handler is left
//  ABSENT (null) from the table — cli() then refuses the verb.
//  JAB-004: a converted entry is `{jab:"args",fn}` so cli() routes plain dispatch.
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
    const c = convention(mod);
    if (c == null) { table[verb] = null; continue; }
    table[verb] = c.how === "args" ? { jab: "args", fn: c.fn } : c.fn;
  }
  return table;
}

//  BE-029: locate a verb's handler file by BE-CLIMBING from `startDir` (default
//  cwd) — at each ancestor try `<dir>/be/verbs/<w>/<w>.js` then `.../views/…`,
//  ceiling at $HOME, first hit wins.  This mirrors jab's own upward be/-scan
//  (require.cpp resolveBe) but anchors on CWD, so a nested be/ shard (e.g.
//  `html/be` for publication) supplies its OWN verbs while core/shared still
//  load from the parent be/ the loop launched from.  Returns the abs path or
//  null.  Distinct from JAB-030's `_here` probe, which sees one root only.
//  BE-041: an optional `dirs` narrows the scan to one tree — ["verbs"] probes
//  the MUTATING verbs only (the pager's act-button gate), default both.
function verbFile(w, startDir, dirs) {
  let dir = startDir || io.cwd();
  const home = io.getenv("HOME");
  for (;;) {
    for (const d of dirs || ["verbs", "views"]) {
      const p = dir + "/be/" + d + "/" + w + "/" + w + ".js";
      try { if (io.stat(p)) return p; }
      catch (e) {}                            // ENOENT — keep climbing
    }
    if (dir === home || dir === "/" || dir === "") break;
    const i = dir.lastIndexOf("/");
    dir = i > 0 ? dir.slice(0, i) : "/";
  }
  return null;
}

//  BE-029: resolve a verb to its handler via the cwd-climb (verbFile), then apply
//  the verb contract (convention).  `requireFn` requires the resolved ABS path
//  (default global require, which handles absolute paths).  Returns the same
//  `{jab:"args",fn}` / legacy-fn shape as build(), or null when absent / unloadable.
function resolveVerb(w, startDir, requireFn) {
  const f = verbFile(w, startDir);
  if (f === null) return null;
  const req = requireFn || require;
  let mod;
  try { mod = req(f); } catch (e) { return null; }   // JS-074: a load error → absent
  const c = convention(mod);
  if (c == null) return null;
  return c.how === "args" ? { jab: "args", fn: c.fn } : c.fn;
}

module.exports = { build: build, convention: convention,
                   verbFile: verbFile, resolveVerb: resolveVerb };
