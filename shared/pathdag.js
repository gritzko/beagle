//  pathdag.js — CFOLD-001: the CONDENSED per-path commit DAG, the JS twin of
//  the BLAME-001 top-down OID descent (`1a5acce8`).  The EXPECTED reading used
//  to replay a path's WHOLE commit ancestry per tip per path, reading every
//  commit's tree through a `readTreeRecursive` flatten; here the walk is
//  floored at the LCA of the tips and every commit that does NOT change the
//  path is PRUNED on an equal tree sha — before any object below it is read.
//  What survives is exactly what `weave.buildDag` folds.
//
//    of(reader, path, tips[], cache, opts)
//      → { floor, seed:{sha,mode}|undefined, nodes:{sha->{blob,mode,parents[]}},
//          order:[sha] (topo, oldest-first), tips:{tip->[retained sha]|null},
//          truncated }
//      `opts.floor === "lca"` floors the walk at the tips' merge base and
//      SEEDS the weave with the path's blob there — correct for EXPECTED /
//      patch (only alive bytes are read), WRONG for `why:`/blame attribution,
//      which passes no floor and gets the prune without the seed.
//      `tips[t]` is the tip's RETAINED representatives (its own sha when the
//      tip changes the path, else its nearest retained ancestors, else `[]` =
//      the seed itself); `null` marks an unreadable tip.
//    at(reader, commitSha, path, cache)   → {sha,mode,kind}|undefined
//    same(reader, path, treeA, treeB, cache) → bool (the prune)
//    cache()                              → the per-RUN memo
//
//  Reuse ([/wiki/ABC]): descent is store.js `descendPath` one segment at a
//  time (memoised per (treeSha, segment), so hits cross paths AND tips), the
//  LCA is dag.js `mergeBase`, the walk cap is dag.js `WALK_CAP`.

"use strict";

const dag = require("./dag.js");
const shalib = require("./util/sha.js");
const isFullSha = shalib.isFullSha;

//  cache(): the per-RUN memo.  `step` is keyed by TREE sha (not commit sha) so
//  a hit crosses paths and tips; the rest memoise the commit spine + the walk.
function cache() {
  return { step: Object.create(null),    // treeSha "\t" seg -> entry|null
           ents: Object.create(null),    // treeSha -> readTree entries|null
           commit: Object.create(null),  // commitSha -> { tree, parents }|null
           walk: Object.create(null),    // tipsKey -> { floor, set, order }
           reader: null,                 // the readTree-memoising reader view
           //  the legacy weave.build fallback (a commit-keyed treeMap cache)
           treeCache: Object.create(null) };
}

//  A reader VIEW whose `readTree` is memoised by tree sha (the BLAME-001 Step 3
//  `(tree_sha,name)->child` cache): descendPath calls `this.readTree`, so one
//  tree object is inflated ONCE per run however many paths step through it.
function memoReader(reader, c) {
  if (c.readerOf === reader) return c.reader;
  //  A cache is per RUN per STORE; a different reader (a recursed sub) starts
  //  its own object memos rather than inheriting another store's misses.
  if (c.readerOf) { c.step = Object.create(null); c.ents = Object.create(null);
                    c.commit = Object.create(null); c.walk = Object.create(null); }
  c.readerOf = reader;
  const view = Object.create(reader);
  view.readTree = function (sha) {
    let e = c.ents[sha];
    if (e === undefined) { e = reader.readTree(sha) || null; c.ents[sha] = e; }
    return e || undefined;
  };
  c.reader = view;
  return view;
}

//  A commit's { tree, parents }, read ONCE per run (one getObject, not two).
function commitOf(reader, c, sha) {
  let m = c.commit[sha];
  if (m !== undefined) return m;
  let pc;
  try { pc = reader.parseCommit(sha); } catch (e) { pc = undefined; }
  m = pc ? { tree: pc.tree, parents: (pc.parents || []).filter(isFullSha) } : null;
  c.commit[sha] = m;
  return m;
}
function treeOf(reader, c, sha) { const m = commitOf(reader, c, sha); return m ? m.tree : undefined; }
function parentsOf(reader, c, sha) { const m = commitOf(reader, c, sha); return m ? m.parents : []; }

//  ONE '/'-segment step through a tree, memoised on (treeSha, segment) — the
//  store's descendPath is the descender, never a hand-rolled tree walker.
function step(reader, c, treeSha, seg) {
  const k = treeSha + "\t" + seg;
  let e = c.step[k];
  if (e !== undefined) return e || undefined;
  let hit;
  try { hit = memoReader(reader, c).descendPath(treeSha, [seg]); } catch (x) { hit = undefined; }
  c.step[k] = hit || null;
  return hit;
}

//  path -> its '/'-segments (a worktree-relative path, already `safeRel`).
function segsOf(path) {
  const out = [];
  for (const s of String(path || "").split("/")) if (s !== "" && s !== ".") out.push(s);
  return out;
}

//  The path's LEAF under a tree: {sha,mode,kind:"f"|"x"|"l"}, or undefined when
//  absent, a directory, or a gitlink (the blobShaAt exclusion it replaces).
function leafAt(reader, c, treeSha, segs) {
  if (!treeSha) return undefined;
  let cur = { sha: treeSha, mode: 0o40000, kind: "tree" };
  for (const seg of segs) {
    if (cur.kind !== "tree") return undefined;
    const e = step(reader, c, cur.sha, seg);
    if (!e) return undefined;
    cur = e;
  }
  if (cur.kind === "tree" || cur.kind === "commit") return undefined;
  return { sha: cur.sha, mode: cur.mode,
           kind: cur.kind === "exe" ? "x" : cur.kind === "link" ? "l" : "f" };
}

