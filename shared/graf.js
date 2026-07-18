//  graf.js — GRAF-001: per-shard cached ahead/behind COUNTS over wh128 runs.
//  open(shard) -> { aheadBehind(keeper, shaA, shaB) -> {ahead, behind}, flush }.
//  Lookup is LAZY: seek the shard's `*.graf.idx` runs (the native graf index
//  family) + the memtable first; a hit walks nothing.  A miss descends A's
//  single-parent chain to a cached (ancestor,B) pair and unwinds the recurrence
//  ahead(A,B)=ahead(p,B)+[A∉anc(B)], behind(A,B)=behind(p,B)−[A∈anc(B)]; a
//  merge/root/unreadable chain end runs a two-tip paint BFS stopped at the
//  common-ancestor CUT.  Counts SATURATE at 0xFFFFF ("at least") and saturated
//  pairs ARE cached; a truncated walk (missing object / cap) is NEVER cached.
//  New pairs go to a mem wh128 index and persist as a fresh ron60 run via
//  idxmaint (EXT-parameterized) under the native `.lock.graf` flock; on a
//  read-only store persistence degrades to the mem-only cache (idxmaint norm).

"use strict";

const idxmaint = require("./idxmaint.js");
const identEpoch = require("./dag.js").identEpoch;
const shalib = require("./util/sha.js");
const join = require("./util/path.js").join;
const isFullSha = shalib.isFullSha;

const EXT = "graf.idx";        // join the native graf run family (10-RON64 names)
const LOCK = ".lock.graf";     // native graf's shard write lock (flock)
//  GRAF-001 pair-record type nibbles: key half 0xA, val half 0xB — disjoint
//  from native graf DAG_T_* 1..5 (and keeper's 1..4 / 0xF PACK family).
const T_A = 0xan, T_B = 0xbn;
const SAT = 0xfffff;           // the 20-bit count saturation cap ("at least")
const WALK_CAP = 1 << 16;      // walk bound, mirrors dag.js (cap hit = truncated)
const MEM_SLOTS = 1 << 14;     // memtable capacity (pairs)
const MEM_FLUSH = 1 << 12;     // auto-persist threshold (pairs)

function warn(e) { try { io.log("graf: " + e + "\n"); } catch (x) {} }
function h40(sha) { return shalib.hashlet60FromBytes(hex.decode(sha)) >> 20n; }
function packHalf(h, cnt, nib) { return (h << 24n) | (BigInt(cnt) << 4n) | nib; }

