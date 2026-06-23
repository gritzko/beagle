//  refs.js — smoke extension (JS-029).  Prints the repo's current branch
//  + baseline sha for the worktree at cwd, using bin/lib/be.js (repo
//  discovery) + bin/lib/wtlog.js (wtlog reader).  Pure JS over JABC; an
//  oracle for `bin/refs.js` vs native `be status` / `be log:` headers.
//
//  Usage:  be refs            (run from inside a worktree)
//          jabc bin/refs.js   (same, the cwd-walk finds the .be)

"use strict";

//  The top-level script's `require` resolves vs cwd (require.cpp binds
//  `require` to "."), and a top-level script gets no `__dirname`.  So
//  derive this script's own dir from process.argv[1] and require the
//  sibling libs by ABSOLUTE path — works from any cwd (be forks jabc
//  with the caller's cwd; the .be-walk needs the real cwd).
const self = process.argv[1];
const here = self.slice(0, self.lastIndexOf("/"));
const be = require(here + "/lib/be.js");
const wtlog = require(here + "/lib/wtlog.js");

const repo = be.find();              // walk up from cwd to the .be anchor
const log = wtlog.open(repo);

const cur = log.curTip();
const base = log.baselineTip();
const bnd = log.boundaries();

//  Default branch label when the wtlog rows carry no `?branch` (trunk).
const branch = cur.branch || "trunk";

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