//  at(reader, commitSha, path, cache): the path's leaf AT a commit — O(depth)
//  memoised steps, replacing blobShaAt + the whole-tree treeMap flatten.
function at(reader, commitSha, path, c) {
  c = c || cache();
  memoReader(reader, c);                    // bind the cache to this store
  return leafAt(reader, c, treeOf(reader, c, commitSha), segsOf(path));
}

//  same(reader, path, treeA, treeB, cache): do two trees carry the SAME content
//  at `path`?  A lockstep descent that STOPS at the first equal child sha (the
//  prune — no object below it is ever read); absent on both sides counts equal.
function same(reader, path, treeA, treeB, c) {
  if (treeA === treeB) return true;                  // equal roots: nothing moved
  if (!treeA || !treeB) return false;
  let a = { sha: treeA, kind: "tree" }, b = { sha: treeB, kind: "tree" };
  for (const seg of segsOf(path)) {
    const ea = a.kind === "tree" ? step(reader, c, a.sha, seg) : undefined;
    const eb = b.kind === "tree" ? step(reader, c, b.sha, seg) : undefined;
    if (!ea && !eb) return true;                     // absent on both sides
    if (!ea || !eb) return false;
    if (ea.sha === eb.sha) return true;              // equal subtree/leaf: prune
    a = ea; b = eb;
  }
  return false;
}

//  A keeper-shaped shim over the memoised parents, so dag.js (mergeBase /
//  ancestors / topoSort) walks the SAME commit reads as everything else.
function shim(reader, c) {
  return { commitParents: function (sha) { return parentsOf(reader, c, sha); } };
}

//  The WALK for a tip set — the tips' LCA floor (GET-047 dag.mergeBase, folded
//  pairwise for N tips), the closure below it, and the commits above it in topo
//  order.  Depends only on the TIPS, so it is computed ONCE per run, not per
//  path.  Returns { floor, set, order, truncated }.
function walkOf(reader, c, tips, wantFloor) {
  const k = (wantFloor ? "lca:" : ":") + tips.join(",");
  let w = c.walk[k];
  if (w !== undefined) return w;
  const sh = shim(reader, c);
  let floor = "";
  if (wantFloor && tips.length > 1) {
    floor = tips[0];
    for (let i = 1; i < tips.length && floor; i++) floor = dag.mergeBase(sh, floor, tips[i]);
  }
  const below = floor ? dag.ancestors(sh, floor) : null;
  //  BFS up from each tip, never entering the floor closure.
  const set = new Set(), queue = [];
  let truncated = false;
  for (const t of tips)
    if (!(below && below.has(t)) && !set.has(t)) { set.add(t); queue.push(t); }
  for (let head = 0; head < queue.length; head++) {
    if (set.size > dag.WALK_CAP) { truncated = true; break; }
    for (const p of parentsOf(reader, c, queue[head])) {
      if (set.has(p) || (below && below.has(p))) continue;
      set.add(p); queue.push(p);
    }
  }
  w = { floor: floor || "", set: set, truncated: truncated,
        order: dag.topoSort(sh, set) };        // parents before children
  c.walk[k] = w;
  return w;
}

//  of(): the condensed DAG — see the header.
function of(reader, path, tips, c, opts) {
  opts = opts || {};
  c = c || cache();
  memoReader(reader, c);                    // bind the cache to this store
  const uniq = [], seen = Object.create(null);
  for (const t of (tips || []))
    if (isFullSha(t) && !seen[t]) { seen[t] = 1; uniq.push(t); }
  //  1. the (per-run, tips-keyed) walk: the LCA floor + the commits above it.
  const w = walkOf(reader, c, uniq, opts.floor === "lca");
  const floor = w.floor, set = w.set, order = w.order;

  //  2. prune + collapse: a commit is RETAINED only when it carries the path
  //  differently from an in-set parent (or from the floor / nothing, at the
  //  bottom of the walk); a pruned commit's edges pass through to its own
  //  representatives, so a retained node's parents are retained nodes.
  const segs = segsOf(path);
  const floorTree = floor ? treeOf(reader, c, floor) : undefined;
  const nodes = Object.create(null), retained = [], rep = Object.create(null);
  for (const sha of order) {
    const tree = treeOf(reader, c, sha);
    const ps = [];
    for (const p of parentsOf(reader, c, sha)) if (set.has(p)) ps.push(p);
    const pr0 = [];
    for (const p of ps) for (const r of (rep[p] || [])) if (pr0.indexOf(r) < 0) pr0.push(r);
    //  An unreadable commit (a shallow shard) carries its parents, never a
    //  synthetic delete — the `foldCommit` "unreadable → carry the parent" rule.
    if (!tree) { rep[sha] = pr0; continue; }
    let changed;
    if (!ps.length) changed = floor ? !same(reader, path, tree, floorTree, c)
                                    : !!leafAt(reader, c, tree, segs);
    else {
      changed = false;
      for (const p of ps)
        if (!same(reader, path, tree, treeOf(reader, c, p), c)) { changed = true; break; }
    }
    if (!changed) { rep[sha] = pr0; continue; }
    const leaf = leafAt(reader, c, tree, segs);
    nodes[sha] = { blob: leaf ? leaf.sha : undefined,
                   mode: leaf ? leaf.mode : undefined, parents: pr0 };
    retained.push(sha);
    rep[sha] = [sha];
  }

  const tipRep = Object.create(null);
  for (const t of uniq) tipRep[t] = commitOf(reader, c, t) ? (rep[t] || []) : null;
  return { floor: floor, seed: floor ? leafAt(reader, c, floorTree, segs) : undefined,
           nodes: nodes, order: retained, tips: tipRep, truncated: w.truncated };
}

module.exports = { cache: cache, of: of, at: at, same: same };
