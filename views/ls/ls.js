//  verbs/ls/ls.js — the `ls:` / `lsr:` worktree-listing view (JAB-018/019).
//  ONE handler, the VERB is the parameter: `ls` lists ONE directory as ONE
//  hunk; `lsr` is the SAME listing plus a fan-out — it enqueues an `lsr:<child>`
//  row per immediate subdir AND per mounted submodule, so the resident loop's
//  job queue (core/job.js) drives the recursion BFS, ONE HUNK PER DIRECTORY,
//  crossing store boundaries into submodules.  verbs/lsr/lsr.js is a one-line
//  re-export of this module — same code, `row.verb` selects recurse.
//
//  libabc+libdog ONLY (be.find / wtlog / store / classify): NO dog spawned, NO
//  sniff, NO /proc.  Each per-directory hunk is built from classify.classifyDir
//  — the O(dir) listing of the scope's IMMEDIATE entries: each immediate file
//  is its status row, each immediate subdir / mount is ONE BLANK-DATED `dir
//  <name>/` row (native ls: dates no dir row, so nothing under it is scanned —
//  the whole reason ls: now costs O(dir), not O(repo)).  Entry names render
//  RELATIVE to the scope dir; the banner names the scope RELATIVE to the top wt
//  (`ls:`, `ls:sub/`, `lsr:chsub/lib/`).  A scope INSIDE a submodule
//  re-discovers that shard via be.find, so `jab ls <path-in-sub>/` lists it.
"use strict";

const be       = require("../../core/discover.js");
const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const classify = require("../../shared/classify.js");
const join     = require("../../shared/util/path.js").join;

//  Strip trailing slashes from an absolute dir (keep a lone "/").
function noSlash(p) { while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1); return p; }

//  `abs` relative to `base`, in DIR form (trailing "/"); "" when equal or not
//  under base.  Drives both the classifyDir scope prefix (base = owning repo
//  wt) and the banner prefix (base = top wt).
function relDir(base, abs) {
  base = noSlash(base); abs = noSlash(abs);
  if (abs === base) return "";
  const pfx = base + "/";
  return abs.indexOf(pfx) === 0 ? abs.slice(pfx.length) + "/" : "";
}

//  The store + wtlog readers for a repo, memoised on ctx so an `lsr` run opens
//  each shard (and builds its pack index) ONCE, reused across every per-dir
//  hunk of that repo — the cross-dir saving the old whole-repo classify cache
//  gave, without the whole-repo classify.
function repoReaders(ctx, repo) {
  const cache = ctx._lsReaders || (ctx._lsReaders = {});
  if (cache[repo.wt]) return cache[repo.wt];
  return (cache[repo.wt] = { log: wtlog.open(repo),
                             k: store.open(repo.storePath, repo.project) });
}

module.exports = function handle(row, ctx) {
  const recurse = row.verb === "lsr";
  const out     = ctx && ctx.out;
  const topWt   = (ctx && ctx.repo && ctx.repo.wt) || ".";

  //  Resolve the scope to an ABSOLUTE dir.  The seed row carries "." (the cwd
  //  placeholder → the top wt), a wt-relative path (`sub/`), or — for an
  //  enqueued child / sub-wt — an absolute path.
  let absScope;
  if (!row.uri || row.uri === ".") absScope = topWt;
  else if (row.uri[0] === "/")     absScope = row.uri;
  else                             absScope = join(topWt, row.uri);
  absScope = noSlash(absScope);

  //  The OWNING repo of the scope dir — be.find re-discovers a submodule's
  //  shard when the scope is inside a mount (the cross-store seam), else the
  //  top repo.  A path anchoring nowhere falls back to the top repo.
  let repo;
  try { repo = be.find(absScope); } catch (e) { repo = (ctx && ctx.repo) || null; }
  if (!repo) return;

  const scopePfx = relDir(repo.wt, absScope);               // rel to OWNING wt
  const banner   = row.verb + ":" + relDir(topWt, absScope); // rel to TOP wt
  const rd        = repoReaders(ctx, repo);
  const res       = classify.classifyDir(repo, rd.log, rd.k, scopePfx);

  //  Merge files + dirs into ONE lex-ordered entry list.  Sort key: a file by
  //  its name, a dir by `<name>/` — so a file `deep.txt` sorts before a dir
  //  `deep/`, exactly as native ls: orders the full paths.  A dir row's date is
  //  BLANK (ts 0n): native ls: dates no directory, and a recursive newest-mtime
  //  was never asked for (computing it was the whole O(repo) cost).
  const entries = [];
  for (const f of res.files) {
    const text = f.bucket === "mov" && f.dst ? f.name + " -> " + f.dst : f.name;
    entries.push({ key: f.name, dir: false, text: text, verb: f.bucket, ts: f.ts });
  }
  for (const name of res.dirs)
    entries.push({ key: name + "/", dir: true, name: name });
  entries.sort(function (a, b) { return a.key < b.key ? -1 : a.key > b.key ? 1 : 0; });

  if (out) {
    out.raw(banner);
    for (const e of entries)
      if (e.dir) out.row(e.name + "/", "dir", 0n);
      else       out.row(e.text, e.verb, e.ts);
  }

  //  lsr: fan out — one `lsr:<child>` row per immediate subdir / mount, in lex
  //  order (BFS via the FIFO queue).  ls: is a leaf (no enqueue).
  if (recurse) {
    const enqueue = [];
    for (const e of entries) if (e.dir)
      enqueue.push({ verb: row.verb, uri: join(absScope, e.name) });
    if (enqueue.length) return { enqueue: enqueue };
  }
};
