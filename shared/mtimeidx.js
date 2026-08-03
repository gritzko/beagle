//  mtimeidx.js — BRO-044: the last-touch lane, ONE `kv64` index in the store
//  shard beside `keeper.idx` and `meta.idx`.  DESPITE THE NAME THE LANE HOLDS
//  COMMIT TIMES, never an fs mtime: an fs mtime only ever appears LIVE, for a
//  dirty file whose bytes hash to no committed blob (that lookup misses here by
//  construction and the caller falls back to stat).
//
//  ROW (kv64: a KEYED lane, a re-put OVERWRITES)
//    key = hashlet60 << 4 | type:4      the wh128 field split (`dog/WHIFF.h:7`,
//                                       the id=0 case `keeper.idx` uses)
//    val = ron60                        the COMMIT TIME of the oldest commit
//                                       carrying that object
//  `type` is the git pack type number: 2 a tree, 3 a blob, 1 a gitlink.  Tree
//  rows are what make a reader sublinear — one row answers a whole directory
//  entry, so an unchanged subtree is never descended.
//
//  WHY OBJECT-KEYED.  A path-keyed cache expires on every commit and needs an
//  ancestry check after a reset.  An object-keyed row does not: new content is
//  a NEW key, and an object's oldest carrier does not depend on which branch is
//  checked out.  Filled once, only ever grows, never invalidated.
//
//  ATTRIBUTION.  A commit that CHANGES a path introduces the object now at it,
//  walking newest-first over `changedpaths.changedObjects` (the PRUNING descent
//  — an unchanged subtree is not opened).  Merging is OLDER-WINS, so a row only
//  ever moves toward the true oldest carrier and never back: a head-gap walk
//  (commits NEWER than everything recorded) can never overwrite, and a tail
//  extension corrects a row that a shallower walk had attributed too new.
//  Attribution follows CONTENT, so a file reverted to earlier text carries that
//  earlier timestamp — accepted, not a bug.
//
//  THE TWO SENTINEL MARKS (type nibble 0, which no git object type uses):
//    key (0 << 4) | 0   HEAD — the tip as of the last fill; val its hashlet60
//    key (1 << 4) | 0   TAIL — the deepest commit that fill reached
//  The covered region is the contiguous mainline segment [HEAD .. TAIL].  ONLY
//  THE MARKS CAN GO STALE: if HEAD is no longer an ancestor of the tip (reset,
//  rebase, a rewritten history) BOTH marks are dropped and the walk starts over
//  from the tip — every cached ROW stays valid and is reused.
"use strict";

const changedpaths = require("./changedpaths.js");
const dag = require("./dag.js");
const shalib = require("./util/sha.js");     // hashlet60FromBytes — never hand-rolled
const isFullSha = shalib.isFullSha;

//  The lane's runs live beside `keeper.idx` in the store shard.
const IDX_EXT = ".mtime.idx";
//  Rows put between two commits must fit ONE 4 KB memtable page (256 kv64
//  rows), so batch at 200 — the shared/ingest.js `idxWriter` discipline.
const IDX_BATCH = 200;
//  The fill ceiling, mirroring lastcommit.js LIST_MAX_WALK / log.js LOG_MAX_WALK.
const FILL_MAX_WALK = 1 << 16;
//  How much deeper a fill pushes the tail frontier when the screen needed
//  nothing — a deep history converges over a few queries, never one stall.
const TAIL_CHUNK = 256;

//  git pack type numbers (store.js twin).
const T_COMMIT = 1, T_TREE = 2, T_BLOB = 3;
//  The reserved nibble the marks ride; no git object type is 0.
const K_MARK = 0x0n;
const MARK_HEAD = 0n, MARK_TAIL = 1n;

//  --- field split ----------------------------------------------------------
function packKey(hashlet60, type) { return (hashlet60 << 4n) | (BigInt(type) & 0xfn); }
function keyHashlet(k) { return k >> 4n; }
function keyType(k) { return Number(k & 0xfn); }
function markKey(which) { return packKey(which, 0); }

//  A 40-hex sha -> its hashlet60 (the MS 60 bits = the first 15 nibbles).
function hashletOf(sha) { return shalib.hashlet60FromBytes(hex.decode(sha)); }
//  ...and back to the 15-nibble prefix a store `resolveHexAny` resolves.
function hexOf(hashlet60) {
  let s = hashlet60.toString(16);
  while (s.length < 15) s = "0" + s;
  return s;
}
//  The row key for an object named by its full sha.
function objKey(sha, type) { return packKey(hashletOf(sha), type); }
//  A tree entry's mode -> the type nibble its row carries.
function typeOfMode(mode) {
  return mode === 0o40000 ? T_TREE : mode === 0o160000 ? T_COMMIT : T_BLOB;
}

