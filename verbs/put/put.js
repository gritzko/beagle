//  put.js — `be put` as a loop HANDLER (JSQUE-010; from the JS-049 one-shot).
//  Reproduces native `be put` byte-equivalently: stage files for the next
//  commit (one `put` row per URI + an mtime restamp) and the ref-write forms
//  (`?br` create, `?br#sha` / `?#sha` / `?<40hex>` set).  Pure JS over the
//  JABC bindings + be/lib/* (libabc+libdog ONLY; staging engine lib/stage.js,
//  row writer ulog, ref writer store.set/createShard).
//
//  JSQUE-010 split: the resolution-at-entry SEED (core/resolve.js) fans the
//  argv into one branch-free path row per arg AND pins the ref-write forms
//  into ctx.refs ONCE; this handler consumes them per row.
//    <file>         → stage one file        (the per-file STAGE LEAF)
//    <dir>/ | <dir> → dir-form: stage each tracked-dirty / untracked file
//    <old>#<new>    → move: rename on disk + a `put <old>#<new>` row (leaf)
//    ?br[#sha] etc. → ref-write, applied once from ctx.refs (no banner)
//    (bare, no arg) → auto-pair moves + tracked walk (PUT-004: bareStage,
//                     classifier+wtlog-sourced whole-tree fold).
//
//  Each staged file: one `put` row + an io.setMtime restamp to that row's ts
//  (so a later `be put` / POST fast-paths it via the stamp-set).  The `put:`
//  banner + per-row lines go through the emit sink (ctx.out); ONE flush at the
//  loop edge.
//
//  Usage:  be put [<path>... | <dir>/ | <old>#<new> | ?<branch>[#<sha>]]
//          jab be/loop.js put [args]        (JSQUE-010 resident-loop handler)

"use strict";

//  JSQUE-010: sibling libs via relative require ("./lib/X.js"), resolved against
//  this module's own dir — robust under the resident loop (not argv[1]/__dirname).
//  JSQUE-016: by-verb reorg — core/discover + shared/ kernel via ../../ .
const be      = require("../../core/discover.js");
const wtlog   = require("../../shared/wtlog.js");
const store   = require("../../shared/store.js");
const stage   = require("../../shared/stage.js");
const classify = require("../../shared/classify.js");
const ulog    = require("../../shared/ulog.js");
const isFullSha = require("../../shared/util/sha.js").isFullSha;

//  JSQUE-010: the `put:` banner + per-row lines now render through the emit sink
//  (ctx.out, JSQUE-005), not a local render.js call — the loop does ONE flush.
const PUTDUP = "PUTDUP";
const SNIFFFAIL = "SNIFFFAIL";

//  Normalise a bareword arg: `.`/`./` → "" (reporoot), strip a leading
//  `./` (mirrors put_stage_named's reporoot normalisation).
function normRel(raw) {
  if (raw === "." || raw === "./") return "";
  if (raw.indexOf("./") === 0) return raw.slice(2);
  return raw;
}

//  --- ref-write forms (PUTCreateBranch / PUTSetBranch) -------------------
//  Write the branch's REFS row via store.set (OUTRIGHT, non-FF; PUT is
//  unconstrained).  `?br` create refuses PUTDUP when the branch resolves.

function refCreate(repo, k, branch) {
  if (k.resolveRef(branch)) {
    io.log("be put: ?" + branch + " already exists\n");
    throw PUTDUP;
  }
  //  Create at cur.tip — label-only fork (POSTPromote allow_create arm).
  const cur = wtlog.open(repo).curTip();
  if (!cur || !cur.sha || !isFullSha(cur.sha)) throw SNIFFFAIL;
  store.set(k.shard, branch, cur.sha);
  return { verb: "put", uri: "?" + branch + "#" + cur.sha.slice(0, 8) };
}

function refSet(repo, k, branch, sha) {
  if (!isFullSha(sha)) throw SNIFFFAIL;
  //  DIS-050: dedup like native REFSAppendVerb (keeper/REFS.c) — setting a
  //  ref to the value it already resolves to writes NO row (keeps .be/refs
  //  bit-identical across repeats); only a real change appends.
  if (k.resolveRef(branch) === sha)
    return { verb: "put", uri: "?" + branch + "#" + sha.slice(0, 8) };
  //  Materialise the shard for a not-yet-existing branch (idempotent), then
  //  append the REFS row.  Trunk ("") writes the project shard's own refs.
  if (branch && !k.resolveRef(branch)) store.createShard(k.shard, branch);
  store.set(k.shard, branch, sha);
  return { verb: "put", uri: "?" + branch + "#" + sha.slice(0, 8) };
}

