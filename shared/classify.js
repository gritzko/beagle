//  classify.js — per-path baseline ⊕ wt ⊕ put ⊕ del merge → status bucket
//  (JS-031).  Pure JS over keeper.js (recursive baseline-tree read),
//  io.readdir/io.stat (wt scan), wtlog.eachPutDelete (staged intent),
//  ignore.js (.gitignore), and `sha1`/`hex` (content-confirmed clean).
//  No C, no dog.  Mirrors sniff/CLASS.c (SNIFFClassify heap-merge) +
//  sniff/SNIFF.exe.c (status_step bucket routing) + CLASS.c::CLASSWtState.
//
//  classify(be, wtlogReader, keeperReader[, opts]) → { rows, counts } where
//    rows   = [{ bucket, path, ts, dst? }]  in lex order, one per
//             distinct path that earns a row (the `ok` bucket is a count
//             only — clean tracked files would flood the output)
//    counts = { ok, put, new, mov, mod, del, mis, unk }
//
//  opts.listing (JAB-018) — the additive LISTING divergence from status,
//  for the `ls:`/`lsr:` views, default OFF so status byte-parity is intact:
//    1. EMIT `eq` rows (a clean tracked file gets a row with its wt mtime),
//       where status keeps `ok` count-only.
//    2. Do NOT suppress a staged move's DESTINATION — show it as a `new` row
//       (the staged-add side of the rename), splitting wt-only into `new`
//       (a move dst) vs `unk` (genuinely untracked).
//  Folded under the ONE opt (NOT a pure status superset).
//
//  Bucket semantics (status_step):
//    del   staged `delete` row                        (takes precedence)
//    put   staged `put` row, path in baseline         (staged mod)
//    new   staged `put` row, path NOT in baseline      (staged add)
//    mov   staged `put` row carrying a dest fragment   (rename src→dst)
//    mis   in baseline, gone from disk, no del row     (rm w/o be delete)
//    mod   in baseline + on disk, bytes != baseline    (unstaged mod)
//    ok    in baseline + on disk, bytes == baseline    (clean, count-only)
//    unk   on disk, not in baseline, no put row         (untracked)
//
//  CLEAN is CONTENT-confirmed (re-hash wt bytes vs the baseline blob
//  sha), never mtime alone — a restored-stamp mtime over edited bytes
//  still reads `mod` (DIS-023).  Submodule (gitlink) rows are recorded
//  as prefixes and their internals dropped; the mount itself is left to
//  JS-033 (no sub row emitted here) but a gitlink that is base-only with
//  no intent counts `ok` (the SUBS dirty axis comes later).

"use strict";

const pathlib = require("./util/path.js");   // JSQUE-016: util libs -> shared/util/
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const wtpath = require("../core/discover.js").wtpath;
const shalib = require("./util/sha.js");
const ulog = require("./ulog.js");           // DIS-057: ronStepMs (ms-correct band)
const conflict = require("./conflict.js");   // STATUS-005: hasConflictMarker (POST-017 scan)
const wtread = require("./wtread.js");        // STATUS-005: the ONE reg-file wt read
const join = pathlib.join;
const isFullSha = shalib.isFullSha;

//  STATUS-005: live marker-triple scan; a con row + this = the `con` bucket,
//  markers gone → plain mod (resolution is detected, never recorded).
function wtHasMarker(wtRoot, rel) {
  const full = wtpath(wtRoot, rel);
  let st;
  try { st = io.lstat(full); } catch (e) { return false; }
  if (st.kind !== "reg" || !st.size) return false;
  const bytes = wtread.readFileBytes(full, st.size);
  return bytes != null && conflict.hasConflictMarker(bytes);
}

