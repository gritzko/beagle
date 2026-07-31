//  stats.js — CFOLD-001 object-read / weave-fold counters, the JS twin of the
//  BLAME-001 `GRAF_BLAME_STATS` hook.  OFF unless `JAB_STATS` is set, so the
//  product path pays one boolean; `core/loop.js` prints ONE line to stderr at
//  the plain edge (`stats: obj=… commit=… tree=… blob=… fold=… merge=…`), which
//  the CFOLD-001 repro case parses to assert the work is O(changes).
"use strict";

const ON = !!(typeof io !== "undefined" && io.getenv && io.getenv("JAB_STATS"));
const counts = { obj: 0, commit: 0, tree: 0, blob: 0, tag: 0, fold: 0, merge: 0 };

//  bump(key): count one event (no-op when the hook is off).
function bump(key) { if (ON) counts[key] = (counts[key] || 0) + 1; }

//  line(): the one-line report ("" when off).
function line() {
  if (!ON) return "";
  const ks = ["obj", "commit", "tree", "blob", "fold", "merge"];
  let s = "stats:";
  for (const k of ks) s += " " + k + "=" + (counts[k] || 0);
  return s + "\n";
}

module.exports = { ON: ON, bump: bump, counts: counts, line: line };
