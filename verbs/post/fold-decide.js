//  decide.js — classify the staged change-set into per-path keep/unlink/add
//  decisions (JS-051).  Pure JS over classify.js (wtScan / wtEqBase),
//  wtlog.js (boundaries + eachPutDelete) and store.js (baseline tree read);
//  no C, no dog.  The JS twin of sniff/POST.c::post_classify_step — the
//  decision ladder that the N-way merge (baseline ⊕ wt ⊕ put ⊕ delete) runs
//  per path, plus the "theirs" (absorbed-patch) 5th input which JS-051 does
//  NOT handle (a present patch row throws, out of scope — see post.js).
//
//  decide(be, wtlogReader, storeReader) → { decisions, baseTreeSha, haveBase,
//                                           hasPatch } where
//    decisions   = [{ verb:"keep"|"unlink"|"add", path, mode, sha, oldSha? }]
//                  LEX-SORTED by path (commit.js needs the lex order to slice
//                  contiguous subtrees).  `keep`/`add` carry a git `mode`
//                  (0o100644/755/120000/160000) + the entry `sha`; `add` also
//                  carries `oldSha` when the path had a baseline entry (the
//                  modify-vs-add distinction + the delta base).  `unlink`
//                  carries neither.
//    baseTreeSha = the baseline commit's tree sha (empty-commit compare)
//    haveBase    = a baseline commit row exists
//    hasPatch    = a `patch` row is in scope (POST-005 "theirs" — out of
//                  scope for JS-051; post.js throws)
//
//  Modes mirror post_kind_to_mode: f→100644, x→100755, l→120000, s→160000.
//  Selective vs implicit (commit-all): `anyPd` = any in-scope put/delete.
//  In selective mode an unnamed dirty tracked file is KEPT (not rewritten);
//  in implicit mode it is content-compared and rewritten only on a genuine
//  diff.  A `.be`/`.git` meta path is never carried into the tree.

"use strict";

//  JSQUE-016: decide.js -> verbs/post/fold-decide.js (post's OWN fold helper);
//  shared/ kernel via ../../ .
const pathlib = require("../../shared/util/path.js");
const shalib = require("../../shared/util/sha.js");
const classify = require("../../shared/classify.js");
const join = pathlib.join;
const isFullSha = shalib.isFullSha;
const frameSha = shalib.frameSha;

const MODE = { f: 0o100644, x: 0o100755, l: 0o120000, s: 0o160000 };

//  A `.be`/`.git` meta path — SNIFFSkipMeta: never carried into a commit
//  tree.  classify.wtScan already drops them on disk; this guards baseline
//  rows + put/delete rows naming them.
function skipMeta(rel) {
  if (rel === ".be" || rel === ".git") return true;
  if (rel.indexOf(".be/") === 0 || rel.indexOf(".git/") === 0) return true;
  return false;
}

//  Git-blob sha of the wt file at `rel` for the given kind (mirrors
//  post_hash_path / CLASS.c::CLASSWtEqBase).  Symlink → hash of the link
//  target; regular/exec → hash of the bytes.  undefined on read failure.
function hashWtPath(wtRoot, rel, kind) {
  const full = join(wtRoot, rel);
  let st;
  try { st = io.lstat(full); } catch (e) { return undefined; }
  let content;
  if (st.kind === "lnk") {
    let tgt;
    try { tgt = io.readlink(full); } catch (e) { return undefined; }
    content = utf8.Encode(tgt);
  } else if (st.kind === "reg") {
    if (st.size === 0) content = new Uint8Array(0);
    else {
      let fd;
      try { fd = io.open(full, "r"); } catch (e) { return undefined; }
      try {
        const b = io.buf(st.size + 16);
        io.readAll(fd, b, st.size);
        content = b.data();
      } catch (e) { try { io.close(fd); } catch (e2) {} return undefined; }
      try { io.close(fd); } catch (e) {}
    }
  } else return undefined;
  return frameSha("blob", content);
}

