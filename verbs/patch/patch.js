//  patch.js — `be patch` as a loop HANDLER (JSQUE-013, was JS-052 one-shot).
//  The (ours, theirs, fork) commit triple + scope are pinned ONCE at seed
//  (resolve.seed -> ctx.triple); this handler walks the three trees in tandem,
//  merges each diverged file into the worktree (conflict fences), restamps
//  every touched file, appends ONE `patch` provenance ULOG row (a BARRIER — one
//  row for the WHOLE absorbed set, never per-file), and pushes the per-file
//  status rows via ctx.out.  No commit — the next `be post` squashes the work.
//
//  Pure JS over JABC + lib/* (libabc+libdog ONLY; NO keeper/graf/sniff
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
//          jab be/loop.js patch '?<branch>'   (JSQUE-013 resident-loop handler)

"use strict";

//  JSQUE-013: sibling libs via relative require ("./lib/X.js") — robust under
//  the resident loop (NOT argv[1]/__dirname; JSQUE-008 idiom).
//  JSQUE-013: scope/triple is seed-resolved (resolve.seed -> patchscope), so
//  this handler no longer requires patchscope/render — output rides ctx.out.
//  JSQUE-016: by-verb reorg — core/discover + shared/ kernel via ../../ .
const be        = require("../../core/discover.js");
const wtlog     = require("../../shared/wtlog.js");
const store     = require("../../shared/store.js");
const checkout  = require("../../shared/checkout.js");
const conflict  = require("../../shared/conflict.js");
const ulog      = require("../../shared/ulog.js");
const pathlib   = require("../../shared/util/path.js");
//  DIFF-010: the shared grow-on-"out full" WEAVE fold/merge retry (mirrors
//  loop.js:128-142) — a large per-file weave no longer throws "out full".
const weave     = require("../../shared/weave.js");
//  DIS-058 D17: descend `be patch` into MOUNTED subs (post-order, mirroring
//  post.js postSubs).  recurse = the read-side `.gitmodules`-order/mount-gate
//  spine; submount = fetch the theirs sub-pin when the sub shard lacks it.
const recurse   = require("../../core/recurse.js");
const submount  = require("../../shared/submount.js");
const join = pathlib.join;

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
    for (let i = 1; i < parents.length; i++)
      base = weave.merge(base, weaveCache[parents[i]], weaveId(sha));  // DIFF-010
  }

  const blobSha = blobShaAt(reader, treeCache, sha, path);
  if (blobSha === undefined) {
    //  File absent at this commit.  If a parent carried it, this commit
    //  DELETES it — fold the empty blob so the deletion is recorded as a
    //  remover stamped by this commit.  If no parent had it either, there is
    //  no history to carry (null propagates).
    if (base === null || base.empty()) return base;   // never present → no-op
    return weave.fold(base, new Uint8Array(0), ext, weaveId(sha));  // DIFF-010
  }

  const bytes = blobBytes(reader, blobSha);
  if (bytes === undefined) return base;                // unreadable → carry parent
  //  Over the source cap → a BLOB we don't tokenise (weave.js policy); carry the
  //  parent weave unchanged.  NB: an oversized revision's change is not woven.
  if (bytes.length > weave.MAX_SOURCE_SIZE) return base;

  //  Optimisation mirroring native's adjacent-equal skip: when the blob is
  //  byte-identical to the single-parent baseline's alive view, this commit
  //  did not touch the file — carry the parent weave unchanged (no spurious
  //  commit id in the scope).
  if (base !== null && parents.length === 1) {
    //  The alive view fits the fixed markup buffer (lazy mmap) — no growth.
    const prev = io.ram(weave.MAX_SOURCE_MARKED_UP);
    base.alive(prev);
    if (bytesEq(prev.data(), bytes)) return base;
  }

  return weave.fold(base, bytes, ext, weaveId(sha));
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

  //  DIS-058 D17: a gitlink never goes through the BLOB merge, but an absorbed
  //  commit that ADVANCED the sub (theirs pin ≠ ours pin) must DESCEND — record
  //  a sub-patch job (post-order, run after the parent merge); the descent
  //  merges the sub files + bumps the parent gitlink (mirrors post.js postSubs).
  if ((o && o.kind === "s") || (t && t.kind === "s") || (f && f.kind === "s")) {
    if (oSha !== tSha && tSha)
      rc.subJobs.push({ path: path, fork: fSha, ours: oSha, theirs: tSha });
    st.noop++; return;
  }

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
//  marker reports `cnf` (markers left in the wt; DIS-057 conf→cnf).
function mergeApply(rc, path, oLeaf) {
  const ctx = { reader: rc.reader, path: path, ext: extOf(path),
                treeCache: rc.treeCache, weaveCache: Object.create(null),
                weaveCap: rc.weaveCap, aliveBuf: rc.aliveBuf };

  //  Reconstruct + union + render through the fixed markup buffers (lazy mmap,
  //  no growth).  If a token-dense file overflows the cap, it is not a text file
  //  we can weave-merge — err out, treat it as a BLOB (failed), don't crash.
  let merged;
  try {
    const ours = buildSideWeave(ctx, rc.ours);
    const theirs = buildSideWeave(ctx, rc.theirs);

    //  Empty-side degeneracy (native's emit_alive_bytes short-circuits): if one
    //  side has no weave, emit the other's tip bytes.
    if (!ours.weave && !theirs.weave) { rc.st.failed++; emit(rc, "failed", path); return; }
    if (!ours.weave)   merged = aliveOf(rc, theirs.weave);
    else if (!theirs.weave) merged = aliveOf(rc, ours.weave);
    else {
      //  Union ours⊕theirs (shared tokens dedup by identity), then render the
      //  two sides' scopes with fences into the fixed markup buffer (lazy mmap).
      const wm = weave.merge(ours.weave, theirs.weave, "0000000000000000");
      const oScope = wm.scope(setArr(ours.ids));
      const tScope = wm.scope(setArr(theirs.ids));
      const out = io.ram(weave.MAX_SOURCE_MARKED_UP);
      wm.merged([oScope, tScope], out);
      merged = out.data();
    }
  } catch (e) {
    if (("" + e).includes("full")) { rc.st.failed++; emit(rc, "failed", path); return; }
    throw e;
  }

  const leaf = oLeaf || { kind: "f" };
  writeBytes(rc, path, leaf, merged);
  if (conflict.hasConflictMarker(merged)) {
    rc.st.mergedConflict++; emit(rc, "cnf", path);   // DIS-057: conf→cnf
  } else { rc.st.merged++; emit(rc, "merged", path); }
}

