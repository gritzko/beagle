//  patch.js — `be patch` as a plain-args verb (JAB-004; was the JSQUE-013 loop
//  HANDLER).  patch(...args) resolves its OWN (ours, theirs, fork) triple via
//  patchscope.resolve (the central seed no longer pins ctx.triple); the run core
//  walks the three trees in tandem,
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
const wtlog     = require("../../shared/wtlog.js");
const store     = require("../../shared/store.js");
const checkout  = require("../../shared/checkout.js");
const conflict  = require("../../shared/conflict.js");
const ulog      = require("../../shared/ulog.js");
const pathlib   = require("../../shared/util/path.js");
//  BE-030: worktree fs paths go THROUGH resolve() — wtpath is the
//  resolve-backed, context-confined replacement for the old wtJoin.
//  PATCH-010: + wtdir/treeAt for the TREE-source address (never raw path math).
const discover = require("../../core/discover.js");
const wtpath = discover.wtpath;
//  DIS-062: THE URI->hash resolver — RULE ZERO, no verb re-derives a hash/rev.
const resolveHash = require("../../core/resolve_hash.js").resolve_hash;
//  DIFF-010: the shared grow-on-"out full" WEAVE fold/merge retry (mirrors
//  loop.js:128-142) — a large per-file weave no longer throws "out full".
const weave     = require("../../shared/weave.js");
//  DIS-058 D17: descend `be patch` into MOUNTED subs (post-order, mirroring
//  post.js postSubs).  recurse = the read-side `.gitmodules`-order/mount-gate
//  spine; submount = fetch the theirs sub-pin when the sub shard lacks it.
const recurse   = require("../../core/recurse.js");
const submount  = require("../../shared/submount.js");
//  JAB-003: TRUE-hunk output via the shared columnar→hunk adapter (ctx.sink),
//  retiring the ctx.out columnar path for this verb.
const hunkrows  = require("../../shared/hunkrows.js");
//  JAB-004: plain-args PATCH resolves its OWN (ours,theirs,fork) triple via
//  patchscope.resolve (the central seed no longer pins ctx.triple); ambient
//  bridges be↔ctx for the defensive direct-handler shape.
const patchscope = require("../../shared/patchscope.js");
const ambient    = require("../../shared/ambient.js");
//  PATCH-011: the fetch-first leg — a TRANSPORT source's objects land in the
//  local shard BEFORE the (then all-local) triple resolve.
const fetchleg   = require("./fetchleg.js");
//  BE-010: the ONE reg-file wt read (wtread) + the git-blob framing (sha) — the
//  ours side of a per-file weave is the wt's CURRENT bytes, so PATCH must probe
//  disk (not just the commit trees) to tell a clean baseline from a dirty edit.
const wtread     = require("../../shared/wtread.js");
const sha        = require("../../shared/util/sha.js");
const join = pathlib.join;   // BE-011: wtJoin confines wt-opens

//  BE-010: read a wt regular file's on-disk bytes via the ONE reg-file read
//  (wtread), or null (absent / not a regular file).  The ours-side wt layer.
function wtBytesAt(wtRoot, path) {
  const full = wtpath(wtRoot, path);               // BE-011
  let ls; try { ls = io.lstat(full); } catch (e) { return null; }
  if (ls.kind !== "reg") return null;
  return wtread.readFileBytes(full, ls.size);
}

//  BE-010: YES iff the wt's on-disk bytes at `path` DIVERGE from the committed
//  `baseSha` blob (an uncommitted edit).  Reuses the classify.wtEqBase idiom
//  (frame the wt bytes as a git blob, compare shas) over the ONE reg-file read.
//  A missing/irregular/unreadable file is NOT dirty (the caller keeps clean arms).
function wtDirty(wtRoot, path, baseSha) {
  if (!baseSha) return false;
  const bytes = wtBytesAt(wtRoot, path);
  if (bytes == null) return false;
  return sha.frameSha("blob", bytes) !== baseSha;
}