//  JSQUE-010: per-arg slot classification (the C is_put split) now lives at the
//  SEED — core/resolve.js pins `?br[#sha]` / `?#sha` / `?<40hex>` to a 40-hex
//  ref op ONCE (resolution-at-entry, JSQUE-004) and delivers it via ctx.refs;
//  applyRefs below just writes those pinned ops.  No per-row re-classification.

//  --- per-arg leaf STAGE (JSQUE-010) -------------------------------------
//  Classify ONE path arg (file / dir / move) into staging ops + banner items
//  via the staging engine.  This is the per-file STAGE LEAF the loop fans to
//  (one seed row per arg, resolution-at-entry).  The bare-walk + move
//  auto-pair (no path arg) is the whole-tree fold handled by bareStage
//  (PUT-004), not this per-arg leaf.
//  Returns { ops, items } where an op is a stage.js op and an item is a
//  banner line (`{type:"row", opIdx}` or `{type:"skip", …}`), native order.
function stageArg(eng, repo, uri) {
  const ops = [], items = [];
  function pushRow(op) { ops.push(op); if (!op.silent && op.path !== null) items.push({ type: "row", opIdx: ops.length - 1 }); }
  function pushSkip(path, reason, whole) { if (reason === "is unchanged") return; items.push({ type: "skip", path: path, reason: reason, whole: !!whole }); }

  const u = new URI(uri);
  //  Move-form: non-empty path AND fragment (the dest path slot).
  if (u.path && u.fragment) {
    pushRow(eng.explicitMove(normRel(u.path), normRel(u.fragment)));
    return { ops: ops, items: items };
  }
  let raw = normRel(u.query || u.path || "");
  if (raw && stage.isMeta(raw)) { pushSkip(raw, "is a meta path"); return { ops: ops, items: items }; }
  //  Dir-form: empty (reporoot), trailing slash, or an on-disk dir.
  let isDir = raw === "" || raw[raw.length - 1] === "/";
  let reframed = false, origRaw = raw;
  if (!isDir) {
    let kind;
    try { kind = io.lstat(join(repo.wt, raw)).kind; } catch (e) {}
    if (kind === "dir") { raw = raw + "/"; isDir = true; reframed = true; }
  }
  if (isDir) {
    if (raw !== "") {
      let kind;
      try { kind = io.lstat(join(repo.wt, raw.replace(/\/$/, ""))).kind; } catch (e) {}
      if (kind !== "dir") { pushSkip(raw, "does not exist"); return { ops: ops, items: items }; }
    }
    const ex = eng.expandDir(raw);
    if (ex.ops.length === 0) {
      if (ex.sawTracked) pushSkip(raw, "is unchanged");
      else if (reframed) pushSkip(origRaw, "has no files to stage — skipped (did you mean `" + raw + "`?)", true);
      else pushSkip(raw, "has no files to stage");
      return { ops: ops, items: items };
    }
    for (const op of ex.ops) pushRow(op);
    return { ops: ops, items: items };
  }
  //  File-form leaf.
  const d = eng.classifyNamed(raw);
  if (!d.stage) { pushSkip(raw, d.reason); return { ops: ops, items: items }; }
  pushRow({ path: raw, kind: "put", restamp: raw });
  return { ops: ops, items: items };
}

//  Write the staging `ops` to the wtlog (one `put` row per non-silent op,
//  ops order) and restamp each op's file to its assigned ts (the provenance
//  pin POST reads).  `floorTs` is the cohort T0 the row ts never precedes
//  (JSQUE-010 cohort-T0 stamp).  Returns the count of rows staged.
function commitOps(repo, ops, floorTs) {
  const stageOps = ops.filter(function (o) { return o.path !== null; });
  if (stageOps.length === 0) {
    for (const op of ops)
      if (op.path === null && op.stampTs != null)
        trySetMtime(join(repo.wt, op.restamp), op.stampTs);
    return 0;
  }
  const rows = [];
  for (const op of stageOps)
    rows.push({ verb: "put", uri: op.dst ? (op.path + "#" + op.dst) : op.path });
  const assigned = appendAndAssign(repo.bePath, rows, floorTs);
  let ri = 0;
  for (const op of ops) {
    if (op.path === null) {
      if (op.stampTs != null) trySetMtime(join(repo.wt, op.restamp), op.stampTs);
      continue;
    }
    const ts = assigned[ri++];
    if (op.restamp) trySetMtime(join(repo.wt, op.restamp), ts);
  }
  return stageOps.length;
}