//  alive (tip) bytes of a weave, as a fresh Uint8Array (copied off the shared
//  scratch buffer so the caller can hold it past the next merge).
function aliveOf(rc, w) {
  //  The alive (tip) render fits the fixed markup buffer (lazy mmap) — no growth.
  const b = io.ram(weave.MAX_SOURCE_MARKED_UP);
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

//  JSQUE-013: `be patch` as a loop HANDLER.  Converted from a `main();`
//  one-shot to `module.exports = handle(row, ctx)` — the (ours,theirs,fork)
//  triple + scope ride ctx.triple (seed-pinned, resolution-at-entry), the repo
//  rides ctx.repo, output goes through ctx.out (ONE flush at the loop edge),
//  sibling libs via relative ./.  No process.argv read, no self-run tail.
module.exports = function handle(row, ctx) {
  const out = ctx && ctx.out;
  const info = (ctx && ctx.repo) || be.find((row && row.uri) || undefined);
  const wtl = (ctx && ctx.resolved && ctx.resolved._wtl) || wtlog.open(info);
  const reader = (ctx && ctx.resolved && ctx.resolved._reader)
                 || store.open(info.storePath, info.project);

  //  Scope + the ours/theirs/fork commit triple are pinned ONCE at seed
  //  (resolve.seed -> ctx.triple; JSQUE-004).  Never re-resolved live here.
  const sc = (ctx && ctx.triple);
  if (!sc) throw "PATCHFAIL: a patch URI is required (`?<br>` | `?<br>!` | `#<sha>`)";

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
               wrote: [], rows: [], subJobs: [] };

  //  FAN-OUT: each path is an independent per-file weave LEAF (classifyAndApply)
  //  given the pinned triple; the verdicts fold into the shared counters below.
  for (const p of all)
    classifyAndApply(rc, p, fMap[p] || null, oMap[p] || null, tMap[p] || null);

  //  DIS-058 D17: descend `be patch` into MOUNTED subs whose pin advanced
  //  (post-order), then bump the parent gitlink for each — BEFORE the parent's
  //  own patch row, so a sub merge that fails refuses before any parent stamp.
  const flags = (ctx && ctx.flags) || [];
  if (flags.indexOf("--nosub") < 0)
    patchSubs(info, ctx, reader, rc, fMap, oMap, tMap);

  //  POST-011 noop gate: nothing absorbed → no row, no restamp.
  const absorbed = st.takeTheirs + st.merged + st.mergedConflict +
                   st.added + st.deleted + st.modDelKept + st.failed;
  if (absorbed === 0) { emitBanner(out, sc, rc.rows, 0n); return; }

  //  THE PROVENANCE-FOLD BARRIER: cohort-T0 — ONE ts for the single `patch`
  //  row AND every file restamp (the stamp-set invariant).  Exactly ONE row
  //  for the WHOLE absorbed set (never per-file) — fanning out would break
  //  postIsCommitAll/baseline resolution (PATCH.md).  Strictly-after the tail
  //  (ULOG refuses ts<=tail); the seed cohort ctx.T0 is the intended stamp.
  const tail = wtl.rows.length ? wtl.rows[wtl.rows.length - 1].ts : 0n;
  const base = ulog.nowAfter(tail);
  //  DIS-057 Task 2 — RESERVE stamp headroom: the 3 stamps occupy the band
  //  [base, base+2ms], but base+1ms/base+2ms are NOT real wtlog rows, so a later
  //  op landing on those exact stamps could be misread as a pat/mrg/cnf file.
  //  Park the patch ROW at the band CEILING (base+2ms) so the wtlog monotonic
  //  tail is strictly past every stamp it used — the next nowAfter(tail) is then
  //  > the ceiling, never colliding with a reserved stamp.  Files stamp BELOW the
  //  row: pat=ceil-2ms, mrg=ceil-1ms, cnf=ceil (patchStamps reads the band back).
  //  DIS-057 REOPEN 2026-06-29: step in MILLISECONDS (ulog.ronStepMs), NOT raw
  //  BigInt — a raw base+2n corrupts the packed ms field (ms>=1000 → an invalid
  //  ron60 → FILESetMtime stamps epoch-0 → the band read misses → `mod`).
  const ts = ulog.ronStepMs(base, 2);                 // patch-row ts = band ceiling
  ulog.append(info.bePath,
              [{ verb: "patch", uri: patchRowUri(sc.scope, sc.theirs), ts: ts }]);
  //  DIS-057 stamp OFFSET: the patch verb knows each file's outcome for free, so
  //  it stamps the mtime to ceil-2ms (clean apply → `pat`), ceil-1ms (merged →
  //  `mrg`), or ceil (conflict → `cnf`).  The unified classifier reads that
  //  offset back as the bucket — no merge recompute at status/post read time.  A
  //  file with no per-file status (defensive) restamps to the floor (pat).
  const statusOf = {};
  for (const r of rc.rows) statusOf[r.path] = r.status;
  for (const p of rc.wrote) {
    let stamp = ulog.ronStepMs(ts, -2);              // pat (clean apply) = base
    if (statusOf[p] === "merged") stamp = ulog.ronStepMs(ts, -1);  // mrg
    else if (statusOf[p] === "cnf") stamp = ts;       // cnf
    try { io.setMtime(join(info.wt, p), stamp); } catch (e) {}
  }

  emitBanner(out, sc, rc.rows, ts);
};

