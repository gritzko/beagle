//  get.js — `be get` as a loop HANDLER (JSQUE-009).  Converted from the
//  one-shot `main()` (JS-038/041) to `module.exports = handle(row, ctx)`:
//  the GET SEED resolves the remote ONCE (fetch/redirect + pinned target
//  tip/tree), writes the wtlog anchor, then FANS OUT the checkout as enqueued
//  rows — a per-dir Merkle-pruned reconcile that emits per-file write/merge
//  leaves + per-path delete rows + recursive subdir rows.  Output goes via
//  ctx.out (one flush at the loop edge).  A dirty-overlap PRE-PASS barrier
//  guards the seed; a del-sweep BARRIER folds the delete leaves (core/barrier.js
//  over the live queue ULog).  Pure JS over JABC + ./lib/* (libdog+abc).
//
//  ROW VOCABULARY (all under the `get` verb so this handler owns the tree):
//    get <remote>                  SEED: fetch/redirect, anchor, enqueue root
//    get <dir>/?<newTree>#<oldTree>    dir-reconcile: read both trees one level,
//                                  Merkle-prune (new==old → skip), fan out
//    get <path>?<newBlob>#<oldBlob>    write/merge LEAF: read own bytes, decide
//                                  clean-overwrite vs 3-way weave, write, emit
//    get <path>?#<oldBlob>         delete LEAF: removed entry (in old, not new)
//    get ::del-sweep               del-sweep BARRIER fold (back-scan the dels)
//
//  Usage:  be get <remote>      (one-shot fork still works via loop.cli)
//          jab be/loop.js get <remote>   (JSQUE-009 resident-loop handler)

"use strict";

//  JSQUE-009: sibling libs via relative require ("./lib/X.js"), resolved against
//  this module's own dir — robust under the resident loop (not argv[1]).
//  JSQUE-016: by-verb reorg — shared/ kernel, core/, view/ via ../../ .
const store    = require("../../shared/store.js");
const wire     = require("../../shared/wire.js");
const checkout = require("../../shared/checkout.js");
const dag      = require("../../shared/dag.js");
const ingest   = require("../../shared/ingest.js");
const pathlib  = require("../../shared/util/path.js");
const ulog     = require("../../shared/ulog.js");
const sha      = require("../../shared/util/sha.js");
const barrier  = require("../../core/barrier.js");
const join = pathlib.join, dirname = pathlib.dirname;
const isFullSha = sha.isFullSha;

const writeWtlog = ulog.write;
const appendWtlog = ulog.append;

//  del-sweep fold-row URI marker.  Plain ASCII (no leading `:` / `?` / `#`):
//  a leading colon mis-frames the NEXT queue ULOG row (a URI-scheme parse
//  hazard — JSQUE-009 own-ticket), so the marker must be scheme-free.
const DELSWEEP = "del-sweep";

//  --- remote URI → { local, srcRoot, srcBe, proj, branch } ---------------
//  No hand-rolled parsing: the URI binding splits scheme/host/path/query.
function parseRemote(uri) {
  const u = new URI(uri);
  const scheme = u.scheme || "";
  const host = u.host || u.authority || "";
  const path = u.path || "";
  const query = u.query || "";
  let proj = "", branch = "";
  if (query && query[0] === "/") {
    const segs = query.slice(1).split("/");
    proj = segs[0] || "";
    branch = segs.slice(1).join("/");
  } else if (query) {
    branch = query;
  }
  const localish = scheme === "file" || scheme === "" ||
                   (scheme === "keeper" && (host === "" || host === "local" ||
                                            host === "localhost"));
  let srcBe = path, srcRoot = path;
  if (localish) {
    srcBe = path.replace(/\/+$/, "");
    srcRoot = srcBe.replace(/\/\.be$/, "");
    if (srcRoot === srcBe) srcRoot = dirname(srcBe);
  }
  return { local: localish, scheme, host, srcRoot, srcBe, proj, branch,
           raw: uri };
}

