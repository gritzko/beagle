//  weave.js — WEAVE fold/merge bindings, the ONE source-size policy, AND the
//  file-weave RECONSTRUCTION (DIFF-010).  The C WEAVE builders (fold/merge/emit*)
//  tokenise a source into a marked-up buffer.  We cap the SOURCE we tokenise at
//  MAX_SOURCE_SIZE; anything bigger is a BLOB (callers skip tokenising/diffing it).
//  Because the source is capped, its markup is too — so every WEAVE/HUNK/render
//  buffer is allocated ONCE at the fixed MAX_SOURCE_MARKED_UP (a lazy anonymous
//  mmap, abc.ram/io.ram — only touched pages fault in), never grown dynamically.
//  The reconstruction (build) replays a file's commit-DAG closure through
//  fold/merge — the ONE copy `patch.js` (ours/theirs merge) and `why:` (blame) share.

"use strict";

//  A source larger than this is a BLOB: not tokenised, not diffed (callers gate
//  on it like the binary check).  One place sets it; everyone imports it.
const MAX_SOURCE_SIZE = 4 << 20;                  // 4 MB
//  A tokenised source runs larger than its raw bytes; 4x covers the worst real
//  case (a fully-changed 2-layer diff measures ~3.3x).  Buffers are this size.
const MAX_SOURCE_MARKED_UP = MAX_SOURCE_SIZE * 4; // 16 MB

//  fold(base, blob, ext, hash): a WEAVENext fold into a fresh fixed WEAVE buffer.
//  `blob` is a source ≤ MAX_SOURCE_SIZE (the caller gates blobs out first).
function fold(base, blob, ext, hash) {
  const w = abc.ram("WEAVE", MAX_SOURCE_MARKED_UP);
  w.fold(base, blob, ext, hash);
  return w;
}

//  merge(a, b, hash): a WEAVEMerge into a fresh fixed WEAVE buffer.
function merge(a, b, hash) {
  const w = abc.ram("WEAVE", MAX_SOURCE_MARKED_UP);
  w.merge(a, b, hash);
  return w;
}

//  BE-010: the synthetic revision id for the wt's on-disk edit, folded onto the
//  OURS side of a per-file weave (mirrors native WEAVE_WT_SRC in graf/GET.c).  A
//  reserved 16-hex hashlet that never collides with a real commit id (the hi64
//  of a sha1), so its tokens read as an ours-side edit under `scope`.
const WT_SRC = "00000000005774ed";

//  BE-010: fold the wt's on-disk `bytes` onto the OURS-side weave as a FINAL
//  synthetic WT_SRC revision layer — the ours side reflects the wt's CURRENT
//  bytes (uncommitted user edits / a prior absorption), not just the ours
//  COMMIT's history (the DEEP part: build() reconstructs from commits only and
//  never reads disk).  Mirrors graf_fold_wt_layer: skip when the bytes match the
//  ours tip alive view (native's used_next==NO → caller keeps the commit weave)
//  or overflow the source cap.  Returns { weave, layered }: `layered` true when a
//  layer was added (the caller then adds WT_SRC to the ours scope/ids).
function foldWt(oursWeave, bytes, ext) {
  if (!oursWeave || bytes == null) return { weave: oursWeave, layered: false };
  if (bytes.length > MAX_SOURCE_SIZE) return { weave: oursWeave, layered: false };
  //  adjacent-equal skip: wt identical to the ours tip => no synthetic layer.
  const prev = io.ram(MAX_SOURCE_MARKED_UP);
  oursWeave.alive(prev);
  if (bytesEq(prev.data(), bytes)) return { weave: oursWeave, layered: false };
  return { weave: fold(oursWeave, bytes, ext, WT_SRC), layered: true };
}

