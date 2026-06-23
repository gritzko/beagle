//  patch.js — `be patch` as a pure-JS extension (JS-052).  Resolve the
//  ours/theirs/fork commit triple + scope (patchscope.js), walk the three
//  trees in tandem, merge each diverged file into the worktree (conflict
//  fences), restamp every touched file, append ONE `patch` ULOG row, and
//  print the per-file status banner.  No commit, no reachability — the next
//  `be post` squashes the absorbed work.
//
//  Pure JS over JABC + bin/lib/* (libabc+libdog ONLY; NO keeper/graf/sniff
//  binding).  The per-file merge is FULL-HISTORY WEAVE RECONSTRUCTION, the
//  way native `be patch`'s mainline GRAFMergeWtFileTunable works: build each
//  tip's weave from its commit-history closure (`weave.fold`=WEAVENext linear,
//  `weave.merge`=WEAVEMerge at merge commits), union ours⊕theirs, then
//  `weave.merged` to fence divergent regions.  Shared-history tokens coincide
//  by real commit id, so the union dedups automatically — NOT a 3-blob merge.
//
//  SCOPE (JS-052 scope a): landing the row still needs native `be post`
//  (post.js POSTSCOPE refuses an in-scope patch row).
//
//  Usage:  be patch '#<sha>'  |  be patch '?<branch>'  |  be patch '?<branch>!'

"use strict";

const self = process.argv[1];
const here = self.slice(0, self.lastIndexOf("/"));
const be        = require(here + "/lib/be.js");
const wtlog     = require(here + "/lib/wtlog.js");
const store     = require(here + "/lib/store.js");
const patchscope = require(here + "/lib/patchscope.js");
const checkout  = require(here + "/lib/checkout.js");
const conflict  = require(here + "/lib/conflict.js");
const ulog      = require(here + "/lib/ulog.js");
const pathlib   = require(here + "/lib/path.js");
const render    = require(here + "/lib/render.js");
const join = pathlib.join;
const dateCol = render.dateCol, verbCol = render.verbCol,
      writeStdout = render.writeStdout;

//  A commit id for the weave is the hi64 of its sha1, a 16-char hex hashlet
//  (weave.hpp::JABCweaveHi64 reads the first 16 hex chars).  The SAME physical
//  commit always yields the SAME id, so shared-history tokens coincide and the
//  ours⊕theirs union dedups them — the DAG fold's whole point.
function weaveId(sha) { return sha.slice(0, 16); }

//  --- tree → path map ----------------------------------------------------
//  A commit's tree flattened to { path → { sha, mode, kind } } over every
//  leaf (file/exec/symlink/gitlink).  Missing commit → empty map.
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

//  blob bytes for a leaf sha (Uint8Array); undefined for a missing object or
//  a non-blob (gitlink).  A symlink blob's bytes are the link target.
function blobBytes(reader, sha) {
  if (!sha) return undefined;
  const obj = reader.getObject(sha);
  if (!obj || obj.type !== "blob") return undefined;
  return obj.bytes;
}

//  file extension (tail after the last '.') — the weave tokenizer selector,
//  like patch_walk's childext / PATHu8sExt.  No dot → empty (generic).
function extOf(path) {
  const slash = path.lastIndexOf("/");
  const base = slash < 0 ? path : path.slice(slash + 1);
  const dot = base.lastIndexOf(".");
  return dot <= 0 ? "" : base.slice(dot + 1);
}

//  --- the file's blob sha at a commit's tree -----------------------------
//  Returns the 40-hex blob sha for `path` in `commitSha`'s tree, or undefined
//  when the file is absent / the path is a tree / a gitlink there.  Cached per
//  (commit) so a DAG diamond reads each tree once.
function blobShaAt(reader, treeCache, commitSha, path) {
  let map = treeCache[commitSha];
  if (map === undefined) { map = treeMap(reader, commitSha); treeCache[commitSha] = map; }
  const leaf = map[path];
  if (!leaf || leaf.kind === "s") return undefined;   // absent or gitlink
  return leaf.sha;
}