//  Append `rows` to the wtlog and return the ASSIGNED ts (BigInt) per row,
//  in order.  Mirrors ulog.append's ts policy (nowAfter(tail) then +1 per
//  row) so the restamp uses the exact stamp the row got.  `floorTs` (the
//  cohort T0, JSQUE-010) raises the first stamp so a multi-row run shares one
//  monotone cohort even across separate per-arg handler calls.
function appendAndAssign(bePath, rows, floorTs) {
  const old = [];
  ulog.each(bePath, function (log) {
    old.push({ verb: log.verb, uri: log.uri, ts: log.time });
  });
  const tail = old.length ? old[old.length - 1].ts : 0n;
  let ts = ulog.nowAfter(tail);
  if (floorTs != null) { const f = BigInt(floorTs); if (f > ts) ts = f; }
  const assigned = [];
  const fresh = rows.map(function (r) {
    const row = { verb: r.verb, uri: r.uri, ts: ts };
    assigned.push(ts);
    ts = ts + 1n;
    return row;
  });
  ulog.write(bePath, old.concat(fresh));
  return assigned;
}

function trySetMtime(full, ts) { try { io.setMtime(full, BigInt(ts)); } catch (e) {} }
function join(d, n) { return d === "/" ? "/" + n : d + "/" + n; }

//  A skip summary line as native put_skip / the dir hint render it (stdout,
//  no date/verb column).  `whole` items already carry the full message.
function skipText(s) {
  if (s.whole) return s.path + " " + s.reason;
  return s.path + " " + s.reason + " — skipped";
}

//  Apply the seed-pinned ref-write ops (ctx.refs) ONCE per run via store.set
//  (OUTRIGHT, non-FF) — `?br` create / `?br#sha` set / `?#sha` trunk / `?<40hex>`
//  set.  No stdout banner (native routes ref-writes around PUTStage).  Refs are
//  pinned at the seed (resolution-at-entry, JSQUE-004); read from ctx, not the
//  row.  Gated by ctx._putRefsDone so a multi-row fan-out applies them once.
function applyRefs(repo, k, ctx) {
  if (!ctx || ctx._putRefsDone) return;
  ctx._putRefsDone = true;
  for (const r of ctx.refs || []) {
    if (r.op === "create") refCreate(repo, k, r.branch);
    else refSet(repo, k, r.branch, r.sha);
  }
}

