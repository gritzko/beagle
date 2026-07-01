//  delete.js — `be delete` as a loop HANDLER (JSQUE-011).  Reproduces native
//  `be delete` byte-equivalently: the per-file UNLINK leaf (dirty-gate →
//  io.unlink → `delete <path>` row), the bare-sweep of tracked files gone from
//  disk, the dir-form recursive unlink with its PREFLIGHT barrier, and the
//  `?br` branch tombstone.  Pure JS over JABC + ./lib/* (libabc+libdog ONLY;
//  the staging engine is lib/stage.js (reused), the row writer ulog.append,
//  the ref-tombstone writer store.tombstone).  See JSQUE-001/008 + DELETE.md.
//
//  LOOP SHAPE (JSQUE-011): converted from a `main();` one-shot to
//  `module.exports = handle(row, ctx)`.  The seed (resolve.seed) scatters args:
//  each PATH form is a seed row, each `?br` form a ctx.refs op.  DELETE's batch
//  spans the full arg list (one shared `delete:` table, the dir preflight that
//  must run before ANY unlink, the batch dirty-abort), so the handler folds the
//  WHOLE batch (ctx.refs branches + ctx.seedRows paths) on its FIRST row and
//  no-ops on the rest (ctx._delDone guard).  Output via ctx.out; no main() tail.
//
//  Slot dispatch (mirrors sniff/SNIFF.exe.c is_delete + sniff/DEL.c):
//    ?br            → branch tombstone (DELBranch) — a PRE-LOOP BARRIER guarded
//                     by trunk / wt-on-branch / active-descendant / `-r`
//                     (deepest-first); refs run before any unlink leaf
//    (bare)         → sweep: a `delete <path>` row per tracked file gone
//                     from disk (del_sweep_missing)
//    <dir>/ | <dir> → dir-form: PREFLIGHT barrier (dirty-check the subtree)
//                     before unlinking all, then one `delete <dir>/` row
//    <file>         → file-form leaf: dirty-gate → io.unlink → `delete <path>`
//
//  DIRTY-GATE mirrors native DEL.c (DIS-004, 9718a03a): refuse only when
//  `!force && mtime ∉ stamp-set && content ≠ baseline` — the mtime miss is a
//  HINT, a file whose bytes still equal the baseline blob is clean, and
//  `--force`/`-r` skips the gate.  DELETE does NOT restamp — the file is gone.

"use strict";

//  JSQUE-008/011: sibling libs via relative require ("./lib/X.js"), resolved
//  against this module's own dir — robust under the resident loop (NOT
//  argv[1]/__dirname; the handler is require'd, never the entry script).
//  JSQUE-016: by-verb reorg — core/discover + shared/ kernel via ../../ .
const be      = require("../../core/discover.js");
const wtlog   = require("../../shared/wtlog.js");
const store   = require("../../shared/store.js");
const stage   = require("../../shared/stage.js");
const ulog    = require("../../shared/ulog.js");
const recurse = require("../../core/recurse.js");     // SUBS-044: mounted-sub walk

const DELDIRTY = "DELDIRTY";
const SNIFFFAIL = "SNIFFFAIL";

function join(d, n) { return d === "/" ? "/" + n : d + "/" + n; }
function statExists(p) { try { io.lstat(p); return true; } catch (e) { return false; } }
function statKind(p) { try { return io.lstat(p).kind; } catch (e) { return undefined; } }

//  Normalise a bareword arg: `.`/`./` → "" (reporoot), strip a leading
//  `./` (mirrors del_stage_named's reporoot normalisation, as in put.js).
function normRel(raw) {
  if (raw === "." || raw === "./") return "";
  if (raw.indexOf("./") === 0) return raw.slice(2);
  return raw;
}

//  SUBS-044: bare `be delete` descends each MOUNTED sub PRE-ORDER, sweeping the
//  sub's interior `mis` (tracked, gone from disk) into the SUB's OWN wtlog and
//  relaying prefixed `delete <sub>/<path>` rows + a `<sub>/swept N` summary +
//  one trailing blank (native's sub-relay frame).  The parent gitlink bump is
//  POST's job.  Reuses core/recurse.walk (mount gate, `.gitmodules` order).
function bareSweepSubs(repo, prefix, items) {
  recurse.walk(repo, prefix, function (subRepo, subPrefix) {
    //  SUBS-044: this sub FIRST (rows + summary + relay-frame blank), THEN its
    //  grandchildren — native's pre-order relay.
    const subK = store.open(subRepo.storePath, subRepo.project);
    const eng = stage.prep(subRepo, wtlog.open(subRepo), subK);
    const rows = [];
    if (eng.haveBase && eng.baseTreeSha)
      subK.readTreeRecursive(eng.baseTreeSha, function (leaf) {
        if (leaf.kind === "s") return;
        if (stage.isMeta(leaf.path)) return;
        if (statExists(join(subRepo.wt, leaf.path))) return;
        rows.push({ uri: leaf.path });
        items.push({ type: "row", path: subPrefix + "/" + leaf.path });
      });
    if (rows.length > 0) {
      const uris = rows.map(function (r) { return { verb: "delete", uri: r.uri }; });
      ulog.append(subRepo.bePath, uris);                // write to the SUB wtlog
      items.push({ type: "summary",
                   text: subPrefix + "/swept " + rows.length + " missing file(s)" });
      items.push({ type: "blank" });                    // native sub-relay frame
    }
    bareSweepSubs(subRepo, subPrefix, items);           // then descend grandchildren
  });
}