//  ===========================================================================
//  File-weave RECONSTRUCTION — replay a file's whole commit-DAG closure through
//  fold/merge to build its attribution weave AS OF a tip.  Was duplicated in
//  patch.js (buildSideWeave/foldCommit) and shared/fileweave.js; unified here.
//  `reader` is the store reader (commitParents/commitTree/readTreeRecursive/
//  getObject) — nothing store-specific lives in this module; the reader is a param.
//  ===========================================================================

//  A weave commit id is the hi64 of the sha1 — a 16-hex hashlet (weave.hpp
//  JABCweaveHi64).  The SAME physical commit always yields the SAME id, so shared
//  history coincides and a union dedups by identity.
function weaveId(sha) { return sha.slice(0, 16); }

//  file extension (tail after the last '.') — the weave tokenizer selector; no
//  dot → "" (generic).
function extOf(path) {
  const slash = path.lastIndexOf("/");
  const base = slash < 0 ? path : path.slice(slash + 1);
  const dot = base.lastIndexOf(".");
  return dot <= 0 ? "" : base.slice(dot + 1);
}

//  GET-056b (moved from verbs/get/get.js): D5 3-blob weave merge
//  (GRAFMerge3Bytes twin) — build the OURS and THEIRS weaves INDEPENDENTLY off
//  a shared base (each base→side), then WEAVEMerge them; the shared base tokens
//  carry the SAME base commit-id so they coincide and dedup, and each side's
//  edit diffs against the base (NOT sequentially).  Disjoint edits coexist
//  cleanly; a divergent region gets the standard conflict markers.  Returns
//  null for an unweavable input (over the source cap) — the caller refuses
//  LOUDLY, never silent-ours.
const _W3_BASE = "0000000000000001", _W3_OURS = "0000000000000002",
      _W3_THRS = "0000000000000003", _W3_MRG = "0000000000000004";
