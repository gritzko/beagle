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
const recurse = require("../../core/recurse.js");
const ulog    = require("../../shared/ulog.js");
const render  = require("../../view/render.js");      // SUBS-044: sub-banner line
const wire    = require("../../shared/wire.js");      // GIT-014: wire push
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

//  --- GIT-014: wire PUT (the UNCONSTRAINED remote ref-write) --------------
//  Scan the raw positional args for a Host-slot push form and run it ONCE.
//  Forms (PUT.mkd § Design invariant 9 — any ref to any sha, force allowed):
//    //host[?br[#sha]]     force-write origin's counterpart (branch from the
//                          reflog, default cur's branch) to a sha (default cur.tip)
//    ssh://host?br[#sha]   set a remote ref to a sha (default cur.tip)
//    https://host?br[#sha] set a remote ref to a sha (default cur.tip)
//    ssh://host/path  (NO ?ref) → LOG-ONLY ([DIS-011]); record the URL, NO push.
//  Gated by ctx._putWireDone so the per-arg fan-out runs it once.
function applyWire(repo, k, ctx) {
  if (!ctx) return;
  if (!ctx._putWirePaths) ctx._putWirePaths = {};      // wire-form row paths
  if (ctx._putWireDone) return;
  for (const a of (ctx && ctx.args) || []) {
    if (!a || a[0] === "#" || a[0] === "-") continue;
    const u = new URI(a);
    if (!u.host && !u.authority) continue;            // not a wire target
    //  The seed strips scheme/host: this arg's seedRow path is u.path, or — when
    //  the URI has no path — the query token (`http://h?br` → row path "br").
    //  Remember every candidate so the handler skips staging it (a wire target,
    //  not a file).
    if (u.path) ctx._putWirePaths[u.path] = 1;
    else if (u.query) ctx._putWirePaths[u.query] = 1;
    ctx._putWireRan = true;
    pushWire(repo, k, ctx, a, u);
  }
  ctx._putWireDone = true;
}

//  Force-push (PUT, NO FF gate) a sha to a remote ref over the JS wire.  `arg`
//  is the raw Host-slot URI; `u` its parse.  No `?ref` ⇒ LOG-ONLY ([DIS-011]).
//  old = the remote's advertised value (advertRefs), neu = the target sha;
//  call wire.push DIRECTLY — no ancestor/FF check (that omission IS the force).
function pushWire(repo, k, ctx, arg, u) {
  const out = ctx && ctx.out;
  const hasQuery = (u.query || "") !== "";
  //  DIS-011: a bare `ssh://host/path` with NO ?ref is recorded, never pushed.
  if (!hasQuery && (u.path || "")) {
    if (out) {
      if (!ctx._putBannerOpen) { ctx._putBannerOpen = true; out.banner("put", "put:", ron.now()); }
      out.row(arg, "put", 0n);
    }
    return;
  }
  //  Branch: explicit ?br wins; else origin's counterpart from the reflog
  //  (eachRemote reverse-grep by authority), else cur's branch.
  let branch = u.query || "";
  if (!branch && !u.scheme) branch = reflogCounterpart(k, u) || "";
  const cur = wtlog.open(repo).curTip();
  const curSha = (cur && cur.sha && isFullSha(cur.sha)) ? cur.sha : "";
  //  Target sha: explicit #sha (resolve a hashlet via the seed) or cur.tip.
  let target = u.fragment || "";
  if (target) { const f = resolveHex(k, target); if (f) target = f; }
  else target = curSha;
  if (!target || !isFullSha(target)) throw "PUTNONE: no sha to push (commit first)";
  const wireRef = "refs/heads/" + ((branch && branch !== "main") ? branch : "main");
  //  old = the remote's advertised value (force write: no ancestor check).
  const adv = wire.advertRefs(arg, "receive-pack");
  const cr = adv.refs.find(r => r.name === wireRef);
  const old = cr ? cr.sha : "";
  //  Pack: objects the remote lacks (want=target, have=remote tips).  A reset
  //  to an already-present sha yields keeper's valid 0-object PACK (32 bytes).
  const serve = repo.storePath + "?/" + (repo.project || "");
  const haves = adv.refs.map(r => r.sha).filter(isFullSha);
  const pack = wire.buildPushPack(serve, target, haves);
  wire.push(arg, [{ ref: wireRef, neu: target, old: old }], pack);
  if (out) {
    if (!ctx._putBannerOpen) { ctx._putBannerOpen = true; out.banner("put", "put:", ron.now()); }
    //  Banner: the remote base (any user #sha stripped) + `?branch#hashlet`.
    const hash = arg.indexOf("#");
    const base = hash >= 0 ? arg.slice(0, hash) : arg;
    out.row(base + (u.query ? "" : "?" + (branch || "")) + "#" + target.slice(0, 8), "put", 0n);
  }
}