//  --- per-side full-history weave reconstruction -------------------------
//  Walk the file's revision DAG from `tip` PARENTS-FIRST, folding each commit
//  that changes the file's blob onto its parent weave (WEAVENext for a single
//  parent, WEAVEMerge at a 2+-parent commit).  Mirrors native
//  build_tip_weave_tunable: the closure replay in topo order, stamping every
//  revision with its real commit id so the two tips' shared prefix coincides.
//
//  Returns { weave, ids } where `ids` is the Set of weave commit-id hashlets
//  that contributed a token on this side (the scope membership for `merged`).
//  A side with no history for the file (never present) → weave undefined.
function buildSideWeave(ctx, tip) {
  const reader = ctx.reader, path = ctx.path, ext = ctx.ext;
  const treeCache = ctx.treeCache, weaveCache = ctx.weaveCache;
  const ids = new Set();

  //  Post-order DFS over parent edges: a commit is folded only after every
  //  parent it depends on is built.  `weaveCache[sha]` memoises the weave AS
  //  OF that commit (covering the file's history up to and including it), so a
  //  diamond merges each shared ancestor once.  An iterative two-colour walk
  //  keeps deep chains off the JS call stack.
  const WHITE = 0, GREY = 1;
  const colour = Object.create(null);
  const stack = [tip];
  while (stack.length) {
    const sha = stack[stack.length - 1];
    if (weaveCache[sha] !== undefined) { stack.pop(); continue; }
    const c = colour[sha] || WHITE;
    if (c === WHITE) {
      colour[sha] = GREY;
      let parents;
      try { parents = reader.commitParents(sha); } catch (e) { parents = undefined; }
      parents = parents || [];
      //  Defer this commit until its parents are built; push unbuilt parents.
      let pending = false;
      for (const p of parents) {
        if (p && weaveCache[p] === undefined && colour[p] !== GREY) {
          stack.push(p); pending = true;
        }
      }
      if (pending) continue;
    }
    //  GREY (or WHITE with all parents already done): all parents built — fold
    //  this commit now.
    stack.pop();
    if (weaveCache[sha] !== undefined) continue;
    weaveCache[sha] = foldCommit(ctx, sha);
  }

  const w = weaveCache[tip];
  if (w === undefined || w === null) return { weave: undefined, ids: ids };
  //  Membership: every commit-id present in this tip's weave is in-scope.  The
  //  weave's `commits` column already lists exactly the contributors (spine +
  //  each folding commit), so read it straight off.
  for (const cid of w.commits) ids.add(cid);
  return { weave: w, ids: ids };
}

//  Fold ONE commit into a weave, given its already-built parent weaves.  The
//  parent baseline is: a single parent's weave (linear step), or WEAVEMerge of
//  the parents (a merge commit) so the merge's two-sided history is present
//  before this commit's blob is diffed in.  Returns the new weave, or null
//  when the file is absent at this commit AND on every parent (no history).
function foldCommit(ctx, sha) {
  const reader = ctx.reader, path = ctx.path, ext = ctx.ext;
  const treeCache = ctx.treeCache, weaveCache = ctx.weaveCache;

  let parents;
  try { parents = reader.commitParents(sha); } catch (e) { parents = undefined; }
  parents = (parents || []).filter(function (p) { return weaveCache[p] != null; });

  //  Parent baseline weave (the prior revision the blob is diffed against).
  let base = null;
  if (parents.length === 1) {
    base = weaveCache[parents[0]];
  } else if (parents.length >= 2) {
    //  Merge commit: union the parents into one baseline weave first.  Fold
    //  pairwise left-to-right (WEAVEMerge keys on identity, so order only sets
    //  the synthetic merge-commit stamp, which carries no token here).
    base = weaveCache[parents[0]];
    for (let i = 1; i < parents.length; i++) {
      const m = abc.ram("WEAVE", ctx.weaveCap);
      m.merge(base, weaveCache[parents[i]], weaveId(sha));
      base = m;
    }
  }

  const blobSha = blobShaAt(reader, treeCache, sha, path);
  if (blobSha === undefined) {
    //  File absent at this commit.  If a parent carried it, this commit
    //  DELETES it — fold the empty blob so the deletion is recorded as a
    //  remover stamped by this commit.  If no parent had it either, there is
    //  no history to carry (null propagates).
    if (base === null || base.empty()) return base;   // never present → no-op
    const w = abc.ram("WEAVE", ctx.weaveCap);
    w.fold(base, new Uint8Array(0), ext, weaveId(sha));
    return w;
  }

  const bytes = blobBytes(reader, blobSha);
  if (bytes === undefined) return base;                // unreadable → carry parent

  //  Optimisation mirroring native's adjacent-equal skip: when the blob is
  //  byte-identical to the single-parent baseline's alive view, this commit
  //  did not touch the file — carry the parent weave unchanged (no spurious
  //  commit id in the scope).
  if (base !== null && parents.length === 1) {
    const prev = ctx.aliveBuf; prev.reset();
    base.alive(prev);
    if (bytesEq(prev.data(), bytes)) return base;
  }

  const w = abc.ram("WEAVE", ctx.weaveCap);
  w.fold(base, bytes, ext, weaveId(sha));
  return w;
}

