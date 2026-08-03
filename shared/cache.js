//  cache.js — BRO-043: the PER-REPO view cache, dropped by the watcher.
//  A resident process (the bro pager) keeps one free-form bucket per repo of
//  whatever a view computed, and an fsw event under that repo throws the bucket
//  away.  INVALIDATION IS THE DESIGN; storage is a plain object.
//
//  The rulings it encodes (gritzko 2026-08-02) — none of them negotiable:
//   1. PER-REPO ONLY.  The bucket key is the repo path (`repo.wt`); a SCOPED
//      read caches nothing and recomputes.  No per-path/per-arg/per-rev keys.
//   2. The value is a free-form JS object; this module never inspects a key.
//   3. Any event under a repo drops that repo's bucket AND its ANCESTORS'
//      (a parent's status embeds its subs'); siblings and descendants keep theirs.
//   4. The cache exists ONLY under a LIVE watcher.  A failed `start` leaves it
//      null and everything recomputes — a missed event is never read as truth.
//      That is the whole safety argument; there is no persisted form.
//   5. Arming is LAZY and lands BEFORE the compute: a MISS is what subscribes,
//      and a write between the read and `fsw.dir` would otherwise be missed.
//
//  Reuse: the walk + sub-repo boundaries are classify.wtWalk (the same one
//  wtScan drives), the ignore rules are util/ignore.js, path joins are
//  core/discover.wtpath.  Subs are NOT armed by their parent's walk — each
//  is its own bucket and reaches the parent through the ancestor rule.
//
//  ONE WATCHER FD PER REPO: `fsw.dir` reports no usable watch descriptor (every
//  record arrives as `wd: 0`, [JAB-032] is not in the running jab), so the
//  watcher fd is the only identity an event carries — the same trick `fsw.watch`
//  uses per dir.  A repo's dirs all go on that repo's fd, so anything drained
//  off it drops exactly that repo; the `wd` is read for the OVERFLOW sentinel
//  only.  The count is one fd per CACHED REPO, not per dir.
//
//    start(root)              open the watcher → true (live) | false (stays null)
//    take(repo, key, compute) THE read path: miss → arm, compute, store
//    peek(repo, key)          the validity probe: no hit, no arm, no store
//    drop(repo)               invalidate one repo + its ancestors
//    dropAll()                overflow, or the watcher died
//    stop()                   MUST precede any pol.init()
//    stats()                  { hits, misses, drops, dirs, watches, repos, fds }

"use strict";

const wtpath = require("../core/discover.js").wtpath;

//  BRO-043: `jsrc` is a symlink to `.`, so `<root>/shared/cache.js` and
//  `<root>/jsrc/shared/cache.js` load as TWO module instances of one file.  ALL
//  state therefore lives in ONE globalThis slot every instance binds to — a
//  second copy can no longer silently no-op (the bench that read live=true with
//  hits=0 and still looked faster from JIT warm-up).
const S = globalThis.__BE_VIEW_CACHE__ || (globalThis.__BE_VIEW_CACHE__ = {
  spare: -1,               // a watcher fd opened but not yet claimed by a repo
  fdOf: null,              // repoWt → watcher fd (the repo's identity)
  buckets: null,           // repoWt → { key: value, lastChange }   null = OFF
  dirOf: null,             // absolute dir → repoWt (armed once, and counted)
  drainBuf: null,          // the drain sink (one burst)
  root: "",
  st: { hits: 0, misses: 0, drops: 0, watches: 0 },
});
const st = S.st;

//  The cache EXISTS iff a watcher is live (ruling 4).
function live() { return S.buckets !== null; }

//  The first watcher fd opens HERE: it both proves a watcher can be had at all
//  (else the cache stays null) and becomes the fd the first armed repo claims.
function start(r) {
  if (live()) return true;
  let fd = -1;
  try { fd = fsw.init(); } catch (e) { fd = -1; }
  if (typeof fd !== "number" || fd < 0) { S.spare = -1; S.buckets = null; return false; }
  S.spare = fd; S.fdOf = {}; S.buckets = {}; S.dirOf = {}; S.root = r || "";
  try { S.drainBuf = io.buf(1 << 16); }
  catch (e) { stop(); return false; }
  return true;
}

//  fsw close MUST precede any pol.init(), which wipes the fd table and would
//  silently strand the watcher ([JAB-032] §6).
function stop() {
  for (const wt in (S.fdOf || {})) { try { fsw.close(S.fdOf[wt]); } catch (e) {} }
  if (S.spare >= 0) { try { fsw.close(S.spare); } catch (e) {} }
  S.spare = -1; S.fdOf = null;
  S.buckets = null; S.dirOf = null; S.drainBuf = null;
}

//  Drain every queued event and apply its drops.  Called at the TOP of `take`,
//  so a hit is only ever answered after the pending invalidations landed.
//  An OVERFLOW record (the kernel dropped events) or a drain throw (the Buf
//  could not hold the burst — those events are LOST) means every bucket is
//  suspect: dropAll, not one dir.  A `ninja` build in a watched tree does this
//  routinely, so it is a normal path, not a corner case.
function poll() {
  if (!live()) return;
  //  A repo at a time: every fd carries one repo's dirs and nothing else.
  for (const wt in S.fdOf) if (!pollFd(wt, S.fdOf[wt])) return;   // false = all dropped
}