//  --- bare `be put` (no path args) — PUT-004 ------------------------------
//  Stage the WHOLE working tree against the baseline, byte-for-byte as native
//  (sniff/PUT.c put_detect_moves + put_visit_tracked).  The dirty list is
//  SOURCED from the non-recursive classifier (shared/classify.js::classifyMerge,
//  which merges base-tree ⊕ wt-scan ⊕ wtlog put/del into per-path buckets) PLUS
//  the wtlog — NOT a hand-rolled second tree walk: a path already staged since
//  the last get/post shows as `put`/`new`/`mov` and is naturally skipped.  The
//  per-path DECISION (mod / mis / unk / ok) comes from the classifier; the
//  baseline-tree WALK order and the get/post stamp fast-path (native's clean
//  short-circuit) are layered from the same baseline tree the engine loaded, so
//  the emitted rows are identical to stage.js::bareWalk (the semantics oracle):
//    - mod (bytes != baseline)              → one `put <path>` row, WALK order
//    - mis↔unk content-sha 1:1 auto-pair    → SILENT `put <old>#<new>` move row
//                                             (BEFORE the dirty rows; PUTAMBIG
//                                              when a sha matches >1 candidate)
//    - ok  (bytes == baseline)              → restamp mtime to baselineTs, NO row
//    - unk that is NOT a move dest          → NEVER staged (needs `be put <f>`)
//  Returns { ops } shaped for commitOps (moves first, then dirty/restamp ops).
function bareStage(repo, wtl, k) {
  const eng = stage.prep(repo, wtl, k);
  const wtRoot = repo.wt;
  if (!eng.haveBase || !eng.baseTreeSha) return { ops: [] };

  //  Dirty list from the classifier (base ⊕ wt ⊕ wtlog put/del → buckets).
  //  wantClean yields the `ok` rows we need for the restamp.  A staged path
  //  already shows as put/new/mov, so it is absent from mod/mis/unk → skipped.
  const cls = classify.classifyMerge(repo, wtl, k, { wantClean: true, skipMeta: true });
  const mod = {}, mis = {}, unk = {}, ok = {};
  for (const r of cls.rows) {
    if (r.bucket === "mod") mod[r.path] = 1;
    else if (r.bucket === "mis") mis[r.path] = r.oldSha;
    else if (r.bucket === "unk") unk[r.path] = 1;
    else if (r.bucket === "ok") ok[r.path] = 1;
  }

  //  get/post stamp gate (native clean fast-path): a tracked file whose mtime
  //  is a get/post stamp is taken clean WITHOUT a content re-hash — left as-is
  //  (no row, no restamp), exactly like stage.js::bareWalk.
  const gpStamp = {};
  for (const r of wtl.rows)
    if (r.verb === "get" || r.verb === "post") gpStamp[r.ron] = true;
  function isGpStamp(rel) {
    const w = eng.wt[rel];
    return !!(w && w.ts != null && gpStamp[ron.encode(w.ts)]);
  }

  const ops = [];

  //  --- auto-pair mis↔unk system-`mv` renames (put_detect_moves) ----------
  //  A tracked path gone from disk (`mis`, baseline blob sha) + an untracked
  //  on-disk file (`unk`, hashed via diskSha) of identical content → one SILENT
  //  `put <old>#<new>` move op (written + dest restamped, NOT in the banner).
  //  Strictly 1:1; a sha matching >1 candidate either side throws PUTAMBIG.
  const baseCand = [];                      // { path, sha } in baseline WALK order
  const baseSeen = {};
  k.readTreeRecursive(eng.baseTreeSha, function (leaf) {
    if (leaf.kind === "s") return;
    if (stage.isMeta(leaf.path)) return;
    if (mis[leaf.path] && isFullSha(mis[leaf.path]) && !baseSeen[leaf.path]) {
      baseSeen[leaf.path] = 1;
      baseCand.push({ path: leaf.path, sha: mis[leaf.path] });
    }
  });
  const wtCand = [];                        // { path, sha }
  for (const rel in unk) {
    const s = stage.diskSha(wtRoot, rel);
    if (s) wtCand.push({ path: rel, sha: s });
  }
  const paired = {};                        // unk path → consumed (suppress later)
  if (baseCand.length && wtCand.length) {
    for (const bc of baseCand) {
      let match = -1, nmatch = 0;
      for (let j = 0; j < wtCand.length; j++)
        if (wtCand[j].sha === bc.sha) { match = j; nmatch++; }
      if (nmatch === 0) continue;
      if (nmatch > 1) throw stage.PUTAMBIG;
      let baseMatches = 0;
      for (const bc2 of baseCand) if (bc2.sha === wtCand[match].sha) baseMatches++;
      if (baseMatches > 1) throw stage.PUTAMBIG;
      const dst = wtCand[match].path;
      paired[dst] = 1;
      ops.push({ path: bc.path, dst: dst, kind: "mov", restamp: dst, silent: true });
    }
  }

  //  --- tracked-dirty walk over the baseline tree, native WALK order ------
  //  mod → `put` row; ok → restamp to baselineTs (no row); a get/post-stamped
  //  file is clean and left untouched (no row, no restamp).
  k.readTreeRecursive(eng.baseTreeSha, function (leaf) {
    const rel = leaf.path;
    if (leaf.kind === "s") return;          // gitlink: nothing to put
    if (stage.isMeta(rel)) return;
    if (!eng.wt[rel]) return;               // vanished on disk → move/skip
    if (isGpStamp(rel)) return;             // clean fast-path
    if (ok[rel]) {
      if (eng.baselineTs != null)
        ops.push({ path: null, kind: "restamp", restamp: rel,
                   stampTs: eng.baselineTs });
      return;
    }
    if (mod[rel]) ops.push({ path: rel, kind: "put", restamp: rel });
  });

  return { ops: ops };
}