//  Weave reconstruction + tree/blob readers now live in shared/weave.js
//  (build/makeCtx/weaveId/extOf/treeMap/blobBytes) — the ONE copy `why:` shares.

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
    //  BE-010: committed ours == base and only theirs changed — BUT the wt may
    //  carry uncommitted edits (spec PATCH.mkd §"Weave merge": the ours side is
    //  the wt's current bytes).  A clean baseline clean-takes theirs; a DIRTY
    //  file routes into the 3-way weave (wt bytes folded onto ours), so disjoint
    //  regions merge and a true overlap fences — never a silent clobber.
    if (wtDirty(rc.wtRoot, path, oSha)) return mergeApply(rc, path, o);
    writeLeaf(rc, path, t, weave.blobBytes(reader, tSha));
    st.takeTheirs++; emit(rc, "applied", path); return;
  }
  if (f && o && t && !oEqF && tEqF) { st.noop++; emit(rc, "mod", path); return; } // only ours
  if (f && o && t && oEqT) { st.noop++; return; }                // same change
  if (f && o && t && !tEqF && !oEqT) return mergeApply(rc, path, o);  // both → merge
  if (!f && !o && t) {                                           // theirs added
    writeLeaf(rc, path, t, weave.blobBytes(reader, tSha));
    st.added++; emit(rc, "applied", path); return;
  }
  if (!f && o && !t) { st.noop++; return; }                      // ours added only
  if (!f && o && t && !oEqT) return mergeApply(rc, path, o);      // add/add → merge
  if (!f && o && t && oEqT) { st.noop++; return; }               // add/add same
  //  Structural delete asymmetry (modify/delete) — content side wins.
  if (f && o && !t) {
    //  BE-010: theirs deleted.  The commit side says ours==base (clean), but the
    //  content side that wins is the wt's CURRENT bytes — a DIRTY uncommitted
    //  edit must be kept (modl), never unlinked.  Only a truly-clean wt deletes.
    if (oSha === fSha && !wtDirty(rc.wtRoot, path, oSha)) {      // theirs deleted, ours clean
      try { deleteLeaf(rc, path); st.deleted++; }
      catch (e) { st.failed++; emit(rc, "failed", path); }
    } else { st.modDelKept++; emit(rc, "modl", path); }         // theirs del, ours mod/dirty → keep ours
    return;
  }
  if (f && !o && t) {
    if (tSha === fSha) { st.noop++; return; }                    // ours deleted, theirs clean
    writeLeaf(rc, path, t, weave.blobBytes(reader, tSha));            // ours del, theirs mod → theirs
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
  //  ONE shared ctx (rc.treeCache + a fresh weaveCache) so a shared ancestor of
  //  ours/theirs folds once — weave.build replays the file's commit closure.
  const ctx = weave.makeCtx(rc.reader, path, rc.treeCache);

  //  Reconstruct + union + render through the fixed markup buffers (lazy mmap,
  //  no growth).  If a token-dense file overflows the cap, it is not a text file
  //  we can weave-merge — err out, treat it as a BLOB (failed), don't crash.
  let merged;
  try {
    const ours = weave.build(rc.reader, path, rc.ours, ctx);
    const theirs = weave.build(rc.reader, path, rc.theirs, ctx);

    //  BE-010 (the DEEP part): build() reconstructs the ours weave from the ours
    //  COMMIT's history ONLY — it never reads disk, so a naive re-route would
    //  still drop the wt's uncommitted bytes.  Fold the wt's on-disk bytes onto
    //  the ours side as a final synthetic WT_SRC layer (mirrors native
    //  graf_fold_wt_layer), and add WT_SRC to the ours scope so the folded edit
    //  reads as an ours-side contribution when the merge renders.
    let ourWeave = ours.weave, ourIds = ours.ids;
    if (ourWeave) {
      const fl = weave.foldWt(ourWeave, wtBytesAt(rc.wtRoot, path), weave.extOf(path));
      if (fl.layered) {
        ourWeave = fl.weave;
        ourIds = new Set(ourIds); ourIds.add(weave.WT_SRC);
      }
    }

    //  Empty-side degeneracy (native's emit_alive_bytes short-circuits): if one
    //  side has no weave, emit the other's tip bytes.
    if (!ourWeave && !theirs.weave) { rc.st.failed++; emit(rc, "failed", path); return; }
    if (!ourWeave)   merged = aliveOf(rc, theirs.weave);
    else if (!theirs.weave) merged = aliveOf(rc, ourWeave);
    else {
      //  Union ours⊕theirs (shared tokens dedup by identity), then render the
      //  two sides' scopes with fences into the fixed markup buffer (lazy mmap).
      const wm = weave.merge(ourWeave, theirs.weave, "0000000000000000");
      const oScope = wm.scope(setArr(ourIds));
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
    rc.st.mergedConflict++; emit(rc, "con", path);   // STATUS-005: con (was DIS-057 cnf)
    rc.conflicts.push(path);                         // STATUS-005: durable con row target
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
//  BE-010: unlink the wt path, letting a REAL failure SURFACE.  The prior inner
//  try{}catch{} swallowed every io.unlink error, so the caller's
//  `catch(e){ st.failed++ }` was DEAD and `st.deleted` counted phantom deletes
//  (an unlink that never happened).  Now the caller counts `deleted` only on a
//  true unlink and routes a genuine failure to `failed`.
function deleteLeaf(rc, path) {
  io.unlink(wtpath(rc.wtRoot, path));              // BE-011
}

//  Per-file status row (deferred to the banner, native emits inline rows).
function emit(rc, status, path) { rc.rows.push({ status: status, path: path }); }

//  --- patch row ----------------------------------------------------------
//  DIS-030 row URI: NAMED → `#<sha>` (fragment), NEXT → `?<sha>`, WHOLE →
//  `?<sha>!`.  The slot/`!` encode the scope POST reads for provenance.
function patchRowUri(scope, theirs) {
  //  URI-013 B10: compose the ref-only key via the URI class.  NAMED = `#<sha>`
  //  (fragment); NEXT = `?<sha>` (query); WHOLE = `?<sha>` + a trailing `!` scope
  //  sigil (NOT a URI slot — appended after the composed URI, byte-preserving).
  if (scope === "NAMED") return URI.make(undefined, undefined, undefined, undefined, theirs);
  if (scope === "WHOLE") return URI.make(undefined, undefined, undefined, theirs, undefined) + "!";
  return URI.make(undefined, undefined, undefined, theirs, undefined);   // NEXT
}

//  JAB-004: plain-args PATCH — `patch(...args)` off global `be`, called ONCE.
function patch() {
  const _be = (typeof be !== "undefined") ? be : null;
  const argv = [];
  for (let i = 0; i < arguments.length; i++) argv.push(String(arguments[i]));
  //  JAB-004: strip a `patch:` scheme prefix off the sole URI arg (cat idiom);
  //  --nosub rides be.flags (loop.js split off the leading '-' flags already).
  let arg = argv.length ? argv[0] : "";
  if (arg.indexOf("patch:") === 0) arg = arg.slice(6);
  return patchRun({ repo: _be && _be.repo, sink: _be && _be.sink,
                    flags: (_be && _be.flags) || [], triple: null, arg: arg });
}
patch.jab = "args";
module.exports = patch;

//  PATCH-010/011: source routing — the verb owns address resolution:
//    ?<br> | ?<br>! | #<sha> | bare ref → patchscope's classic legs (null here)
//    //WT → the tree leg, LOCAL store only;  file:<wt-path> → the tree leg,
//        a FOREIGN-store wt fetches its cur tip's closure first (PATCH-011);
//    file:<store>[?/<proj>] | be: | ssh:/http(s): → the PATCH-011 fetch leg;
//    anything else → patchscope's ONE uniform PATCHFAIL refusal.
//  Returns { tip, branch } with the objects now LOCAL, or null (classic arg).
function resolveSource(info, arg) {
  if (fetchleg.isFetchable(arg)) return fetchleg.fetchSource(info, arg);
  const shape = patchscope.parseShape(arg);      // throws the loud refusal
  if (shape.scope !== "TREE") return null;
  //  PATCH-011: a query-less file:<path> whose `.be` is a DIR is a STORE
  //  source — the fetch leg (tip = its trunk), incl. the dead-source refusal.
  if (!shape.nav && !fetchleg.isWtPath(shape.tree))
    return fetchleg.fetchSource(info, arg);
  if (shape.nav) {
    //  DIS-062: `//X[/sub]` resolves THROUGH resolve_hash (step 5.5 = that
    //  tree's own cur tip) — never a hand-rolled wtlog.open(x).curTip().
    const ctx = discover.navCwd(discover.ctxDir());
    let r; try { r = resolveHash(ctx, arg); }
    catch (e) { throw "PATCHFAIL: " + e; }
    if (!store.open(info.storePath, info.project).getObject(r.chash))
      throw "PATCHFAIL: source tip " + r.chash + " is not in the local store";
    return { tip: r.chash, branch: "" };
  }
  //  PATCH-010: `file:<path>` anchors via discover.treeAt (abs or cwd-relative
  //  — the GET-038 local-source idiom).
  let src; try { src = discover.treeAt(shape.tree); } catch (e) { src = null; }
  if (!src) throw "PATCHFAIL: no worktree at '" + shape.tree + "'";
  //  theirs = the addressed wt's CUR TIP, read via the ONE wtlog reader.
  const cur = wtlog.open(src).curTip();
  if (!cur || !cur.sha)
    throw "PATCHFAIL: source tree '" + shape.tree + "' has no cur tip";
  if (src.storePath !== info.storePath) {
    //  PATCH-011: the wt anchors ANOTHER store — fetch the tip's closure from
    //  THAT store first; the absorb below is then the ordinary local patch.
    fetchleg.fetchWtTip(info, src, cur.sha, arg);
  } else if (!store.open(info.storePath, info.project).getObject(cur.sha)) {
    throw "PATCHFAIL: source tip " + cur.sha + " is not in the local store";
  }
  return { tip: cur.sha, branch: cur.branch };
}

//  JAB-004: the run core (plain entry + the sub re-entry share it).  Resolves
//  its OWN (ours,theirs,fork) triple: the central seed no longer pins it, so
//  here we open the store reader + wtlog and call patchscope.resolve ourselves
//  (a pre-pinned ctx.triple, from the sub re-entry, is honoured as-is).
function patchRun(ctx) {
  //  PATCH-011: a scheme'd source arg never reaches be.treeAt (it is a source
  //  URI, not a context path) — the repo is the ambient/cwd context.
  const schemed = !ctx.triple && fetchleg.isSchemed(ctx.arg || "");
  const info = ctx.repo || be.treeAt(schemed ? undefined : (ctx.arg || undefined));
  ctx.repo = info;
  const wtl = wtlog.open(info);
  //  PATCH-010/011: resolve + (when foreign) FETCH the source FIRST — the
  //  reader below must see the landed objects; a fetch failure refuses loudly
  //  here, BEFORE any wt mutation.
  const src = ctx.triple ? null : resolveSource(info, ctx.arg || "");
  const reader = store.open(info.storePath, info.project);

  //  Scope + the ours/theirs/fork commit triple.  The plain path has NO central
  //  seed (resolve.seed no longer pins ctx.triple), so PATCH pins its OWN triple
  //  from its commit arg via patchscope.resolve over the same wtl + reader.  A
  //  sub re-entry supplies ctx.triple directly (the advanced sub pins).  A
  //  tree/store/wire source hands its now-local tip to patchscope.resolveTree
  //  (the PATCH-010 rebase point — the wholeTriple stopgap is gone).
  const sc = ctx.triple ||
      (src ? patchscope.resolveTree(src.tip, src.branch, wtl, reader)
           : patchscope.resolve(ctx.arg || "", wtl, reader));
  if (!sc) throw "PATCHFAIL: a patch URI is required (`?<br>` | `?<br>!` | `#<sha>`)";

  //  Build the three tree maps; the union of their paths is the walk set.
  const treeCache = Object.create(null);
  const fMap = weave.treeMap(reader, sc.fork);  treeCache[sc.fork] = fMap;
  const oMap = weave.treeMap(reader, sc.ours);  treeCache[sc.ours] = oMap;
  const tMap = weave.treeMap(reader, sc.theirs); treeCache[sc.theirs] = tMap;
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
               wrote: [], rows: [], subJobs: [], conflicts: [] };  // STATUS-005: con paths

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
  if (absorbed === 0) { emitBanner(ctx, sc, rc.rows, 0n); return; }

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
    else if (statusOf[p] === "con") stamp = ts;       // con (was DIS-057 cnf)
    try { io.setMtime(wtpath(info.wt, p), stamp); } catch (e) {}   // BE-011
  }

  //  STATUS-005: durable `con <path>` row per conflicted file (append-only,
  //  like `put`) — survives mtime churn, unlike the fragile stamp band above.
  if (rc.conflicts.length)
    ulog.append(info.bePath,
                rc.conflicts.map(function (p) {
                  return { verb: "con", uri: URI.make(undefined, undefined, p) };
                }));

  emitBanner(ctx, sc, rc.rows, ts);
}

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
                [{ verb: "put", uri: URI.make(undefined, undefined, job.path, undefined, job.theirs) }]);
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
                     pin: job.theirs, source: null });
  }
  //  The sub triple mirrors the parent's: ours/theirs/fork gitlink pins.  An
  //  empty fork (root-pinned sub) drops to the no-base degenerate merge.
  const subTriple = { scope: "NAMED", branch: "", ours: job.ours,
                      theirs: job.theirs, fork: job.fork };
  //  JAB-004: re-enter the run core DIRECTLY with the pinned sub triple (a child
  //  synthetic ctx) — no global `be` swap, so the sub's rows ride the SAME sink
  //  and aggregate into the parent banner ([Submodules] §"sub reports aggregated").
  patchRun({ repo: subRepo, sink: ctx && ctx.sink, triple: subTriple,
             flags: (ctx && ctx.flags) || [], arg: subRepo.wt });
}

//  Banner as a TRUE hunk: a ref-only addressing uri (`#<theirs>`/`?<theirs>`) header,
//  then per-file status rows.  DIS-060: NO phantom `patch:` — a VERB isn't a SCHEME.
function emitBanner(ctx, sc, rows, ts) {
  if (!(ctx && ctx.sink)) return;
  const out = hunkrows(ctx.sink, patchRowUri(sc.scope, sc.theirs));
  for (const r of rows) out.row(r.path, r.status, 0n);
  out.done();
}
