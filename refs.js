//  refs.js — smoke extension (JS-029), JSQUE-002 handler form.  Reports the
//  repo's current branch + baseline sha.  Converted from a file-scope `main();`
//  one-shot to `module.exports = handle(row, ctx)`: args come from the ROW (the
//  worktree path in row.uri), not process.argv/io.cwd; output goes via ctx.out
//  (a no-op stub today; JSQUE-005's emit sink drops in).  No process.exit.
"use strict";

//  Sibling libs resolve via __dirname (require.cpp passes the module's own dir),
//  NOT process.argv[1] — under the resident loop argv[1] is loop.js, so the old
//  `here = process.argv[1]` idiom would scan the wrong dir (JSQUE-002).
const be = require(__dirname + "/lib/be.js");
const wtlog = require(__dirname + "/lib/wtlog.js");

//  handle(row, ctx): row.uri is the worktree path to inspect (empty -> the
//  loop's cwd via be.find()).  A trivial leaf verb: no fan-out, returns nothing.
module.exports = function handle(row, ctx) {
  const wt = (row && row.uri) ? row.uri : undefined;
  const repo = (ctx && ctx.repo) ? ctx.repo : be.find(wt);
  const log = wtlog.open(repo);

  const cur = log.curTip();
  const base = log.baselineTip();
  const bnd = log.boundaries();
  const branch = cur.branch || "trunk";   // trunk when no `?branch` row

  io.log("project:  " + (repo.project || "(unnamed)") + "\n");
  io.log("wt:       " + repo.wt + "\n");
  io.log("store:    " + repo.storePath + "\n");
  io.log("be:       " + repo.bePath + "\n");
  io.log("branch:   ?" + branch + "\n");
  io.log("cur:      " + (cur.sha || "(none)") + "\n");
  io.log("baseline: " + (base.sha || "(none)") + "\n");
  io.log("rows:     " + log.rows.length + "\n");
  io.log("boundary: pd="    + (bnd.pd    == null ? "-" : ron.encode(bnd.pd))
                  + " patch=" + (bnd.patch == null ? "-" : ron.encode(bnd.patch))
                  + "\n");
};