function exists(p) { try { io.stat(p); return true; } catch (e) { return false; } }

//  Newest tip sha recorded in an existing wtlog (the last `#<40hex>` row).
function oldTipOf(bePath) {
  let tip = "";
  ulog.each(bePath, function (log) {
    const u = new URI(log.uri);
    let f = u.fragment || "";
    if (f[0] === "?") f = f.slice(1);
    if (isFullSha(f)) tip = f;
  });
  return tip;
}

//  --- the GET SEED: resolve the remote ONCE, anchor, return pinned coords --
function seedLocal(rem, wt) {
  const k = store.open(rem.srcRoot, rem.proj);
  const tip = k.resolveRef(rem.branch || "");
  if (!tip || !isFullSha(tip))
    throw "be get: cannot resolve " + (rem.branch || "trunk") +
          " in " + rem.srcBe;
  const bePath = join(wt, ".be");
  const fresh = !exists(bePath);
  const oldTip = fresh ? "" : oldTipOf(bePath);
  const redirect = "file:" + rem.srcBe + "/?/" + rem.proj;
  const tipRow = { verb: "get", uri: "?" + (rem.branch || "") + "#" + tip };
  if (fresh) writeWtlog(bePath, [{ verb: "get", uri: redirect }, tipRow]);
  else appendWtlog(bePath, [tipRow]);
  return { k, tip, oldTip, fresh, branch: rem.branch || "" };
}

function seedRemote(rem, wt) {
  const beDir = join(wt, ".be");
  const fresh = !exists(beDir);
  const proj = rem.proj || "repo";
  const f = wire.fetch(rem.raw, rem.branch || "");
  const tip = f.want;
  if (!tip || !isFullSha(tip)) throw "be get: peer gave no tip";
  const branch = rem.branch || f.branch || "";
  const shard = join(beDir, proj);
  const wtl = join(beDir, "wtlog");
  let oldTip = "";
  if (fresh) {
    ingest.clone(f.pack, beDir, proj, tip, rem.raw);
    const anchor = "file:" + beDir + "/" + proj + "/";
    writeWtlog(wtl, [{ verb: "get", uri: anchor },
                     { verb: "get", uri: "?" + branch + "#" + tip }]);
  } else {
    oldTip = oldTipOf(wtl);
    ingest.add(f.pack, shard, rem.raw, tip);
    appendWtlog(wtl, [{ verb: "get", uri: "?" + branch + "#" + tip }]);
  }
  const k = store.open(wt, proj);
  return { k, tip, oldTip, fresh, branch };
}

//  --- per-level tree map: name → { sha, mode, isDir } --------------------
function treeMap(k, treeSha) {
  const m = {};
  if (!treeSha) return m;
  const entries = k.readTree(treeSha);
  if (!entries) return m;
  for (const e of entries)
    m[e.name] = { sha: e.sha, mode: e.mode, isDir: e.mode === 0o40000 };
  return m;
}

//  Map a git mode to checkout.js's leaf kind ("f"/"x"/"l"/"s").
function kindOf(mode) {
  if (mode === 0o160000) return "s";
  if (mode === 0o120000) return "l";
  if (mode === 0o100755) return "x";
  return "f";
}

//  --- URI shape classifiers for the fan-out rows -------------------------
//  A SEED carries a transport remote (scheme/authority) or a local store path
//  (a `file:`/scheme-less path ending `.be`).  A reconcile/leaf/fold row never
//  carries a remote — only pinned blob/tree shas (or the ::del-sweep marker).
function isSeedUri(uri) {
  if (uri === DELSWEEP) return false;
  const u = new URI(uri);
  if (u.scheme || u.authority) return true;        // transport / file://
  const path = u.path || "";
  return path.replace(/\/+$/, "").slice(-3) === ".be";
}

//  --- the handler --------------------------------------------------------
module.exports = function handle(row, ctx) {
  const uri = (row && row.uri) || "";
  if (uri === DELSWEEP) return delSweep(row, ctx);   // BARRIER fold row
  if (isSeedUri(uri)) return handleSeed(uri, ctx);
  return handleReconcileOrLeaf(uri, ctx);
};