//  --- wt scan ----------------------------------------------------------
//  Walk the worktree depth-first via io.readdir({recursive}), lstat each
//  file, and build a map relPath → { ts(mtime ron60), kind }.  Skips
//  `.gitignore`-matched paths + `.git`/`.be` meta + nested repos
//  (a subdir holding its own `.git`/`.be` file — a separate repo).
//  mtime comes straight off io.lstat (JS-042 surfaced it as a ron60
//  BigInt) — no `/usr/bin/stat` subprocess anymore (JS-044).
function wtScan(wtRoot, ignore) {
  const out = {};            // rel → { ts, kind: 'f'|'x'|'l' }
  //  io.readdir recursive returns the flat subtree, dirs marked with a
  //  trailing '/'.  We can't easily prune nested-repo subtrees with the
  //  flat form, so detect a nested-repo prefix and drop paths under it.
  //  hidden:true — native scans dotfiles too (`.gitignore` is tracked);
  //  only `.git`/`.be` are meta, filtered by the ignore matcher below.
  let names;
  try { names = io.readdir(wtRoot, { recursive: true, hidden: true }); }
  catch (e) { return out; }

  //  First pass: find nested-repo dir prefixes (a dir D with D/.git or a
  //  D/.be FILE).  We approximate by checking, per directory entry,
  //  whether it hosts a `.git` or `.be` marker.
  const nestedPrefixes = [];
  for (const nm of names) {
    if (nm[nm.length - 1] !== "/") continue;          // dirs only
    const dirRel = nm.slice(0, -1);
    if (ignore.match(dirRel, true)) continue;
    const full = wtpath(wtRoot, dirRel);
    if (statKind(join(full, ".git")) !== undefined) { nestedPrefixes.push(dirRel + "/"); continue; }
    const beKind = statKind(join(full, ".be"));
    //  SUBS-049: a PRIMARY nested wt (`.be` DIR holding wtlog, a green-field
    //  remote-get clone) is a repo boundary too — not only the `.be` FILE form.
    if (beKind === "reg" || (beKind === "dir" &&
        statKind(join(full, ".be/wtlog")) === "reg")) nestedPrefixes.push(dirRel + "/");
  }
  function underNested(rel) {
    for (const p of nestedPrefixes) if (rel === p.slice(0, -1) || rel.indexOf(p) === 0) return true;
    return false;
  }

  for (const nm of names) {
    if (nm[nm.length - 1] === "/") continue;          // skip dir entries
    const rel = nm;
    if (ignore.match(rel, false)) continue;
    if (underNested(rel)) continue;
    const full = wtpath(wtRoot, rel);
    //  io.lstat does NOT follow symlinks (FILELStat), so a dangling link
    //  stats fine — and it carries mtime (ron60 BigInt, JS-042) for the
    //  date column directly, no subprocess.
    let st;
    try { st = io.lstat(full); } catch (e) { continue; }
    let kind;
    if (st.kind === "lnk") kind = "l";
    else if (st.kind === "reg") kind = (st.mode && (st.mode & 0o111)) ? "x" : "f";
    else continue;                                     // dirs/other skip
    out[rel] = { ts: st.mtime || 0n, kind: kind, full: full };
  }
  return out;
}

function statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }

//  --- content-confirmed clean ------------------------------------------
//  Hash the wt bytes at `rel` as a git blob and compare to baseSha.
//  Mirrors CLASS.c::CLASSWtEqBase (symlink → hash of the link target).
function wtEqBase(wtRoot, rel, baseSha) {
  if (!isFullSha(baseSha)) return false;
  const full = wtpath(wtRoot, rel);
  let st;
  try { st = io.lstat(full); } catch (e) { return false; }
  let content;
  if (st.kind === "lnk") {
    //  A symlink's git blob is its TARGET path verbatim (CLASS.c
    //  CLASSWtEqBase: FILEReadLink → KEEPObjSha(BLOB, target)).  Read the
    //  link target (no follow) and hash it as a blob — a re-pointed link
    //  reads `mod`, an unchanged one reads `ok`.
    let tgt;
    try { tgt = io.readlink(full); } catch (e) { return false; }
    content = utf8.Encode(tgt);
  } else if (st.kind === "reg") {
    if (st.size === 0) content = new Uint8Array(0);
    else {
      //  Read via open/readAll/close — NOT io.mmap.  io.mmap leaks the
      //  mapping (no JS-side unmap), so a wt with >~1000 tracked files
      //  exhausts the process's mmap regions partway through the content
      //  sweep; every later map then fails and the file reads as a false
      //  `mod` (the 279-file real-repo regression).  A pooled fd read has
      //  no such ceiling — the fd is closed each call.
      let fd;
      try { fd = io.open(full, "r"); } catch (e) { return false; }
      try {
        const b = io.buf(st.size + 16);
        io.readAll(fd, b, st.size);
        content = b.data();
      } catch (e) { try { io.close(fd); } catch (e2) {} return false; }
      try { io.close(fd); } catch (e) {}
    }
  } else return false;
  return shalib.frameSha("blob", content) === baseSha;
}