//  --- DELStage (path / bare forms): build the row list, then write -------
//  Mirrors sniff/DEL.c::del_stage_named (named) + del_sweep_missing (bare).
//  Returns { banner, dirty } where `banner` is the ordered stdout line list
//  (the `delete:` table: a header for the named form, rows + skips + the
//  count/sweep summary) and `dirty` is true on a DELDIRTY refusal.
function delStage(repo, k, pathRaws, force) {
  const eng = stage.prep(repo, wtlog.open(repo), k);
  const wtl = wtlog.open(repo);
  const rows = [];            // { uri } delete rows in emit order
  const items = [];           // stdout banner items, native order
  let unlinked = 0, skipped = 0, dirtyRaw = null;

  //  --- bare sweep (no path args) -------------------------------------
  if (pathRaws.length === 0) {
    //  No baseline → nothing tracked → quiet no-op (native: empty table).
    if (eng.haveBase && eng.baseTreeSha) {
      //  Walk the baseline tree in native WALK order (depth-first, git tree
      //  position); a tracked LEAF gone from disk gets a `delete` row.
      k.readTreeRecursive(eng.baseTreeSha, function (leaf) {
        if (leaf.kind === "s") return;             // gitlink subtree, skip
        if (stage.isMeta(leaf.path)) return;
        if (statExists(join(repo.wt, leaf.path))) return;
        rows.push({ uri: leaf.path });
        items.push({ type: "row", path: leaf.path });
      });
    }
    if (rows.length > 0)
      items.push({ type: "summary",
                   text: "swept " + rows.length + " missing file(s)" });
    //  SUBS-044: recurse mounted subs (pre-order) — sub `mis` rows write to the
    //  sub wtlogs and ride `items` prefixed; the parent `rows` stay parent-only.
    bareSweepSubs(repo, "", items);
    return { banner: { bare: true, items: items }, dirty: false,
             rows: rows };
  }

  //  --- named delete (one or more path args) --------------------------
  //  Native del_stage_named has NO meta-path skip: a named `.be/...` falls
  //  through to the normal file/dir logic (dirty-gate / baseline check), so
  //  we don't special-case meta here (only the dir walk + baseline skip it).
  const files = [];           // file-form rels deferred to a second pass
  for (const raw0 of pathRaws) {
    const raw = normRel(raw0);
    //  Dir-form: empty (reporoot), trailing slash, or an on-disk dir.
    let isDir = raw === "" || raw[raw.length - 1] === "/";
    if (!isDir && statKind(join(repo.wt, raw)) === "dir") isDir = true;
    if (isDir) {
      const dirRaw = raw === "" ? "" : (raw[raw.length - 1] === "/" ? raw : raw + "/");
      const r = delDir(repo, eng, wtl, dirRaw, force);
      if (r.dirty) { dirtyRaw = r.dirtyPath; break; }
      //  Per-dir summary (`<dir>/ — N file(s) unlinked`, DEL.c del_dir's
      //  HUNKTableSummary) when the dir existed; the dir unlinks do NOT feed
      //  the final `deleted N file(s)` count (that tallies file-form only).
      if (r.existed)
        items.push({ type: "summary",
                     text: dirRaw + " — " + r.unlinked + " file(s) unlinked" });
      //  One `delete <dir>/` row even when the dir was already absent
      //  (native appends it idempotently after del_dir's done).
      rows.push({ uri: dirRaw });
      items.push({ type: "row", path: dirRaw });
      continue;
    }
    files.push(raw);
  }

  //  File-form pass (after every dir arg), in arg order.
  if (!dirtyRaw) for (const raw of files) {
    const full = join(repo.wt, raw);
    if (statExists(full)) {
      //  Dirty-gate (DIS-004): mtime ∈ stamp-set ⇒ tracked-clean; a mtime
      //  miss is only a HINT — bytes still == baseline blob ⇒ clean; `force`
      //  skips the gate.  ∉ stamp-set AND content drift ⇒ refuse.
      const w = eng.wt[raw];
      const known = w && w.ts != null && wtl.has(w.ts);
      const clean = known || force ||
        (eng.base[raw] && stage.wtEqBase(repo.wt, raw, eng.base[raw].sha));
      if (!clean) { dirtyRaw = raw; break; }
      io.unlink(full);
      unlinked++;
    } else {
      //  Already absent: emit a row only if the path was in the baseline
      //  tree (tracked); otherwise a no-op (a typo / never-tracked path).
      if (!eng.base[raw]) { skipped++; continue; }
    }
    rows.push({ uri: raw });
    items.push({ type: "row", path: raw });
  }

  if (dirtyRaw)
    return { banner: { bare: false, items: items }, dirty: true,
             dirtyPath: dirtyRaw, rows: rows };

  //  Final count summary (`deleted N file(s) (M row(s)[, K skipped])`).
  let summ = "deleted " + unlinked + " file(s) (" + rows.length + " row(s)";
  if (skipped > 0) summ += ", " + skipped + " skipped";
  summ += ")";
  items.push({ type: "summary", text: summ });
  return { banner: { bare: false, items: items }, dirty: false, rows: rows };
}

