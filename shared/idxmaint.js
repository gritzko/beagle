//  idxmaint.js — JS-116: keeper.idx run lifecycle maintenance.  The shard's
//  LSM runs only ever accumulate (one per land/post, two per patch fetch) and
//  the native query leaves hard-cap at 64 runs; a log landed without its run
//  leaves objects silently unlocatable.  This module, hooked from store.js
//  diskIndex() (on open) and ingest.land()/fold-commit (after add):
//   1. compacts on open when >32 runs (half the native cap), batching an
//      already-overfull (>64) stack below the cap first;
//   2. restores the 1/8 size-tiered invariant after a run is added;
//   3. persists missing tail runs: the 0xF PACK coverage rows are ranged and
//      compared against io.stat over the `NNNNNNNNNN.keeper` logs;
//   4. unlinks collapsed source runs (crash-safe: book a dot-`.tmp`, close,
//      rename into place, unlink sources only AFTER the rename).
//  Deliberately NOT abc.index.compact(): its `String(seq).padStart(8)` run
//  naming corrupts ron60 name-sort==age-sort; merged/tail runs here are named
//  `ron.encode(ron.now()) + ".keeper.idx"` exactly like ingest.buildIndex.
//  Best-effort throughout: on a read-only store every write failure degrades
//  to opening as-is (a read verb must never crash on maintenance).

"use strict";

const join = require("./util/path.js").join;   // JSQUE-016: util libs -> shared/util/

const EXT = "keeper.idx";      // run suffix, matching abc.index({ext}) exactly
const SLOT = 16;               // wh128 run slot: 2 u64 per (key,val) entry
const RUN_CAP = 64;            // the native _compact_/_seekrange_ hard cap
const OPEN_CAP = 32;           // compact-on-open threshold (half the cap)

function warn(e) { try { io.log("idxmaint: " + e + "\n"); } catch (x) {} }

//  Run file names in the shard, name-sorted (== age-sorted for ron60 names).
function listRuns(shard) {
  const out = [];
  let names = [];
  try { names = io.readdir(shard); } catch (e) { return out; }
  for (const nm of names) if (nm.endsWith(EXT)) out.push(nm);
  out.sort();
  return out;
}

//  Entry counts per run via io.stat — the cheap (no-mmap) invariant probe.
function runSizes(shard, names) {
  const out = [];
  for (const nm of names) {
    let sz = 0;
    try { sz = io.stat(join(shard, nm)).size; } catch (e) {}
    out.push((sz / SLOT) | 0);
  }
  return out;
}

//  The HITwh128IsCompact twin: every newer run <= 1/8 of its predecessor.
function isCompact(sizes) {
  for (let i = 0; i + 1 < sizes.length; i++)
    if (sizes[i + 1] * 8 > sizes[i]) return false;
  return true;
}

//  mmap a run RO; watermark = whole file (the cont.cpp openRun recipe).
function openRun(path) {
  const r = abc.mmap("HEAPwh128", path, "r");
  const slot = 2 * r.BYTES_PER_ELEMENT;
  if ((r.byteLength % slot) !== 0)
    throw "idxmaint: misaligned run " + path + " (" + r.byteLength + " % " + slot + ")";
  r.buffer.watermark = (r.byteLength / slot) | 0;
  r._path = path;
  return r;
}

function sliceOf(r) { return r.subarray(0, (r.buffer.watermark | 0) * 2); }
function dropView(r) { r.buffer._map = null; }   // unpin (GC munmaps)

//  Fresh ron60 run name, collision-safe: a pinned clock (SOURCE_DATE_EPOCH)
//  repeats ron.now() across processes, and a clobbered/renamed-over run loses
//  coverage — bump the stamp until the name is free (still name-sorts last).
function freshRunName(shard) {
  let t = ron.now();
  for (;;) {
    const name = ron.encode(t) + "." + EXT;
    try { io.stat(join(shard, name)); } catch (e) { return name; }
    t += 1n;
  }
}

//  Land a merged run under a fresh ron60 name: book a dot-`.tmp`, fill via
//  `fill(book)` -> live entry count (or <0 to abort), close (msync+trim),
//  io.rename into place.  The caller unlinks its collapsed sources only AFTER
//  this returns — a crash before the rename leaves just a dot-file; a crash
//  after leaves a duplicate-covering (harmless) run.
function landRun(shard, total, fill) {
  const name = freshRunName(shard);
  const tmp = join(shard, "." + name + ".tmp");
  const book = abc.book("HEAPwh128", tmp, total || 1);
  let live;
  try { live = fill(book); }
  catch (e) { abc.close(book); try { io.unlink(tmp); } catch (x) {} throw e; }
  if (live < 0) { abc.close(book); try { io.unlink(tmp); } catch (x) {} return null; }
  book.buffer.watermark = live | 0;
  abc.close(book);                               // msync + trim to live size
  io.rename(tmp, join(shard, name));
  return name;
}

//  One ladder step over the <=64 YOUNGEST runs of `views` (oldest-first, live
//  mmaps; mutated in place).  Returns m (runs collapsed); m<2 = already compact.
function compactStep(shard, views) {
  const tail = views.slice(-RUN_CAP);
  const slices = tail.map(sliceOf);
  let total = 0;
  for (const s of slices) total += s.length / 2;
  let m = 0;
  const name = landRun(shard, total, function (book) {
    const r = abc._compact_wh128(slices, book);
    m = r[1];
    return m < 2 ? -1 : r[0];                    // -1: nothing collapsed, abort
  });
  if (!name) return 0;
  const dead = views.splice(views.length - m, m);
  for (const old of dead) {                      // sources die AFTER the rename
    try { io.unlink(old._path); } catch (e) {}
    dropView(old);
  }
  views.push(openRun(join(shard, name)));
  return m;
}