//  JSQUE-010: `be put` as a loop HANDLER (converted from the `main();` one-shot).
//  Each call handles ONE seed row (one path arg, resolution-at-entry fan-out);
//  the `put:` banner header opens ONCE per run (ctx._putBannerOpen) and every
//  staged op pushes a blank-date row via ctx.out — the loop does ONE flush.  The
//  per-file STAGE is the LEAF; the bare-walk + move auto-pair (no path arg) is
//  the PUT-004 whole-tree fold (bareStage).  Ref-write forms are applied once
//  from ctx.refs.
module.exports = function handle(row, ctx) {
  const out = ctx && ctx.out;
  const repo = (ctx && ctx.repo) || be.find((row && row.uri) || undefined);
  const k = store.open(repo.storePath, repo.project);

  //  Ref-write forms first (no banner), once per run.
  applyRefs(repo, k, ctx);

  //  A 0-count seed means `row.uri` is the synthetic "." placeholder (a ref-only
  //  or no-arg run), NOT a real path arg — never stage it.  A ref-only run is
  //  done; a true no-arg `be put` is the bare whole-tree walk (PUT-004).
  const placeholder = ctx && ctx.seededRowCount === 0;
  if (placeholder) {
    if ((ctx.refs || []).length) return;       // ref-only run: nothing to stage
    //  PUT-004: bare `be put` — auto-pair moves + tracked-dirty walk, sourced
    //  from the classifier + wtlog (bareStage).  Single in-process whole-tree
    //  fold (one handler invocation, like delete.js's batch sweep), reusing
    //  commitOps for the row-write + restamp and ctx.out for the `put:` banner.
    //  Open the `put:` header BEFORE the walk (native PUTStage opens its table
    //  before move detection) so a PUTAMBIG refusal carries the same partial
    //  banner native does (the loop edge-catch flushes it on the throw).
    if (out && !ctx._putBannerOpen) { ctx._putBannerOpen = true; out.banner("put", "put:", ron.now()); }
    const r = bareStage(repo, wtlog.open(repo), k);  // may throw PUTAMBIG
    commitOps(repo, r.ops, ctx && ctx.T0);
    if (out)
      for (const op of r.ops)
        if (op.path !== null && !op.silent)
          out.row(op.dst ? (op.path + "#" + op.dst) : op.path, "put", 0n);
    return;
  }

  //  Open the shared `put:` table header ONCE (native opens it for every
  //  PUTStage run; the row lines below carry a BLANK date, native HUNK `.ts=0`).
  if (out && !ctx._putBannerOpen) { ctx._putBannerOpen = true; out.banner("put", "put:", ron.now()); }

  //  Stage this one arg (file / dir / move leaf), write its rows under the
  //  cohort T0, restamp, and push the banner lines (rows + skips) via ctx.out.
  const eng = stage.prep(repo, wtlog.open(repo), k);
  const r = stageArg(eng, repo, (row && row.uri) || "");
  commitOps(repo, r.ops, ctx && ctx.T0);
  if (out) {
    for (const it of r.items) {
      if (it.type === "skip") out.raw(skipText(it));
      else {
        //  blank-date row column (ts 0n, native HUNK `.ts=0`).
        const op = r.ops[it.opIdx];
        out.row(op.dst ? (op.path + "#" + op.dst) : op.path, "put", 0n);
      }
    }
  }
  //  JSQUE-014: tally staged/skipped across the per-arg fan-out (no enqueue, so
  //  one call per seed row).  On the LAST named row, an all-skip run is PUTNONE:
  //  emit the diag + throw so the loop edge flushes the partial banner + skips
  //  before the non-zero exit (native PUT.c put_stage_named, PUTNONE).
  ctx._putStaged = (ctx._putStaged || 0) + r.ops.filter(function (o) { return o.path !== null; }).length;
  ctx._putSkipped = (ctx._putSkipped || 0) + r.items.filter(function (it) { return it.type === "skip"; }).length;
  ctx._putCalls = (ctx._putCalls || 0) + 1;
  if (ctx._putCalls >= (ctx.seededRowCount || 1) && ctx._putStaged === 0) {
    if (ctx._putSkipped > 0) io.log("be put: no eligible paths\n");
    throw "PUTNONE";                     // non-zero exit (native PUTNONE)
  }
};