//  --- dir-form recursive delete (del_dir) -------------------------------
//  Two passes: PREFLIGHT barrier (refuse DELDIRTY on the first dirty
//  descendant — mtime ∉ stamp-set AND content ≠ baseline, per DIS-004; `force`
//  skips it) then apply (unlink every descendant).  The whole subtree must
//  pass the gate before ANY unlink — atomic dir delete.  An already-absent dir
//  is an OK no-op (caller still appends the dir row).  Empty dirs are not removed.
function delDir(repo, eng, wtl, dirRaw, force) {
  const prefix = dirRaw;     // ends in "/" (or "" = reporoot)
  if (prefix !== "" && statKind(join(repo.wt, prefix.replace(/\/$/, ""))) !== "dir")
    return { dirty: false, unlinked: 0, existed: false };   // already absent

  //  Descendants on disk = wt-scan entries under the prefix (meta skipped
  //  by the scan/ignore already; double-guard with isMeta).
  const desc = [];
  for (const rel in eng.wt) {
    if (stage.isMeta(rel)) continue;
    if (prefix === "" ? true : rel.indexOf(prefix) === 0) desc.push(rel);
  }
  //  PREFLIGHT barrier: the first dirty descendant aborts (mtime ∉ stamp-set
  //  AND content ≠ baseline; `force` skips — DIS-004).  Runs before any unlink.
  for (const rel of desc) {
    const w = eng.wt[rel];
    const known = w && w.ts != null && wtl.has(w.ts);
    if (!known && !force &&
        !(eng.base[rel] && stage.wtEqBase(repo.wt, rel, eng.base[rel].sha)))
      return { dirty: true, dirtyPath: rel };
  }
  //  Apply: unlink every descendant.
  let n = 0;
  for (const rel of desc) { try { io.unlink(join(repo.wt, rel)); n++; } catch (e) {} }
  return { dirty: false, unlinked: n, existed: true };
}

//  --- branch tombstone (DELBranch) --------------------------------------
//  `?br` → a REFS `delete ?br#0…0` tombstone via store.tombstone.  A PRE-LOOP
//  BARRIER (runs before any unlink leaf).  Refuses:
//    * trunk (empty query)
//    * the wt's own current branch (would orphan the wt pointer)
//    * an active descendant `<target>/<sub>` exists, unless `recursive`
//  With `recursive`, drop descendants deepest-first to a fixed point first.
//  Each pass re-opens the store reader so the descendant scan sees the
//  tombstones written so far (the reader memoises its `refs` drain).
function delBranch(repo, target, recursive) {
  if (!target) {
    io.log("be delete: refusing to drop trunk\n");
    throw SNIFFFAIL;
  }
  //  wt-on-branch guard: the baseline tip's branch == target → refuse.
  const baseTip = wtlog.open(repo).baselineTip();
  if (baseTip && baseTip.query && baseTip.query === target) {
    io.log("be delete: wt is on `" + target + "` — switch to another "
           + "branch first (`be get ?..`)\n");
    throw SNIFFFAIL;
  }
  const k = store.open(repo.storePath, repo.project);   // fresh refs view
  //  Active-descendant scan: any non-tombstone local tip keyed `<target>/…`.
  if (hasDescendant(k, target)) {
    if (!recursive) {
      io.log("be delete: `" + target + "` has active descendant branches"
             + " — pass `--force` (or `-r`) to drop the subtree\n");
      throw SNIFFFAIL;
    }
    //  Recursive: drop the deepest descendant per pass to a fixed point
    //  (each pass re-opens the reader so prior tombstones are visible).
    for (;;) {
      const kk = store.open(repo.storePath, repo.project);
      let best = null;
      eachDescendant(kk, target, function (q) {
        if (best === null || q.length > best.length) best = q;
      });
      if (best === null) break;
      delBranch(repo, best, false);
    }
  }
  store.tombstone(k.shard, target);
  io.log("be delete: deleted ?" + target + "\n");
}

