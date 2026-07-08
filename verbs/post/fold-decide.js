//  decide.js — the `post` CONSUMER of the DIS-057 unified classifier.  Maps
//  the output ulog of shared/classify.js::classifyMerge (the SAME base ⊕ put ⊕
//  wt ⊕ theirs N-way merge `status` renders) onto per-path keep/unlink/add
//  decisions.  No second merge: post and status share one source of truth.
//  Pure JS over classify.js + wtlog.js + store.js; no C, no dog.
//
//  decide(be, wtlogReader, storeReader[, narrow]) → { decisions, baseTreeSha,
//                                                     haveBase, hasPatch } where
//    decisions   = [{ verb:"keep"|"unlink"|"add", path, mode, sha, oldSha? }]
//                  LEX-SORTED by path (commit.js needs the lex order to slice
//                  contiguous subtrees).  `keep`/`add` carry a git `mode`
//                  (0o100644/755/120000/160000) + the entry `sha`; `add` also
//                  carries `oldSha` when the path had a baseline entry (the
//                  modify-vs-add distinction + the delta base).  `unlink`
//                  carries neither.
//    baseTreeSha = the baseline commit's tree sha (empty-commit compare)
//    haveBase    = a baseline commit row exists
//    hasPatch    = false (DIS-057 subsumes the POST-005 patch-row throw — an
//                  in-scope patch's pat/mrg/cnf files commit their merged bytes)
//
//  Bucket → decision: ok→keep baseline; del→unlink iff tracked/on-disk;
//  mis→keep(selective)/unlink(implicit); put/new→add (or unlink a missing
//  put); mov→add the dst, rmv→unlink the src (the move pair); mod/pat/mrg/cnf→
//  keep(selective)/add-wt(implicit); unk→add only in a fresh (base-less) repo.
//  Modes mirror post_kind_to_mode: f→100644, x→100755, l→120000, s→160000.
//  Selective vs implicit (commit-all): `anyPd` = any in-scope put/delete.  A
//  `.be`/`.git` meta path is never carried into the tree (classifyMerge drops
//  them via skipMeta).

"use strict";

//  JSQUE-016: decide.js -> verbs/post/fold-decide.js (post's OWN fold helper);
//  shared/ kernel via ../../ .
const pathlib = require("../../shared/util/path.js");
const shalib = require("../../shared/util/sha.js");
const classify = require("../../shared/classify.js");
const join = pathlib.join, wtJoin = pathlib.wtJoin;   // BE-011: wtJoin confines wt-opens
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
const { readFileBytes } = require("../../shared/wtread.js");   // CODE-020
//  target; regular/exec → hash of the bytes.  undefined on read failure.
function hashWtPath(wtRoot, rel, kind) {
  const full = wtJoin(wtRoot, rel);                // BE-011
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
      content = readFileBytes(full, st.size);   // CODE-020: shared wt read
      if (content === null) return undefined;
    }
  } else return undefined;
  return frameSha("blob", content);
}

//  DIS-054 Path slot: `narrow` (a file path or `dir/` subtree prefix) restricts
//  the commit to that path.  A path OUTSIDE the narrow scope keeps its BASELINE
//  state in the new tree (its dirty/staged change is NOT committed); a path
//  INSIDE is classified normally.  underNarrow(p) → is p in scope.  Mirrors
//  POST.mkd Path slot 1 ("narrow the commit to that path"); submodules out of
//  scope (DIS-054).  An exact file match (p === narrow) or a subtree prefix
//  (p startswith narrow + "/") is in scope; narrow may itself name a dir.
function makeNarrow(narrow) {
  if (!narrow) return null;
  //  Canonicalise: shed a leading `./`, a trailing `/`.
  let n = narrow;
  if (n.indexOf("./") === 0) n = n.slice(2);
  while (n.length && n[n.length - 1] === "/") n = n.slice(0, -1);
  if (!n) return null;
  const pfx = n + "/";
  return function underNarrow(p) { return p === n || p.indexOf(pfx) === 0; };
}

