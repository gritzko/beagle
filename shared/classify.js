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
const shalib = require("./util/sha.js");
const join = pathlib.join;
const isFullSha = shalib.isFullSha;

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
    const full = join(wtRoot, dirRel);
    if (statKind(join(full, ".git")) !== undefined) { nestedPrefixes.push(dirRel + "/"); continue; }
    const beKind = statKind(join(full, ".be"));
    if (beKind === "reg") nestedPrefixes.push(dirRel + "/");
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
    const full = join(wtRoot, rel);
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
  const full = join(wtRoot, rel);
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

//  --- the merge --------------------------------------------------------
//  STATUS classify: a clean file is count-only `ok`, a move's dst is suppressed
//  (its row rides the src `mov`).  The `ls:`/`lsr:` LISTING view does NOT use
//  this whole-tree pass — it calls classifyDir (below), O(dir) not O(repo).
function classify(be, wtlogReader, keeperReader, opts) {
  opts = opts || {};
  const wtRoot = be.wt;
  const ignore = require(libDir() + "/util/ignore.js").load(wtRoot);  // JSQUE-016

  //  1. baseline tree leaves: rel → { sha, kind }  (kind f/x/l/s).
  const base = {};
  const baseTip = wtlogReader.baselineTip();
  let haveBase = false;
  let baseTreeSha = undefined;
  if (baseTip && baseTip.sha && isFullSha(baseTip.sha)) {
    const treeSha = keeperReader.commitTree(baseTip.sha);
    if (treeSha) {
      baseTreeSha = treeSha;
      keeperReader.readTreeRecursive(treeSha, function (leaf) {
        base[leaf.path] = { sha: leaf.sha, kind: leaf.kind };
      });
      haveBase = true;
    }
  }

  //  2. wt scan: rel → { ts, kind }.
  const wt = wtScan(wtRoot, ignore);

  //  3. staged put/del since the last post: rel → row.  Move-form put
  //  rows carry a dest path in the fragment (skip sha-fragment "bumps").
  const puts = {}, dels = {};
  const bnd = wtlogReader.boundaries();
  const floor = bnd.pd;       // SNIFFAtLastPostTs floor
  wtlogReader.eachPutDelete(floor, function (r) {
    const u = r.uri;
    let path = u.path || "";
    if (path === "" || path[path.length - 1] === "/") return;   // dir-prefix rows
    if (r.verb === "put") {
      let frag = u.fragment || "";
      puts[path] = { ts: r.ts, dst: frag };
    } else if (r.verb === "delete") {
      dels[path] = { ts: r.ts };
    }
  });

  //  4. merge keys: union of base/wt/put/del paths, lex sorted.
  const keys = {};
  for (const k in base) keys[k] = 1;
  for (const k in wt) keys[k] = 1;
  for (const k in puts) keys[k] = 1;
  for (const k in dels) keys[k] = 1;
  const paths = Object.keys(keys).sort();

  //  Submodule prefixes (gitlink baseline rows) — drop descendants.
  const subPrefixes = [];
  function underSub(p) { for (const s of subPrefixes) if (p.indexOf(s) === 0) return true; return false; }
  //  Suppress the destination side of a move (its row is on the source).
  const movDsts = {};
  for (const k in puts) { const d = puts[k].dst; if (d && !isFullSha(d)) movDsts[d] = 1; }

  const counts = { ok: 0, put: 0, new: 0, mov: 0, mod: 0, del: 0, mis: 0, unk: 0 };
  const rows = [];
  //  Base-only gitlinks with no put/del intent: deferred to JS-033's
  //  SUBSDirty classifier (the 3-axis pin-vs-tip compare needs the sub
  //  shard, which classify deliberately doesn't open).  status.js reads
  //  this list, classifies each, and either counts `ok` or emits a `mod`
  //  row — so they are NOT folded into counts.ok here.
  const gitlinks = [];
  function push(bucket, path, ts, dst) {
    counts[bucket]++;
    if (bucket === "ok") return;       // ok is count-only, no row
    rows.push({ bucket: bucket, path: path, ts: ts || 0n, dst: dst });
  }

  for (const path of paths) {
    if (underSub(path)) continue;
    const b = base[path], w = wt[path], p = puts[path], d = dels[path];

    //  Gitlink (submodule) baseline row: record prefix, drop internals.
    //  No sub row here (JS-033); a clean gitlink with no intent → ok.
    if (b && b.kind === "s") {
      subPrefixes.push(path + "/");
      if (d) { push("del", path, d.ts); continue; }
      if (p) {
        //  staged bump: put (in base) — fragment is a sha, not a dest.
        push(b ? "put" : "new", path, p.ts); continue;
      }
      //  base-only / both gitlink with no intent: defer to the SUBSDirty
      //  classifier (JS-033) — record the path + R1 pin (the baseline
      //  gitlink sha) so status.js can run the 3-axis pin-vs-tip compare
      //  on the sub's own shard.  NOT counted ok here.
      gitlinks.push({ path: path, pin: b.sha });
      continue;
    }

    //  Staged groups take precedence (status_step).
    if (d) { push("del", path, d.ts); continue; }
    if (p) {
      const frag = p.dst || "";
      const isBump = isFullSha(frag);
      if (frag && !isBump) { push("mov", path, p.ts, frag); continue; }
      if (b) push("put", path, p.ts);
      else   push("new", path, p.ts);
      continue;
    }

    //  No staged intent — classify by presence + content.
    const inBase = !!b, onDisk = !!w;
    if (onDisk && !inBase) {
      //  wt-only.  status SUPPRESSES a move destination (its row rides the src
      //  `mov` row); a LISTING view (opts.listing) instead SHOWS it as a `new`
      //  row — the staged-add side of the rename (JAB-018).  The ls:/lsr: views
      //  no longer use this path (they call classifyDir below), but status may.
      if (movDsts[path]) { if (opts.listing) push("new", path, w.ts); continue; }
      push("unk", path, w.ts);
      continue;
    }
    if (inBase && !onDisk) {
      //  base-only: gone from disk → mis (gitlinks handled above).
      push("mis", path, 0n);
      continue;
    }
    if (inBase && onDisk) {
      //  both: content-confirmed clean vs modified.  wtEqBase handles
      //  every kind uniformly (CLASS.c CLASSWtEqBase) — a symlink hashes
      //  its readlink target, so a re-pointed link reads `mod` and an
      //  unchanged one `ok` (no more assume-clean hack).
      if (wtEqBase(wtRoot, path, b.sha)) {
        counts.ok++;   // clean → count-only
        //  opts.listing: a listing view emits the clean file as an `eq` row
        //  (with its wt mtime); status keeps `ok` count-only (JAB-018).
        if (opts.listing) rows.push({ bucket: "eq", path: path, ts: w.ts });
      } else push("mod", path, w.ts);
      continue;
    }
    //  (no base, no wt — only put/del, already handled above)
  }

  return { rows: rows, counts: counts, haveBase: haveBase,
           gitlinks: gitlinks, baseTreeSha: baseTreeSha };
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
//  → { files: [{ bucket, name, ts, dst? }], dirs: [name, ...] }  names are
//    RELATIVE to the scope (a mov `dst` too); buckets are the listing set
//    eq/mod/unk/new/mov/mis/del/put; `dirs` = immediate subdir + mount names.
function classifyDir(be, wtlogReader, keeperReader, scopePfx) {
  const wtRoot = be.wt;
  const ignore = require(libDir() + "/util/ignore.js").load(wtRoot);
  const scopeAbs = scopePfx ? join(wtRoot, scopePfx.slice(0, -1)) : wtRoot;
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
        const dst = frag.indexOf(scopePfx) === 0 ? frag.slice(scopePfx.length) : frag;
        files.push({ bucket: "mov", name: n, ts: p.ts, dst: dst });
      } else files.push({ bucket: b ? "put" : "new", name: n, ts: p.ts });
      continue;
    }
    if (w && !b) {
      //  baseline dir/mount now a wt file or symlink → `mod` (type change,
      //  matches native `mod be`); else a move dst is `new`, otherwise `unk`.
      const bucket = baseDir[n] ? "mod" : (movDsts[full] ? "new" : "unk");
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
                   wtScan: wtScan, wtEqBase: wtEqBase };