//  --- patch-stamp axis (DIS-057) ---------------------------------------
//  Each in-scope `patch` row sits at the TOP of a reserved 3-stamp band: the
//  patch verb stamps every merged file's mtime to the row ceil-2ms (clean apply
//  → `pat`), ceil-1ms (`mrg`), or ceil (`cnf`), so the OUTCOME rides the stamp
//  offset — no per-file row, no merge recompute at read time.  The row ts is the
//  band CEILING (not the floor) so the wtlog monotonic tail already sits past
//  every stamp it produced (DIS-057 Task 2): a later nowAfter(tail) lands above
//  the whole band.  patchStamps(wtl) → a mtime(BigInt)→bucket map over every
//  in-scope patch row's {ceil-2ms:pat, ceil-1ms:mrg, ceil:cnf}.  Empty when no
//  patch row is in scope, so the whole axis is a no-op for the common case.
//  DIS-057 REOPEN 2026-06-29: step in MILLISECONDS (ulog.ronStepMs), the SAME
//  ms-correct step the patch verb uses — a raw t-2n corrupts the packed ms field
//  so the reconstructed band would not match the on-disk mtime → `pat` lost.
function patchStamps(wtlogReader) {
  const map = {};
  if (!wtlogReader || typeof wtlogReader.patchFloor !== "function") return map;
  const floor = wtlogReader.patchFloor();
  for (const r of wtlogReader.rows) {
    if (r.verb !== "patch") continue;
    if (floor != null && r.ts <= floor) continue;
    const t = r.ts;
    map[ulog.ronStepMs(t, -2).toString()] = "pat";
    map[ulog.ronStepMs(t, -1).toString()] = "mrg";
    map[t.toString()]                     = "cnf";
  }
  return map;
}