//  YES iff some active (non-tombstone) local tip is a strict descendant of
//  `target` (key `<target>/<sub>` with extra bytes).  store.eachTip yields
//  latest-per-key, tombstones already filtered — matching native's REFSEach
//  over the latest rows.
function hasDescendant(k, target) {
  let found = false;
  eachDescendant(k, target, function () { found = true; });
  return found;
}

function eachDescendant(k, target, cb) {
  const pre = target + "/";
  k.eachTip(function (t) {
    const q = t.key === "?" ? "" : (t.key || "");
    if (q.length > pre.length && q.indexOf(pre) === 0) cb(q);
  });
}

//  --- emit the `delete:` banner via ctx.out (JSQUE-011) -----------------
//  Header + per-row lines columnise via out.row (dated header, blank-ts rows);
//  a summary line is plain text (no date/verb column) via out.raw.  Mirrors
//  the active HUNK `delete:` table native opens for every DELStage run.
function emitBanner(out, banner) {
  if (!banner.bare) out.row("delete:", "delete", ron.now());
  for (const it of banner.items) {
    if (it.type === "row") out.row(it.path, "delete", 0n);
    else if (it.type === "summary") out.raw(it.text);
    else if (it.type === "blank") out.raw("");          // SUBS-044 sub-relay frame
  }
}

//  JSQUE-011: `be delete` as a loop HANDLER.  Folds the WHOLE batch on its
//  FIRST row (ctx._delDone guard) — branch tombstones (ctx.refs) are the
//  pre-loop barrier, then DELStage runs the bare/path/dir forms; output via
//  ctx.out (ONE flush at the loop edge).  A DELDIRTY refusal throws past the
//  loop (process exit code) AFTER emitting the partial banner, matching native.
module.exports = function handle(row, ctx) {
  //  Batch-fold guard: the seed lowers each path arg to its own row, but
  //  DELETE's `delete:` table / dir-preflight / dirty-abort span the full arg
  //  list — process once, no-op on every later row.
  if (ctx._delDone) return;
  ctx._delDone = true;

  const flags = (ctx && ctx.flags) || [];
  const recursive = flags.indexOf("-r") >= 0 || flags.indexOf("--force") >= 0;
  const out = ctx && ctx.out;
  const repo = (ctx && ctx.repo) || be.find();
  const k = store.open(repo.storePath, repo.project);

  //  Branch tombstones FIRST (the pre-loop barrier): each `?br` form pinned by
  //  the seed into ctx.refs (op create/set) — a tombstone drops the label.
  for (const ref of (ctx.refs || [])) {
    if (ref.branch == null) continue;
    delBranch(repo, ref.branch || "", recursive);
  }

  //  Path/bare forms: the seed scatters path args one-per-row.  Reassemble the
  //  batch from ctx.seedRows, dropping the synthetic "." a no-path seed carries
  //  (loop.js collapses an EMPTY seed to ONE "." row — true for both a bare
  //  `be delete` AND a branch-only `be delete ?br`; ctx.refs disambiguates).
  const pathRaws = [];
  for (const sr of (ctx.seedRows || [])) {
    const u = sr.uri;
    if (u == null || u === ".") continue;
    pathRaws.push(u);
  }
  //  A pure branch-form invocation (only `?br` args, no path) prints nothing
  //  extra — DELStage does NOT run.  Only a bare `be delete` (no args at all,
  //  hence no ctx.refs) runs the sweep; with refs present + no paths, the synth
  //  "." is the branch op's residue, not a sweep request.
  if (pathRaws.length === 0 && (ctx.refs || []).length > 0) return;

  const res = delStage(repo, k, pathRaws, recursive);
  if (res.rows.length > 0) {
    const uris = res.rows.map(function (r) { return { verb: "delete", uri: r.uri }; });
    ulog.append(repo.bePath, uris);
  }
  emitBanner(out, res.banner);          // always (the open `delete:` table)
  if (res.dirty) {
    //  JSQUE-014: the loop edge now flushes the partial banner before the throw
    //  propagates (no per-handler flush); just emit the diag + throw.
    io.log("be delete: " + res.dirtyPath + " has unstamped changes — "
           + "stage with `be put` or revert before deleting\n");
    throw DELDIRTY;                      // non-zero exit (native DELDIRTY)
  }
};
