//  ci.js — CI-004: the extension entry over shared/ci.js.  A bareword routes to
//  the verb loop now, so the entry spelling is the `.js` one (jab resolves it by
//  its own upward jsrc/-scan).  Three modes, all arg-blind — they act on the
//  tree the cwd is in, which is what [CI-001]'s fleet will run inside a VM:
//    jab ci.js          print the detected default build+test command line
//    jab ci.js run      start it in the background, print the log path
//    jab ci.js status   print the verdict row (green/red, exit code, log path)
//  "nothing detected" is the DISTINCT result: a plain-words line and a throw,
//  which the jab edge maps to a non-zero exit ([BE-002]: no process.exit).
"use strict";

const CI = require("./shared/ci.js");
const discover = require("./core/discover.js");

const args = (typeof process !== "undefined" && process.argv || []).slice(2);
const mode = args.length ? String(args[0]) : "detect";

let wt = null;
try { wt = discover.treeAt(io.cwd()).wt; } catch (e) { wt = io.cwd(); }

if (mode === "status") {
  const r = CI.row(wt);
  console.log(r ? CI.badge(r) + "  " + (r.cmd || "") : "ci: nothing has run in this tree");
} else if (mode === "run") {
  const r = CI.run(wt);
  console.log(r.message);
  if (!r.started) throw "ci: nothing started";
} else {
  const d = CI.detect(wt);
  if (!d) {
    console.error("ci: no build or test command found in this tree");
    throw "ci: nothing detected";
  }
  console.log(d.cmd);
}
