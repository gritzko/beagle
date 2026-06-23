//  be.js — repo discovery (JS-029).  Pure JS over the JABC runtime
//  (io.cwd/getenv/stat/mmap + the URI binding); no C, no dog.  Mirrors
//  dog/HOME.c::home_walk_up + home_anchor_resolve.
//
//  find(cwd?) walks UP from `cwd` (default io.cwd()) to the first
//  ancestor anchoring a worktree — a `.be` that is either a FILE
//  (secondary wt: the file IS the wtlog) or a DIRECTORY containing a
//  `wtlog` (primary wt) — never escaping above $HOME.  Returns
//      { root, wt, bePath, storePath, project }
//  where
//    wt        = the anchor dir (where `.be` lives), the worktree root
//    bePath    = the on-disk wtlog path: <wt>/.be (secondary) or
//                <wt>/.be/wtlog (primary)
//    storePath = the store root, from row-0's anchor URI path
//                (DOGRepoFromBe: split on /.be/); == wt for a colocated
//                primary store with no redirect
//    project   = the store's Title, from row-0's `?/<title>/<branch>`
//                query (preferred) or the path-after-`.be` segment
//    root      = alias of storePath (the home `h->root`)

"use strict";

const pathlib = require("./path.js");
const ulog = require("./ulog.js");
const join = pathlib.join, dirname = pathlib.dirname;

const BE = ".be";
const WTLOG = "wtlog";

function statKind(p) {
  try { return io.stat(p).kind; } catch (e) { return undefined; }
}
function isFile(p) { return statKind(p) === "reg"; }
function isDir(p) { return statKind(p) === "dir"; }

//  DOGRepoFromBe: the store root is everything before the first `/.be/`
//  separator in a row-0 anchor URI path; falls back to stripping a
//  trailing `.be` (and trailing slashes) when no `/.be/` is present.
function repoFromBe(path) {
  let p = path;
  while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1);
  const i = p.indexOf("/" + BE + "/");
  if (i >= 0) return p.slice(0, i);
  if (p.endsWith("/" + BE)) p = p.slice(0, -(BE.length + 1));
  else if (p.endsWith(BE)) p = p.slice(0, -BE.length);
  while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1);
  return p;
}

//  DOGQueryProject: absolute `/<title>` or `/<title>/<branch>` → title;
//  a non-absolute or empty query → "".
function projectFromQuery(query) {
  if (!query || query[0] !== "/") return "";
  const rest = query.slice(1);
  const j = rest.indexOf("/");
  return j < 0 ? rest : rest.slice(0, j);
}

//  DOGProjectFromBe: the first path segment after `/.be/`, unless it is
//  itself `.be` (a doubled store dir) — then treat as elided.
function projectFromPath(path) {
  let p = path;
  while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1);
  const i = p.indexOf("/" + BE + "/");
  if (i < 0) return "";
  const seg = p.slice(i + BE.length + 2).split("/")[0] || "";
  return seg === BE ? "" : seg;
}

//  Read row 0 of a secondary wt's `.be` (which IS the wtlog) and return
//  its anchor URI string, or undefined.  A secondary anchor row 0 is a
//  `get`/`repo` row pointing at the shared store.
function row0Uri(bePath) {
  let uri;
  ulog.each(bePath, function (log) { if (uri === undefined) uri = log.uri; });
  return uri;
}

//  Resolve the anchor at `wt` (a dir holding `.be`) into store/project.
//  Primary (`.be` is a dir): store == wt; project from the dir's row-0
//  wtlog anchor when present, else single-shard scan is left to keeper.
//  Secondary (`.be` is a file): store + project from row 0's redirect.
function resolveAnchor(wt) {
  const be = join(wt, BE);
  const kind = statKind(be);
  let bePath, storePath, project = "";

  if (kind === "reg") {
    //  Secondary worktree: the `.be` file is the wtlog.
    bePath = be;
    const u = row0Uri(be);
    if (u) {
      const p = new URI(u);
      storePath = repoFromBe(p.path);
      project = projectFromQuery(p.query) || projectFromPath(p.path);
    }
    if (!storePath) storePath = wt;
  } else {
    //  Primary worktree: <wt>/.be/wtlog.
    bePath = join(be, WTLOG);
    storePath = wt;                          // colocated store == wt
    const u = isFile(bePath) ? row0Uri(bePath) : undefined;
    if (u) {
      const p = new URI(u);
      const sp = repoFromBe(p.path);
      if (sp) storePath = sp;
      project = projectFromQuery(p.query) || projectFromPath(p.path);
    }
  }
  return { root: storePath, wt: wt, bePath: bePath, storePath: storePath,
           project: project };
}

//  Walk up from `start` to the first ancestor that anchors a worktree:
//    * `.be` is a FILE                              → secondary wt
//    * `.be` is a DIR holding `wtlog`               → primary wt
//    * `.be` is a DIR that is shield-like (≤1 shard)→ fresh/single store
//  Stop at $HOME (after probing $HOME/.be); never ascend above it.
//  Throws when the walk reaches the top without an anchor.
function find(cwd) {
  let here = cwd || io.cwd();
  const home = io.getenv("HOME");

  for (;;) {
    const be = join(here, BE);
    const kind = statKind(be);
    if (kind !== undefined) {
      let isWt = (kind === "reg");
      if (kind === "dir") {
        if (isFile(join(be, WTLOG))) isWt = true;
        else if (shieldLike(be)) isWt = true;   // fresh / single-shard store
      }
      if (isWt) return resolveAnchor(here);
    }
    //  Stop at $HOME AFTER the probe (so $HOME/.be still counts).
    if (home && here === home) break;
    const up = dirname(here);
    if (up === here || up === "." ) break;       // reached /
    here = up;
  }
  throw "be.find: no .be worktree anchor from '" + (cwd || io.cwd()) + "'";
}

//  A `.be` dir is shield-like (a valid anchor) iff it has ≤1 immediate
//  non-dotted subdirectory — a fresh worktree shield or a single-project
//  store.  A multi-project store (>1 shard) is NOT a wt; keep walking.
//  Mirrors dog/HOME.c::home_dir_shieldlike / home_be_subdirs.
function shieldLike(beDir) {
  let subdirs = 0;
  try {
    io.readdir(beDir, function (name) {
      //  readdir marks dirs with a trailing '/'; dotted entries
      //  (".be" etc.) are never shards.
      if (name[name.length - 1] !== "/") return "more";
      const base = name.slice(0, -1);
      if (base === "" || base[0] === ".") return "more";
      if (++subdirs > 1) return "enough";
      return "more";
    });
  } catch (e) { return false; }              // dir vanished / unreadable
  return subdirs <= 1;
}

module.exports = { find: find,
                   //  exported for wtlog.js / tests
                   repoFromBe: repoFromBe,
                   projectFromQuery: projectFromQuery,
                   projectFromPath: projectFromPath };