//  SEED: resolve the remote once (resolution-at-entry), anchor the wtlog, emit
//  the `get ?<branch>#<hashlet>` banner + any pulled-commit rows, then run the
//  dirty-overlap PRE-PASS and ENQUEUE the root dir-reconcile.  Pins
//  { k, wt, tip, oldTip, kinds } on ctx so each fan-out child reuses the SAME
//  reader/wt without re-resolving (JSQUE-004).
function handleSeed(uri, ctx) {
  const out = ctx && ctx.out;
  const wt = io.cwd();
  const rem = parseRemote(uri);
  const r = rem.local ? seedLocal(rem, wt) : seedRemote(rem, wt);

  //  Pin the checkout coordinates on ctx; ctx.T0 is the cohort ts (one date
  //  column for the whole get).  `kinds` carries each leaf's tree-entry kind
  //  (the reconcile reads the mode; the queue round-trip drops it, so it rides
  //  ctx — set before the leaf is dispatched).  `dels` tallies the del-sweep.
  ctx._get = { k: r.k, wt: wt, tip: r.tip, oldTip: r.oldTip, fresh: r.fresh,
               branch: r.branch, ts: ctx.T0, kinds: {}, dels: 0 };

  //  Edge flush order (native get): pulled-commit `post` rows first (newest
  //  first, kept in push order), then file rows — new+upd interleaved lex,
  //  THEN del lex.  The fan-out arrives in queue order, so SORT at the flush.
  ctx.outSort = function (rows) { return sortGetRows(rows); };

  //  Resolution-at-entry: pin the root NEW + OLD tree shas (old empty on a
  //  fresh wt) — the reconcile fan-out walks from these, branch-free.
  const newTree = r.k.commitTree(r.tip);
  if (!newTree) throw "be get: tip " + r.tip + " has no tree";
  const oldTree = (r.oldTip && r.oldTip !== r.tip)
        ? r.k.commitTree(r.oldTip) : "";

  //  JSQUE-014: the dirty-overlap PRE-PASS barrier (SNIFFOVRL, GET.c:585-591)
  //  refuses BEFORE HUNKTableOpen — native emits NO banner on GETOVRL, so run
  //  the check ahead of any out.* push (the loop edge then flushes nothing).
  //  Refuse if a dirty wt file overlays a NEW target path with NO baseline to
  //  merge; a fresh clone has no baseline so it is a no-op (guards no-base only).
  dirtyOverlapCheck(r.k, newTree, oldTree, wt, r.fresh);

  out.banner("get", "?" + (r.branch || "") + "#" + r.tip.slice(0, 8), ctx.T0);

  //  Pulled-commit rows (UPDATE only), newest-first; rendered above file rows.
  if (!r.fresh && r.oldTip && r.oldTip !== r.tip) {
    const ahead = dag.aheadBehind(r.k, r.tip, r.oldTip).ahead;
    for (const c of ahead)
      out.row("?" + c.hashlet + (c.subject ? "#" + c.subject : ""), "post",
              c.ts, { _post: true });
  }

  //  ENQUEUE the root reconcile then the del-sweep fold row LAST (post-order:
  //  the fold sits after every delete leaf the reconcile fans out).
  return { enqueue: [{ verb: "get", uri: "?" + newTree + "#" + oldTree },
                     { verb: "get", uri: DELSWEEP }] };
}