//  Drain ONE repo's watcher.  Any record at all is an event under that repo, so
//  the whole bucket goes; the `wd` is read for the OVERFLOW sentinel only.
function pollFd(wt, fd) {
  for (let guard = 0; guard < 1024; guard++) {
    let n = 0;
    try { n = fsw.drain(fd, S.drainBuf); }
    catch (e) { S.drainBuf.reset(); dropAll(); return false; }
    if (!n) { S.drainBuf.reset(); return true; }
    let recs;
    try { recs = fsw.records(S.drainBuf); }
    catch (e) { S.drainBuf.reset(); dropAll(); return false; }
    S.drainBuf.reset();
    for (const r of recs) if (r.wd === fsw.OVERFLOW) { dropAll(); return false; }
    drop(wt);
  }
  return true;
}

//  The repo's own watcher fd, opened on its FIRST arm (start()'s fd goes to the
//  first comer).  -1 = no watcher for this repo, so nothing may be stored.
function repoFd(wt) {
  let fd = S.fdOf[wt];
  if (fd !== undefined) return fd;
  fd = S.spare; S.spare = -1;
  if (fd < 0) { try { fd = fsw.init(); } catch (e) { return -1; } }
  if (typeof fd !== "number" || fd < 0) return -1;
  S.fdOf[wt] = fd;
  return fd;
}

//  Arm one dir level on its repo's fd and record dir → repo.  A dir that cannot
//  be armed (gone, or `max_user_watches` reached) is simply not covered: what it
//  would have protected is dropped by its parent's event, or by an overflow.
function armDir(wt, fd, abs) {
  if (S.dirOf[abs] === wt) return;         // already armed on this repo's fd
  try { fsw.dir(fd, abs); } catch (e) { return; }
  st.watches++;
  S.dirOf[abs] = wt;
}

//  Arm every non-ignored dir of `wt` that is not under a nested repo, plus the
//  wt's own `.be/`.  THE divergence from `ignore.match`: `.be/` is RE-ADMITTED
//  (its wtlog rows are a status input, so a `be put` from a second process has
//  to fire) while everything BELOW it — the store's object dirs, which churn on
//  every fetch — stays ignored.
//  → false when the repo got no watcher at all: it could never be invalidated,
//  so the caller must compute and store NOTHING (ruling 4, one repo at a time).
function arm(wt) {
  const fd = repoFd(wt);
  if (fd < 0) return false;
  const ignore = require("./util/ignore.js").load(wt);
  const w = require("./classify.js").wtWalk(wt, ignore);
  armDir(wt, fd, wt);                                // the repo root itself
  for (const nm of w.names) {
    if (nm[nm.length - 1] !== "/") continue;         // dirs only
    const rel = nm.slice(0, -1);
    if (rel !== ".be" && ignore.match(rel, true)) continue;
    if (w.underNested(rel)) continue;                // a sub is its own bucket
    armDir(wt, fd, wtpath(wt, rel));
  }
  return true;
}

//  THE read path (one call, never a get/put pair — a split surface is what
//  lets a caller arm AFTER computing and be born stale).
function take(repo, key, compute) {
  const wt = repo && repo.wt;
  //  1. not live → compute, store NOTHING.  This is the whole safety argument.
  if (!live() || !wt) return compute();
  poll();
  const b = S.buckets[wt];
  //  2. hit.
  if (b && Object.prototype.hasOwnProperty.call(b, key)) { st.hits++; return b[key]; }
  //  3. miss → arm BEFORE computing (a write landing between the read and
  //  fsw.dir would otherwise be missed and the entry born stale).
  st.misses++;
  if (!arm(wt)) return compute();                   // unwatchable → store nothing
  //  4. compute, store under `key`, stamp lastChange.
  const v = compute();
  const nb = S.buckets[wt] || (S.buckets[wt] = {});
  nb[key] = v;
  try { nb.lastChange = ron.now(); } catch (e) { nb.lastChange = 0n; }
  return v;
}

//  BRO-043: the VALIDITY PROBE — read a stored value without counting a hit and
//  without arming, so a consumer can compare its `state` fingerprint first and
//  DROP a stale record instead of serving it.  This is not a second read path:
//  `peek` can neither compute nor store, so the born-stale hazard a get/put pair
//  opens (arming AFTER the compute) cannot happen through it.
function peek(repo, key) {
  const wt = repo && repo.wt;
  if (!live() || !wt) return undefined;
  poll();
  const b = S.buckets[wt];
  return (b && Object.prototype.hasOwnProperty.call(b, key)) ? b[key] : undefined;
}

//  Drop one repo's bucket AND its ANCESTORS' (a parent's status embeds its
//  subs'), by path prefix.  Siblings and descendants keep theirs.
function drop(repo) {
  const wt = (repo && repo.wt) || repo;
  if (!live() || typeof wt !== "string" || !wt) return;
  for (const k in S.buckets) {
    if (k === wt || wt.indexOf(k + "/") === 0) { delete S.buckets[k]; st.drops++; }
  }
}

function dropAll() {
  if (!live()) return;
  for (const k in S.buckets) { delete S.buckets[k]; st.drops++; }
}

function stats() {
  let dirs = 0, repos = 0, fds = 0;
  for (const k in (S.dirOf || {})) dirs++;
  for (const k in (S.buckets || {})) repos++;
  for (const k in (S.fdOf || {})) fds++;
  return { hits: st.hits, misses: st.misses, drops: st.drops, dirs: dirs,
           watches: st.watches, repos: repos, fds: fds, live: live(), root: S.root };
}

module.exports = { start: start, stop: stop, take: take, peek: peek, drop: drop,
                   dropAll: dropAll, stats: stats, poll: poll };
