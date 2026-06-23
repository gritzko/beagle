//  put.js — `be put` as a pure-JS extension (JS-049).  Reproduces native
//  `be put` byte-equivalently: stage files for the next commit (one `put`
//  row per URI + an mtime restamp) and the ref-write forms (`?br` create,
//  `?br#sha` / `?#sha` / `?<40hex>` set).  Pure JS over JABC + bin/lib/*
//  (libabc+libdog ONLY; the staging engine is bin/lib/stage.js, the row
//  writer ulog.append, the ref writer store.set/createShard).
//
//  Slot dispatch (mirrors sniff/SNIFF.exe.c::is_put, sniff/PUT.c):
//    ?br            → create the branch at cur.tip (PUTDUP if it exists)
//    ?br#sha        → set ?br OUTRIGHT to sha (non-FF; the reflog escape)
//    ?#sha          → set trunk OUTRIGHT to sha
//    ?<40hex>       → set cur's branch OUTRIGHT to sha
//    (bare)         → bare put: auto-pair moves + tracked-dirty walk
//    <file>         → stage one file
//    <dir>/ | <dir> → dir-form: stage each tracked-dirty / untracked file
//    <old>#<new>    → move: rename on disk + a `put <old>#<new>` row
//
//  Each staged file: one `put` row via ulog.append + an io.setMtime restamp
//  to that row's ts (so a later `be put` / POST fast-paths it via the
//  stamp-set).  The `put:` banner + per-row lines render via render.js.
//
//  Usage:  be put [<path>... | <dir>/ | <old>#<new> | ?<branch>[#<sha>]]

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
const isFullSha = require(here + "/lib/sha.js").isFullSha;

const PUTNONE = "PUTNONE";
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

//  --- per-arg slot classification (SNIFF.exe.c is_put split) -------------
//  Returns one of:
//    { kind:"ref", op:"create"|"set", branch, sha? }
//    { kind:"path", uri }   (a path / dir / bareword / move, → PUTStage)
function classifyArg(u, repo, k) {
  const q = u.query || "";
  const path = u.path || "";
  const frag = u.fragment || "";
  const auth = u.authority || "";
  const data = u.href || "";
  const hasQ = q !== "", hasPath = path !== "", hasFrag = frag !== "",
        hasAuth = auth !== "";

  //  Trunk reset: `?#<sha>` — empty query, hex fragment, no path/auth,
  //  data starts with `?`.
  if (!hasQ && !hasPath && !hasAuth && hasFrag &&
      data[0] === "?" && isHexish(frag)) {
    const full = resolveHex(k, frag);
    if (!full) { io.log("be put: cannot resolve ?#" + frag + "\n"); throw SNIFFFAIL; }
    return { kind: "ref", op: "set", branch: "", sha: full };
  }
  //  `?<40hex>` (no fragment) — set cur's branch to this sha.
  if (hasQ && !hasPath && !hasAuth && !hasFrag && isFullSha(q)) {
    const full = resolveHex(k, q);
    if (!full) { io.log("be put: cannot resolve ?" + q + "\n"); throw SNIFFFAIL; }
    const cur = wtlog.open(repo).curTip();
    const target = (cur && cur.query) ? cur.query : "";
    return { kind: "ref", op: "set", branch: target, sha: full };
  }
  //  `?br` / `?br#<sha>`.
  if (hasQ && !hasPath && !hasAuth) {
    if (isHexish(frag)) {
      const full = resolveHex(k, frag);
      if (!full) { io.log("be put: cannot resolve ?" + q + "#" + frag + "\n"); throw SNIFFFAIL; }
      return { kind: "ref", op: "set", branch: q, sha: full };
    }
    return { kind: "ref", op: "create", branch: q };
  }
  //  Path / bare / dir / move → PUTStage.
  return { kind: "path", uri: u };
}

function isHexish(s) { return !!s && s.length >= 6 && s.length <= 40 && /^[0-9a-f]+$/.test(s); }

//  Resolve a 40-hex sha (passthrough) or a 6..39 hashlet prefix against the
//  object index (KEEPResolveHex twin).  undefined when unresolvable.
function resolveHex(k, hexish) {
  if (isFullSha(hexish)) {
    return k.getObject(hexish) ? hexish : undefined;
  }
  //  Short hashlet: scan the local tips + remotes for a sha with this
  //  prefix (the common `?br#<hashlet>` case).  A pure index prefix scan is
  //  a JS-034 follow-up; tip/remote scan covers the documented forms.
  let hit;
  k.eachTip(function (t) { if (!hit && t.sha.indexOf(hexish) === 0) hit = t.sha; });
  if (!hit) k.eachRemote(function (rt) { if (!hit && rt.sha.indexOf(hexish) === 0) hit = rt.sha; });
  return hit;
}

