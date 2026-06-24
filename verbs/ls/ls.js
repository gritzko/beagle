//  verbs/ls/ls.js — the `ls:` / `lsr:` worktree-listing view (JAB-018/019).
//  ONE handler, the VERB is the parameter: `ls` lists ONE directory as ONE
//  hunk; `lsr` is the SAME listing plus a fan-out — it enqueues an `lsr:<child>`
//  row per immediate subdir AND per mounted submodule, so the resident loop's
//  job queue (core/job.js) drives the recursion BFS, ONE HUNK PER DIRECTORY,
//  crossing store boundaries into submodules.  verbs/lsr/lsr.js is a one-line
//  re-export of this module — same code, `row.verb` selects recurse.
//
//  libabc+libdog ONLY (be.find / wtlog / store / classify): NO dog spawned, NO
//  sniff, NO /proc.  Each per-directory hunk is built from classify.js's listing
//  mode (eq rows + move-dst split) filtered to the scope dir's IMMEDIATE
//  entries: each immediate file is its status row, each immediate subdir / mount
//  collapses to ONE `dir <name>/` row.  Entry names render RELATIVE to the scope
//  dir; the banner names the scope RELATIVE to the top wt (`ls:`, `ls:sub/`,
//  `lsr:chsub/lib/`).  A scope INSIDE a submodule re-discovers that shard via
//  be.find, so `jab ls <path-in-sub>/` lists the sub directly.
"use strict";

const be       = require("../../core/discover.js");
const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const classify = require("../../shared/classify.js");
const join     = require("../../shared/util/path.js").join;

//  Strip trailing slashes from an absolute dir (keep a lone "/").
function noSlash(p) { while (p.length > 1 && p.endsWith("/")) p = p.slice(0, -1); return p; }

//  A directory row carries the NEWEST mtime of anything under it (computed from
//  the entry rows below), so every row has a meaningful date — no blank-date
//  irregularity (native ls: leaves dir dates blank).  `dirTs` is the FALLBACK
//  for a dir with no listed content (an empty mount): the dir's own mtime, 0n
//  if it vanished / can't be stat'd.
function dirTs(p) { try { return io.lstat(p).mtime || 0n; } catch (e) { return 0n; } }

//  `abs` relative to `base`, in DIR form (trailing "/"); "" when equal or not
//  under base.  Drives both the classify scope prefix (base = owning repo wt)
//  and the banner prefix (base = top wt).
function relDir(base, abs) {
  base = noSlash(base); abs = noSlash(abs);
  if (abs === base) return "";
  const pfx = base + "/";
  return abs.indexOf(pfx) === 0 ? abs.slice(pfx.length) + "/" : "";
}

//  Classify a repo ONCE per run (memoised on ctx) in listing mode — the whole
//  wt scan is reused across every directory hunk of that repo (and every sub).
function classifyRepo(ctx, repo) {
  const cache = ctx._lsCache || (ctx._lsCache = {});
  if (cache[repo.wt]) return cache[repo.wt];
  const log = wtlog.open(repo);
  const k   = store.open(repo.storePath, repo.project);
  return (cache[repo.wt] = classify.classify(repo, log, k, { listing: true }));
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
  const res      = classifyRepo(ctx, repo);

  //  Build the IMMEDIATE-entry set under the scope: a classify row is either an
  //  immediate file (no further `/`) or lives under an immediate subdir (collapse
  //  to ONE dir row).  A gitlink (mount) under the scope is a `dir` row too — its
  //  internals are a separate shard, listed only when lsr recurses into it.
  const files = {};   // name -> { verb, ts, text }
  const dirs  = {};   // seg  -> newest ts (BigInt) of content under it
  function bumpDir(seg, ts) { if (dirs[seg] === undefined || (ts && ts > dirs[seg])) dirs[seg] = ts || 0n; }

  for (const r of res.rows) {
    if (scopePfx && r.path.indexOf(scopePfx) !== 0) continue;
    const rel = r.path.slice(scopePfx.length);
    const slash = rel.indexOf("/");
    if (slash >= 0) { bumpDir(rel.slice(0, slash), r.ts); continue; }
    let text = rel;
    if (r.bucket === "mov" && r.dst) {
      const d = r.dst.indexOf(scopePfx) === 0 ? r.dst.slice(scopePfx.length) : r.dst;
      text = rel + " -> " + d;
    }
    files[rel] = { verb: r.bucket, ts: r.ts, text: text };
  }
  for (const gl of res.gitlinks || []) {
    if (scopePfx && gl.path.indexOf(scopePfx) !== 0) continue;
    const rel = gl.path.slice(scopePfx.length);
    const slash = rel.indexOf("/");
    const seg = slash < 0 ? rel : rel.slice(0, slash);
    //  A mount (or a dir holding only the mount) has no content rows here — fall
    //  back to the dir's own mtime so it still carries a date.
    if (dirs[seg] === undefined) dirs[seg] = dirTs(join(absScope, seg));
  }

  //  Merge into ONE lex-ordered entry list.  Sort key: a file by its name, a dir
  //  by `<seg>/` — so a file `deep.txt` sorts before a dir `deep/`, exactly as
  //  native ls: orders the full paths.
  const entries = [];
  for (const name in files) entries.push({ key: name,      dir: false, name: name, f: files[name] });
  for (const seg  in dirs)  entries.push({ key: seg + "/", dir: true,  name: seg, ts: dirs[seg] });
  entries.sort(function (a, b) { return a.key < b.key ? -1 : a.key > b.key ? 1 : 0; });

  if (out) {
    out.raw(banner);
    for (const e of entries)
      if (e.dir) out.row(e.name + "/", "dir", e.ts);
      else       out.row(e.f.text, e.f.verb, e.f.ts);
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