function decide(be, wtlogReader, storeReader) {
  const wtRoot = be.wt;
  const ignore = require(libDir() + "/../../shared/util/ignore.js").load(wtRoot);  // JSQUE-016

  //  1. baseline tree leaves: rel → { sha, mode } (git modes, not kinds).
  const base = {};
  let baseTreeSha, haveBase = false;
  const baseTip = wtlogReader.baselineTip();
  if (baseTip && baseTip.sha && isFullSha(baseTip.sha)) {
    const treeSha = storeReader.commitTree(baseTip.sha);
    if (treeSha) {
      baseTreeSha = treeSha;
      storeReader.readTreeRecursive(treeSha, function (leaf) {
        if (skipMeta(leaf.path)) return;
        base[leaf.path] = { sha: leaf.sha, mode: leaf.mode };
      });
      haveBase = true;
    }
  }

  //  2. wt scan: rel → { ts, kind } (kind f/x/l).
  const wt = classify.wtScan(wtRoot, ignore);

  //  3. staged put/del since the pd floor, plus a `patch`-row presence
  //  probe (POST-005 "theirs"; out of scope → post.js throws).  Move-form
  //  put rows carry a dest in the fragment; a 40-hex fragment is a gitlink
  //  pin (the sub bump), not a dest.
  const puts = {}, dels = {};
  let hasPatch = false;
  const bnd = wtlogReader.boundaries();
  const floor = bnd.pd;
  wtlogReader.eachPutDelete(floor, function (r) {
    const u = r.uri;
    let path = u.path || "";
    if (path === "" || path[path.length - 1] === "/") return;   // dir-prefix rows
    if (skipMeta(path)) return;
    if (r.verb === "put") puts[path] = { ts: r.ts, frag: u.fragment || "" };
    else if (r.verb === "delete") dels[path] = { ts: r.ts };
  });
  //  patch rows in scope (above the patch floor) → out-of-scope theirs tree.
  for (const r of wtlogReader.rows) {
    if (r.verb !== "patch") continue;
    if (bnd.patch != null && r.ts <= bnd.patch) continue;
    hasPatch = true;
  }

  const anyPd = Object.keys(puts).length > 0 || Object.keys(dels).length > 0;

  //  4. merge keys: union of base/wt/put/del, lex sorted.
  const keys = {};
  for (const k in base) keys[k] = 1;
  for (const k in wt) keys[k] = 1;
  for (const k in puts) keys[k] = 1;
  for (const k in dels) keys[k] = 1;
  const paths = Object.keys(keys).sort();

  //  Submodule (gitlink) baseline prefixes — descendants dropped.
  const subPrefixes = [];
  function underSub(p) { for (const s of subPrefixes) if (p.indexOf(s) === 0) return true; return false; }

  const decisions = [];
  function keep(path, mode, sha) { decisions.push({ verb: "keep", path, mode, sha }); }
  function unlink(path) { decisions.push({ verb: "unlink", path }); }
  function add(path, mode, sha, oldSha) {
    const d = { verb: "add", path, mode, sha };
    if (oldSha && isFullSha(oldSha)) d.oldSha = oldSha;
    decisions.push(d);
  }

  for (const path of paths) {
    if (underSub(path)) continue;
    const b = base[path], w = wt[path], p = puts[path], d = dels[path];

    //  --- Gitlink bump (`put <sub>#<40-hex>`): the put fragment is a pin,
    //  wins over any on-disk file (SUBS-019).  Subsumes bump + fresh add.
    if (p && isFullSha(p.frag)) {
      const baseIsSub = b && b.mode === MODE.s;
      if (baseIsSub && b.sha === p.frag) { keep(path, MODE.s, b.sha); continue; }
      add(path, MODE.s, p.frag, baseIsSub ? b.sha : undefined);
      continue;
    }

    //  Gitlink with no bump: carry through verbatim (no on-disk file).
    if (b && b.mode === MODE.s) {
      subPrefixes.push(path + "/");
      keep(path, MODE.s, b.sha);
      continue;
    }

    //  Explicit delete: drop; unlink iff tracked or on disk.
    if (d) { if (b || w) unlink(path); continue; }

    //  Explicit put (regular blob — gitlink bump handled above).
    if (p) {
      if (!w) { if (b) unlink(path); continue; }   // put of a missing file
      const mode = MODE[w.kind] || MODE.f;
      const sha = hashWtPath(wtRoot, path, w.kind);
      if (!sha) continue;
      add(path, mode, sha, b ? b.sha : undefined);
      continue;
    }

    //  No explicit rule.  Branch by (in baseline?) × (on disk?).
    if (!w) {
      //  Missing from wt.
      if (anyPd) { if (b) keep(path, b.mode, b.sha); continue; }   // selective: keep
      if (b) unlink(path);                                          // implicit: delete
      continue;
    }
    if (!b && anyPd) continue;          // untracked + selective → ignore

    //  On disk.  Tracked + dirty: selective keeps, implicit content-compares.
    if (b) {
      if (anyPd) { keep(path, b.mode, b.sha); continue; }
      if (classify.wtEqBase(wtRoot, path, b.sha)) { keep(path, b.mode, b.sha); continue; }
      const mode = MODE[w.kind] || MODE.f;
      const sha = hashWtPath(wtRoot, path, w.kind);
      if (!sha) continue;
      add(path, mode, sha, b.sha);
      continue;
    }
    //  Untracked + on disk.  Fresh repo (no baseline): auto-stage all dirty.
    if (!haveBase) {
      const mode = MODE[w.kind] || MODE.f;
      const sha = hashWtPath(wtRoot, path, w.kind);
      if (!sha) continue;
      add(path, mode, sha, undefined);
      continue;
    }
    //  Untracked + dirty + has-base → must be `be put`-staged; ignore here.
  }

  return { decisions, baseTreeSha, haveBase, hasPatch };
}

function libDir() {
  return (typeof __dirname !== "undefined" && __dirname) ? __dirname : ".";
}

module.exports = { decide: decide, MODE: MODE, skipMeta: skipMeta,
                   hashWtPath: hashWtPath };