//  --- PUTStage (path forms): build the op list, then write rows ----------
//  Splits each path arg into bare / file / dir / move via the staging
//  engine, collects ops, writes one `put` row per op (ulog.append), and
//  restamps each op's file to its assigned ts.  Returns
//    { banner, none, diag }  where `banner` is the ordered stdout line list
//  (always emitted — native opens the `put:` table on every PUTStage run),
//  `none` is true on PUTNONE (no row staged), and `diag` is the stderr
//  diagnostic native prints alongside the PUTNONE error.
function putStage(repo, k, pathUris) {
  const eng = stage.prep(repo, wtlog.open(repo), k);
  const ops = [];          // staging ops (path/dst/restamp/stampTs), row order
  //  bannerItems: stdout lines in NATIVE order — pass-1 (move/dir) rows+skips
  //  in arg order, then pass-2 (file) rows+skips in arg order.  A `row` item
  //  links to its op via `opIdx` so the assigned ts can be filled post-write.
  const items = [];
  //  A row op gets a banner line UNLESS it is silent (bare-walk auto-paired
  //  move) or restamp-only (content-equal bare-walk file: path === null).
  function pushRow(op) { ops.push(op); if (!op.silent && op.path !== null) items.push({ type: "row", opIdx: ops.length - 1 }); }
  //  BE-008: a benign "is unchanged" no-op is not worth a banner line.
  function pushSkip(path, reason, whole) { if (reason === "is unchanged") return; items.push({ type: "skip", path: path, reason: reason, whole: !!whole }); }

  if (pathUris.length === 0) {
    //  Bare put.  The `put:` banner is still emitted (empty) on no-baseline.
    if (!eng.haveBase)
      return { banner: [], none: true,
               diag: "no baseline (fresh repo); name files explicitly" };
    for (const op of eng.bareWalk()) pushRow(op);
  } else {
    const files = [];      // raw rels deferred to the file-form pass
    for (const u of pathUris) {
      //  Move-form: non-empty path AND fragment.
      if (u.path && u.fragment) {
        pushRow(eng.explicitMove(normRel(u.path), normRel(u.fragment)));
        continue;
      }
      let raw = normRel(u.query || u.path || "");
      if (raw && stage.isMeta(raw)) { pushSkip(raw, "is a meta path"); continue; }
      //  Dir-form: empty (reporoot), trailing slash, or an on-disk dir.
      let isDir = raw === "" || raw[raw.length - 1] === "/";
      let reframed = false, origRaw = raw;
      if (!isDir) {
        let kind;
        try { kind = io.lstat(join(repo.wt, raw)).kind; } catch (e) {}
        if (kind === "dir") { raw = raw + "/"; isDir = true; reframed = true; }
      }
      if (isDir) {
        //  Confirm the dir exists (empty raw = reporoot, always exists).
        if (raw !== "") {
          let kind;
          try { kind = io.lstat(join(repo.wt, raw.replace(/\/$/, ""))).kind; } catch (e) {}
          if (kind !== "dir") { pushSkip(raw, "does not exist"); continue; }
        }
        const ex = eng.expandDir(raw);
        if (ex.ops.length === 0) {
          if (ex.sawTracked) pushSkip(raw, "is unchanged");
          else if (reframed) pushSkip(origRaw, "has no files to stage — skipped (did you mean `" + raw + "`?)", true);
          else pushSkip(raw, "has no files to stage");
          continue;
        }
        for (const op of ex.ops) pushRow(op);
        continue;
      }
      files.push(raw);
    }
    //  File-form pass (after every move/dir arg), in arg order.
    for (const raw of files) {
      const d = eng.classifyNamed(raw);
      if (!d.stage) { pushSkip(raw, d.reason); continue; }
      pushRow({ path: raw, kind: "put", restamp: raw });
    }
  }

  //  Build the banner line stream (rows + skips, native order) — emitted by
  //  the caller on stdout regardless of PUTNONE (native opens the table for
  //  every run).  Row `ts` is filled post-write below.
  function buildBanner() {
    const out = [];
    for (const it of items) {
      if (it.type === "skip") { out.push({ skip: it }); continue; }
      const op = ops[it.opIdx];
      out.push({ path: op.dst ? (op.path + "#" + op.dst) : op.path });
    }
    return out;
  }

  //  Nothing staged → PUTNONE.  Skips still ride the stdout banner; native
  //  adds a `no eligible paths` / `no changes` stderr diagnostic.
  const stageOps = ops.filter(function (o) { return o.path !== null; });
  if (stageOps.length === 0) {
    //  Apply any content-equal restamps (bare-walk) even on PUTNONE.
    for (const op of ops)
      if (op.path === null && op.stampTs != null)
        trySetMtime(join(repo.wt, op.restamp), op.stampTs);
    const sawSkip = items.some(function (it) { return it.type === "skip"; });
    return { banner: buildBanner(), none: true,
             diag: pathUris.length === 0 ? "no changes"
                 : (sawSkip ? "no eligible paths" : "no changes") };
  }

  //  Write the rows (row order = ops order) and get each one's assigned ts.
  const rows = [];
  for (const op of stageOps)
    rows.push({ verb: "put", uri: op.dst ? (op.path + "#" + op.dst) : op.path });
  const assigned = appendAndAssign(repo.bePath, rows);

  //  Restamp each staged file to its row ts; restamp-only ops (content-equal
  //  bare-walk files) stamp to baselineTs with no row.
  let ri = 0;
  for (const op of ops) {
    if (op.path === null) {
      if (op.stampTs != null) trySetMtime(join(repo.wt, op.restamp), op.stampTs);
      continue;
    }
    const ts = assigned[ri++];
    if (op.restamp) trySetMtime(join(repo.wt, op.restamp), ts);
  }

  return { banner: buildBanner(), none: false, diag: null };
}

