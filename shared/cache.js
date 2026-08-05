//  cache.js — STATUS-019: the per-dir REV TREE, the universal fs change witness
//  (it replaces [BRO-043]'s per-repo value buckets).  ONE shared counter; every
//  fsw event increments it and stamps the event's dir AND every ANCESTOR dir
//  with it, so a consumer that remembers the rev it saw for a spot knows in one
//  integer compare whether anything under that spot moved.  THIS MODULE STORES
//  NO VIEW VALUES: each consumer keeps its own memo (status.js's rows, todo.js's
//  wtStat) and compares `rev`.
//
//  The rulings it encodes (gritzko 2026-08-02/04) — none of them negotiable:
//   1. The spot key is an ABSOLUTE DIR.  An event stamps that dir and its
//      ancestors; siblings and descendants keep their revs.
//   2. An OVERFLOW record (the kernel dropped events), a drain the Buf could not
//      hold (the burst is LOST) and the pager's `R`/`r` all bump the ROOT: every
//      spot moves, so every consumer misses.
//   3. Revs exist ONLY under a LIVE watcher.  A failed `start`, or a dir that
//      could not be armed, hands out a FRESH TOKEN per query — no two queries
//      agree, so everything recomputes.  A missed event is never read as truth;
//      there is no persisted form.
//   4. Arming is LAZY and lands ON THE QUERY, before the caller computes: a
//      write between the read and `fsw.dir` would otherwise be missed.
//
//  Reuse: the walk + sub-repo boundaries are classify.wtWalk (the same one
//  wtScan drives), the ignore rules are util/ignore.js, path joins are
//  core/discover.wtpath.  A nested repo is NOT armed by its parent's walk — it
//  arms on its own first query and reaches the parent through the ancestor rule.
//
//  ONE WATCHER FD for the whole tree ([JAB-032]): `fsw.dir` returns a real
//  signed wd, so a wd→dir map does the attribution.  A wd the map does not know
//  is an unclaimed leftover watch (ABC-013: `fsw.unwatch` leaves the kernel one
//  alive) and is ignored.
//
//    start(root)   open the ONE watcher → true (live) | false (stays null)
//    rev(path)     THE query: arm lazily, answer the spot's rev
//                  (a fresh never-repeating token when there is no watcher)
//    poll()        drain the queue and apply its bumps (rev does it first)
//    bumpRoot()    overflow, a lost burst, or the pager's `R`/`r`
//    stop()        MUST precede any pol.init()
//    stats()       { hits, misses, drops, dirs, watches, spots, rev, live, root }

"use strict";

const wtpath = require("../core/discover.js").wtpath;
//  CODE-028: classify.js requires this module back; both FILL their exports.
const ignorelib = require("./util/ignore.js");
const classify = require("./classify.js");

//  BRO-043: `jsrc` is a symlink to `.`, so `<root>/shared/cache.js` and
//  `<root>/jsrc/shared/cache.js` load as TWO module instances of one file.  ALL
//  state therefore lives in ONE globalThis slot every instance binds to — a
//  second copy can no longer silently hand out its own revs.
const S = globalThis.__BE_REV_TREE__ || (globalThis.__BE_REV_TREE__ = {
  wfd: -1,                 // the ONE watcher fd
  rev: 0,                  // THE shared counter
  spot: null,              // absolute dir → rev            null = OFF
  wdOf: null,              // absolute dir → watch descriptor
  dirOf: null,             // watch descriptor → absolute dir (attribution)
  armed: null,             // absolute dir → the rev its walk was done at
  seen: null,              // absolute dir → the rev last handed out (STATS only)
  token: 0,                // the no-watcher token counter (always fresh)
  drainBuf: null,          // the drain sink (one burst)
  walk: null,              // TODO-006: the arming walk, published for its caller
  root: "",
  st: { hits: 0, misses: 0, bumps: 0, watches: 0 },
});
const st = S.st;

//  The rev tree EXISTS iff a watcher is live (ruling 3).
function live() { return S.spot !== null; }

function start(r) {
  if (live()) return true;
  let fd = -1;
  try { fd = fsw.init(); } catch (e) { fd = -1; }
  if (typeof fd !== "number" || fd < 0) { S.wfd = -1; S.spot = null; return false; }
  S.wfd = fd; S.spot = {}; S.wdOf = {}; S.dirOf = {}; S.armed = {}; S.seen = {};
  S.root = r || "";
  try { S.drainBuf = io.buf(1 << 16); }
  catch (e) { stop(); return false; }
  return true;
}

//  fsw close MUST precede any pol.init(), which wipes the fd table and would
//  silently strand the watcher ([JAB-032] §6).
function stop() {
  if (S.wfd >= 0) { try { fsw.close(S.wfd); } catch (e) {} }
  S.wfd = -1;
  S.spot = null; S.wdOf = null; S.dirOf = null; S.armed = null; S.seen = null;
  S.drainBuf = null; S.walk = null;
}

//  STATUS-019: THE stamp — one increment of the shared counter, applied to the
//  event's dir and every ANCESTOR spot up the path (a parent embeds its subs).
function bump(dir) {
  const n = ++S.rev;
  st.bumps++;
  for (let p = dir; ;) {
    if (S.spot[p] !== undefined) S.spot[p] = n;
    const i = p.lastIndexOf("/");
    if (i <= 0) return;
    p = p.slice(0, i);
  }
}

