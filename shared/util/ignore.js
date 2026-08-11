//  ignore.js — hierarchical `.gitignore` matcher (JS-031).  STATUS-020: the
//  per-file matching is `dog._igno_open/_match/_close` (one mmapped set per
//  `.gitignore`); JS keeps the parts C has no notion of — the CHAIN walk
//  (repo borders, the hive rule), the shallow→deep fold and the meta test.
//
//  load(wtRoot) → matcher with match(relPath, isDir) → bool, close() → undefined.
//    Anchors at wtRoot, walks UP opening every `.gitignore` found
//    (set[0] deepest).  A nearer/deeper file overrides a shallower one;
//    `!` negation honored.  `.git` and `.be` path segments are ALWAYS
//    ignored, with or without a `.gitignore`.  The walk stops at the repo
//    boundary, crossing it only into a parent that DECLARES this path a
//    sub (STATUS-018), and never above $HOME (or `/`).
//
//  A matcher OWNS its handles: every load() must be paired with exactly one
//  close() (idempotent), and match() after close() is an error.  The gitignore
//  rules themselves (blank/`#`, trailing `/`, leading `/`, `!`, `*`/`**`/`?`,
//  unanchored basename, the parent `dir/` rule) live in dog/git/IGNO.

"use strict";

const pathlib = require("./path.js");
const gitmodules = require("../gitmodules.js");
const join = pathlib.join, dirname = pathlib.dirname, basename = pathlib.basename;

//  Normalize: collapse `//`, strip a trailing `/` (keep root `/`).
function norm(p) {
  if (!p) return p;
  const parts = p.split("/");
  const out = [];
  for (const s of parts) { if (s !== "" && s !== ".") out.push(s); }
  const abs = p[0] === "/";
  let r = (abs ? "/" : "") + out.join("/");
  if (r === "") r = abs ? "/" : ".";
  return r;
}

//  `.git` / `.be` / `..be.idx` segments are always meta (mirror
//  igno_is_meta).  STATUS-020: stays in JS — the C leaves are per-FILE
//  scope and have no notion of repo meta.
function isMeta(rel) {
  if (!rel) return false;
  for (const seg of rel.split("/")) {
    if (seg === ".git" || seg === ".be" || seg === "..be.idx") return true;
  }
  return false;
}

function statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }

//  A repo root: carries a `.be` (wt store anchor) or a `.git`.
function isRepo(dir) {
  return statKind(join(dir, ".be")) !== undefined ||
         statKind(join(dir, ".git")) !== undefined;
}

//  STATUS-018 (the JS twin of STATUS-002): at a repo boundary the chain
//  crosses ONE way only — up into an enclosing repo that DECLARES this path
//  in its `.gitmodules` (a real sub, SUBS-045), so a parent's `.gitignore`
//  governs the sub's paths.  An enclosing repo that merely CONTAINS the wt
//  (journal's `work/` hive, BE-031) must never swallow it — test/status/hive.
function declaredSubOf(dir, home) {
  let rel = basename(dir), cur = dir, up = dirname(dir);
  for (let guard = 0; guard < 64 && up !== cur; guard++) {
    if (isRepo(up)) {
      const text = readFileText(join(up, ".gitmodules"));
      if (text == null) return false;
      const subs = gitmodules.parseText(text);
      for (const s of subs) if (s.path === rel) return true;
      return false;
    }
    if (home && up === home) return false;
    if (up.length <= 1) return false;             // "/" reached
    rel = basename(up) + "/" + rel;
    cur = up; up = dirname(up);
  }
  return false;
}

function readFileText(path) {
  try {
    //  io.mmap RO maps the whole file as DATA (FILEMapRO); .data() is
    //  the full byte view (no watermark fix-up needed, unlike abc.mmap).
    return utf8.Decode(io.mmap(path, "r").data());
  } catch (e) { return null; }
}

//  Build the anchor→$HOME stack of { h, prefix }.  prefix = the anchor path
//  relative to this file's dir (empty for the deepest).  STATUS-020: a level
//  with no readable `.gitignore` opens as 0 and is skipped outright.
function load(wtRoot) {
  const anchor = norm(wtRoot);
  let home = io.getenv("HOME");
  if (home) home = norm(home);

  const sets = [];
  let cur = anchor;
  for (let guard = 0; guard < 64; guard++) {
    //  prefix = anchor relative to cur.
    let prefix = "";
    if (anchor.length > cur.length && anchor.indexOf(cur) === 0) {
      let tail = anchor.slice(cur.length);
      while (tail[0] === "/") tail = tail.slice(1);
      prefix = tail;
    }
    const h = dog._igno_open(join(cur, ".gitignore"));
    if (h) sets.push({ h: h, prefix: prefix });

    //  a .gitignore has effect only INSIDE its own repo — EXCEPT that a sub
    //  is inside its parent: at a `.git`/`.be` boundary keep walking only
    //  when an enclosing repo declares this path a sub (STATUS-018).
    if (isRepo(cur) && !declaredSubOf(cur, home)) break;
    if (home && cur === home) break;
    if (cur.length <= 1) break;     // "/" reached
    const up = dirname(cur);
    if (up === cur) break;
    cur = up;
  }

  let closed = false;
  return {
    //  match(rel, isDir): rel is relative to the wt root.  Walk the
    //  chain shallow→deep; a definite (>=0) decision from a deeper set
    //  overrides.  Meta (.git/.be) always ignored.
    match: function (rel, isDir) {
      if (!rel) return false;
      if (isMeta(rel)) return true;
      if (closed) throw "ignore: this worktree's .gitignore matcher is already closed";
      let decided = -1;
      //  shallow→deep so deeper overrides: iterate from the last
      //  (shallowest) set down to set[0] (deepest), letting a deeper
      //  set's definite result win.
      for (let i = sets.length - 1; i >= 0; i--) {
        const s = sets[i];
        const p = s.prefix ? (s.prefix + "/" + rel) : rel;
        const d = dog._igno_match(s.h, p, !!isDir);
        if (d >= 0) decided = d;
      }
      return decided === 1;
    },
    //  STATUS-020: release every mmapped set.  Idempotent, like the leaf.
    close: function () {
      for (let i = 0; i < sets.length; i++) dog._igno_close(sets[i].h);
      sets.length = 0;
      closed = true;
    }
  };
}

module.exports = { load: load };