//  --- the lane handle ------------------------------------------------------
//  Open the lane in `shard`.  `abc.index` io.mkdir()s its dir, so never CONJURE
//  a shard: a missing one is a repo without a store, said in plain words.
function openIndex(shard) {
  let kind = null;
  try { kind = io.stat(shard).kind; } catch (e) { kind = null; }
  if (kind !== "dir")
    throw "last-touch index: there is no store shard at " + shard +
          " — the last-touch times live in the project store";
  return abc.index("kv64", { dir: shard, ext: IDX_EXT });
}

//  Is there a usable shard to open?  The lane is an ACCELERATOR: a caller with
//  no store shard falls back to its own walk rather than failing.
function hasShard(shard) {
  if (!shard) return false;
  try { return io.stat(shard).kind === "dir"; } catch (e) { return false; }
}

//  A batching writer over the lane (the shared/ingest.js idxWriter discipline).
//  A seal NEVER carries a mark — the marks are the fill's LAST writes, so a
//  crash mid-fill leaves only rows that are true and an un-advanced frontier.
function idxWriter(ix) {
  let n = 0;
  return {
    put: function (k, v) { ix.put(k, v); if (++n >= IDX_BATCH) { ix.commit(); n = 0; } },
    seal: function () { if (n) { ix.commit(); n = 0; } }
  };
}

//  ONE merged pass over the whole stack -> { rows: Map(key -> ron60), marks }.
//  KNOWN TRAP: `prefix(p,bits)` / `range(lo,hi)` silently return ZERO rows when
//  the upper bound reaches 2^64 — and a hashlet60 key spans exactly that — so a
//  full pass MUST ride the `seek(0n)` + `next()` cursor.
function readAll(ix) {
  const rows = new Map();
  const marks = { head: null, tail: null };
  const c = ix.seek(0n);
  while (c.next()) {
    const k = c.key, v = c.val;
    if (keyType(k) === 0) {
      const which = keyHashlet(k);
      if (which === MARK_HEAD) marks.head = v;
      else if (which === MARK_TAIL) marks.tail = v;
      continue;
    }
    rows.set(k, v);
  }
  return { rows: rows, marks: marks };
}

//  A mark's stored hashlet60 -> the full commit sha it names (undefined when the
//  commit is gone from the store, which reads exactly like "no mark").
function markSha(k, hashlet60) {
  if (hashlet60 === null || hashlet60 === undefined) return undefined;
  let sha;
  try { sha = k.resolveHexAny(hexOf(hashlet60)); } catch (e) { sha = undefined; }
  return isFullSha(sha) ? sha : undefined;
}

//  --- the fill walk --------------------------------------------------------
//  Absorb ONE commit: every object it INTRODUCES (a path whose leaf sha differs
//  from the mainline parent's) gets this commit's time, OLDER WINS.
function absorb(k, sha, parent, ts, rows, dirty, need) {
  changedpaths.changedObjectsCommits(k, parent || "", k, sha, function (o) {
    if (!o.sha || !isFullSha(o.sha)) return;
    const key = packKey(hashletOf(o.sha), o.type);
    const have = rows.get(key);
    if (have !== undefined && have <= ts) return;      // an older carrier stands
    rows.set(key, ts);
    dirty.add(key);
    if (need) need.delete(key);
  });
}

//  Walk the mainline newest-first from `from`, absorbing each commit, stopping
//  BEFORE `stopAt` (exclusive; "" = walk to the root) or when the budget runs
//  out.  There is NO "stop as soon as the screen is satisfied": that leaves a
//  row attributed to a carrier the walk merely happened to see FIRST, and since
//  a satisfied screen never fills again the too-new row would be permanent.
//  The BUDGET is what bounds the walk; a row only moves OLDER, so the frontier
//  deepening (below) can only correct it.  Returns { visited, last, bottom }.
function walkSegment(k, from, stopAt, budget, parentOf, rows, dirty, need) {
  let sha = from, visited = 0, last = null, bottom = false;
  while (visited < budget) {
    if (!isFullSha(sha)) break;
    if (stopAt && sha === stopAt) break;
    const pc = k.parseCommit(sha);
    if (!pc) break;                              // missing/non-commit: clean stop
    const parents = k.commitParents(sha) || [];
    const parent = parentOf(k, parents);
    absorb(k, sha, parent, dag.commitTs(k, sha), rows, dirty, need);
    visited++; last = sha;
    if (!parent) { bottom = true; break; }        // root commit: history bottomed
    sha = parent;
  }
  return { visited: visited, last: last, bottom: bottom };
}

