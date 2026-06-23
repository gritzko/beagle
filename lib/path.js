//  path.js — POSIX path helpers shared by bin/*.js (JS-043).  Pure JS, no
//  JABC calls — just string math over `/`-separated paths.  Consolidates
//  the join/dirname/basename copies that were defined ad-hoc in be.js,
//  ignore.js, checkout.js, keeper.js, classify.js, subs.js, ingest.js and
//  get.js, with ONE agreed dirname semantics.
//
//  dirname semantics (the be.js/ignore.js form, kept verbatim):
//    dirname("/")        → "/"      (root is its own parent)
//    dirname("")         → ""       (empty stays empty)
//    dirname("foo")      → "."      (a slashless name → cwd)
//    dirname("/foo")     → "/"
//    dirname("a/b")      → "a"
//    dirname("/a/b")     → "/a"
//  This differs from the old checkout.js/keeper.js form ONLY for inputs
//  with no slash or a single leading slash (they returned ""/"/").  All
//  three former call sites — be.js's walk-up, ignore.js's walk-up, and
//  get.js's srcBe strip — only ever pass ABSOLUTE paths (a leading `/`
//  plus an interior `/`), so they hit the identical `slice(0, i)` branch
//  and behave the same under this unified rule.  The "." / "/" edge cases
//  are reached only by the defensive break-guards (`up === "."` etc.),
//  which this form satisfies.

"use strict";

function join(dir, name) {
  return dir === "/" ? "/" + name : dir + "/" + name;
}

function dirname(p) {
  if (p === "/" || p === "") return p;
  const i = p.lastIndexOf("/");
  if (i < 0) return ".";
  return i === 0 ? "/" : p.slice(0, i);
}

function basename(p) {
  const i = p.lastIndexOf("/");
  return i < 0 ? p : p.slice(i + 1);
}

module.exports = { join: join, dirname: dirname, basename: basename };