//  open(shard) -> the pair cache over ONE (store,project) shard dir.  The
//  keeper is opened by the CALLER and borrowed per call.  opts.satCap is a
//  TEST hook (small cap); production always saturates at SAT.
function open(shard, opts) {
  opts = opts || {};
  let satCap = Number(opts.satCap) || SAT;
  if (satCap > SAT || satCap < 1) satCap = SAT;

  let memIx = abc.index("wh128", { mem: MEM_SLOTS });
  let memPairs = 0;
  let roDegraded = false;      // a failed persist: stay a mem-only cache
  let diskIx;                  // undefined = not probed; null = no runs
  const stats = { walks: 0, hits: 0 };

  function disk() {
    if (diskIx === undefined) {
      diskIx = null;
      if (idxmaint.listRuns(shard, EXT).length) {
        try { diskIx = abc.index("wh128", { dir: shard, ext: EXT }); }
        catch (e) { warn(e); diskIx = null; }
      }
    }
    return diskIx;
  }

  //  Seek the contiguous span [hA<<24,(hA+1)<<24) and match B in the val's
  //  top 40 bits (the keyFor shape: numeric order == hash order).
  function scanRange(ix, ha, hb) {
    let hit = null;
    const lo = ha << 24n;
    const hi = ha >= 0xffffffffffn ? 0xffffffffffffffffn : (ha + 1n) << 24n;
    ix.range(lo, hi, function (kv) {
      if ((kv[0] & 0xfn) !== T_A) return true;
      if ((kv[1] & 0xfn) !== T_B) return true;
      if ((kv[1] >> 24n) !== hb) return true;
      hit = { ahead: Number((kv[0] >> 4n) & 0xfffffn),
              behind: Number((kv[1] >> 4n) & 0xfffffn) };
      return false;
    });
    return hit;
  }

  function seekPair(ha, hb) {
    if (ha === hb) return null;      // a 40-bit collision pair is never cached
    if (memPairs) { const h = scanRange(memIx, ha, hb); if (h) return h; }
    const d = disk();
    if (d) { const h = scanRange(d, ha, hb); if (h) return h; }
    return null;
  }

  //  One pair -> TWO mirror rows, so both orientations are a single seek:
  //  key half carries ahead(key,val), val half carries behind(key,val).
  function cachePut(ha, hb, a, b) {
    if (ha === hb) return;
    if (a > satCap) a = satCap; if (b > satCap) b = satCap;
    if (a < 0) a = 0; if (b < 0) b = 0;
    if (memPairs >= MEM_SLOTS - 64) return;   // mem-only cache is full: drop
    memIx.put(packHalf(ha, a, T_A), packHalf(hb, b, T_B));
    memIx.put(packHalf(hb, b, T_A), packHalf(ha, a, T_B));
    memPairs += 2;
    if (memPairs >= MEM_FLUSH && !roDegraded) flush();
  }

  //  Persist the memtable as ONE fresh ron60 run + fold the ladder, under the
  //  native `.lock.graf` flock.  Best-effort: failure keeps the mem cache.
  function flush() {
    if (!memPairs) return;
    const rows = [];
    memIx.range(0n, 0xffffffffffffffffn,
                function (kv) { rows.push(kv[0], kv[1]); return true; });
    if (!rows.length) return;
    let fd = -1;
    try {
      fd = io.open(join(shard, LOCK), "c");
      io.lock(fd, true);
      const ram = abc.ram("HEAPwh128", rows.length / 2 + 1);
      for (let i = 0; i < rows.length; i += 2) ram.push(rows[i], rows[i + 1]);
      ram.sort();
      idxmaint.landRun(shard, rows.length / 2, function (book) {
        abc.merge([ram], book);
        return book.buffer.watermark | 0;
      }, EXT);
      idxmaint.compactAfterAdd(shard, EXT);
    } catch (e) { warn(e); roDegraded = true; return; }
    finally {
      if (fd >= 0) {
        try { io.unlock(fd); } catch (e) {}
        try { io.close(fd); } catch (e) {}
      }
    }
    memIx = abc.index("wh128", { mem: MEM_SLOTS });
    memPairs = 0;
    diskIx = undefined;                        // re-probe: see the fresh run
  }

  //  anc(root) incl. root, with a completeness flag (cap / unreadable commit
  //  = incomplete -> the caller must not cache what it derives from it).
  function closureOf(keeper, root) {
    const ids = new Set([root]);
    const queue = [root];
    let complete = true, head = 0;
    while (head < queue.length) {
      if (ids.size > WALK_CAP) { complete = false; break; }
      let ps;
      try { ps = keeper.commitParents(queue[head++]); } catch (e) { ps = undefined; }
      if (ps === undefined) { complete = false; continue; }
      for (const p of ps)
        if (isFullSha(p) && !ids.has(p)) { ids.add(p); queue.push(p); }
    }
    return { ids: ids, complete: complete };
  }

  //  Two-tip paint BFS (GRAF-001 ruling): newest-first by committer ts; a node
  //  reached from both sides is COMMON and propagates commonness to visited
  //  parents; terminate when every queued node is common (the maximal-common-
  //  ancestor CUT) — side-exclusive paints above the cut are then exact.
  function paintWalk(keeper, A, B) {
    const nodes = new Map();     // sha -> { mask, ts, ps, expanded }
    const heap = [];             // max-heap of unexpanded nodes by ts
    let pending = 0;             // unexpanded NON-common nodes
    let complete = true;

    function hpush(n) {
      heap.push(n);
      let i = heap.length - 1;
      while (i > 0) {
        const p = (i - 1) >> 1;
        if (heap[p].ts >= heap[i].ts) break;
        const t = heap[p]; heap[p] = heap[i]; heap[i] = t; i = p;
      }
    }
    function hpop() {
      const top = heap[0], last = heap.pop();
      if (heap.length) {
        heap[0] = last;
        let i = 0;
        for (;;) {
          const l = 2 * i + 1, r = l + 1;
          let m = i;
          if (l < heap.length && heap[l].ts > heap[m].ts) m = l;
          if (r < heap.length && heap[r].ts > heap[m].ts) m = r;
          if (m === i) break;
          const t = heap[m]; heap[m] = heap[i]; heap[i] = t; i = m;
        }
      }
      return top;
    }
    //  Commonness propagates DOWN through already-visited parents (nothing
    //  below a common node is exclusive) — iterative, cycles guarded by mask.
    function commonize(n) {
      const stack = [n];
      while (stack.length) {
        const c = stack.pop();
        if (c.mask === 3) continue;
        c.mask = 3;
        if (!c.expanded) pending--;
        if (c.expanded && c.ps)
          for (const p of c.ps) {
            const pn = nodes.get(p);
            if (pn && pn.mask !== 3) stack.push(pn);
          }
      }
    }
    function visit(sha, mask) {
      const have = nodes.get(sha);
      if (have) {
        const m = have.mask | mask;
        if (m !== have.mask) { if (m === 3) commonize(have); else have.mask = m; }
        return;
      }
      let pc;
      try { pc = keeper.parseCommit(sha); } catch (e) { pc = undefined; }
      const n = { mask: mask, expanded: false,
                  ps: pc ? (pc.parents || []).filter(isFullSha) : null,
                  ts: pc ? identEpoch(pc.committer || pc.author || "") : 0 };
      if (!pc) { complete = false; n.expanded = true; }   // unreadable dead-end
      nodes.set(sha, n);
      if (!n.expanded) {
        hpush(n);
        if (n.mask !== 3) pending++;
      }
    }

    visit(A, 1);
    visit(B, 2);
    while (pending > 0 && heap.length) {
      if (nodes.size > WALK_CAP) { complete = false; break; }
      const n = hpop();
      n.expanded = true;
      if (n.mask !== 3) pending--;
      for (const p of n.ps) visit(p, n.mask);
    }

    let ahead = 0, behind = 0;
    for (const n of nodes.values()) {
      if (n.mask === 1) ahead++;
      else if (n.mask === 2) behind++;
    }
    if (ahead > satCap) ahead = satCap;
    if (behind > satCap) behind = satCap;
    return { ahead: ahead, behind: behind, complete: complete };
  }

  //  aheadBehind(keeper, shaA, shaB) -> { ahead, behind } saturating counts.
  function aheadBehind(keeper, shaA, shaB) {
    if (!isFullSha(shaA) || !isFullSha(shaB)) return { ahead: 0, behind: 0 };
    if (shaA === shaB) return { ahead: 0, behind: 0 };
    const hb = h40(shaB);
    const quick = seekPair(h40(shaA), hb);
    if (quick) { stats.hits++; return { ahead: quick.ahead, behind: quick.behind }; }
    stats.walks++;

    //  Descend A's single-parent chain to a cached (ancestor,B) pair, B
    //  itself, or a walk boundary (merge / root / unreadable / cap).
    const chain = [];
    let cur = shaA, base = null;
    for (;;) {
      if (cur === shaB) { base = { ahead: 0, behind: 0, complete: true }; break; }
      const c = seekPair(h40(cur), hb);
      if (c) { base = { ahead: c.ahead, behind: c.behind, complete: true }; break; }
      if (chain.length > WALK_CAP) break;
      let ps;
      try { ps = keeper.commitParents(cur); } catch (e) { ps = undefined; }
      if (!ps || ps.length !== 1 || !isFullSha(ps[0])) break;   // merge/root/miss
      chain.push(cur);
      cur = ps[0];
    }
    if (!base) {
      base = paintWalk(keeper, cur, shaB);
      if (base.complete) cachePut(h40(cur), hb, base.ahead, base.behind);
    }

    //  Unwind the single-parent recurrence, caching every complete step.
    let a = base.ahead, b = base.behind, complete = base.complete;
    let bClosure = null;         // lazy anc(B), for the ahead==0 membership test
    for (let i = chain.length - 1; i >= 0; i--) {
      const n = chain[i];
      if (a > 0) {
        if (a < satCap) a++;                   // saturated stays "at least"
      } else {
        if (!bClosure) bClosure = closureOf(keeper, shaB);
        if (!bClosure.complete) complete = false;
        if (bClosure.ids.has(n)) { if (b < satCap && b > 0) b--; }
        else a = 1;
      }
      if (complete) cachePut(h40(n), hb, a, b);
    }
    return { ahead: a, behind: b };
  }

  return { aheadBehind: aheadBehind, flush: flush, stats: stats };
}

module.exports = { open: open, EXT: EXT };
