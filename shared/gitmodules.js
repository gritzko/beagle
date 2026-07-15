//  PUT-004: ONE shared `.gitmodules` reader — consolidates the three
//  copy-pasted parsers (core/recurse.js::gitmodulesOrder, views/status/
//  status.js::gitmodulesOrder, shared/submount.js::gitmodulesUrl).  A minimal
//  git-config reader over the wt copy of `.gitmodules`; absent/unreadable → []/"".
//  Parsing mirrors the old readers EXACTLY (comment strip, submodule-section
//  gate, key=val, dedup-by-path keep-first).

"use strict";

const safeRel = require("./util/path.js").safeRel;   // BE-026: source gate
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const wtpath = require("../core/discover.js").wtpath;

//  parse(wtRoot) → [{ name, path, url }] in DECLARATION order, one entry per
//  `[submodule "<name>"]` block that declared a `path`; deduped by `path`
//  keeping the FIRST.  Absent/unreadable `<wtRoot>/.gitmodules` → [].
function parse(wtRoot) {
  const p = wtpath(wtRoot, ".gitmodules");
  let text;
  try { text = utf8.Decode(io.mmap(p, "r").data()); } catch (e) { return []; }
  const out = [], seen = {};
  let inSub = false, name = "", curPath = "", curUrl = "";
  //  Close the open block: emit it iff it declared a path not yet seen.
  //  BE-026: DROP-and-continue any block whose `path` is not a safe in-tree
  //  relative (absolute/`..`/reserved) — a path-traversal escape refused at
  //  the source, so a poisoned `.gitmodules` entry never reaches stat/be.treeAt.
  function flush() {
    if (inSub && curPath && safeRel(curPath) && !seen[curPath]) {
      seen[curPath] = true;
      out.push({ name: name, path: curPath, url: curUrl });
    }
  }
  for (let line of text.split("\n")) {
    line = line.replace(/[#;].*$/, "").trim();      // strip comments + ws
    if (!line) continue;
    if (line[0] === "[") {                           // section header
      flush();
      inSub = /^\[\s*submodule\b/i.test(line);
      const m = line.match(/^\[\s*submodule\s+"([^"]*)"/i);
      name = m ? m[1] : "";
      curPath = ""; curUrl = "";
      continue;
    }
    if (!inSub) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    const key = line.slice(0, eq).trim().toLowerCase();
    const val = line.slice(eq + 1).trim();
    if (key === "path") curPath = val;
    else if (key === "url") curUrl = val;
  }
  flush();                                           // close final block at EOF
  return out;
}

//  paths(wtRoot) → declared submodule `path` values in declaration order
//  (the gitmodulesOrder twin).
function paths(wtRoot) {
  return parse(wtRoot).map(function (s) { return s.path; });
}

//  urlOf(wtRoot, path) → the `url` of the first block whose `path` matches,
//  else "" (the gitmodulesUrl twin).
function urlOf(wtRoot, path) {
  const subs = parse(wtRoot);
  for (const s of subs) if (s.path === path) return s.url;
  return "";
}

module.exports = { parse: parse, paths: paths, urlOf: urlOf };
