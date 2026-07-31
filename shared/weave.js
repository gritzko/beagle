//  weave.js — CFOLD fold/merge bindings (DIS-082), the ONE source-size policy,
//  AND the file-weave RECONSTRUCTION (DIFF-010).  A CFOLD weave is ONE append-
//  only weave per file for its WHOLE commit DAG: a commit folds once with its
//  ANCESTOR closure (ids of commits already in the weave), a merge commit
//  carries no content, and any rev reads back with produce(rev).  The old
//  per-commit weave array and the pairwise WEAVEMerge cascade are gone.
//  We cap the SOURCE we tokenise at MAX_SOURCE_SIZE; anything bigger is a BLOB
//  (callers skip tokenising/diffing it).  Because the source is capped, its
//  markup is too — so every weave/HUNK/render buffer is allocated ONCE at the
//  fixed MAX_SOURCE_MARKED_UP (a lazy anonymous mmap, abc.ram/io.ram — only
//  touched pages fault in), never grown dynamically.

"use strict";

const stats = require("./util/stats.js");   // CFOLD-001: env-gated fold counters

//  A source larger than this is a BLOB: not tokenised, not diffed (callers gate
//  on it like the binary check).  One place sets it; everyone imports it.
const MAX_SOURCE_SIZE = 4 << 20;                  // 4 MB
//  A tokenised source runs larger than its raw bytes; 4x covers the worst real
//  case (a fully-changed 2-layer diff measures ~3.3x).  Buffers are this size.
const MAX_SOURCE_MARKED_UP = MAX_SOURCE_SIZE * 4; // 16 MB

//  fold(base, blob, ext, hash, ancestors): one CFOLDFold into a fresh fixed
//  buffer.  `ancestors` lists the hashlets of the commit's whole causal
//  closure among the commits already folded (itself excluded); everything
//  folded and NOT named lands in the new commit's ignore-set.  `blob` is a
//  source ≤ MAX_SOURCE_SIZE (the caller gates blobs out first).
function fold(base, blob, ext, hash, ancestors) {
  stats.bump("fold");
  const w = abc.ram("CFOLD", MAX_SOURCE_MARKED_UP);
  w.fold(base, blob, ext, hash, ancestors || []);
  return w;
}

//  merge(base, hash, ancestors): a CONTENTLESS merge commit — appends nothing,
//  records the union view of `ancestors` (the intersected ignore-set).
function merge(base, hash, ancestors) {
  stats.bump("merge");
  const w = abc.ram("CFOLD", MAX_SOURCE_MARKED_UP);
  w.merge(base, hash, ancestors || []);
  return w;
}

