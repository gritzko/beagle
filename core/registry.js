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

//  JS-074: the jab loader (jab/require.cpp resolveJsrc/resolve) reports a
//  genuine NOT-FOUND as `require: cannot find '<spec>' from '<dir>'`.  ONLY that
//  string is a fall-through-to-the-other-tree / leave-absent signal; ANY other
//  caught error (a SYNTAX error, a throwing top-level, a bad nested require) is
//  a module that EXISTS but failed to LOAD and MUST surface, not be masked as an
//  unresolved verb.  `e` may be the loader's plain string or a thrown Error.
function _notFound(e) {
  const m = (e && e.message != null) ? e.message : e;
  return ("" + m).indexOf("cannot find") >= 0;
}

//  GIT-016: verbs register by FILE — a bareword resolves to verbs/<verb>/<verb>.js
//  here (no explicit list); `head` (verbs/head/head.js) is picked up automatically.
//  build(verbs, requireFn): map each distinct verb name to its handler.
//  `requireFn` is the shard-relative require of the CALLING module (so the
//  upward jsrc/-scan finds the shard nearest loop.js, not cwd); default the
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
    //  verbs/ (the names are disjoint); the shard-relative scan (require.cpp)
    //  finds the shard nearest the requirer.
    try { mod = req("views/" + verb + "/" + verb + ".js"); }
    catch (e) {
      //  JS-074: the views/ module was FOUND but threw at load -> surface it;
      //  do NOT fall through to verbs/ (a real not-found does fall through).
      if (!_notFound(e)) throw e;
      try { mod = req("verbs/" + verb + "/" + verb + ".js"); }
      //  JS-074: verbs/ not-found -> genuinely absent (null); any other error
      //  is a found-but-broken module -> surface it.
      catch (e2) { if (!_notFound(e2)) throw e2; table[verb] = null; continue; }
    }
    const c = convention(mod);
    if (c == null) { table[verb] = null; continue; }
    table[verb] = c.how === "args" ? { jab: "args", fn: c.fn } : c.fn;
  }
  return table;
}

//  BE-029: locate a verb's handler file by CLIMBING from `startDir` (default
//  cwd) — at each ancestor try `<dir>/jsrc/verbs/<w>/<w>.js` then `.../views/…`,
//  ceiling at $HOME, first hit wins.  This mirrors jab's own upward jsrc/-scan
//  (require.cpp resolveJsrc) but anchors on CWD, so a nested jsrc/ shard
//  supplies its OWN verbs while core/shared still load from the parent shard
//  the loop launched from.  Returns the abs path or null.  Distinct from
//  JAB-030's `_here` probe, which sees one root only.
//  BE-041: an optional `dirs` narrows the scan to one tree — ["verbs"] probes
//  the MUTATING verbs only (the pager's act-button gate), default both.
function verbFile(w, startDir, dirs) {
  let dir = startDir || io.cwd();
  const home = io.getenv("HOME");
  for (;;) {
    for (const d of dirs || ["verbs", "views"]) {
      const p = dir + "/jsrc/" + d + "/" + w + "/" + w + ".js";
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
  if (f === null) return null;                 // genuinely no such verb -> absent
  const req = requireFn || require;
  let mod;
  //  JS-074: verbFile ALREADY proved the file exists (io.stat), so a throw from
  //  req(f) is NEVER a not-found — it is a genuine LOAD failure (syntax error,
  //  throwing top-level, bad nested require).  Re-throw it wrapped, naming the
  //  verb + file, so the author sees the real error instead of the phantom
  //  "verb '<w>' has no plain-args handler" the loop raised when this returned
  //  null (the 2026-07-19 live hit: a cold `get.js` throw masked as missing).
  try { mod = req(f); }
  catch (e) {
    throw "cannot load verb '" + w + "' (" + f + "): " +
          ("" + ((e && e.message != null) ? e.message : e));
  }
  const c = convention(mod);
  //  JS-074: the module LOADED but exports an unrecognized shape.  It sits
  //  exactly where a verb handler lives, so this is an authoring bug, not an
  //  absent verb — say so loudly rather than let it decay into the same phantom
  //  "no plain-args handler" refusal downstream.
  if (c == null)
    throw "verb '" + w + "' (" + f + ") exports no plain-args handler " +
          "(bad module.exports shape)";
  return c.how === "args" ? { jab: "args", fn: c.fn } : c.fn;
}

module.exports = { build: build, convention: convention,
                   verbFile: verbFile, resolveVerb: resolveVerb };