//  Resolve origin's counterpart branch for a `//host` push: the recentmost
//  remote-tracking tip whose authority matches `u` (the reflog reverse-grep,
//  [Store]) gives the be-side branch label; "" (trunk) if none.
function reflogCounterpart(k, u) {
  const want = (u.authority || u.host || "");
  let hit = "";
  k.eachRemote(function (rt) {
    if (hit) return;
    if ((rt.host || "") === want || (rt.key || "").indexOf(want) === 0)
      hit = stripQ(rt.query || "");
  });
  return hit;
}
function stripQ(q) { return (q && q[0] === "?") ? q.slice(1) : q; }

//  GIT-014: resolve a #sha hashlet to a full sha via the local tips/objects
//  (the seed's resolveHex twin) — a full sha passes iff present; else scan tips.
function resolveHex(k, hexish) {
  if (isFullSha(hexish)) return k.getObject(hexish) ? hexish : hexish;
  let hit;
  k.eachTip(function (t) { if (!hit && t.sha.indexOf(hexish) === 0) hit = t.sha; });
  return hit;
}

//  --- bare `be put` (no path args) — PUT-004 ------------------------------
//  PUT-004: stage the whole wt vs baseline from the classifier's buckets —
//  mod→put, mis↔unk→silent move, ok→restamp (no row), unk→skip; returns { ops }.
function bareStage(repo, wtl, k) {
  const eng = stage.prep(repo, wtl, k);
  const wtRoot = repo.wt;
  if (!eng.haveBase || !eng.baseTreeSha) return { ops: [] };

  //  PUT-004: dirty list from the classifier (base⊕wt⊕wtlog put/del → buckets);
  //  wantClean adds the `ok` rows for restamp; already-staged paths are absent.
  const cls = classify.classifyMerge(repo, wtl, k, { wantClean: true, skipMeta: true });
  const mod = {}, mis = {}, unk = {}, ok = {};
  for (const r of cls.rows) {
    if (r.bucket === "mod") mod[r.path] = 1;
    else if (r.bucket === "mis") mis[r.path] = r.oldSha;
    else if (r.bucket === "unk") unk[r.path] = 1;
    else if (r.bucket === "ok") ok[r.path] = 1;
  }

  //  PUT-004: clean fast-path — a tracked file whose mtime is a get/post stamp is
  //  taken clean (no row, no restamp), like stage.js::bareWalk.
  const gpStamp = {};
  for (const r of wtl.rows)
    if (r.verb === "get" || r.verb === "post") gpStamp[r.ron] = true;
  function isGpStamp(rel) {
    const w = eng.wt[rel];
    return !!(w && w.ts != null && gpStamp[ron.encode(w.ts)]);
  }

  const ops = [];

  //  --- auto-pair mis↔unk system-`mv` renames (put_detect_moves) ----------
  //  PUT-004: a `mis` (gone, baseline sha) + an `unk` (on disk, diskSha) of equal
  //  content → one SILENT `put <old>#<new>` move (1:1, else PUTAMBIG).
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

//  SUBS-044: bare `be put` descends each MOUNTED sub PRE-ORDER ([Submodules]:
//  stage the sub's interior first), running the sub's OWN bareStage over its
//  baseline⊕wt and writing those rows to the SUB's wtlog; rows emit under a
//  `put:<sub>` banner, prefixed `<sub>/…` — exactly as native relays them.  The
//  parent gitlink bump is POST's job (like stageInSub), so the parent records
//  nothing here.  Reuses core/recurse.walk (mount gate, `.gitmodules` order).
function bareStageSubs(repo, prefix, ctx) {
  const out = ctx && ctx.out;
  recurse.walk(repo, prefix, function (subRepo, subPrefix) {
    //  SUBS-044: this sub FIRST (banner + own rows + relay-frame blanks), THEN
    //  its grandchildren — native's pre-order, banner-on-entry, then descend.
    const subK = store.open(subRepo.storePath, subRepo.project);
    const r = bareStage(subRepo, wtlog.open(subRepo), subK);  // may throw PUTAMBIG
    commitOps(subRepo, r.ops, ctx && ctx.T0);
    if (out) {
      out.raw(render.dateCol(ron.now()) + " " + render.verbCol("put") +
              " put:" + subPrefix);                                // sub banner
      let staged = 0;
      for (const op of r.ops)
        if (op.path !== null && !op.silent) {
          out.row(subPrefix + "/" + (op.dst ? (op.path + "#" + op.dst) : op.path), "put", 0n);
          staged++;
        }
      //  SUBS-044: two-blank `put:` close ONLY when this sub staged a row
      //  (native skips them for an empty banner that exists only to descend).
      if (staged) { out.raw(""); out.raw(""); }
    }
    bareStageSubs(subRepo, subPrefix, ctx);          // then descend grandchildren
  });
}

//  JSQUE-010: `be put` as a loop HANDLER (converted from the `main();` one-shot).
//  Each call handles ONE seed row (one path arg, resolution-at-entry fan-out);
//  the `put:` banner header opens ONCE per run (ctx._putBannerOpen) and every
//  staged op pushes a blank-date row via ctx.out — the loop does ONE flush.  The
//  per-file STAGE is the LEAF; the bare-walk + move auto-pair (no path arg) is
//  the PUT-004 whole-tree fold (bareStage).  Ref-write forms are applied once
//  from ctx.refs.
//  SUBS-039/PUT ([Submodules] §3): the shallowest MOUNTED-sub prefix of `rel`
//  (a `<sub>/.be` file, never a symlink — recurse.isMount), or "".
function subMountPrefix(repo, rel) {
  const segs = rel.split("/");
  let pfx = "";
  for (let i = 0; i < segs.length - 1; i++) {
    pfx = pfx ? pfx + "/" + segs[i] : segs[i];
    if (recurse.isMount(repo.wt, pfx)) return pfx;
  }
  return "";
}

//  SUBS-039/PUT: stage a sub-crossing path INSIDE the sub (its own wtlog); the
//  parent records nothing (the gitlink bump is POST's job).  The row shows its
//  full top-relative path under the `put:` banner; tallies feed PUTNONE.
function stageInSub(repo, pfx, uri, ctx) {
  const out = ctx && ctx.out;
  const subRepo = be.find(join(repo.wt, pfx));
  const subK = store.open(subRepo.storePath, subRepo.project);
  const u = new URI(uri);
  let subUri = normRel(u.path).slice(pfx.length + 1);
  if (u.fragment) {
    let dst = normRel(u.fragment);
    if (dst.indexOf(pfx + "/") === 0) dst = dst.slice(pfx.length + 1);
    subUri = subUri + "#" + dst;
  }
  if (out && !ctx._putBannerOpen) { ctx._putBannerOpen = true; out.banner("put", "put:", ron.now()); }
  const eng = stage.prep(subRepo, wtlog.open(subRepo), subK);
  const r = stageArg(eng, subRepo, subUri);
  commitOps(subRepo, r.ops, ctx && ctx.T0);
  if (out)
    for (const it of r.items) {
      if (it.type === "skip") out.raw(skipText({ path: pfx + "/" + it.path, reason: it.reason, whole: it.whole }));
      else { const op = r.ops[it.opIdx]; out.row(pfx + "/" + (op.dst ? op.path + "#" + op.dst : op.path), "put", 0n); }
    }
  ctx._putStaged = (ctx._putStaged || 0) + r.ops.filter(function (o) { return o.path !== null; }).length;
  ctx._putSkipped = (ctx._putSkipped || 0) + r.items.filter(function (it) { return it.type === "skip"; }).length;
  ctx._putCalls = (ctx._putCalls || 0) + 1;
  if (ctx._putCalls >= (ctx.seededRowCount || 1) && ctx._putStaged === 0) {
    if (ctx._putSkipped > 0) io.log("be put: no eligible paths\n");
    throw "PUTNONE";
  }
}

module.exports = function handle(row, ctx) {
  const out = ctx && ctx.out;
  const repo = (ctx && ctx.repo) || be.find((row && row.uri) || undefined);
  const k = store.open(repo.storePath, repo.project);

  //  Ref-write forms first (no banner), once per run.
  applyRefs(repo, k, ctx);

  //  GIT-014: wire PUSH forms (`//host` / `ssh://host?br` / `https://host?br`)
  //  ride ctx.args (a host URI the queue uri can't carry whole); run them ONCE.
  //  A wire-form ROW (this arg's seedRow) must NOT be staged — return early.
  applyWire(repo, k, ctx);
  const ruRaw = (row && row.uri) || "";
  const ru = new URI(ruRaw);
  //  This row IS the wire target (the seed stripped its scheme/host to a bare
  //  path) — the push already ran in applyWire; never stage it as a file.
  if (ru.host || ru.authority || (ctx && ctx._putWirePaths &&
      ctx._putWirePaths[ru.path || ruRaw])) return;

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
    //  SUBS-044: then recurse mounted subs (pre-order), staging their interior.
    bareStageSubs(repo, "", ctx);
    return;
  }

  //  SUBS-039/PUT: a path arg crossing a mounted submodule stages INSIDE the sub
  //  ([Submodules] §3) — delegate, don't refuse "exists but is not stageable".
  const argPath = normRel(new URI((row && row.uri) || "").path || "");
  const subPfx = argPath ? subMountPrefix(repo, argPath) : "";
  if (subPfx) return stageInSub(repo, subPfx, (row && row.uri) || "", ctx);

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