//  DIS-058 D17 (POST-ORDER sub descent, mirrors post.js postSubs): for each
//  MOUNTED sub whose gitlink pin advanced (theirs ≠ ours), recurse `be patch`
//  into the sub wt — the sub's own classifyAndApply weave-merges its changed/
//  added files — then synthesise a `put <sub>#<theirsPin>` bump in the PARENT
//  wtlog so the parent records the advance (the SUBS-019 / D7 primitive, the
//  same gitlink-add path post.js uses).  Reuses the read-side recurse spine
//  (`.gitmodules` order + the `<sub>/.be` mount gate) to walk the live subs.
function patchSubs(info, ctx, reader, rc, fMap, oMap, tMap) {
  if (!rc.subJobs.length) return;
  //  Index the advance jobs by gitlink path (the walk visits `.gitmodules`-
  //  declared mounts; only the advanced ones carry a job).
  const jobs = Object.create(null);
  for (const j of rc.subJobs) jobs[j.path] = j;

  recurse.walk(info, "", function (subRepo, subPrefix, sub) {
    const job = jobs[sub.path];
    if (!job) return;                      // mounted but pin unchanged → skip
    runSubPatch(info, ctx, subRepo, job);
    //  Record the advance into the PARENT gitlink (D7): a `put <sub>#<newpin>`
    //  wtlog row — fold-decide turns it into a `160000` add on the parent tree
    //  at the next `be post`.  `ulog.append` stamps strictly past the live tail,
    //  so a second sub's bump never collides with the first's.
    ulog.append(info.bePath,
                [{ verb: "put", uri: job.path + "#" + job.theirs }]);
  }, { gitlinks: subGitlinkMap(rc.subJobs) });
}

