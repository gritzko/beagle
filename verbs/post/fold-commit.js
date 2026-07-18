//  commit.js — build the git tree + commit objects from decisions, write a
//  keeper pack-log (+ idx), all pure JS over libabc+libdog (JS-051).  The JS
//  twin of sniff/POST.c's tree build (post_build_tree :789), commit-body
//  assembly (:2817, git-strict header order) and pack feed (:2929) — but the
//  pack WRITE is the dog/git binding `git.pack.book`/`.header`/`.feed`/
//  `.finish` (js/cont.cpp), and the `.keeper.idx` is ingest.js::buildIndex.
//  NO keeper/graf/sniff binding.
//
//  buildTree(decisions) → { rootTreeSha, bodies } where `bodies` is the
//    DFS post-order list of git tree objects (children before parents) so a
//    keeper feed in commit→trees→blobs order has every sha already.  Trees
//    are built bottom-up from the LEX-SORTED decisions: a contiguous run of
//    rows sharing a dir prefix is one subtree.  `unlink` rows emit no entry.
//
//  buildCommit({ treeSha, parents, author, message }) → { sha, body } in the
//    strict git grammar: `tree`, then ALL `parent` lines, then `author`,
//    `committer`, then the blank line + message.  (JS-051 is FF-only and
//    single-parent + no foster/picked — those ride a later ticket.)
//
//  writePack(store, decisions, commitBody, rootTreeSha, treeBodies) writes a
//    NEW NNNNNNNNNN.keeper pack-log into the shard (next free sequence
//    number, like ingest.js::add) carrying the commit, every new tree, and
//    every `add` blob, then builds its `.keeper.idx`.  Returns the log name.
//    Feed order mirrors native: commit, then trees parent-first (root → leaf),
//    then blobs.

"use strict";

//  JSQUE-016: commit.js -> verbs/post/fold-commit.js (post's OWN fold helper);
//  shared/ kernel via ../../ .
const pathlib = require("../../shared/util/path.js");
//  BE-030: worktree fs paths go THROUGH resolve() — wtpath is the
//  resolve-backed, context-confined replacement for the old wtJoin.
const wtpath = require("../../core/discover.js").wtpath;
const shalib = require("../../shared/util/sha.js");
const ingest = require("../../shared/ingest.js");
const idxmaint = require("../../shared/idxmaint.js");   // JS-116
const join = pathlib.join;   // BE-011: wtJoin confines wt-opens
const frameSha = shalib.frameSha;

const MODE = { f: 0o100644, x: 0o100755, l: 0o120000, s: 0o160000, dir: 0o40000 };

//  Git tree object bytes for one level's entries.  Each entry is
//  `<octal-mode> <name>\0<20-byte raw sha>`, entries already in git order
//  (the caller feeds them in lex-of-full-path order, which is git's tree
//  order — a subdir name sorts as if suffixed by '/').  Returns the body
//  Uint8Array (or undefined when empty).
function treeBody(entries) {
  if (!entries.length) return undefined;
  const b = io.buf(entries.length * 64 + 64);
  for (const e of entries) {
    const hdr = utf8.Encode(e.mode.toString(8) + " " + e.name + "\0");
    b.feed(hdr);
    b.feed(hex.decode(e.sha));     // 20 raw bytes
  }
  return b.data().slice();
}

//  Recursively build the tree for the lex-sorted decisions whose path starts
//  with `prefix` (a contiguous index range [lo,hi)).  Pushes each non-empty
//  tree's { sha, body } into `bodies` in DFS post-order and returns the
//  subtree sha (or undefined when the subtree has no entries).
function buildSubtree(decisions, lo, hi, prefix, bodies) {
  const entries = [];
  let i = lo;
  while (i < hi) {
    const d = decisions[i];
    if (d.verb === "unlink") { i++; continue; }   // no tree entry
    const rest = d.path.slice(prefix.length);
    const slash = rest.indexOf("/");
    if (slash < 0) {
      entries.push({ mode: d.mode, name: rest, sha: d.sha });
      i++;
      continue;
    }
    //  Subdir: gather the contiguous run sharing `prefix + dir + "/"`.
    const dir = rest.slice(0, slash);
    const subprefix = prefix + dir + "/";
    let j = i;
    while (j < hi && decisions[j].path.indexOf(subprefix) === 0) j++;
    const subSha = buildSubtree(decisions, i, j, subprefix, bodies);
    if (subSha) entries.push({ mode: MODE.dir, name: dir, sha: subSha });
    i = j;
  }
  const body = treeBody(entries);
  if (!body) return undefined;
  const sha = frameSha("tree", body);
  bodies.push({ sha: sha, body: body });
  return sha;
}

//  buildTree(decisions) → { rootTreeSha, bodies }.  decisions must be lex
//  sorted by path.  An all-unlink (or empty) decision set yields no root
//  tree (rootTreeSha undefined) — the caller feeds the empty tree.
function buildTree(decisions) {
  const bodies = [];
  const rootTreeSha = buildSubtree(decisions, 0, decisions.length, "", bodies);
  return { rootTreeSha: rootTreeSha, bodies: bodies };
}

//  The empty git tree (no entries) — its sha + zero-length body.
const EMPTY_TREE_BODY = new Uint8Array(0);
const EMPTY_TREE_SHA = frameSha("tree", EMPTY_TREE_BODY);