//  Forced merge of the k youngest runs (abc.merge, full-element dedup) — the
//  escape when the ladder reports compact but the stack still tops the caps.
function forceMerge(shard, views, k) {
  if (k > views.length) k = views.length;
  if (k > RUN_CAP) k = RUN_CAP;
  if (k < 2) return 0;
  const tail = views.slice(-k);
  let total = 0;
  for (const r of tail) total += r.buffer.watermark | 0;
  const name = landRun(shard, total, function (book) {
    abc.merge(tail, book);
    return book.buffer.watermark | 0;
  });
  if (!name) return 0;
  views.splice(views.length - k, k);
  for (const old of tail) {
    try { io.unlink(old._path); } catch (e) {}
    dropView(old);
  }
  views.push(openRun(join(shard, name)));
  return k;
}

//  Cascade compaction over the shard's whole run stack until the 1/8 ladder
//  holds; batches an overfull (>64) stack through the youngest-64 window.
function compactAll(shard, names) {
  const views = names.map(function (nm) { return openRun(join(shard, nm)); });
  try {
    for (;;) {
      if (compactStep(shard, views) >= 2) continue;
      //  ladder compact; force below the caps if still overfull (a >32-run
      //  geometric stack is practically unreachable, but the cap is hard).
      if (views.length > OPEN_CAP &&
          forceMerge(shard, views, views.length - OPEN_CAP + 1)) continue;
      break;
    }
  } finally {
    for (const v of views) dropView(v);
  }
}

//  Per-file coverage from the 0xF PACK summary rows, newest-wins collapsed to
//  max: key = ((12<<20 | file_id)<<4) | 0xF, val = count<<32 | logBytes-12
//  (ingest.buildIndex / keeper KEEP.h).  Returns { fid: coveredLogBytes-12 },
//  or null when NO run carries a PACK row at all — a legacy/foreign stack
//  whose coverage is unknowable (assume covered, do not rescan).
function coverage(views) {
  if (!views.length || views.length > RUN_CAP) return null;
  const slices = views.map(sliceOf);
  const lo = 12n << 24n;                                  // ((12<<20|0)<<4)|0x0
  const hi = ((((12n << 20n) | 0xfffffn) << 4n) | 0xfn) + 1n;
  const cov = {};
  let any = false;
  abc._seekrange_wh128(slices, lo, 0n, hi, 0n, function (kv) {
    if ((kv[0] & 0xfn) !== 0xfn) return true;             // not a PACK row
    const fid = Number((kv[0] >> 4n) & 0xfffffn);
    const bytes = Number(kv[1] & 0xffffffffn);
    if (!(cov[fid] >= bytes)) cov[fid] = bytes;
    any = true;
    return true;
  });
  return any ? cov : null;
}

//  The shard's `NNNNNNNNNN.keeper` logs with io.stat sizes, fid-sorted.
function listLogs(shard) {
  const out = [];
  try {
    for (const nm of io.readdir(shard)) {
      if (!/^\d{10}\.keeper$/.test(nm)) continue;
      let sz;
      try { sz = io.stat(join(shard, nm)).size; } catch (e) { continue; }
      out.push({ nm: nm, fid: parseInt(nm, 10), size: sz });
    }
  } catch (e) {}
  out.sort(function (a, b) { return a.fid - b.fid; });
  return out;
}

//  After-add hook (ingest.land / fold-commit writePack): cheap stat probe,
//  compact only when the 1/8 invariant is violated.  Never throws.
function compactAfterAdd(shard) {
  try {
    const names = listRuns(shard);
    if (names.length < 2 || isCompact(runSizes(shard, names))) return;
    compactAll(shard, names);
  } catch (e) { warn(e); }                        // read-only store: leave as-is
}

//  The diskIndex()-open maintenance: (a) batch an overfull stack below the
//  64 cap, (b) build + persist tail runs for logs the 0xF rows do not cover,
//  (c) compact when >32 runs or the tail build broke the ladder.  Returns the
//  post-maintenance run count (0 = no runs: caller keeps the in-RAM fallback).
//  Best-effort: any write failure degrades to opening whatever is there.
function maintain(shard) {
  let names = listRuns(shard);
  if (!names.length) return 0;
  try {
    if (names.length > RUN_CAP) {                 // can't even range: batch first
      compactAll(shard, names);
      names = listRuns(shard);
    }
    let built = false;
    if (names.length && names.length <= RUN_CAP) {
      const views = names.map(function (nm) { return openRun(join(shard, nm)); });
      let cov;
      try { cov = coverage(views); }
      finally { for (const v of views) dropView(v); }
      if (cov) {
        const ingest = require("./ingest.js");    // lazy: ingest requires us back
        for (const lg of listLogs(shard)) {
          if (cov[lg.fid] !== undefined && cov[lg.fid] >= lg.size - 12) continue;
          try { ingest.buildIndex(shard, lg.nm, lg.fid); built = true; }
          catch (e) { warn(e); }                  // thin/odd log or RO store: skip
        }
        if (built) names = listRuns(shard);
      }
    }
    if (names.length > OPEN_CAP ||
        (built && names.length > 1 && !isCompact(runSizes(shard, names))))
      compactAll(shard, names);
  } catch (e) { warn(e); }
  return listRuns(shard).length;
}

module.exports = { maintain: maintain, compactAfterAdd: compactAfterAdd,
                   listRuns: listRuns, isCompact: isCompact,
                   coverage: coverage, freshRunName: freshRunName,
                   RUN_CAP: RUN_CAP, OPEN_CAP: OPEN_CAP };
