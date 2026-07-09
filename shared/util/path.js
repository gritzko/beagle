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

//  safeRel(rel) — the ONE worktree-confinement guard (JS-065).  YES iff `rel`
//  is a relative in-tree path: no absolute leading `/`, no NUL, and every
//  `/`-split segment is a real name (not ""/"."/".."/".git"/".be"/"..be.idx").
//  Parity with keeper/WALK name validation; rejects the path-traversal escape.
function safeRel(rel) {
  if (typeof rel !== "string" || rel === "" || rel[0] === "/") return false;
  if (rel.indexOf("\0") >= 0) return false;
  for (const seg of rel.split("/")) {
    if (seg === "" || seg === "." || seg === "..") return false;
    if (seg === ".git" || seg === ".be" || seg === "..be.idx") return false;
  }
  return true;
}

//  --- BE-011: segment-based path calculator (split · resolveInTree · merge) ---
//  In-tree paths are computed over SEGMENT ARRAYS, never by hand-rolled
//  `a + "/" + b` string math.  Paths here are worktree-RELATIVE (no leading
//  "/"); the absolute root (the hive cell, `<srcRoot>/name`) is prepended by the caller.

//  split(p) — a "/"-path to its non-empty segments; leading/trailing/doubled
//  slashes collapse away ("a//b/" → ["a","b"], "/a" → ["a"], "" → []).
function split(p) {
  if (typeof p !== "string") return [];
  const out = [];
  for (const seg of p.split("/")) if (seg !== "") out.push(seg);
  return out;
}

//  merge(segs) — segments back to a "/"-joined relative path ("" for none).
//  The inverse of split(); the ONE place in-tree path text is (re)composed.
function merge(segs) {
  return segs.join("/");
}

//  resolveInTree(base, rel) — resolve the relative path `rel` against the
//  worktree-relative `base`, applying "." (skip) and ".." (pop one segment)
//  over segment arrays.  THROWS "NAVESCAPE" when a ".." would pop above the
//  worktree root (segs empty) — the path leads OUT of the tree, a refusal not
//  a silent clamp.  Returns the normalized in-tree path (no "."/".."/"" segs).
function resolveInTree(base, rel) {
  const segs = split(base);
  for (const seg of split(rel)) {
    if (seg === ".") continue;
    if (seg === "..") {
      if (segs.length === 0) throw "NAVESCAPE: path escapes the worktree";
      segs.pop();
      continue;
    }
    segs.push(seg);
  }
  return merge(segs);
}

//  BE-011: wtJoin(wtRoot, rel) — the ONE way to compose an ABSOLUTE worktree file
//  path from a wt root + an (untrusted) in-tree `rel`.  resolveInTree normalises
//  `.`/`..` and THROWS "NAVESCAPE" on any climb above the tree root; "" → the root
//  itself.  Every site that OPENS a wt file routes through this, not join(wt, rel).
//  (Write leaves additionally keep safeRel's .git/.be reserved-name policy on top.)
function wtJoin(wtRoot, rel) {
  const sub = resolveInTree("", rel || "");
  return sub ? join(wtRoot, sub) : wtRoot;
}

module.exports = { join: join, dirname: dirname, basename: basename,
                   safeRel: safeRel,
                   split: split, merge: merge, resolveInTree: resolveInTree,
                   wtJoin: wtJoin };
