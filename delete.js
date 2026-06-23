//  delete.js — `be delete` as a pure-JS extension (JS-050).  Reproduces
//  native `be delete` byte-equivalently: stage a tracked file's removal
//  (dirty-gate → unlink → `delete <path>` row), bare-sweep tracked files
//  gone from disk, dir-form recursive unlink, and the `?br` branch
//  tombstone.  Pure JS over JABC + bin/lib/* (libabc+libdog ONLY; the
//  staging engine is bin/lib/stage.js (reused, not reimplemented), the row
//  writer ulog.append, the ref-tombstone writer store.tombstone).
//
//  Slot dispatch (mirrors sniff/SNIFF.exe.c is_delete + sniff/DEL.c):
//    ?br            → branch tombstone via store.tombstone (DELBranch) with
//                     the trunk / wt-on-branch / active-descendant / `-r`
//                     (deepest-first) guards
//    (bare)         → sweep: a `delete <path>` row per tracked file gone
//                     from disk (del_sweep_missing)
//    <dir>/ | <dir> → dir-form: preflight dirty-check, unlink all, one row
//    <file>         → file-form: dirty-gate → io.unlink → `delete <path>`
//
//  DIRTY-GATE is mtime-only (`wtlog.has(mtime)`) to match native DEL.c:492
//  for byte-parity with the `be delete` oracle (DIS-004 tracks adding a
//  content double-check to NATIVE; matching it here would diverge).  DELETE
//  does NOT restamp — the file is gone.
//
//  Usage:  be delete [-r] [<path>... | <dir>/ | ?<branch>]

"use strict";

const self = process.argv[1];
const here = self.slice(0, self.lastIndexOf("/"));
const be      = require(here + "/lib/be.js");
const wtlog   = require(here + "/lib/wtlog.js");
const store   = require(here + "/lib/store.js");
const stage   = require(here + "/lib/stage.js");
const ulog    = require(here + "/lib/ulog.js");
const render  = require(here + "/lib/render.js");
const dateCol = render.dateCol, verbCol = render.verbCol,
      writeStdout = render.writeStdout;

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

//  --- DELStage (path / bare forms): build the row list, then write -------
//  Mirrors sniff/DEL.c::del_stage_named (named) + del_sweep_missing (bare).
//  Returns { banner, dirty } where `banner` is the ordered stdout line list
//  (the `delete:` table: a header for the named form, rows + skips + the
//  count/sweep summary) and `dirty` is true on a DELDIRTY refusal.
function delStage(repo, k, pathRaws) {
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
      const r = delDir(repo, eng, wtl, dirRaw);
      if (r.dirty) { dirtyRaw = r.dirtyPath; break; }
      unlinked += r.unlinked;
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
      //  Dirty-gate (mtime-only): the file's mtime must be in the stamp-set
      //  (last written by a tracked op).  ∉ stamp-set ⇒ user-edited; refuse.
      const w = eng.wt[raw];
      const known = w && w.ts != null && wtl.has(w.ts);
      if (!known) { dirtyRaw = raw; break; }
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
//  Two passes: preflight (refuse DELDIRTY on the first descendant whose
//  mtime ∉ stamp-set) then apply (unlink every descendant).  An already-
//  absent dir is an OK no-op (caller still appends the dir row).  Empty
//  dirs are not removed (POST won't emit them either).
function delDir(repo, eng, wtl, dirRaw) {
  const prefix = dirRaw;     // ends in "/" (or "" = reporoot)
  if (prefix !== "" && statKind(join(repo.wt, prefix.replace(/\/$/, ""))) !== "dir")
    return { dirty: false, unlinked: 0 };           // already absent

  //  Descendants on disk = wt-scan entries under the prefix (meta skipped
  //  by the scan/ignore already; double-guard with isMeta).
  const desc = [];
  for (const rel in eng.wt) {
    if (stage.isMeta(rel)) continue;
    if (prefix === "" ? true : rel.indexOf(prefix) === 0) desc.push(rel);
  }
  //  Preflight: any descendant with ∉ stamp-set mtime aborts.
  for (const rel of desc) {
    const w = eng.wt[rel];
    if (!(w && w.ts != null && wtl.has(w.ts))) return { dirty: true, dirtyPath: rel };
  }
  //  Apply: unlink every descendant.
  let n = 0;
  for (const rel of desc) { try { io.unlink(join(repo.wt, rel)); n++; } catch (e) {} }
  return { dirty: false, unlinked: n };
}

//  --- branch tombstone (DELBranch) --------------------------------------
//  `?br` → a REFS `delete ?br#0…0` tombstone via store.tombstone.  Refuses:
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

//  --- render the `delete:` banner ---------------------------------------
//  Named form: a `delete:` header (real ts) + one `delete <path>` row line
//  (blank ts) per emitted row + the summary line(s).  Bare form: no header,
//  just the row lines + the `swept N` summary.  Mirrors the active HUNK
//  `delete:` table native opens for every DELStage run (del_skip / count).
function emitBanner(banner) {
  const ts = ron.now();
  let body = "";
  if (!banner.bare)
    body += dateCol(ts) + " " + verbCol("delete") + " delete:\n";
  for (const it of banner.items) {
    if (it.type === "row")
      body += dateCol(0n) + " " + verbCol("delete") + " " + it.path + "\n";
    else if (it.type === "summary")
      body += it.text + "\n";
  }
  writeStdout(body);
}

function main() {
  const argv = process.argv.slice(2);
  const recursive = argv.indexOf("-r") >= 0 || argv.indexOf("--force") >= 0;
  const args = argv.filter(function (a) { return a[0] !== "-"; });

  const repo = be.find();
  const k = store.open(repo.storePath, repo.project);

  //  Split branch-forms (literal leading `?`) from path-forms.  Branch-forms
  //  each go through delBranch; path/bare-forms batch into ONE delStage.
  const pathRaws = [];
  for (const a of args) {
    if (a[0] === "?") {
      const u = new URI(a);
      delBranch(repo, u.query || "", recursive);
    } else {
      pathRaws.push(a);
    }
  }

  //  DELStage runs for the bare form (no args at all) or any path-form arg;
  //  a pure branch-form invocation prints nothing extra.
  if (pathRaws.length > 0 || args.length === 0) {
    const res = delStage(repo, k, pathRaws);
    if (res.rows.length > 0) {
      const uris = res.rows.map(function (r) { return { verb: "delete", uri: r.uri }; });
      ulog.append(repo.bePath, uris);
    }
    emitBanner(res.banner);             // always (the open `delete:` table)
    if (res.dirty) {
      io.log("be delete: " + res.dirtyPath + " has unstamped changes — "
             + "stage with `be put` or revert before deleting\n");
      throw DELDIRTY;                    // non-zero exit (native DELDIRTY)
    }
  }
}

main();