//  Dirty-overlap PRE-PASS (SNIFFOVRL): on an UPDATE, a wt file present on disk
//  that the NEW tree introduces but the OLD baseline never carried, AND whose
//  bytes differ from the target, is an un-mergeable overlay → refuse.  A whole-
//  tree set difference (new \ old) intersected with dirty wt files — a barrier,
//  not a per-file job.  Fresh clone → no baseline → no overlap (no-op).
function dirtyOverlapCheck(k, newTree, oldTree, wt, fresh) {
  if (fresh || !oldTree) return;
  const oldPaths = {};
  k.readTreeRecursive(oldTree, function (l) { oldPaths[l.path] = l.sha; });
  const conflicts = [];
  k.readTreeRecursive(newTree, function (l) {
    if (oldPaths[l.path] !== undefined) return;     // had a baseline → mergeable
    const full = join(wt, l.path);
    if (!exists(full)) return;                       // no on-disk overlay
    const obj = k.getObject(l.sha);
    const bytes = obj ? obj.bytes : new Uint8Array(0);
    if (!checkout.leafUnchanged(full, { kind: kindOf(l.mode) }, bytes))
      conflicts.push(l.path);
  });
  if (conflicts.length)
    throw "be get: GETOVRL dirty wt overlays un-baselined target: " +
          conflicts.slice(0, 5).join(", ");
}

//  RECONCILE or LEAF: route on the URI shape.  A dir-reconcile row has an empty
//  path or a path ending `/`; a write/merge/delete leaf names a single file.
function handleReconcileOrLeaf(uri, ctx) {
  const u = new URI(uri);
  const path = u.path || "";
  const isDir = path === "" || path[path.length - 1] === "/";
  if (isDir) return reconcileDir(uri, ctx);
  return leaf(uri, ctx);
}

//  dir-reconcile: read the old + new trees ONE level, MERKLE-PRUNE unchanged
//  subtrees (old==new sha → skip the whole subtree), and emit per-entry rows:
//    a file/exe/symlink/gitlink leaf changed → `get <path>?<new>#<old>`,
//    a removed entry (old, not new)          → `get <path>?#<old>` (delete),
//    a changed subdir                        → recurse `get <sub>/?<new>#<old>`.
//  Each leaf's kind (the tree-entry mode) is stashed on ctx._get.kinds[path]
//  (the queue round-trip carries only the sha).  The new-vs-mod label is the
//  LEAF's call; the del-sweep collapse is the trailing BARRIER fold.
function reconcileDir(uri, ctx) {
  const g = ctx._get;
  const u = new URI(uri);
  const prefix = u.path || "";                 // "" (root) or "<dir>/"
  const newTree = u.query || "";
  const oldTree = (u.fragment || "").replace(/^\?/, "");

  const nm = treeMap(g.k, newTree);
  const om = treeMap(g.k, oldTree);
  const enqueue = [];

  const names = {};
  for (const n in nm) names[n] = 1;
  for (const n in om) names[n] = 1;
  const sorted = Object.keys(names).sort();

  for (const name of sorted) {
    const ne = nm[name], oe = om[name];
    const rel = prefix + name;
    //  MERKLE-PRUNE: identical sha + dir-ness on both sides → unchanged; skip.
    if (ne && oe && ne.sha === oe.sha && ne.isDir === oe.isDir) continue;

    if (ne && ne.isDir) {                       // changed/new subdir → recurse
      enqueue.push({ verb: "get",
                     uri: rel + "/?" + ne.sha + "#" + (oe && oe.isDir ? oe.sha : "") });
      if (oe && !oe.isDir) { g.kinds[rel] = kindOf(oe.mode);   // stale file → del
        enqueue.push({ verb: "get", uri: rel + "?#" + oe.sha }); }
      continue;
    }
    if (ne) {                                   // a (changed) file/exe/lnk/sub
      g.kinds[rel] = kindOf(ne.mode);
      enqueue.push({ verb: "get",
                     uri: rel + "?" + ne.sha + "#" + (oe && !oe.isDir ? oe.sha : "") });
      if (oe && oe.isDir)                        // old dir → recurse to delete
        enqueue.push({ verb: "get", uri: rel + "/?#" + oe.sha });
      continue;
    }
    //  removed entry (old, not new).
    if (oe && oe.isDir)
      enqueue.push({ verb: "get", uri: rel + "/?#" + oe.sha });
    else { g.kinds[rel] = kindOf(oe.mode);
      enqueue.push({ verb: "get", uri: rel + "?#" + oe.sha }); }
  }
  return { enqueue: enqueue };
}