//  Append `rows` to the wtlog and return the ASSIGNED ts (BigInt) per row,
//  in order.  Mirrors ulog.append's ts policy (nowAfter(tail) then +1 per
//  row) so the restamp uses the exact stamp the row got.
function appendAndAssign(bePath, rows) {
  const old = [];
  ulog.each(bePath, function (log) {
    old.push({ verb: log.verb, uri: log.uri, ts: log.time });
  });
  const tail = old.length ? old[old.length - 1].ts : 0n;
  let ts = ulog.nowAfter(tail);
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

//  --- render the `put:` banner -------------------------------------------
//  The `put:` HUNK table is opened ONLY by PUTStage (the path/bare forms);
//  the ref-write forms (`?br`, `?br#sha`) are a pure REFS op with NO stdout
//  banner (sniff/SNIFF.exe.c routes them around PUTStage).  So a banner is
//  emitted iff a path/bare stage ran.
function emitBanner(bannerItems) {
  const ts = ron.now();
  let body = dateCol(ts) + " " + verbCol("put") + " put:\n";
  //  Row lines carry a BLANK date (native HUNK rep `.ts=0`); skip lines are
  //  bare summary text (HUNKTableSummary), no date/verb column.  Interleaved
  //  in the native processing order.
  for (const it of bannerItems) {
    if (it.skip) body += skipLine(it.skip) + "\n";
    else body += dateCol(0n) + " " + verbCol("put") + " " + it.path + "\n";
  }
  writeStdout(body);
}

//  A skip summary line as native put_skip / the dir hint render it (stdout,
//  no date/verb column).  `whole` items already carry the full message.
function skipLine(s) {
  if (s.whole) return s.path + " " + s.reason;
  return s.path + " " + s.reason + " — skipped";
}

function main() {
  const argv = process.argv.slice(2);
  const args = argv.filter(function (a) { return a[0] !== "-"; });

  const repo = be.find();
  const k = store.open(repo.storePath, repo.project);

  const pathUris = [];
  let didRef = false;
  for (const a of args) {
    const u = new URI(a);
    const c = classifyArg(u, repo, k);
    if (c.kind === "ref") {
      didRef = true;
      if (c.op === "create") refCreate(repo, k, c.branch);
      else refSet(repo, k, c.branch, c.sha);
    } else {
      pathUris.push(c.uri);
    }
  }

  //  A banner is emitted iff a path/bare stage ran (PUTStage opens the
  //  `put:` table for every run, including PUTNONE); a pure ref-write
  //  invocation prints nothing.
  if (pathUris.length > 0 || (args.length === 0 && !didRef)) {
    const res = putStage(repo, k, pathUris);
    emitBanner(res.banner);             // always (the open `put:` table)
    if (res.none) {
      if (res.diag) io.log("be put: " + res.diag + "\n");
      throw PUTNONE;                     // non-zero exit (native 206/NONE)
    }
  }
}

main();
