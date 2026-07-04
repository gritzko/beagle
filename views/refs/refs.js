//  refs.js — smoke extension (JS-029) → JAB-004 plain-args view.  Reports the
//  repo's current branch + baseline sha as ONE `refs:` hunk fed to be.sink (the
//  loop edge renders it to fd 1 in the active mode, byte-identical to the legacy
//  ctx.sink feed).  No positional arg — refs inspects be.repo (the worktree the
//  loop is pointed at); a bare `jab refs` still emits (the legacy seed's row).
"use strict";

//  Sibling libs resolve via __dirname (require.cpp passes the module's own dir),
//  NOT process.argv[1] — under the resident loop argv[1] is loop.js, so the old
//  `here = process.argv[1]` idiom would scan the wrong dir (JSQUE-002).
const wtlog = require(__dirname + "/../../shared/wtlog.js");
//  JAB-003: TRUE-hunk output via the shared columnar→hunk adapter (be.sink),
//  retiring the io.log fd-1 bypass for this view.
const hunkrows = require("../../shared/hunkrows.js");
const navlib   = require("../../shared/nav.js");   // URI-011: full-URI banner

//  JAB-004: emit ONE `refs:` report hunk for `repo` to `sink` (plain path).
function refsOne(repo, sink) {
  if (!repo || !sink) return;
  const log = wtlog.open(repo);
  const cur = log.curTip();
  const base = log.baselineTip();
  const bnd = log.boundaries();
  //  DIS-053: trunk/no-branch is the bare `?` sigil, never a literal `?trunk`
  //  — byte-match C `be head` (graf/LOG.c trunk label is `?`).
  const branch = cur.branch || "";

  //  JAB-003: each raw() line appends its own "\n"; done() flushes the hunk.
  const out = hunkrows(sink, navlib.navUri("refs", ""));   // URI-011: refs://name
  out.raw("project:  " + (repo.project || "(unnamed)"));
  out.raw("wt:       " + repo.wt);
  out.raw("store:    " + repo.storePath);
  out.raw("be:       " + repo.bePath);
  out.raw("branch:   ?" + branch);
  out.raw("cur:      " + (cur.sha || "(none)"));
  out.raw("baseline: " + (base.sha || "(none)"));
  out.raw("rows:     " + log.rows.length);
  out.raw("boundary: pd="    + (bnd.pd    == null ? "-" : ron.encode(bnd.pd))
                  + " patch=" + (bnd.patch == null ? "-" : ron.encode(bnd.patch)));
  out.done();
}

//  JAB-004: PLAIN verb (`.jab="args"`) — refs takes NO positional; be.repo is the
//  worktree, be.sink the target.
function refs() {
  const _be = (typeof be !== "undefined") ? be : null;
  refsOne(_be && _be.repo, _be && _be.sink);
}
refs.jab = "args";
module.exports = refs;