//  LEAF: a single-path effect.  `get <path>?<new>#<old>` writes/weaves the new
//  blob (the leaf reads its OWN on-disk bytes to label new-vs-mod and to decide
//  clean-overwrite vs 3-way weave); `get <path>?#<old>` deletes a removed path.
//  Gitlink (submodule) leaves are recorded-only (mount recursion is a follow-up).
function leaf(uri, ctx) {
  const g = ctx._get, out = ctx.out;
  const u = new URI(uri);
  const rel = u.path || "";
  const newSha = u.query || "";
  let oldSha = u.fragment || "";
  if (oldSha[0] === "?") oldSha = oldSha.slice(1);
  const full = join(g.wt, rel);
  const kind = g.kinds[rel] || "f";

  if (!newSha) {                                // delete LEAF (removed path)
    try { io.unlink(full); out.row(rel, "del", g.ts); g.dels++; } catch (e) {}
    return;
  }
  if (kind === "s") return;                     // gitlink: recorded, not written

  const obj = g.k.getObject(newSha);
  if (!obj) return;                             // unresolved → skip
  const bytes = obj.bytes;
  const existed = exists(full);
  //  Skip if the on-disk path already matches the target (preserves dirty bytes
  //  equal to the target; emits no row — native get only reports what moved).
  if (existed && checkout.leafUnchanged(full, { kind: kind }, bytes)) return;
  //  v1 clean-overwrites (the dirty 3-way weave is the follow-up); the leaf is
  //  where that decision belongs (it reads its own bytes).
  checkout.materialise(g.wt, rel, { kind: kind }, bytes);
  out.row(rel, existed ? "upd" : "new", g.ts);
}

//  del-sweep BARRIER (get_drain_unlinks, GET.c:668): the trailing fold over the
//  delete leaves.  Back-scans the live queue ULog from this fold row to the
//  newest delete-bearing reconcile boundary, RE-READING the `get <p>?#<old>`
//  delete rows in range (core/barrier.js) — a durable, idempotent aggregate.
//  JS has no rmdir leaf (checkout.js note), so the empty-dir collapse is a
//  no-op; the per-path `del` rows ARE the durable effect (already emitted).
function delSweep(row, ctx) {
  const q = ctx && ctx.queue;
  if (!q || !q.path || row.offset == null) return;
  //  Fold ALL rows before this fold row, counting the delete leaves
  //  (`get <p>?#<old>`: empty query + a fragment).  A sentinel markerVerb that
  //  never appears makes seekBack miss, so the fold spans the whole queue head
  //  → here (core/barrier.js).  Report-only: JS has no rmdir leaf, so the
  //  empty-dir collapse is a no-op; the per-path `del` rows are the effect.
  const res = barrier.fold(q.path, row.offset, "::root", function (acc, r) {
    const u = new URI(r.uri);
    if ((u.query || "") === "" && (u.fragment || "") !== "") acc++;   // a delete
    return acc;
  }, 0);
  ctx._get.delSwept = res.acc;
}

//  Edge flush comparator (native get layout): pulled-commit `post` rows first
//  in push order (newest-first by author ts), then file rows — new+upd
//  interleaved lex by path, THEN del lex by path.  A stable partition keeps the
//  post block put; the file groups sort by uri.
function sortGetRows(rows) {
  const post = [], nu = [], del = [];
  for (const r of rows) {
    if (r._post) post.push(r);
    else if (r.verb === "del") del.push(r);
    else nu.push(r);
  }
  const byUri = function (a, b) { return a.uri < b.uri ? -1 : a.uri > b.uri ? 1 : 0; };
  nu.sort(byUri); del.sort(byUri);
  return post.concat(nu, del);
}

//  jab injects module.exports for a required module; the loop requires this as
//  the `get` verb handler.  No self-run tail (JSQUE-009: handler, not main()).