//  --- the unified N-way merge (DIS-057) --------------------------------
//  classifyMerge: ONE classifier built as an N-way merge of FOUR input ulogs —
//  the base tree (path→{sha,mode}), the put list (wtlog put/delete/move
//  intents), the wt scan (path→{mtime,kind}), and the patched-in (theirs)
//  stamps — whose OUTPUT is itself a ulog: one row per dirty path, lex-sorted,
//    { path, bucket, ts, sha?, mode?, oldSha?, dst?, kind?, eq? }
//  `bucket` is the [Dirty] status (rendered by `status`); `sha`/`mode` is the
//  resolved content the wt file would commit as (consumed by `post`).  Both
//  surfaces map from this single output — no second merge.
//
//  opts:
//    listing   — JAB-018 listing divergence (emit `eq` rows; SHOW a move dst
//                as `new` instead of suppressing it).  status default OFF.
//    underNarrow(p) — DIS-054 post Path-slot scope test (in-scope?).  Out-of-
//                scope paths are dropped from the output (post keeps baseline).
//    skipMeta  — drop `.be`/`.git` rows from base + put/del (post needs this;
//                status's wtScan already drops them on disk).
//  Returns { rows, counts, haveBase, baseTreeSha, gitlinks }.
function classifyMerge(be, wtlogReader, reader, opts) {
  opts = opts || {};
  const wtRoot = be.wt;
  //  SUBS-045: a real submodule is DECLARED in `.gitmodules`; an undeclared
  //  base-gitlink (the `be -> .` self-locator) must NOT be sub-classified.
  const declaredSubs = new Set(require(libDir() + "/gitmodules.js").paths(wtRoot));
  const ignore = require(libDir() + "/util/ignore.js").load(wtRoot);  // JSQUE-016
  const dropMeta = !!opts.skipMeta;
  const underNarrow = opts.underNarrow || null;

  //  1. base tree ulog: rel → { sha, kind, mode }.  DIS-057 RULING 2026-06-29:
  //  the base is the OURS tree — the latest get/post sha-tip, patch rows EXCLUDED
  //  (curTip, NOT baselineTip).  baselineTip folds a patch row's THEIRS sha into
  //  the baseline, so a clean take-theirs file equalled that (theirs) baseline
  //  and collapsed to `ok` — `pat` never appeared.  The patched-in (theirs)
  //  tree(s) are a SEPARATE 4th input (step 4 below), never the baseline.
  const base = {};
  const baseTip = wtlogReader.curTip();
  let haveBase = false, baseTreeSha = undefined;
  if (baseTip && baseTip.sha && isFullSha(baseTip.sha)) {
    const treeSha = reader.commitTree(baseTip.sha);
    if (treeSha) {
      baseTreeSha = treeSha;
      reader.readTreeRecursive(treeSha, function (leaf) {
        if (dropMeta && isMeta(leaf.path)) return;
        base[leaf.path] = { sha: leaf.sha, kind: leaf.kind, mode: leaf.mode };
      });
      haveBase = true;
    }
  }

  //  2. wt scan ulog: rel → { ts, kind }.
  const wt = wtScan(wtRoot, ignore);

  //  3. put list ulog: staged put/del since the pd floor.  A move-form put
  //  carries a dest path in the fragment; a 40-hex fragment is a gitlink pin.
  const puts = {}, dels = {};
  const floor = wtlogReader.boundaries().pd;
  wtlogReader.eachPutDelete(floor, function (r) {
    const u = r.uri;
    let path = u.path || "";
    if (path === "" || path[path.length - 1] === "/") return;   // dir-prefix rows
    if (dropMeta && isMeta(path)) return;
    if (r.verb === "put") puts[path] = { ts: r.ts, dst: u.fragment || "" };
    else if (r.verb === "delete") dels[path] = { ts: r.ts };
  });
  const anyPd = Object.keys(puts).length > 0 || Object.keys(dels).length > 0;

  //  4. patched-in (theirs) stamps: mtime → pat/mrg/cnf bucket — the cheap
  //  per-file OUTCOME tag the patch verb wrote.  And the theirs TREE ulog itself
  //  (rel → {sha,kind,mode}), the merge's SEPARATE 4th input: a patch-stamped,
  //  ours-modified file is `pat` when wt == theirs (a clean take-theirs), else
  //  `mrg`/`cnf` (a merge of ours+theirs).  Read each in-scope patch row's
  //  theirs tree (later rows win on a path collision — newest absorb).  Empty
  //  when no patch row is in scope, so the whole axis is a no-op otherwise.
  const pstamps = patchStamps(wtlogReader);
  //  STATUS-005: durable con rows; a named path with LIVE markers is `con` —
  //  verb-agnostic, mtime-proof, unlike the DIS-057 band a get never earns.
  const conSet = (wtlogReader && typeof wtlogReader.conPaths === "function")
        ? wtlogReader.conPaths() : new Set();
  const theirs = {};
  if (typeof wtlogReader.patchTheirs === "function") {
    for (const tsha of wtlogReader.patchTheirs()) {
      if (!isFullSha(tsha)) continue;
      const ttree = reader.commitTree(tsha);
      if (!ttree) continue;
      reader.readTreeRecursive(ttree, function (leaf) {
        if (dropMeta && isMeta(leaf.path)) return;
        theirs[leaf.path] = { sha: leaf.sha, kind: leaf.kind, mode: leaf.mode };
      });
    }
  }

  //  merge keys: union of all FOUR input ulogs (base ⊕ wt ⊕ put/del ⊕ theirs),
  //  lex sorted.  theirs contributes a key for a path theirs ADDED that ours
  //  lacks (a take-theirs add lands on disk wt-only; without theirs in the union
  //  it would still surface via wt, but listing it keeps the inputs symmetric).
  const keys = {};
  for (const k in base) keys[k] = 1;
  for (const k in wt) keys[k] = 1;
  for (const k in puts) keys[k] = 1;
  for (const k in dels) keys[k] = 1;
  for (const k in theirs) keys[k] = 1;
  const paths = Object.keys(keys).sort();

  //  Move dst index: a put-with-dest yields `rmv` on the source and `mov` on
  //  the destination (the move pair); the dst's own wt-only row is suppressed.
  const movDsts = {};
  for (const k in puts) {
    const dst = puts[k].dst;
    if (dst && !isFullSha(dst)) movDsts[dst] = 1;
  }

  //  Submodule baseline prefixes — descendants dropped.
  const subPrefixes = [];
  function underSub(p) { for (const s of subPrefixes) if (p.indexOf(s) === 0) return true; return false; }

  //  STATUS-005: ONE visible conflict name — the DIS-057 `cnf` band outcome
  //  is translated to `con` at the push sites (patchStamps keeps `cnf`).
  const counts = { ok: 0, put: 0, new: 0, mov: 0, rmv: 0, pat: 0, mrg: 0,
                   con: 0, mod: 0, del: 0, mis: 0, unk: 0 };
  const rows = [];
  const gitlinks = [];   // base-only gitlinks → JS-033 SUBSDirty (status only)
  //  `ok` is count-only for STATUS (a clean file would flood the output), but
  //  `post` (opts.wantClean) NEEDS each clean row to `keep` it in the tree.
  const emitClean = !!(opts.listing || opts.wantClean);
  function push(o) {
    counts[o.bucket]++;
    if (o.bucket === "ok" && !emitClean) return;   // status: ok is count-only
    rows.push(o);
  }

  for (const path of paths) {
    if (underSub(path)) continue;
    //  DIS-054 Path slot (post): an out-of-scope path keeps baseline → no row.
    if (underNarrow && !underNarrow(path)) continue;
    const b = base[path], w = wt[path], p = puts[path], d = dels[path];

    //  Gitlink bump (`put <sub>#<40hex>`): the put fragment is a PIN (a full
    //  sha), winning over any on-disk file (SUBS-019).  Subsumes bump + add.
    if (p && isFullSha(p.dst)) {
      const baseIsSub = b && (b.mode === 0o160000 || b.kind === "s");
      push({ bucket: baseIsSub ? "put" : "new", path: path, ts: p.ts,
             gitlink: true, mode: 0o160000, sha: p.dst,
             oldSha: baseIsSub ? b.sha : undefined });
      continue;
    }

    //  Gitlink (submodule) baseline row: record prefix, drop internals.
    //  SUBS-045: only when DECLARED in `.gitmodules`; an undeclared base-gitlink
    //  (the `be` self-locator) falls through, never a sub row / `adv`.
    if (b && (b.kind === "s" || b.mode === 0o160000) && declaredSubs.has(path)) {
      subPrefixes.push(path + "/");
      if (d) { push({ bucket: "del", path: path, ts: d.ts, inBase: true }); continue; }
      //  base-only/both gitlink, no intent → defer to JS-033 (status); post
      //  carries it through verbatim (it has the base sha/mode).
      gitlinks.push({ path: path, pin: b.sha, mode: b.mode || 0o160000 });
      continue;
    }

    //  Staged groups take precedence (status_step / post_classify_step).
    if (d) {
      push({ bucket: "del", path: path, ts: d.ts, inBase: !!b, onDisk: !!w });
      continue;
    }
    if (p) {
      const frag = p.dst || "";
      if (frag && !isFullSha(frag)) {
        //  Move PAIR (Dirty.mkd): `rmv` on the SOURCE (base-present, wt-absent)
        //  and `mov` on the DESTINATION (base-absent, wt-present).  status
        //  collapses the pair to one `mov src#dst` row (native parity); post
        //  unlinks the source and adds the dest.  Both rows carry `src` so the
        //  renderer can spell `src#dst`.
        push({ bucket: "rmv", path: path, ts: p.ts, src: path,
               oldSha: b ? b.sha : undefined, inBase: !!b });
        push({ bucket: "mov", path: frag, ts: p.ts, src: path, dst: frag,
               kind: wt[frag] ? wt[frag].kind : undefined, onDisk: !!wt[frag] });
        continue;
      }
      push({ bucket: b ? "put" : "new", path: path, ts: p.ts,
             oldSha: b ? b.sha : undefined, kind: w ? w.kind : undefined,
             onDisk: !!w, inBase: !!b });
      continue;
    }

    //  No staged intent — classify by presence + content (+ patch stamp).  The
    //  patch axis (DIS-057 RULING 2026-06-29) refines a patch-STAMPED file's
    //  outcome against the THEIRS input, NOT the (ours) baseline: the stamp band
    //  carries pat/mrg/cnf coarsely, and theirs corroborates `pat` = wt == theirs.
    const inBase = !!b, onDisk = !!w;
    const t = theirs[path];
    const pStamp = w ? pstamps[(w.ts || 0n).toString()] : undefined;
    //  STATUS-005: con row + live markers → `con`, winning over mod/pat/mrg;
    //  markers gone → fall through, resolution degrades to mod.
    if (onDisk && conSet.has(path) && wtHasMarker(wtRoot, path)) {
      push({ bucket: "con", path: path, ts: w.ts, kind: w.kind,
             oldSha: b ? b.sha : undefined, onDisk: true, inBase: inBase });
      continue;
    }
    //  DIS-057's stamp band spells a conflict `cnf`; STATUS-005 folds it into the
    //  single visible `con` name (patchStamps still returns `cnf` internally).
    const pStampCon = pStamp === "cnf" ? "con" : pStamp;
    if (onDisk && !inBase) {
      //  wt-only.  A move destination already has its `mov` row (emitted from
      //  the source's put), so SUPPRESS its standalone wt-only row here.
      if (movDsts[path]) continue;
      //  A patch-stamped take-theirs ADD (theirs added a path ours lacked):
      //  pat/mrg/cnf from the stamp band, carrying theirs' sha/mode as the
      //  resolved content (post commits it; ours-base has no oldSha here).
      if (pStamp && t) {
        push({ bucket: pStampCon, path: path, ts: w.ts, kind: w.kind,
               onDisk: true, inBase: false });
        continue;
      }
      push({ bucket: "unk", path: path, ts: w.ts, kind: w.kind, onDisk: true });
      continue;
    }
    if (inBase && !onDisk) {
      push({ bucket: "mis", path: path, ts: 0n, inBase: true,
             oldSha: b.sha, mode: b.mode });
      continue;
    }
    if (inBase && onDisk) {
      //  Patch axis FIRST: a content-modified (vs OURS), patch-STAMPED file reads
      //  its stamp-offset bucket (pat/mrg/cnf).  The "modified?" test is against
      //  OURS (b.sha) now — so a clean take-theirs (wt == theirs != ours) is
      //  modified-vs-ours and surfaces `pat`, no longer collapsing to `ok`.
      const pb = pStampCon;
      const eqBase = wtEqBase(wtRoot, path, b.sha);
      if (pb && !eqBase) {
        push({ bucket: pb, path: path, ts: w.ts, kind: w.kind,
               oldSha: b.sha, onDisk: true, inBase: true });
        continue;
      }
      if (eqBase) {
        push({ bucket: "ok", path: path, ts: w.ts, oldSha: b.sha,
               mode: b.mode, eq: true, clean: true });
      } else {
        push({ bucket: "mod", path: path, ts: w.ts, kind: w.kind,
               oldSha: b.sha, onDisk: true, inBase: true });
      }
      continue;
    }
  }

  return { rows: rows, counts: counts, haveBase: haveBase, anyPd: anyPd,
           gitlinks: gitlinks, baseTreeSha: baseTreeSha, base: base };
}