//  Byte-equality of two Uint8Arrays.
function bytesEq(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

//  --- per-path 3-way classification (mirrors patch_walk_inner) -----------
//  Each path is classified from its (fork f, ours o, theirs t) leaves into a
//  wt action; counters track the absorbed set (the patch-row gate).  A diverged
//  file routes into the full-history reconstruction merge.
function classifyAndApply(rc, path, f, o, t) {
  const reader = rc.reader, st = rc.st;
  const oSha = o && o.sha, tSha = t && t.sha, fSha = f && f.sha;
  const oEqF = !!(f && o && oSha === fSha);
  const tEqF = !!(f && t && tSha === fSha);
  const oEqT = !!(o && t && oSha === tSha);

  //  gitlinks never go through the blob merge — out of scope (parent recursion
  //  re-gets the pin in native).  Skip; report nothing.
  if ((o && o.kind === "s") || (t && t.kind === "s") || (f && f.kind === "s"))
    { st.noop++; return; }

  if (f && o && t && oEqF && tEqF) { st.noop++; return; }       // unchanged both
  if (f && o && t && oEqF && !tEqF) {                            // only theirs
    writeLeaf(rc, path, t, blobBytes(reader, tSha));
    st.takeTheirs++; emit(rc, "applied", path); return;
  }
  if (f && o && t && !oEqF && tEqF) { st.noop++; emit(rc, "mod", path); return; } // only ours
  if (f && o && t && oEqT) { st.noop++; return; }                // same change
  if (f && o && t && !tEqF && !oEqT) return mergeApply(rc, path, o);  // both → merge
  if (!f && !o && t) {                                           // theirs added
    writeLeaf(rc, path, t, blobBytes(reader, tSha));
    st.added++; emit(rc, "applied", path); return;
  }
  if (!f && o && !t) { st.noop++; return; }                      // ours added only
  if (!f && o && t && !oEqT) return mergeApply(rc, path, o);      // add/add → merge
  if (!f && o && t && oEqT) { st.noop++; return; }               // add/add same
  //  Structural delete asymmetry (modify/delete) — content side wins.
  if (f && o && !t) {
    if (oSha === fSha) {                                         // theirs deleted, ours clean
      try { deleteLeaf(rc, path); st.deleted++; }
      catch (e) { st.failed++; emit(rc, "failed", path); }
    } else { st.modDelKept++; emit(rc, "modl", path); }         // theirs del, ours mod → keep ours
    return;
  }
  if (f && !o && t) {
    if (tSha === fSha) { st.noop++; return; }                    // ours deleted, theirs clean
    writeLeaf(rc, path, t, blobBytes(reader, tSha));            // ours del, theirs mod → theirs
    st.modDelKept++; emit(rc, "modl", path); return;
  }
  if (f && !o && !t) { st.noop++; return; }                      // both removed
  st.noop++;
}

//  Full-history merge of one diverged file: reconstruct ours/theirs weaves
//  from their commit DAGs, union them, render with conflict fences over the
//  per-side scopes.  Mirrors GRAFMergeWtFileTunable.  A residual conflict
//  marker reports `conf` (markers left in the wt, like native).
function mergeApply(rc, path, oLeaf) {
  const ctx = { reader: rc.reader, path: path, ext: extOf(path),
                treeCache: rc.treeCache, weaveCache: Object.create(null),
                weaveCap: rc.weaveCap, aliveBuf: rc.aliveBuf };

  const ours = buildSideWeave(ctx, rc.ours);
  const theirs = buildSideWeave(ctx, rc.theirs);

  //  Empty-side degeneracy (native's emit_alive_bytes short-circuits): if one
  //  side has no weave, emit the other's tip bytes.
  let merged;
  if (!ours.weave && !theirs.weave) { rc.st.failed++; emit(rc, "failed", path); return; }
  if (!ours.weave)   merged = aliveOf(rc, theirs.weave);
  else if (!theirs.weave) merged = aliveOf(rc, ours.weave);
  else {
    //  Union ours⊕theirs (shared tokens dedup by identity), then render the
    //  two sides' scopes with fences.
    const wm = abc.ram("WEAVE", rc.weaveCap * 2);
    wm.merge(ours.weave, theirs.weave, "0000000000000000");
    const oScope = wm.scope(setArr(ours.ids));
    const tScope = wm.scope(setArr(theirs.ids));
    const out = rc.outBuf; out.reset();
    wm.merged([oScope, tScope], out);
    merged = out.data();
  }

  const leaf = oLeaf || { kind: "f" };
  writeBytes(rc, path, leaf, merged);
  if (conflict.hasConflictMarker(merged)) {
    rc.st.mergedConflict++; emit(rc, "conf", path);
  } else { rc.st.merged++; emit(rc, "merged", path); }
}

//  alive (tip) bytes of a weave, as a fresh Uint8Array (copied off the shared
//  scratch buffer so the caller can hold it past the next merge).
function aliveOf(rc, w) {
  const b = rc.outBuf; b.reset();
  w.alive(b);
  return b.data().slice();
}

//  A Set of hashlet strings → a plain Array (weave.scope wants an array).
function setArr(s) { const a = []; for (const x of s) a.push(x); return a; }

//  Materialise a leaf (symlink/exec/regular) from its committed blob bytes,
//  then mark the path for restamp.  Mirrors write_blob + stamp_wrote.
function writeLeaf(rc, path, leaf, bytes) {
  if (bytes == null) { rc.st.failed++; emit(rc, "failed", path); return; }
  writeBytes(rc, path, leaf, bytes);
}
function writeBytes(rc, path, leaf, bytes) {
  checkout.materialise(rc.wtRoot, path, leaf, bytes);
  rc.wrote.push(path);
}
function deleteLeaf(rc, path) {
  try { io.unlink(join(rc.wtRoot, path)); } catch (e) {}
}

//  Per-file status row (deferred to the banner, native emits inline rows).
function emit(rc, status, path) { rc.rows.push({ status: status, path: path }); }

//  --- patch row ----------------------------------------------------------
//  DIS-030 row URI: NAMED → `#<sha>` (fragment), NEXT → `?<sha>`, WHOLE →
//  `?<sha>!`.  The slot/`!` encode the scope POST reads for provenance.
function patchRowUri(scope, theirs) {
  if (scope === "NAMED") return "#" + theirs;
  if (scope === "WHOLE") return "?" + theirs + "!";
  return "?" + theirs;                                   // NEXT
}

function main() {
  const argv = process.argv.slice(2).filter(function (a) { return a[0] !== "-"; });
  const arg = argv[0];
  if (!arg)
    throw "PATCHFAIL: a patch URI is required (`?<br>` | `?<br>!` | `#<sha>`)";

  const wt = io.cwd();
  const info = be.find(wt);
  const wtl = wtlog.open(info);
  const reader = store.open(info.storePath, info.project);

  //  Resolve scope + the ours/theirs/fork commit triple.
  const sc = patchscope.resolve(arg, wtl, reader);

  //  Build the three tree maps; the union of their paths is the walk set.
  const treeCache = Object.create(null);
  const fMap = treeMap(reader, sc.fork);  treeCache[sc.fork] = fMap;
  const oMap = treeMap(reader, sc.ours);  treeCache[sc.ours] = oMap;
  const tMap = treeMap(reader, sc.theirs); treeCache[sc.theirs] = tMap;
  const paths = Object.create(null);
  for (const k in fMap) paths[k] = true;
  for (const k in oMap) paths[k] = true;
  for (const k in tMap) paths[k] = true;
  const all = Object.keys(paths).sort();

  const st = { noop: 0, takeTheirs: 0, merged: 0, mergedConflict: 0,
               added: 0, deleted: 0, modDelKept: 0, failed: 0 };
  //  rc: the run context shared across paths.  treeCache memoises trees across
  //  the whole walk (a file's history shares ancestor trees); out/alive bufs
  //  are reused scratch.  weaveCap sizes each per-revision weave buffer.
  const rc = { reader: reader, wtRoot: info.wt, st: st,
               ours: sc.ours, theirs: sc.theirs,
               treeCache: treeCache, weaveCap: 1 << 16,
               outBuf: io.buf(1 << 20), aliveBuf: io.buf(1 << 20),
               wrote: [], rows: [] };

  for (const p of all)
    classifyAndApply(rc, p, fMap[p] || null, oMap[p] || null, tMap[p] || null);

  //  POST-011 noop gate: nothing absorbed → no row, no restamp.
  const absorbed = st.takeTheirs + st.merged + st.mergedConflict +
                   st.added + st.deleted + st.modDelKept + st.failed;
  if (absorbed === 0) {
    emitBanner(sc, st, rc.rows, 0n);
    return;
  }

  //  One ts for the row + every touched file (the stamp-set invariant).
  const tail = wtl.rows.length ? wtl.rows[wtl.rows.length - 1].ts : 0n;
  const ts = ulog.nowAfter(tail);
  ulog.append(info.bePath,
              [{ verb: "patch", uri: patchRowUri(sc.scope, sc.theirs), ts: ts }]);
  for (const p of rc.wrote) {
    try { io.setMtime(join(info.wt, p), ts); } catch (e) {}
  }

  emitBanner(sc, st, rc.rows, ts);
}

//  Banner: a `patch:` header, then the per-file status rows (applied / merged
//  / conf / del / modl), matching native's patch table.  File rows carry ts=0.
function emitBanner(sc, st, rows, ts) {
  const dc = dateCol(ts);
  const blank = dateCol(0n);
  let body = dc + " " + verbCol("patch") + " patch:\n";
  for (const r of rows)
    body += blank + " " + verbCol(r.status) + " " + r.path + "\n";
  writeStdout(body);
}

main();