//  Ruling 2: the root moved, so EVERY spot under it moved — all consumers miss.
function bumpRoot() {
  if (!live()) return;
  const n = ++S.rev;
  st.bumps++;
  for (const k in S.spot) S.spot[k] = n;
}

//  Drain every queued event and stamp it.  Called at the TOP of `rev`, so a
//  query is only ever answered after the pending bumps landed.  An OVERFLOW
//  record or a drain/records throw (the Buf could not hold the burst — those
//  events are LOST) is the same fact: bump the ROOT, not one dir.
function poll() {
  if (!live()) return;
  for (let guard = 0; guard < 1024; guard++) {
    let n = 0;
    try { n = fsw.drain(S.wfd, S.drainBuf); }
    catch (e) { S.drainBuf.reset(); bumpRoot(); return; }
    if (!n) { S.drainBuf.reset(); return; }
    let recs;
    try { recs = fsw.records(S.drainBuf); }
    catch (e) { S.drainBuf.reset(); bumpRoot(); return; }
    S.drainBuf.reset();
    for (const r of recs) {
      if (r.wd === fsw.OVERFLOW) { bumpRoot(); continue; }
      //  kqueue reports name "" (rescan this dir) — for revs the dir is all we
      //  need, so the name is never read; an unclaimed wd (ABC-013) is ignored.
      const d = S.dirOf[r.wd];
      if (d !== undefined) bump(d);
    }
  }
}

//  Arm ONE dir level on the shared fd and open its spot at the CURRENT rev (a
//  brand-new spot is born fresh, never stale).  A dir that cannot be armed gets
//  no spot: `rev` then hands out a token and the caller recomputes (ruling 3).
function armDir(abs) {
  if (S.wdOf[abs] !== undefined) return;
  let wd = -1;
  try { wd = fsw.dir(S.wfd, abs); } catch (e) { return; }
  //  wd 0 = a pre-[JAB-032] jab that cannot attribute events at all.
  if (typeof wd !== "number" || wd <= 0) return;
  S.wdOf[abs] = wd; S.dirOf[wd] = abs; st.watches++;
  if (S.spot[abs] === undefined) S.spot[abs] = S.rev;
}

//  Arm `wt` and every non-ignored dir under it that is not inside a nested repo,
//  plus its own `.be/`.  THE divergence from `ignore.match`: `.be/` is
//  RE-ADMITTED (its wtlog rows are a status input, so a `be put` from a second
//  process has to fire) while everything BELOW it — the store's object dirs,
//  which churn on every fetch — stays ignored.
//  The walk re-runs when the spot moved since the last one: that is exactly when
//  a new subdir may have appeared, and the caller is recomputing anyway.
function arm(wt) {
  if (S.armed[wt] !== undefined && S.armed[wt] === S.spot[wt]) return;
  armDir(wt);                                        // the spot itself first
  if (S.spot[wt] === undefined) return;              // unwatchable: no rev at all
  const ignore = ignorelib.load(wt);
  const w = classify.wtWalk(wt, ignore);
  for (const nm of w.names) {
    if (nm[nm.length - 1] !== "/") continue;         // dirs only
    const rel = nm.slice(0, -1);
    if (rel !== ".be" && ignore.match(rel, true)) continue;
    if (w.underNested(rel)) continue;                // a nested repo arms itself
    armDir(wtpath(wt, rel));
  }
  S.armed[wt] = S.spot[wt];
  //  TODO-006: PUBLISH it — the caller queried this spot because it is about to
  //  classify that very wt, and that walk is the arming walk over again (6 s of
  //  a 10.6 s cold board).  ONE slot: the take below is the next thing to run.
  S.walk = { wt: wt, rev: S.spot[wt], w: w };
}

//  TODO-006: hand the arming walk to the compute that follows the query — ONCE,
//  and only while the spot still stands where the walk was done.  No hit, no
//  slot, a moved spot: the caller walks itself (classify.wtWalk).
function takeWalk(wt) {
  const s = S.walk;
  if (!live() || !s || s.wt !== wt) return null;
  S.walk = null;
  return s.rev === S.spot[wt] ? s.w : null;
}

//  THE query.  No watcher, or a dir that could not be armed → a FRESH TOKEN, so
//  the caller's compare always fails and it recomputes (ruling 3).
function rev(path) {
  if (!live() || typeof path !== "string" || !path) return --S.token;
  poll();
  arm(path);
  const r = S.spot[path];
  if (r === undefined) return --S.token;
  //  STATS only: a query that gets the same rev this spot last answered is what
  //  a consumer's memo serves — the hit/miss line the pager prints.
  if (S.seen[path] === r) st.hits++; else { st.misses++; S.seen[path] = r; }
  return r;
}

function stats() {
  let dirs = 0, spots = 0;
  for (const k in (S.wdOf || {})) dirs++;
  for (const k in (S.spot || {})) spots++;
  return { hits: st.hits, misses: st.misses, drops: st.bumps, dirs: dirs,
           watches: st.watches, spots: spots, rev: S.rev, live: live(),
           root: S.root };
}

//  CODE-028: FILL the exports object, never REPLACE it — classify.js requires
//  this module at top level and would freeze an empty handle.
Object.assign(module.exports, {
                   start: start, stop: stop, rev: rev, poll: poll,
                   bumpRoot: bumpRoot, stats: stats, takeWalk: takeWalk });