//  The mount-gate map recurse.walk wants: { <subpath> -> { path, pin } } for
//  each advanced sub (only these paths are visited + recursed).
function subGitlinkMap(subJobs) {
  const m = Object.create(null);
  for (const j of subJobs) m[j.path] = { path: j.path, pin: j.theirs };
  return m;
}

//  Recurse `be patch` into ONE mounted sub at the advanced triple {ours=the
//  parent's ours-pin, theirs=the parent's theirs-pin, fork=the parent's
//  fork-pin}.  If the sub shard lacks the theirs commit, mount-fetch it (the
//  `.gitmodules`-URL fallback in submount.mount also checks it out — a clean
//  forward absorb has no local edit to preserve).  Then re-invoke this handler
//  on the sub with a child ctx carrying the sub triple; the sub's rows ride the
//  SAME ctx.out so they aggregate into the parent banner ([Submodules] §"sub
//  reports aggregated").  A real sub conflict/refusal BUBBLES UP (never dropped).
function runSubPatch(info, ctx, subRepo, job) {
  const subReader = store.open(subRepo.storePath, subRepo.project);
  //  Ensure the theirs sub-commit is locally resolvable; fetch + checkout via
  //  submount.mount otherwise (same-source unavailable here → `.gitmodules` URL).
  if (!subReader.getObject(job.theirs)) {
    submount.mount({ wt: info.wt, beDir: info.storePath, subpath: job.path,
                     pin: job.theirs, source: null,
                     parentTitle: info.project, parentBranch: "" });
  }
  //  The sub triple mirrors the parent's: ours/theirs/fork gitlink pins.  An
  //  empty fork (root-pinned sub) drops to the no-base degenerate merge.
  const subTriple = { scope: "NAMED", branch: "", ours: job.ours,
                      theirs: job.theirs, fork: job.fork };
  const subCtx = { repo: subRepo, out: ctx && ctx.out, triple: subTriple,
                   flags: (ctx && ctx.flags) || [] };
  module.exports({ uri: subRepo.wt }, subCtx);
}

//  Banner via ctx.out: a `patch:` header (raw framing) then the per-file status
//  rows (applied / merged / cnf / del / modl) at ts=0n (blank-date column).
//  DIS-057 untied JS patch output from native (conf→cnf, pat/mrg/cnf stamps).
function emitBanner(out, sc, rows, ts) {
  out.row("patch:", "patch", ts);
  for (const r of rows) out.row(r.path, r.status, 0n);
}
