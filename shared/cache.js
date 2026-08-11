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
//  Reuse: the walk + sub-repo boundaries are wtWalk below (the same one
//  classify's wtScan drives), the ignore rules are util/ignore.js, path joins are
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
const ignorelib = require("./util/ignore.js");
const join = require("./util/path.js").join;

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
  walk: null,              // TODO-006: the arming walk + BE-064: its matcher
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
  S.drainBuf = null; dropWalk();
}

//  STATUS-020: the published slot OWNS its matcher's open `.gitignore` maps —
//  clearing the slot without a take must release them, never just drop them.
function dropWalk() {
  const s = S.walk;
  S.walk = null;
  if (s && s.ig) s.ig.close();
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

//  --- THE worktree walk (CODE-030: moved here from classify.js) ------------
//  Walk the worktree depth-first via io.readdir({recursive}) and report the
//  names plus the nested-repo boundaries.  BRO-043 extracted it verbatim out of
//  wtScan; it belongs HERE, next to the rev keying its memo — wtScan (which
//  wants the FILE half) and arm() (which wants the DIR half) are its two
//  callers, and classify.wtWalk is now a one-line re-export.  Skips
//  `.gitignore`-matched paths + `.git`/`.be` meta + nested repos (a subdir
//  holding its own `.git`/`.be` — a separate repo).
//  → { names, nestedPrefixes, underNested }
function wtWalk(wtRoot, ignore) {
  //  TODO-006: the rev tree arms a wt by walking it, right before the caller
  //  computes — take THAT walk instead of doing the same one twice.
  const pre = takeWalk(wtRoot);
  if (pre) return pre;
  //  ONE PRUNING descent: io.readdir's cb `"skip"` directive cuts a subtree at
  //  its dir and keeps scanning the siblings, so an ignored dir and a nested
  //  repo are never enumerated and never stat'd.  The dir ENTRY itself is
  //  delivered before its directive is read, so it stays in `names` (arm()
  //  arms `.be/` off exactly that) — only the subtree goes.  A jab without the
  //  directive reads `"skip"` as `"more"`: same answer, the old cost.
  //  hidden:true — native scans dotfiles too (`.gitignore` is tracked);
  //  only `.git`/`.be` are meta, filtered by the ignore matcher below.
  //  Nested-repo dir prefixes are found in the SAME pass (a dir D with D/.git
  //  or a D/.be FILE): the boundary is what stops the descent, so a sub's own
  //  inner subs never enter the list — `underNested` answers for them off the
  //  outermost prefix, which is all either caller ever asked of it.
  const names = [], nestedPrefixes = [];
  try {
    io.readdir(wtRoot, { recursive: true, hidden: true, callback: function (nm) {
      names.push(nm);
      if (nm[nm.length - 1] !== "/") return "more";    // dirs decide descent
      const dirRel = nm.slice(0, -1);
      if (ignore.match(dirRel, true)) return "skip";
      const full = wtpath(wtRoot, dirRel);
      if (statKind(join(full, ".git")) !== undefined) {
        nestedPrefixes.push(dirRel + "/"); return "skip";
      }
      const beKind = statKind(join(full, ".be"));
      //  SUBS-049: a PRIMARY nested wt (`.be` DIR holding wtlog, a green-field
      //  remote-get clone) is a repo boundary too — not only the `.be` FILE form.
      if (beKind === "reg" || (beKind === "dir" &&
          statKind(join(full, ".be/wtlog")) === "reg")) {
        nestedPrefixes.push(dirRel + "/"); return "skip";
      }
      return "more";
    } });
  } catch (e) { names.length = 0; nestedPrefixes.length = 0; }
  function underNested(rel) {
    for (const p of nestedPrefixes) if (rel === p.slice(0, -1) || rel.indexOf(p) === 0) return true;
    return false;
  }
  return { names: names, nestedPrefixes: nestedPrefixes, underNested: underNested };
}

function statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }

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
  const w = wtWalk(wt, ignore);
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
  //  BE-064: the MATCHER that produced the walk rides with it — classifyMerge
  //  re-loaded it for the same root microseconds later (151 of 355 loads).
  dropWalk();                                        // STATUS-020: free a stale slot
  S.walk = { wt: wt, rev: S.spot[wt], w: w, ig: ignore };
}

//  TODO-006: hand the arming walk to the compute that follows the query — ONCE,
//  and only while the spot still stands where the walk was done.  No hit, no
//  slot, a moved spot: wtWalk above walks it itself.  CODE-030: internal now.
function takeWalk(wt) {
  const a = takeArm(wt);
  if (!a) return null;
  a.ig.close();          // STATUS-020: this caller wants the walk half only
  return a.w;
}

//  BE-064: THE take, for BOTH halves — the walk and the matcher it was walked
//  with.  `arm` fires for topic dirs too, so the `s.wt` guard carries.  {w,ig}|null
function takeArm(wt) {
  const s = S.walk;
  if (!live() || !s || s.wt !== wt) return null;
  S.walk = null;
  if (s.rev === S.spot[wt]) return { w: s.w, ig: s.ig };
  s.ig.close();          // STATUS-020: stale walk, and with it its matcher
  return null;
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

module.exports = { start: start, stop: stop, rev: rev, poll: poll,
                   bumpRoot: bumpRoot, stats: stats, wtWalk: wtWalk,
                   takeArm: takeArm };