function bytesEq(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

//  PATCH-025 (DIS-080): the MARKERLESS merged render — the RGA reading of the
//  weave at `rev` (every alive token in document order, NO fences).
//  `groupIds` is one hashlet-id array per side; returns { bytes, spans } —
//  spans are the [from,to) byte ranges that conflict.  Membership is BLAME:
//  the cursor yields body offsets (identity), blame(off) names the inserter.
//  A run of non-shared tokens CONFLICTS iff two of its membership masks are
//  disjoint; a conflicting run whose groups spell EQUAL bytes collapses to
//  one copy (content re-absorbed under another birth id — never a conflict).
function mergedLive(wm, rev, groupIds) {
  const ng = groupIds.length;
  const spine = ng >= 32 ? 0xFFFFFFFF : ((1 << ng) - 1);
  const sets = groupIds.map(function (g) { return new Set(g); });
  const text = [], mask = [], live = [];
  wm.rewind(rev);
  while (wm.next()) {
    const t = wm.tok;
    const ins = wm.blame(t.off);
    let m = 0;
    for (let g = 0; g < ng; g++) if (sets[g].has(ins)) m |= (1 << g);
    text.push(t.text); mask.push(m); live.push(!!t.alive);
  }

  const n = text.length, parts = [], spans = [];
  let at = 0;
  function put(b) { parts.push(b); at += b.length; }
  //  the alive bytes of one membership mask within [lo,hi), concatenated.
  function gather(lo, hi, m) {
    let len = 0;
    for (let j = lo; j < hi; j++) if (live[j] && mask[j] === m) len += text[j].length;
    const b = new Uint8Array(len);
    let o = 0;
    for (let j = lo; j < hi; j++)
      if (live[j] && mask[j] === m) { b.set(text[j], o); o += text[j].length; }
    return b;
  }

  let i = 0;
  while (i < n) {
    if (!live[i]) { i++; continue; }
    if (mask[i] === spine) { put(text[i]); i++; continue; }
    //  Divergent run: spans until the next shared token (dead tokens ride along).
    let hi = i;
    while (hi < n && !(live[hi] && mask[hi] === spine)) hi++;
    const seen = [];
    for (let j = i; j < hi; j++)
      if (live[j] && seen.indexOf(mask[j]) < 0) seen.push(mask[j]);
    let clash = false;
    for (let a = 0; a < seen.length && !clash; a++)
      for (let b = a + 1; b < seen.length; b++)
        if ((seen[a] & seen[b]) === 0) { clash = true; break; }
    if (clash && seen.length >= 2) {
      const g0 = gather(i, hi, seen[0]);
      let allEq = true;
      for (let g = 1; g < seen.length && allEq; g++)
        if (!bytesEq(g0, gather(i, hi, seen[g]))) allEq = false;
      if (allEq) { put(g0); i = hi; continue; }   // re-absorbed, not a conflict
    }
    const from = at;
    for (let j = i; j < hi; j++) if (live[j]) put(text[j]);
    if (clash) spans.push({ from: from, to: at });
    i = hi;
  }

  const bytes = new Uint8Array(at);
  let o = 0;
  for (const p of parts) { bytes.set(p, o); o += p.length; }
  return { bytes: bytes, spans: spans };
}

//  BE-010: the synthetic revision id for the wt's on-disk edit, folded onto the
//  OURS side of a per-file weave (mirrors native WEAVE_WT_SRC in graf/GET.c).  A
//  reserved 16-hex hashlet that never collides with a real commit id (the hi64
//  of a sha1), so its tokens read as an ours-side edit under blame.
const WT_SRC = "00000000005774ed";

//  The reserved id for the ours/theirs JOIN merge commit (a contentless
//  weave.merge over both sides' ids) — patch/expected/classify share it.
const JOIN_ID = "0000000000000000";

//  BE-010: fold the wt's on-disk `bytes` as a FINAL synthetic WT_SRC revision
//  layer over the ours view `rev` (whose visible ids are `ids`) — the ours side
//  reflects the wt's CURRENT bytes, not just the ours COMMIT's history (the
//  DEEP part: build() reconstructs from commits only and never reads disk).
//  Skip when the bytes match the ours view (caller keeps the commit weave) or
//  overflow the source cap.  Returns { weave, layered }: `layered` true when a
//  layer was added (the caller then uses WT_SRC as the ours rev and adds it to
//  the ours ids).
function foldWt(oursWeave, rev, ids, bytes, ext) {
  if (!oursWeave || rev == null || bytes == null)
    return { weave: oursWeave, layered: false };
  if (bytes.length > MAX_SOURCE_SIZE) return { weave: oursWeave, layered: false };
  //  adjacent-equal skip: wt identical to the ours view => no synthetic layer.
  const prev = io.ram(MAX_SOURCE_MARKED_UP);
  oursWeave.produce(rev, prev);
  if (bytesEq(prev.data(), bytes)) return { weave: oursWeave, layered: false };
  return { weave: fold(oursWeave, bytes, ext, WT_SRC, Array.from(ids)),
           layered: true };
}

//  ===========================================================================
//  File-weave RECONSTRUCTION — replay a file's whole commit-DAG closure into
//  ONE weave, folding each commit once after its parents.  `reader` is the
//  store reader (commitParents/commitTree/readTreeRecursive/getObject) —
//  nothing store-specific lives in this module; the reader is a param.
//  ===========================================================================

//  A weave commit id is the hi64 of the sha1 — a 16-hex hashlet (cfold.hpp
//  JABCcfoldHi64).  The SAME physical commit always yields the SAME id, so
//  shared history coincides by construction — one weave, no identity join.
function weaveId(sha) { return sha.slice(0, 16); }

//  file extension (tail after the last '.') — the weave tokenizer selector; no
//  dot → "" (generic).
function extOf(path) {
  const slash = path.lastIndexOf("/");
  const base = slash < 0 ? path : path.slice(slash + 1);
  const dot = base.lastIndexOf(".");
  return dot <= 0 ? "" : base.slice(dot + 1);
}

//  GET-056b: D5 3-blob weave merge (GRAFMerge3Bytes twin) — base, then ours
//  and theirs as CONCURRENT folds on it (each diffs against the base, NOT
//  sequentially), then a contentless merge over all three.  Disjoint edits
//  coexist cleanly; a divergent region reads back markerless with conflict
//  spans (PATCH-025).  Returns null for an unweavable input (over the source
//  cap) — the caller refuses LOUDLY, never silent-ours.
const _W3_BASE = "0000000000000001", _W3_OURS = "0000000000000002",
      _W3_THRS = "0000000000000003", _W3_MRG = "0000000000000004";
function weave3(base, ours, theirs, ext) {
  base = base || new Uint8Array(0);
  //  PATCH-025 (DIS-080): trivial resolutions carry no conflict spans.
  const clean = function (b) { return { bytes: b, spans: [] }; };
  if (bytesEq(ours, theirs)) return clean(ours);   // same edit both sides
  if (bytesEq(ours, base)) return clean(theirs);   // only theirs changed
  if (bytesEq(theirs, base)) return clean(ours);   // only ours changed
  //  PATCH-012: over the shared source cap is a BLOB — not weavable.
  if (base.length > MAX_SOURCE_SIZE ||
      ours.length > MAX_SOURCE_SIZE ||
      theirs.length > MAX_SOURCE_SIZE) return null;
  const wb = fold(null, base, ext, _W3_BASE, []);
  const wo = fold(wb, ours, ext, _W3_OURS, [_W3_BASE]);
  const wt = fold(wo, theirs, ext, _W3_THRS, [_W3_BASE]);   // concurrent w/ ours
  const wm = merge(wt, _W3_MRG, [_W3_BASE, _W3_OURS, _W3_THRS]);
  return mergedLive(wm, _W3_MRG, [[_W3_BASE, _W3_OURS], [_W3_BASE, _W3_THRS]]);
}

//  A commit's tree flattened to { path -> { sha, mode, kind } } over every leaf.
function treeMap(reader, commitSha) {
  const map = Object.create(null);
  if (!commitSha) return map;
  let treeSha;
  try { treeSha = reader.commitTree(commitSha); } catch (e) { return map; }
  if (!treeSha) return map;
  reader.readTreeRecursive(treeSha, function (leaf) {
    map[leaf.path] = { sha: leaf.sha, mode: leaf.mode, kind: leaf.kind };
  });
  return map;
}

//  blob bytes for a leaf sha (Uint8Array); undefined for a missing object or a
//  non-blob (gitlink).  A symlink blob's bytes are the link target.
function blobBytes(reader, sha) {
  if (!sha) return undefined;
  const obj = reader.getObject(sha);
  if (!obj || obj.type !== "blob") return undefined;
  return obj.bytes;
}

//  the file's blob sha at a commit's tree, or undefined (absent / gitlink); the
//  tree is read once per commit via `treeCache`.
function blobShaAt(reader, treeCache, commitSha, path) {
  let map = treeCache[commitSha];
  if (map === undefined) { map = treeMap(reader, commitSha); treeCache[commitSha] = map; }
  const leaf = map[path];
  if (!leaf || leaf.kind === "s") return undefined;   // absent or gitlink
  return leaf.sha;
}

//  Fold ONE commit into the ONE weave, its parents already processed.  Per sha
//  the ctx tracks: closure[sha] = Set of RECORDED ids visible at sha, revOf[sha]
//  = the recorded id representing sha's view (null = file not there yet),
//  hasBytes[sha] = view non-empty.  A commit RECORDS only when it changes the
//  view (content fold, delete fold) or joins two differing views (contentless
//  merge); everything else CARRIES the parent — long untouched stretches cost
//  no commit records at all.  Stamps ctx.idToSha[weaveId(sha)] = sha (blame).
//  CFOLD-001: `opt` lets a CONDENSED walk (pathdag) supply what it already
//  knows — `opt.parents` the retained parents, `opt.node.blob` the path's blob
//  sha there (undefined = absent/delete, null = carries its parents' bytes) —
//  so no tree is read here, and `opt.known` drops the adjacent-equal produce a
//  retained node can never hit (by construction it differs from its parents).
function foldCommit(ctx, sha, opt) {
  const reader = ctx.reader, path = ctx.path, ext = ctx.ext;
  ctx.idToSha[weaveId(sha)] = sha;

  let parents;
  if (opt && opt.parents) parents = opt.parents;
  else {
    try { parents = reader.commitParents(sha); } catch (e) { parents = undefined; }
  }
  parents = (parents || []).filter(function (p) { return ctx.done[p]; });

  //  the union closure this commit descends from, and the distinct parent views
  const anc = new Set();
  const prevs = [];
  let hasBytes = false;
  for (const p of parents) {
    for (const id of ctx.closure[p]) anc.add(id);
    const r = ctx.revOf[p];
    if (r != null && prevs.indexOf(r) < 0) prevs.push(r);
    if (ctx.hasBytes[p]) hasBytes = true;
  }
  const ancArr = Array.from(anc);
  const id = weaveId(sha);

  //  defaults: CARRY the first parent's view
  let rev = prevs.length ? prevs[0] : null;
  let recorded = false;

  const record = function () {
    ctx.closure[sha] = new Set(anc); ctx.closure[sha].add(id);
    ctx.revOf[sha] = id;
    recorded = true;
  };
  const carry = function () {
    ctx.closure[sha] = anc;   // no own record: the parents' union suffices
    ctx.revOf[sha] = rev;
  };

  const blobSha = (opt && opt.node) ? opt.node.blob
                                    : blobShaAt(reader, ctx.treeCache, sha, path);
  if (blobSha === null) {
    //  CFOLD-001: a PRUNED commit (a tip whose retained parents carry its
    //  bytes): no content of its own — carry one view, join several (the tail).
    carry();
  } else if (blobSha === undefined) {
    if (prevs.length === 0 || !hasBytes) { carry(); }       // never/no longer there
    else {
      //  DELETE relative to the (possibly multi-parent) view: fold empty.
      ctx.w = fold(ctx.w, new Uint8Array(0), ext, id, ancArr);
      hasBytes = false;
      record();
    }
  } else {
    const bytes = blobBytes(reader, blobSha);
    if (bytes === undefined || bytes.length > MAX_SOURCE_SIZE) {
      //  unreadable / BLOB: not woven.  Two differing parent views still need
      //  a join so descendants and produce() have ONE rev to stand on.
      if (prevs.length >= 2) { ctx.w = merge(ctx.w, id, ancArr); record(); }
      else carry();
    } else {
      //  adjacent-equal skip: identical to the single inherited view => carry.
      let same = false;
      if (prevs.length === 1 && !(opt && opt.known)) {
        const prev = io.ram(MAX_SOURCE_MARKED_UP);
        ctx.w.produce(rev, prev);
        same = bytesEq(prev.data(), bytes);
      }
      if (same) carry();
      else {
        ctx.w = fold(ctx.w, bytes, ext, id, ancArr);
        hasBytes = bytes.length > 0;
        record();
      }
    }
  }
  if (!recorded && prevs.length >= 2 && hasBytes) {
    //  differing non-empty parent views but nothing recorded above: join
    //  them so the union view exists as ONE rev to produce/descend from.
    ctx.w = merge(ctx.w, id, ancArr);
    ctx.closure[sha] = new Set(anc); ctx.closure[sha].add(id);
    ctx.revOf[sha] = id;
  }
  ctx.hasBytes[sha] = hasBytes;
  ctx.done[sha] = true;
}

//  A reconstruction context: reader, file path, its ext, the caches, and THE
//  one weave.  Pass a shared `treeCache` (and reuse the returned ctx) to fold
//  a shared ancestor once across several tips (patch's ours/theirs share one
//  ctx per file — and now one WEAVE, so shared history dedups by construction).
function makeCtx(reader, path, treeCache) {
  return { reader: reader, path: path, ext: extOf(path),
           treeCache: treeCache || Object.create(null),
           w: null,
           done: Object.create(null),
           closure: Object.create(null),
           revOf: Object.create(null),
           hasBytes: Object.create(null),
           idToSha: Object.create(null) };
}

//  Build the file weave AS OF `tip`: an iterative two-colour post-order DFS over
//  parent edges (deep chains stay off the JS stack), folding each commit only
//  after its parents.  Returns { weave, rev, ids, idToSha, ctx }: `weave`
//  undefined when the file never existed as of `tip`; `rev` = the recorded id
//  representing the tip's view (produce/emit with it); `ids` = Set of recorded
//  hashlets visible at the tip (mergedLive group membership); `idToSha` =
//  hashlet -> sha40 (blame click); `ctx` reusable for another tip.
function build(reader, path, tip, ctx) {
  ctx = ctx || makeCtx(reader, path);
  const WHITE = 0, GREY = 1;
  const colour = Object.create(null);
  const stack = [tip];
  while (stack.length) {
    const sha = stack[stack.length - 1];
    if (ctx.done[sha]) { stack.pop(); continue; }
    const c = colour[sha] || WHITE;
    if (c === WHITE) {
      colour[sha] = GREY;
      let parents;
      try { parents = reader.commitParents(sha); } catch (e) { parents = undefined; }
      parents = parents || [];
      let pending = false;
      for (const p of parents)
        if (p && !ctx.done[p] && colour[p] !== GREY) { stack.push(p); pending = true; }
      if (pending) continue;
    }
    stack.pop();
    if (ctx.done[sha]) continue;
    foldCommit(ctx, sha);
  }
  const rev = ctx.revOf[tip];
  return { weave: rev == null ? undefined : ctx.w,
           rev: rev == null ? undefined : rev,
           ids: new Set(rev == null ? [] : ctx.closure[tip]),
           idToSha: ctx.idToSha, ctx: ctx };
}

//  CFOLD-001: the view a PRUNED tip stands on — its retained representatives
//  (`pd.tips[tip]`), or the floor when it has none.  Several differing reps
//  join under the tip's own id (foldCommit's contentless-merge tail).
function viewAt(ctx, pd, tip) {
  const reps = pd.tips[tip] || [];
  const src = reps.length ? reps
            : (pd.floor && ctx.done[pd.floor] ? [pd.floor] : []);
  let rev = null;
  if (src.length === 1) rev = ctx.revOf[src[0]];
  else if (src.length > 1) {
    if (!ctx.done[tip])
      foldCommit(ctx, tip, { parents: src, node: { blob: null }, known: true });
    rev = ctx.revOf[tip];
  }
  const home = src.length === 1 ? src[0] : tip;
  return { weave: rev == null ? undefined : ctx.w,
           rev: rev == null ? undefined : rev,
           ids: new Set(rev == null ? [] : ctx.closure[home]),
           idToSha: ctx.idToSha, ctx: ctx };
}

//  CFOLD-001: fold ONE condensed path-DAG (pathdag.of) into THE one weave.
//  Every RETAINED node folds exactly once — shared history is folded once for
//  all tips — over a seed: the path's blob at the LCA floor, folded as the
//  floor commit itself.  `blobShaAt`/`treeMap` never run on this path: a node
//  hands its blob sha straight to blobBytes.  Returns { ctx, at(tip) }, `at`
//  shaped exactly like build()'s result.
function buildDag(reader, path, pd, ctx) {
  ctx = ctx || makeCtx(reader, path);
  if (pd.floor && pd.seed && pd.seed.sha && !ctx.done[pd.floor])
    foldCommit(ctx, pd.floor,
               { parents: [], node: { blob: pd.seed.sha }, known: true });
  for (const sha of pd.order) {
    if (ctx.done[sha]) continue;
    const n = pd.nodes[sha];
    let ps = n.parents || [];
    if (!ps.length && pd.floor && ctx.done[pd.floor]) ps = [pd.floor];
    foldCommit(ctx, sha, { parents: ps, node: n, known: true });
  }
  return { ctx: ctx, at: function (tip) { return viewAt(ctx, pd, tip); } };
}

module.exports = { fold: fold, merge: merge,
  //  GET-056b: the one 3-blob weave merge (get.js + checkout.js share it).
  weave3: weave3,
  //  PATCH-025: the markerless (RGA live bytes + conflict spans) merged render.
  mergedLive: mergedLive,
  MAX_SOURCE_SIZE: MAX_SOURCE_SIZE, MAX_SOURCE_MARKED_UP: MAX_SOURCE_MARKED_UP,
  //  DIFF-010: file-weave reconstruction, now ONE weave per file.
  build: build, makeCtx: makeCtx, weaveId: weaveId, extOf: extOf,
  //  CFOLD-001: the condensed-DAG fold (the EXPECTED reading's hot path).
  buildDag: buildDag,
  treeMap: treeMap, blobBytes: blobBytes, blobShaAt: blobShaAt,
  //  BE-010: the wt-on-disk edit fold-layer (mirrors native WEAVE_WT_SRC).
  foldWt: foldWt, WT_SRC: WT_SRC, JOIN_ID: JOIN_ID };