//  --- the merge --------------------------------------------------------
//  STATUS classify: a renderer-facing view over the unified merge.  A clean
//  file is count-only `ok`, a move's dst rides the source `mov` row.  The
//  `ls:`/`lsr:` LISTING view does NOT use this whole-tree pass — it calls
//  classifyDir (below), O(dir) not O(repo).
function classify(be, wtlogReader, keeperReader, opts) {
  opts = opts || {};
  const m = classifyMerge(be, wtlogReader, keeperReader,
                          { listing: opts.listing });
  //  Map the output ulog rows onto status's render rows ({bucket,path,ts}).
  //  DIS-057: a staged rename surfaces as the Dirty.mkd move PAIR — `rmv` on
  //  the SOURCE (present-in-base, absent-in-wt) and `mov` on the DESTINATION
  //  (absent-in-base, present-in-wt), TWO plain `<bucket> <path>` rows.  The
  //  earlier native-parity collapse to one `mov src#dst` row is gone (the tests
  //  are untied from C and the collapse contradicts Dirty.mkd).  A clean
  //  `ok`/`eq` row is count-only in status (no row) but a listed `eq` row in a
  //  listing view.
  const rows = [];
  for (const r of m.rows) {
    if (r.bucket === "ok") {
      if (opts.listing) rows.push({ bucket: "eq", path: r.path, ts: r.ts });
      continue;
    }
    rows.push({ bucket: r.bucket, path: r.path, ts: r.ts || 0n });
  }
  return { rows: rows, counts: m.counts, haveBase: m.haveBase,
           gitlinks: m.gitlinks, baseTreeSha: m.baseTreeSha };
}