function decide(be, wtlogReader, storeReader, narrow) {
  const wtRoot = be.wt;
  const underNarrow = makeNarrow(narrow);

  //  DIS-057: ONE classifier.  `post` is now a CONSUMER of the unified N-way
  //  merge's OUTPUT ulog (shared/classify.js classifyMerge) — the SAME base ⊕
  //  put ⊕ wt ⊕ theirs merge `status` renders.  We map each output row's bucket
  //  (+ resolved sha/mode) to a keep/unlink/add decision; no second merge.
  const m = classify.classifyMerge(be, wtlogReader, storeReader,
                                   { underNarrow: underNarrow, skipMeta: true,
                                     wantClean: true });
  const base = m.base, anyPd = m.anyPd, haveBase = m.haveBase;
  const baseTreeSha = m.baseTreeSha;

  const decisions = [];
  function keep(path, mode, sha) { decisions.push({ verb: "keep", path, mode, sha }); }
  function unlink(path) { decisions.push({ verb: "unlink", path }); }
  function add(path, mode, sha, oldSha) {
    const d = { verb: "add", path, mode, sha };
    if (oldSha && isFullSha(oldSha)) d.oldSha = oldSha;
    decisions.push(d);
  }
  //  Hash + add a wt file (mod/add of a regular/exec/symlink blob).
  function addWt(path, kind, oldSha) {
    const mode = MODE[kind] || MODE.f;
    const sha = hashWtPath(wtRoot, path, kind);
    if (!sha) return;
    add(path, mode, sha, oldSha);
  }

  //  Consume the output ulog.  Buckets carry the resolved content + the merge's
  //  presence flags (inBase/onDisk) and the move pair (mov dst + rmv source).
  for (const r of m.rows) {
    switch (r.bucket) {
      case "ok":           // clean tracked → keep baseline verbatim
        keep(r.path, r.mode, r.oldSha);
        break;
      case "del":          // staged delete → drop; unlink iff tracked/on disk
        if (r.inBase || r.onDisk) unlink(r.path);
        break;
      case "mis":          // gone from disk: selective keeps, implicit deletes
        if (anyPd) keep(r.path, r.mode, r.oldSha);
        else unlink(r.path);
        break;
      case "put": case "new":   // staged put/add
        if (r.gitlink) {         // gitlink bump (pin in the fragment)
          if (r.oldSha && r.oldSha === r.sha) keep(r.path, MODE.s, r.oldSha);
          else add(r.path, MODE.s, r.sha, r.oldSha);
          break;
        }
        if (!r.onDisk) { if (r.inBase) unlink(r.path); break; }  // put of a missing file
        addWt(r.path, r.kind, r.oldSha);
        break;
      case "mov":          // move dst: add the new path (its content rode here)
        addWt(r.path, kindAt(wtRoot, r.path), undefined);
        break;
      case "rmv":          // move source: drop the old path from the tree
        unlink(r.path);
        break;
      case "mod": case "pat": case "mrg": case "cnf":
        //  Tracked + content-modified (incl. a patch-derived merge): selective
        //  keeps baseline, implicit (commit-all) rewrites the wt content.
        if (anyPd) keep(r.path, r.mode || baseMode(base, r.path), r.oldSha);
        else addWt(r.path, r.kind, r.oldSha);
        break;
      case "unk":          // untracked on disk: fresh repo auto-stages; else ignore
        if (!haveBase && !anyPd) addWt(r.path, r.kind, undefined);
        break;
      default: break;
    }
  }

  //  Gitlinks with no intent: carry through verbatim (no on-disk file).
  for (const gl of m.gitlinks || []) keep(gl.path, MODE.s, gl.pin);

  //  DIS-054 Path slot: out-of-scope BASELINE paths keep their baseline state
  //  (the merge dropped their rows; an untracked out-of-scope path has no base
  //  entry, so it is naturally dropped).  Re-emit each as a `keep`.
  if (underNarrow) {
    for (const p in base) {
      if (underNarrow(p)) continue;
      const bb = base[p];
      keep(p, bb.mode, bb.sha);
    }
  }

  //  ENTRY-TYPE-CHANGE (GET-039): a baseline DIR becoming a wt FILE/LINK (or the
  //  reverse) leaves a stale `keep` on the OTHER node type at the SAME name — a
  //  selective commit keeps the gone subtree's children as `mis`, and the live
  //  wt leaf is `add`ed, so the tree carries BOTH a blob `X` and a tree `X` and
  //  a fresh get re-materialises the old type.  A path is exactly ONE git node:
  //  the LIVE wt node (the `add` / move-`add`) wins, so drop any `keep` that
  //  COLLIDES with it as the opposite node type — a kept LEAF whose name an
  //  added subtree now occupies (file->dir), or a kept subtree CHILD that lives
  //  under a now-added leaf (dir->file/link).  Generic (no `be` special-case).
  const live = {};                 // added leaf path -> 1 (the wt node)
  for (const d of decisions) if (d.verb === "add") live[d.path] = 1;
  const liveLeaves = Object.keys(live);
  function shadowed(path) {
    if (live[path]) return false;  // the live node itself never drops
    //  kept LEAF under a live subtree: some added leaf sits at `path + "/"`.
    const pfx = path + "/";
    for (const a of liveLeaves) if (a.indexOf(pfx) === 0) return true;
    //  kept subtree CHILD under a live leaf: an ancestor `A` of `path` is added.
    for (let i = path.indexOf("/"); i >= 0; i = path.indexOf("/", i + 1))
      if (live[path.slice(0, i)]) return true;
    return false;
  }
  const kept = decisions.filter(function (d) {
    return !(d.verb === "keep" && shadowed(d.path));
  });

  //  commit.js needs the decision list LEX-sorted by path (contiguous subtree
  //  slices); the merge emits in lex order but the move pair + gitlink/narrow
  //  re-emits interleave, so sort the final set.
  kept.sort(function (a, b) { return a.path < b.path ? -1 : a.path > b.path ? 1 : 0; });

  return { decisions: kept, baseTreeSha, haveBase, hasPatch: false };
}

//  The wt kind (f/x/l) at `rel` for a move-dst add (the dst is a wt-only file
//  the merge surfaced without a base entry).  Mirrors wtScan's kind probe.
function kindAt(wtRoot, rel) {
  let st;
  try { st = io.lstat(wtJoin(wtRoot, rel)); } catch (e) { return "f"; }   // BE-011
  if (st.kind === "lnk") return "l";
  if (st.kind === "reg") return (st.mode && (st.mode & 0o111)) ? "x" : "f";
  return "f";
}

//  Baseline mode for `rel` (a selective keep of a dirty tracked file), default
//  100644 when absent (defensive — a kept row is always baselined).
function baseMode(base, rel) { return (base[rel] && base[rel].mode) || MODE.f; }

function libDir() {
  return (typeof __dirname !== "undefined" && __dirname) ? __dirname : ".";
}

module.exports = { decide: decide, MODE: MODE, skipMeta: skipMeta,
                   hashWtPath: hashWtPath };