function _w3eq(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}
function weave3(base, ours, theirs, ext) {
  base = base || new Uint8Array(0);
  if (_w3eq(ours, theirs)) return ours;          // same edit both sides
  if (_w3eq(ours, base)) return theirs;          // only theirs changed
  if (_w3eq(theirs, base)) return ours;          // only ours changed
  //  PATCH-012: over the shared source cap is a BLOB — not weavable.
  if (base.length > MAX_SOURCE_SIZE ||
      ours.length > MAX_SOURCE_SIZE ||
      theirs.length > MAX_SOURCE_SIZE) return null;
  const wo = fold(fold(null, base, ext, _W3_BASE), ours,   ext, _W3_OURS);
  const wt = fold(fold(null, base, ext, _W3_BASE), theirs, ext, _W3_THRS);
  const wm = merge(wo, wt, _W3_MRG);
  const oScope = wm.scope([_W3_BASE, _W3_OURS]);
  const tScope = wm.scope([_W3_BASE, _W3_THRS]);
  //  PATCH-012: render into the shared fixed markup buffer (lazy mmap).
  const out = io.ram(MAX_SOURCE_MARKED_UP);
  wm.merged([oScope, tScope], out);
  return out.data().slice();
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

function bytesEq(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

//  Fold ONE commit onto its already-built parent weaves: single parent = linear
//  WEAVENext; 2+ = WEAVEMerge the parents first (the merge's two-sided history is
//  present before this commit's blob diffs in).  Absent-here + present-on-parent =
//  a delete (fold empty, stamping a remover).  Over the cap or unreadable = carry
//  the parent.  An adjacent-equal blob (unchanged) carries the parent unchanged.
//  Stamps ctx.idToSha[weaveId(sha)] = sha (the blame click target).
function foldCommit(ctx, sha) {
  const reader = ctx.reader, path = ctx.path, ext = ctx.ext;
  const treeCache = ctx.treeCache, weaveCache = ctx.weaveCache;
  ctx.idToSha[weaveId(sha)] = sha;

  let parents;
  try { parents = reader.commitParents(sha); } catch (e) { parents = undefined; }
  parents = (parents || []).filter(function (p) { return weaveCache[p] != null; });

  let base = null;
  if (parents.length === 1) {
    base = weaveCache[parents[0]];
  } else if (parents.length >= 2) {
    base = weaveCache[parents[0]];
    for (let i = 1; i < parents.length; i++)
      base = merge(base, weaveCache[parents[i]], weaveId(sha));
  }

  const blobSha = blobShaAt(reader, treeCache, sha, path);
  if (blobSha === undefined) {
    if (base === null || base.empty()) return base;   // never present → no-op
    return fold(base, new Uint8Array(0), ext, weaveId(sha));   // delete → fold empty
  }
  const bytes = blobBytes(reader, blobSha);
  if (bytes === undefined) return base;                // unreadable → carry parent
  if (bytes.length > MAX_SOURCE_SIZE) return base;     // BLOB → not woven
  //  adjacent-equal skip: identical to the single-parent alive view => no touch.
  if (base !== null && parents.length === 1) {
    const prev = io.ram(MAX_SOURCE_MARKED_UP);
    base.alive(prev);
    if (bytesEq(prev.data(), bytes)) return base;
  }
  return fold(base, bytes, ext, weaveId(sha));
}

//  A reconstruction context: reader, file path, its ext, and the caches.  Pass a
//  shared `treeCache` (and reuse the returned ctx) to fold a shared ancestor once
//  across several tips (patch's ours/theirs share one ctx per file).
function makeCtx(reader, path, treeCache) {
  return { reader: reader, path: path, ext: extOf(path),
           treeCache: treeCache || Object.create(null),
           weaveCache: Object.create(null),
           idToSha: Object.create(null) };
}

//  Build the file weave AS OF `tip`: an iterative two-colour post-order DFS over
//  parent edges (deep chains stay off the JS stack), folding each commit only
//  after its parents.  Returns { weave, ids, idToSha, ctx }: `weave` undefined when
//  the file never existed; `ids` = Set of contributing commit-id hashlets (scope
//  membership for merged); `idToSha` = hashlet -> sha40 (blame click); `ctx` reusable.
function build(reader, path, tip, ctx) {
  ctx = ctx || makeCtx(reader, path);
  const WHITE = 0, GREY = 1;
  const colour = Object.create(null);
  const stack = [tip];
  while (stack.length) {
    const sha = stack[stack.length - 1];
    if (ctx.weaveCache[sha] !== undefined) { stack.pop(); continue; }
    const c = colour[sha] || WHITE;
    if (c === WHITE) {
      colour[sha] = GREY;
      let parents;
      try { parents = reader.commitParents(sha); } catch (e) { parents = undefined; }
      parents = parents || [];
      let pending = false;
      for (const p of parents)
        if (p && ctx.weaveCache[p] === undefined && colour[p] !== GREY) { stack.push(p); pending = true; }
      if (pending) continue;
    }
    stack.pop();
    if (ctx.weaveCache[sha] !== undefined) continue;
    ctx.weaveCache[sha] = foldCommit(ctx, sha);
  }
  const w = ctx.weaveCache[tip];
  const weav = (w === undefined || w === null) ? undefined : w;
  const ids = new Set();
  if (weav) for (const cid of weav.commits) ids.add(cid);
  return { weave: weav, ids: ids, idToSha: ctx.idToSha, ctx: ctx };
}

module.exports = { fold: fold, merge: merge,
  //  GET-056b: the one 3-blob weave merge (get.js + checkout.js share it).
  weave3: weave3,
  MAX_SOURCE_SIZE: MAX_SOURCE_SIZE, MAX_SOURCE_MARKED_UP: MAX_SOURCE_MARKED_UP,
  //  DIFF-010: file-weave reconstruction (was patch.js buildSideWeave + fileweave.js).
  build: build, makeCtx: makeCtx, weaveId: weaveId, extOf: extOf,
  treeMap: treeMap, blobBytes: blobBytes, blobShaAt: blobShaAt,
  //  BE-010: the wt-on-disk edit fold-layer (mirrors native WEAVE_WT_SRC).
  foldWt: foldWt, WT_SRC: WT_SRC };