//  Is `rel` a `.be`/`.git` meta path (SNIFFSkipMeta)?  Never carried into a
//  commit tree; status's wtScan already drops them on disk.
function isMeta(rel) {
  if (rel === ".be" || rel === ".git") return true;
  return rel.indexOf(".be/") === 0 || rel.indexOf(".git/") === 0;
}

//  --- scoped one-level listing (JAB-018 ls:/lsr:) ----------------------
//  classifyDir(be, wtlogReader, keeperReader, scopePfx) → the IMMEDIATE
//  entries of ONE directory, O(dir) not O(repo).  It descends the baseline
//  tree to the scope node and reads ONE level, readdirs the scope dir
//  NON-recursively, and content-hashes ONLY the scope's immediate files —
//  it NEVER walks underneath.  A subdir / mount is a NAME only (native
//  `be ls:` dates no dir row), so nothing below it is read or hashed; THAT
//  is what makes `ls:<dir>` scale with the dir, not the whole repo.  Replaces
//  the old whole-tree classify({listing}) + post-filter (which paid O(repo) —
//  every file hashed — to list one directory, just to date dir rows with a
//  newest-mtime-under-dir value native never asked for).
//  `scopePfx` is the dir RELATIVE to be.wt in DIR form ("" root, "sub/").
//
//  → { files: [{ bucket, name, ts }], dirs: [name, ...] }  names are RELATIVE to
//    the scope; buckets are the listing set eq/mod/unk/new/mov/rmv/mis/del/put.
//    DIS-057 RULING 2026-06-29: a staged RENAME is the SAME `rmv`(src)+`mov`(dst)
//    move PAIR `status` renders (untied from native's `mov src -> dst` + `new
//    dst`) — two plain rows, no `-> dst` arrow.  `dirs` = immediate subdir +
//    mount names.
function classifyDir(be, wtlogReader, keeperReader, scopePfx) {
  const wtRoot = be.wt;
  const ignore = require(libDir() + "/util/ignore.js").load(wtRoot);
  //  BE-028: defensive floor — resolveInTree THROWS NAVESCAPE on any `..` climb
  //  above the wt root, so a lexical scopePfx can never readdir outside the wt.
  const scopeRel = pathlib.resolveInTree("", scopePfx || "");
  const scopeAbs = scopeRel ? wtpath(wtRoot, scopeRel) : wtRoot;
  const dirSet = {};            // immediate REAL-wt-dir name → 1 (recursable)
  const baseDir = {};           // names the BASELINE records as a dir/mount

  //  1. baseline IMMEDIATE children: descend the tree to the scope node by one
  //  readTree per path segment, then read ONE level.  A missing / non-dir
  //  segment leaves the scope baseline-less (e.g. an untracked or absent dir).
  const baseFile = {};          // name → { sha, kind }
  const baseTip = wtlogReader.baselineTip();
  if (baseTip && baseTip.sha && isFullSha(baseTip.sha)) {
    let treeSha = keeperReader.commitTree(baseTip.sha);
    if (treeSha && scopePfx) {
      for (const seg of scopePfx.slice(0, -1).split("/")) {
        const ents = treeSha ? keeperReader.readTree(treeSha) : undefined;
        treeSha = undefined;
        if (ents) for (const e of ents)
          if (e.name === seg && e.mode === 0o40000) { treeSha = e.sha; break; }
        if (!treeSha) break;
      }
    }
    const ents = treeSha ? keeperReader.readTree(treeSha) : undefined;
    if (ents) for (const e of ents) {
      //  A baseline dir/mount does NOT make a `dir` row — only a REAL wt dir does
      //  (step 2).  When the baseline records a dir but the wt has a symlink/file
      //  there (a `be -> .` self-symlink committed as a recursive tree), it is a
      //  FILE row (`mod`), NEVER recursed — else lsr loops be/be/be/… (JAB-018).
      if (e.mode === 0o40000 || e.mode === 0o160000) baseDir[e.name] = 1;
      else baseFile[e.name] = { sha: e.sha,
        kind: e.mode === 0o120000 ? "l" : e.mode === 0o100755 ? "x" : "f" };
    }
  }

  //  2. wt IMMEDIATE children (NON-recursive readdir).  lstat (NO-follow) is the
  //  ONLY dir test — NEVER readdir's trailing-slash mark, which FOLLOWS symlinks:
  //  a self-symlink (`be -> .`) would read back as a dir and lsr would recurse
  //  be/be/be/… forever.  A symlink is kind "lnk" → a FILE row (its link target,
  //  like native ls:), never a dir, so a filesystem cycle can never drive the
  //  recursion (JAB-018).  Skip ignored + meta (.git/.be) entries.
  const wtFile = {};            // name → { ts, kind }
  let names;
  try { names = io.readdir(scopeAbs, { hidden: true }); } catch (e) { names = []; }
  for (let nm of names) {
    if (nm[nm.length - 1] === "/") nm = nm.slice(0, -1);   // drop readdir's mark
    let st;
    try { st = io.lstat(join(scopeAbs, nm)); } catch (e) { continue; }
    const isDir = st.kind === "dir";
    if (ignore.match(scopePfx + nm, isDir)) continue;
    if (isDir)                  dirSet[nm] = 1;
    else if (st.kind === "lnk") wtFile[nm] = { ts: st.mtime || 0n, kind: "l" };
    else if (st.kind === "reg") wtFile[nm] = { ts: st.mtime || 0n, kind: (st.mode && (st.mode & 0o111)) ? "x" : "f" };
  }

  //  3. staged put/del since the last post.  movDsts collects EVERY move's dst
  //  (a dst may be immediate even when its src is not), so an immediate wt-only
  //  file that is a move dst lists as `new`, not `unk`.  put/del FILE rows are
  //  kept only for paths IMMEDIATELY under the scope.
  function imm(p) {
    if (scopePfx && p.indexOf(scopePfx) !== 0) return false;
    const rel = p.slice(scopePfx.length);
    return rel.length > 0 && rel.indexOf("/") < 0;
  }
  const puts = {}, dels = {}, movDsts = {};
  wtlogReader.eachPutDelete(wtlogReader.boundaries().pd, function (r) {
    const path = r.uri.path || "";
    if (path === "" || path[path.length - 1] === "/") return;
    if (r.verb === "put") {
      const frag = r.uri.fragment || "";
      if (frag && !isFullSha(frag)) movDsts[frag] = 1;
      if (imm(path)) puts[path.slice(scopePfx.length)] = { ts: r.ts, dst: frag };
    } else if (r.verb === "delete") {
      if (imm(path)) dels[path.slice(scopePfx.length)] = { ts: r.ts };
    }
  });

  //  4. merge per immediate FILE name (mirrors classify's status_step, plus the
  //  listing divergences: a clean file is an `eq` row, a move dst is `new`).
  const files = [];
  const nameSet = {};
  for (const n in baseFile) nameSet[n] = 1;
  for (const n in wtFile)   nameSet[n] = 1;
  for (const n in puts)     nameSet[n] = 1;
  for (const n in dels)     nameSet[n] = 1;
  for (const n in nameSet) {
    if (dirSet[n]) continue;                       // a dir/mount, not a file row
    const b = baseFile[n], w = wtFile[n], p = puts[n], d = dels[n];
    const full = scopePfx + n;
    if (d) { files.push({ bucket: "del", name: n, ts: d.ts }); continue; }
    if (p) {
      const frag = p.dst || "";
      if (frag && !isFullSha(frag)) {
        //  DIS-057 RULING 2026-06-29: untie ls:/lsr: from the native `mov src ->
        //  dst` + `new dst` form — render the SAME `rmv`(src)+`mov`(dst) move
        //  PAIR `status` does.  The SOURCE (this immediate put row) is `rmv`; the
        //  DESTINATION's own wt-only row becomes `mov` (suppressing its `new`)
        //  below.  Two plain `<bucket> <name>` rows, no `-> dst` arrow.
        files.push({ bucket: "rmv", name: n, ts: p.ts });
      } else files.push({ bucket: b ? "put" : "new", name: n, ts: p.ts });
      continue;
    }
    if (w && !b) {
      //  baseline dir/mount now a wt file or symlink → `mod` (type change,
      //  matches native `mod be`); else a move DESTINATION is the `mov` half of
      //  the pair (DIS-057 RULING 2026-06-29 — untied from native's `new`),
      //  otherwise a genuinely untracked file is `unk`.
      const bucket = baseDir[n] ? "mod" : (movDsts[full] ? "mov" : "unk");
      files.push({ bucket: bucket, name: n, ts: w.ts }); continue;
    }
    if (b && !w) { files.push({ bucket: "mis", name: n, ts: 0n }); continue; }
    if (b && w)  { files.push({ bucket: wtEqBase(wtRoot, full, b.sha) ? "eq" : "mod", name: n, ts: w.ts }); continue; }
  }

  return { files: files, dirs: Object.keys(dirSet) };
}

//  Resolve this module's own dir so it can require ignore.js by absolute
//  path regardless of the top-level script's cwd-bound `require`.  The
//  JABC require loader injects __dirname (require.cpp).
function libDir() {
  return (typeof __dirname !== "undefined" && __dirname) ? __dirname : ".";
}

module.exports = { classify: classify, classifyDir: classifyDir,
                   classifyMerge: classifyMerge, isMeta: isMeta,
                   wtScan: wtScan, wtEqBase: wtEqBase,
                   patchStamps: patchStamps };
