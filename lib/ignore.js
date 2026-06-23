//  ignore.js — hierarchical `.gitignore` matcher (JS-031).  Pure JS over
//  io.stat/io.mmap + io.getenv; no C, no dog.  Mirrors dog/git/IGNO.c
//  (IGNOLoad / IGNOMatch / IGNOGlob / TryMatch) so `bin/status.js`'s
//  wt-scan skips exactly the paths native `be status` skips.
//
//  load(wtRoot) → matcher with match(relPath, isDir) → bool.
//    Anchors at wtRoot, walks UP to $HOME (or `/`), stacking every
//    `.gitignore` found (set[0] deepest).  A nearer/deeper file
//    overrides a shallower one; `!` negation honored.  `.git` and `.be`
//    path segments are ALWAYS ignored, with or without a `.gitignore`.
//
//  Gitignore rules implemented:
//    - blank / `#` lines skipped
//    - trailing `/` → directory-only
//    - leading `/` → anchored to the file's dir
//    - leading `!` → negation
//    - `*` matches non-`/`, `**` matches across `/`, `?` matches one char
//    - a pattern with no `/` (unanchored) matches the basename anywhere
//    - a `dir/` match on any parent prefix ignores everything beneath

"use strict";

const pathlib = require("./path.js");
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

//  Gitignore glob (mirror IGNOGlob): * non-/, ** across /, ? one char.
function glob(pat, str) {
  //  iterative-with-recursion on `*`/`**`, exactly like the C.
  let pi = 0, si = 0;
  while (pi < pat.length && si < str.length) {
    const pc = pat[pi];
    if (pc === "*") {
      const dbl = pat[pi + 1] === "*";
      if (dbl) {
        let np = pi + 2;
        if (pat[np] === "/") np++;
        const subPat = pat.slice(np);
        //  ** matches everything incl. '/': try every suffix.
        for (let t = si; t <= str.length; t++) {
          if (glob(subPat, str.slice(t))) return true;
        }
        return false;
      }
      const subPat = pat.slice(pi + 1);
      //  * matches anything except '/': try suffixes, stop past a '/'.
      for (let t = si; t <= str.length; t++) {
        if (glob(subPat, str.slice(t))) return true;
        if (str[t] === "/") break;
      }
      return false;
    }
    if (pc === "?") {
      if (str[si] === "/") return false;
      pi++; si++; continue;
    }
    if (pc !== str[si]) return false;
    pi++; si++;
  }
  while (pi < pat.length && pat[pi] === "*") pi++;
  return pi === pat.length && si === str.length;
}

//  Parse one .gitignore file's text into pattern records.
function parseSet(text) {
  const pats = [];
  for (let line of text.split("\n")) {
    //  trailing CR
    if (line.endsWith("\r")) line = line.slice(0, -1);
    //  strip trailing whitespace (git keeps escaped trailing space; we
    //  don't model `\ ` — rare; toy repos don't use it).
    line = line.replace(/\s+$/, "");
    if (line === "" || line[0] === "#") continue;
    let negated = false, anchored = false, dirOnly = false;
    if (line[0] === "!") { negated = true; line = line.slice(1); }
    if (line.endsWith("/")) { dirOnly = true; line = line.slice(0, -1); }
    if (line[0] === "/") { anchored = true; line = line.slice(1); }
    const hasSlash = line.indexOf("/") >= 0;
    if (line === "") continue;
    pats.push({ pattern: line, negated: negated, anchored: anchored,
                dirOnly: dirOnly, hasSlash: hasSlash });
  }
  return pats;
}

//  Match one pattern against a path (mirror TryMatch).  `path` is the
//  path relative to THIS file's dir (i.e. prefix already applied).
function tryMatch(pat, path, isDir) {
  if (pat.dirOnly && !isDir) return false;
  const mp = pat.pattern;   // leading / already stripped at parse
  if (mp === "") return false;
  if (!pat.hasSlash && !pat.anchored) {
    return glob(mp, basename(path));
  }
  //  has slash or anchored — match against full (already prefixed) path.
  let mpath = path;
  if (mpath[0] === "/") mpath = mpath.slice(1);
  if (pat.anchored) return glob(mp, mpath);
  //  unanchored with slash: try matching at each directory level.
  let tp = mpath;
  while (tp.length) {
    if (glob(mp, tp)) return true;
    const i = tp.indexOf("/");
    if (i < 0) break;
    tp = tp.slice(i + 1);
  }
  return false;
}

//  Decide one set for `path` (-1 none, 0 negated/un-ignored, 1 ignore).
function setDecide(pats, path, isDir) {
  let decision = -1;
  for (const p of pats) {
    if (tryMatch(p, path, isDir)) decision = p.negated ? 0 : 1;
  }
  return decision;
}

//  Set decide + the dir-prefix rule: a `dir/` match on any parent
//  prefix ignores everything beneath (definitive).
function setDecideDeep(pats, path, isDir) {
  const d = setDecide(pats, path, isDir);
  for (let i = 0; i < path.length; i++) {
    if (path[i] !== "/") continue;
    if (setDecide(pats, path.slice(0, i), true) === 1) return 1;
  }
  return d;
}

//  `.git` / `.be` / `..be.idx` segments are always meta (mirror
//  igno_is_meta).
function isMeta(rel) {
  if (!rel) return false;
  for (const seg of rel.split("/")) {
    if (seg === ".git" || seg === ".be" || seg === "..be.idx") return true;
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

//  Build the anchor→$HOME stack of { pats, prefix }.  prefix = the
//  anchor path relative to this file's dir (empty for the deepest).
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
    const text = readFileText(join(cur, ".gitignore"));
    sets.push({ pats: text == null ? [] : parseSet(text), prefix: prefix });

    if (home && cur === home) break;
    if (cur.length <= 1) break;     // "/" reached
    const up = dirname(cur);
    if (up === cur) break;
    cur = up;
  }

  return {
    //  match(rel, isDir): rel is relative to the wt root.  Walk the
    //  chain shallow→deep; a definite (>=0) decision from a deeper set
    //  overrides.  Meta (.git/.be) always ignored.
    match: function (rel, isDir) {
      if (!rel) return false;
      if (isMeta(rel)) return true;
      let decided = -1;
      //  shallow→deep so deeper overrides: iterate from the last
      //  (shallowest) set down to set[0] (deepest), letting a deeper
      //  set's definite result win.
      for (let i = sets.length - 1; i >= 0; i--) {
        const s = sets[i];
        const p = s.prefix ? (s.prefix + "/" + rel) : rel;
        const d = setDecideDeep(s.pats, p, !!isDir);
        if (d >= 0) decided = d;
      }
      return decided === 1;
    },
    _sets: sets
  };
}

module.exports = { load: load, glob: glob };