//  buildCommit({ treeSha, parents:[40hex…], author, epochSec, message })
//  → { sha, body }.  Strict git header order: tree, parent(s), author,
//  committer, blank, message + trailing '\n'.  author/committer share the
//  identity + ` <epoch> +0000` line (mirrors POST.c — committer == author).
function buildCommit(opts) {
  const author = opts.author;
  const tsLine = " " + opts.epochSec + " +0000\n";
  let s = "tree " + opts.treeSha + "\n";
  for (const p of (opts.parents || [])) s += "parent " + p + "\n";
  s += "author " + author + tsLine;
  s += "committer " + author + tsLine;
  //  PATCH-015: extra provenance headers git does not understand ride AFTER
  //  committer (git preserves unknown headers, ignores them for DAG walks) —
  //  `foster` (a `patch!` rebase-flavour link, local-only) and `picked` (a
  //  `#hash` cherry, dedup-only).  Never `parent`, so history stays linear.
  for (const p of (opts.foster || [])) s += "foster " + p + "\n";
  for (const p of (opts.picked || [])) s += "picked " + p + "\n";
  s += "\n" + opts.message + "\n";
  const body = utf8.Encode(s);
  return { sha: frameSha("commit", body), body: body };
}

//  Next free `NNNNNNNNNN.keeper` sequence number in `shard` (ingest.add's
//  rule): one past the highest existing log, or 1 for an empty shard.
function nextLogName(shard) {
  let max = 0;
  try {
    for (const nm of io.readdir(shard)) {
      const m = /^(\d{10})\.keeper$/.exec(nm);
      if (m) { const v = parseInt(m[1], 10); if (v > max) max = v; }
    }
  } catch (e) {}
  return ingest.logName(max + 1);
}

const { readFileBytes } = require("../../shared/wtread.js");   // CODE-020
//  Read the wt blob bytes for an `add` decision (symlink → target, else
//  the file bytes).  undefined on failure.
function readAddBytes(wtRoot, d) {
  const full = wtpath(wtRoot, d.path);             // BE-011
  if (d.mode === MODE.l) {
    let tgt;
    try { tgt = io.readlink(full); } catch (e) { return undefined; }
    return utf8.Encode(tgt);
  }
  let st;
  try { st = io.lstat(full); } catch (e) { return undefined; }
  if (st.size === 0) return new Uint8Array(0);
  const b = readFileBytes(full, st.size);   // CODE-020: shared wt read
  return b === null ? undefined : b;
}

//  writePack(shard, wtRoot, commitBody, rootTreeSha, treeBodies, decisions)
//  → logName.  Writes a fresh keeper pack-log carrying the commit, all new
//  trees (parent-first), and every `add` blob; then builds its idx.  Pack
//  WRITE = git.pack.book/header/feed/finish; idx = ingest.buildIndex.
function writePack(shard, wtRoot, commitBody, rootTreeSha, treeBodies, decisions) {
  //  Upper-bound the pack size: every object's content + generous per-record
  //  framing slack (header + zlib never exceeds content + 256 in practice).
  let cap = commitBody.length + 256;
  for (const t of treeBodies) cap += t.body.length + 256;
  const addBytes = [];
  for (const d of decisions) {
    if (d.verb !== "add") continue;
    //  DIS-058 D7: a gitlink (`160000`) add records a sub-shard COMMIT sha, not
    //  a wt blob — there is no blob to feed (the object lives in the sub shard).
    //  Skip the blob read/feed for it; the tree entry already carries the sha.
    if (d.mode === MODE.s) continue;
    const bytes = readAddBytes(wtRoot, d);
    if (bytes == null) throw "commit: cannot read add path " + d.path;
    addBytes.push(bytes);
    cap += bytes.length + 256;
  }
  cap += 64;

  //  JS-117: append to the tail of the highest log under the size threshold,
  //  else open a fresh NNNNNNNNNN.keeper (empty shard / over cap).
  const tgt = ingest.appendTarget(shard);
  const path = join(shard, tgt.logName);
  if (!tgt.append) {
    const pk = git.pack.book(path, cap);
    pk.header();
    feedPack(pk, commitBody, rootTreeSha, treeBodies, addBytes);
    pk.finish();
    abc.close(pk);
    ingest.buildIndex(shard, tgt.logName, tgt.fileId);
  } else {
    //  JS-117: build the pack in RAM, append its records (past the 12-byte
    //  header); records+sync THEN idx run — a torn tail is unindexed = dead.
    const scr = git.pack.over(new Uint8Array(cap));
    scr.header();
    feedPack(scr, commitBody, rootTreeSha, treeBodies, addBytes);
    scr.finish();
    const recLen = Number(scr.buffer.watermark) - 12;
    const records = scr.subarray(12, 12 + recLen).slice();
    const firstOff = ingest.appendRecords(path, records);
    ingest.indexAppended(shard, tgt.fileId, firstOff, scr, recLen);
  }
  idxmaint.compactAfterAdd(shard);   // JS-116: restore the 1/8 run ladder
  return tgt.logName;
}

//  JS-117: feed the pack body — commit, trees parent-first (treeBodies is DFS
//  post-order, so reverse; empty tree when the set is all-unlink), then blobs.
function feedPack(pk, commitBody, rootTreeSha, treeBodies, addBytes) {
  pk.feed("commit", commitBody, -1, null);
  if (rootTreeSha) {
    for (let i = treeBodies.length - 1; i >= 0; i--)
      pk.feed("tree", treeBodies[i].body, -1, null);
  } else {
    pk.feed("tree", EMPTY_TREE_BODY, -1, null);
  }
  for (const bytes of addBytes) pk.feed("blob", bytes, -1, null);
}

module.exports = {
  buildTree: buildTree, buildCommit: buildCommit, writePack: writePack,
  treeBody: treeBody, EMPTY_TREE_SHA: EMPTY_TREE_SHA, MODE: MODE,
  nextLogName: nextLogName, readAddBytes: readAddBytes
};