//  fill(ix, k, tip, wantKeys, opts) -> { rows, rec }.
//  Bring the lane up to date far enough to answer `wantKeys` (a Set of row keys
//  that are on screen), then stop.  Rows the walk never reached simply have no
//  row: the caller renders them blank and they attribute on a later, deeper
//  query.  `opts.parentOf(k, parents)` picks the mainline parent (defaults to
//  the first); `opts.cap` overrides the walk ceiling.
function fill(ix, k, tip, wantKeys, opts) {
  const o = opts || {};
  const parentOf = o.parentOf || function (kk, parents) {
    return (parents && parents.length && isFullSha(parents[0])) ? parents[0] : undefined;
  };
  const budget = o.cap && o.cap > 0 ? o.cap : FILL_MAX_WALK;
  const rec = { walked: 0, put: 0, head: null, tail: null, dropped: false,
                cold: false, gap: 0, extend: 0 };

  const state = readAll(ix);
  const rows = state.rows, dirty = new Set();
  let head = markSha(k, state.marks.head), tail = markSha(k, state.marks.tail);
  rec.cold = !head;
  //  ONLY the marks go stale: a HEAD that no longer leads to the tip means the
  //  history was reset/rebased, so the covered REGION is unknown — drop both
  //  marks and re-walk.  Every cached row survives untouched.
  if (head && !dag.isAncestor(k, head, tip)) { head = null; tail = null; rec.dropped = true; }
  if (!head || !tail) { head = null; tail = null; }

  const need = new Set();
  for (const key of (wantKeys || [])) if (!rows.has(key)) need.add(key);

  let deepest = tail, spent = 0, bottom = false;
  //  1. the HEAD GAP: the commits made since the last fill (newest, so their
  //     rows can only lose to an older carrier already recorded).
  if (need.size || tip !== head) {
    const seg = walkSegment(k, tip, head || "", budget, parentOf, rows, dirty, need);
    spent += seg.visited; rec.gap = seg.visited;
    bottom = seg.bottom;
    if (!head || seg.visited >= budget || seg.bottom) {
      //  no mark (a cold or dropped-mark fill), or the budget cut the gap short
      //  — the covered region is exactly what this segment walked.
      if (seg.last) { deepest = seg.last; tail = seg.last; }
    }
  }
  //  2. the TAIL FRONTIER.  A screen still missing a row pulls the frontier down
  //     as far as the ceiling allows; a satisfied screen still pushes it one
  //     CHUNK deeper, so a deep history converges over a few invocations instead
  //     of one stall — and every row it corrects can only move OLDER.
  for (const key of (wantKeys || [])) if (!rows.has(key)) need.add(key);
  if (!bottom && tail && spent < budget) {
    const parents = k.commitParents(tail) || [];
    const below = parentOf(k, parents);
    const room = need.size ? (budget - spent) : Math.min(TAIL_CHUNK, budget - spent);
    if (below && room > 0) {
      const seg = walkSegment(k, below, "", room, parentOf, rows, dirty, need);
      spent += seg.visited; rec.extend = seg.visited;
      if (seg.last) { deepest = seg.last; tail = seg.last; }
    }
  }
  rec.walked = spent;

  //  3. write the rows, then the marks LAST (an intermediate seal must never
  //     carry a frontier the rows do not back).
  const w = idxWriter(ix);
  for (const key of dirty) { w.put(key, rows.get(key)); rec.put++; }
  w.seal();
  if (spent > 0 || rec.dropped) {
    if (isFullSha(tip)) { ix.put(markKey(MARK_HEAD), hashletOf(tip)); rec.head = tip; }
    if (deepest && isFullSha(deepest)) { ix.put(markKey(MARK_TAIL), hashletOf(deepest)); rec.tail = deepest; }
    ix.commit();
  }
  return { rows: rows, rec: rec };
}

module.exports = {
  IDX_EXT: IDX_EXT, IDX_BATCH: IDX_BATCH, FILL_MAX_WALK: FILL_MAX_WALK,
  TAIL_CHUNK: TAIL_CHUNK,
  T_COMMIT: T_COMMIT, T_TREE: T_TREE, T_BLOB: T_BLOB,
  MARK_HEAD: MARK_HEAD, MARK_TAIL: MARK_TAIL,
  packKey: packKey, keyHashlet: keyHashlet, keyType: keyType, markKey: markKey,
  hashletOf: hashletOf, hexOf: hexOf, objKey: objKey, typeOfMode: typeOfMode,
  openIndex: openIndex, hasShard: hasShard, readAll: readAll, markSha: markSha,
  fill: fill
};
