//  dag.js — commit-graph ancestry over keeper commits (JS-032).  Pure JS
//  over keeper.js (`commitParents` / `parseCommit`); no C, no dog, no
//  graf linked.  Mirrors sniff/GET.c::GETStatusCommitDiff + dog DAG.c
//  (DAGAncestors / DAGTopoSort) — the cur-vs-tip commit divergence that
//  `be status` prepends (GET-021).
//
//  aheadBehind(keeper, curSha, tipSha) → { ahead:[…], behind:[…] } where
//    ahead  = commits reachable from cur but NOT from tip (local, unposted
//             → rendered `post`), newest-first.
//    behind = commits reachable from tip but NOT from cur (in the tip, not
//             materialized here → rendered `miss`), newest-first.
//  Each list element is { sha, hashlet, ts, subject }:
//    sha      40-hex commit id
//    hashlet  first 8 hex (the `?<hashlet>` column key; SHA1_HASHLEN_LEN)
//    ts       ron60 of the commit's AUTHOR time (the date column)
//    subject  first line of the commit message (the `#<subject>` tail)
//
//  Ancestry is a bounded parent-walk: a visited Set in JS caps the work,
//  FIRST-PARENT + MERGE parents are followed, `foster`/rebase parents are
//  EXCLUDED from the walk (keeper.commitParents already returns only the
//  real `parent` edges, foster lives in a separate slot — matches
//  git/graf per dog/git/GIT.h).  The walk is capped at WALK_CAP commits
//  per side so a pathological history can't run unbounded (the C uses
//  GET_CRANGE_ANC_CAP; we mirror with a generous JS ceiling).

"use strict";

const shalib = require("./util/sha.js");                // JSQUE-016: -> shared/util/
const isFullSha = shalib.isFullSha;
const hashlet60FromBytes = shalib.hashlet60FromBytes;
const hexDecode = hex.decode;                           // JABC hex.decode (codec)

const WALK_CAP = 1 << 16;   // ~65k commits/side — matches the C anc cap order

//  GIT-016: git commit pack type (WHIFFKeyPack low nibble), matches store.js.
const T_COMMIT = 1;

//  GIT-016: WHIFFKeyPack(type, hashlet60) — type in the low 4 bits (store.js
//  keyFor twin); the remote wh128 index keys commit edges by the commit hashlet.
function keyFor(type, hashlet60) { return (hashlet60 << 4n) | (BigInt(type) & 0xfn); }
function hashletOf(sha) { return hashlet60FromBytes(hexDecode(sha)); }

//  GIT-016: node identity for a walk — the full sha when there is NO remote
//  index (unchanged 2-arg behaviour), else the 60-bit hashlet so a local sha
//  and a remote hashlet-keyed edge share one identity space (graf DAGAncestors).
function idOf(remoteIx, sha) { return remoteIx ? hashletOf(sha) : sha; }

//  GIT-016: parents of a node (id=identity, sha=full sha if known) as
//  [{id, sha}] — keeper.commitParents FIRST (full shas, when sha is known),
//  then the remote wh128 index by hashlet (parent hashlets, sha unknown).  A
//  parent that is itself local re-enters keeper because it carries its sha.
function parentsVia(keeper, remoteIx, node) {
  let parents;
  if (node.sha) { try { parents = keeper.commitParents(node.sha); } catch (e) { parents = undefined; } }
  if (parents && parents.length) {
    const out = [];
    for (const p of parents)
      if (isFullSha(p)) out.push({ id: remoteIx ? hashletOf(p) : p, sha: p });
    return out;
  }
  if (!remoteIx || node.id == null) return [];
  //  Remote fallback: range the commit's hashlet key span; each hit's val is a
  //  parent hashlet (sha unknown here, so keeper is not re-consulted for it).
  const lo = keyFor(T_COMMIT, node.id), out = [];
  remoteIx.range(lo, lo + 1n, function (kv) { out.push({ id: kv[1], sha: undefined }); });
  return out;
}

//  Collect the ancestor SET of `root` (INCLUDING root itself) by a
//  bounded BFS over keeper.commitParents.  Returns a Set of 40-hex shas.
//  Unreadable / missing commits terminate that branch quietly (a shallow
//  shard may lack deep ancestors — the C walk is equally tolerant).
//  GIT-016: an optional remote wh128 index (commit->parent hashlet edges) is
//  consulted after keeper for a commit's parents; identities are then hashlets.
//  Returns a Map id->node ({id, sha?}); node.sha is set whenever it is known.
function ancestorNodes(keeper, root, remoteIx) {
  const seen = new Map();
  if (!isFullSha(root)) return seen;
  const rootId = idOf(remoteIx, root);
  const rootNode = { id: rootId, sha: root };
  const queue = [rootNode];
  seen.set(rootId, rootNode);
  let head = 0;
  while (head < queue.length) {
    if (seen.size > WALK_CAP) break;
    const node = queue[head++];
    for (const p of parentsVia(keeper, remoteIx, node)) {
      if (p.id == null) continue;
      const have = seen.get(p.id);
      if (have) { if (!have.sha && p.sha) have.sha = p.sha; continue; }
      seen.set(p.id, p);
      queue.push(p);
    }
  }
  return seen;
}

//  ancestors(keeper, root, remoteIx?) → Set of node identities (see header).
function ancestors(keeper, root, remoteIx) {
  return new Set(ancestorNodes(keeper, root, remoteIx).keys());
}

//  topoSort(keeper, set) → an Array of the shas in `set`, PARENTS-BEFORE-
//  CHILDREN (a topological order over the parent edges, oldest-first).  The
//  JS twin of dog DAG.c::DAGTopoSort: an iterative post-order DFS over
//  commitParents restricted to commits inside `set` (the DAGAncestors
//  closure), with a bounded visited Set so a pathological / cyclic history
//  can't run unbounded.  A commit appears AFTER all of its in-set parents, so
//  a caller emitting newest-first reverses the result.  Additive — existing
//  dag.js APIs are untouched (JAB-013).
function topoSort(keeper, set) {
  const out = [];
  const done = new Set();          // emitted (post-order complete)
  if (!set || !set.size) return out;
  //  Iterative DFS: a frame is { sha, i } over its in-set parent list; when
  //  every parent is emitted, the node itself is appended (post-order).
  for (const root of set) {
    if (done.has(root)) continue;
    const stack = [{ sha: root, parents: null, i: 0 }];
    while (stack.length) {
      if (done.size > WALK_CAP) break;
      const top = stack[stack.length - 1];
      if (top.parents === null) {
        if (done.has(top.sha)) { stack.pop(); continue; }
        let ps;
        try { ps = keeper.commitParents(top.sha); } catch (e) { ps = undefined; }
        //  Only follow parents that lie inside the closure `set`.
        top.parents = (ps || []).filter(function (p) {
          return isFullSha(p) && set.has(p);
        });
      }
      if (top.i < top.parents.length) {
        const p = top.parents[top.i++];
        if (!done.has(p) && !onStack(stack, p)) {   // skip a back-edge (cycle)
          stack.push({ sha: p, parents: null, i: 0 });
        }
        continue;
      }
      //  All parents emitted → emit this node (post-order = parents-first).
      if (!done.has(top.sha)) { done.add(top.sha); out.push(top.sha); }
      stack.pop();
    }
  }
  return out;
}

//  Is `sha` already an open frame on the DFS stack (a cycle back-edge)?
function onStack(stack, sha) {
  for (let i = 0; i < stack.length; i++) if (stack[i].sha === sha) return true;
  return false;
}

//  Parse a git ident string `Name <email> <epoch> <tz>` → epoch seconds
//  (the author/committer time).  Returns 0 when no trailing epoch.
function identEpoch(ident) {
  if (!ident) return 0;
  //  Trailing `<epoch> <tz>`: split on spaces, the epoch is the
  //  second-to-last token (last is the timezone).
  const toks = ident.trim().split(/\s+/);
  if (toks.length < 2) return 0;
  const tz = toks[toks.length - 1];
  const ep = toks[toks.length - 2];
  //  tz looks like +0000 / -0530; epoch is all digits.
  if (!/^[+-]\d{4}$/.test(tz)) return 0;
  if (!/^\d+$/.test(ep)) return 0;
  return parseInt(ep, 10);
}

//  First non-blank line of a commit body, clipped — the `#<subject>`
//  tail.  Trims leading blank lines; a TAB terminates the subject too
//  (ULOG field separator), matching get_emit_one_commit_verb.
const SUBJ_MAX = 64;
function subjectOf(body) {
  if (!body) return "";
  let i = 0;
  while (i < body.length && (body[i] === "\n" || body[i] === "\r")) i++;
  let j = i;
  while (j < body.length && body[j] !== "\n" && body[j] !== "\r" &&
         body[j] !== "\t") j++;
  let s = body.slice(i, j);
  if (s.length > SUBJ_MAX) s = s.slice(0, SUBJ_MAX);
  return s;
}

//  Build a divergence row for `sha`: { sha, hashlet, ts, subject }.
//  ts = commitTs (the commit's AUTHOR-time ron60, 0n when none) — the same
//  helper subs.js uses, so every divergence/sub row shares one ts rule.
function rowFor(keeper, sha) {
  let pc;
  try { pc = keeper.parseCommit(sha); } catch (e) { pc = undefined; }
  const ts = commitTs(keeper, sha);
  const subject = pc ? subjectOf(pc.body || "") : "";
  return { sha: sha, hashlet: sha.slice(0, 8), ts: ts, subject: subject };
}

//  GIT-016: a divergence row for a walk node — the full sha row when known
//  (local / keeper-resolved), else a hashlet-only row for a remote-only node
//  (its sha is not on hand until the pull side fetches it, T4/T5).
function rowForNode(keeper, node) {
  if (node.sha) return rowFor(keeper, node.sha);
  return { sha: undefined, hashlet: node.id, ts: 0n, subject: "" };
}

//  aheadBehind(keeper, curSha, tipSha) → { ahead, behind } (see header).
//  An equal cur/tip, a missing sha, or a no-divergence pair → both empty.
//  Lists are ordered newest-first by commit AUTHOR time (the C topo-sorts
//  then walks newest→oldest; commit time is the stable proxy with no
//  graf run index available in pure JS).
//  GIT-016: an optional remote wh128 index resolves the tip side's parents that
//  keeper lacks (pull side); identities become hashlets, so rows carry the sha
//  only for keeper-resolved nodes (a remote-only node gets a hashlet-keyed row).
function aheadBehind(keeper, curSha, tipSha, remoteIx) {
  const out = { ahead: [], behind: [] };
  if (!isFullSha(curSha) || !isFullSha(tipSha)) return out;
  if (curSha === tipSha) return out;

  const ancCur = ancestorNodes(keeper, curSha, remoteIx);
  const ancTip = ancestorNodes(keeper, tipSha, remoteIx);

  const ahead = [], behind = [];
  for (const [id, n] of ancCur) if (!ancTip.has(id)) ahead.push(rowForNode(keeper, n));
  for (const [id, n] of ancTip) if (!ancCur.has(id)) behind.push(rowForNode(keeper, n));

  //  Newest-first by author ts (BigInt desc); ties keep insertion order.
  const byTsDesc = function (a, b) {
    if (a.ts === b.ts) return 0;
    return a.ts > b.ts ? -1 : 1;
  };
  ahead.sort(byTsDesc);
  behind.sort(byTsDesc);
  out.ahead = ahead;
  out.behind = behind;
  return out;
}

//  isAncestor(keeper, ancSha, descSha) → YES iff `ancSha` is reachable
//  from `descSha` by parent edges (ancSha is an ancestor of descSha, i.e.
//  descSha DESCENDS ancSha).  Mirrors keeper KEEPIsAncestor(from=desc,
//  target=anc).  Used by subs.js for the R1-pin / R4-tip relationship.
//  A bounded parent-walk from descSha; stops as soon as ancSha is hit.
//  GIT-016: an optional remote wh128 index supplies parents keeper lacks; the
//  walk then compares node identities as hashlets (idOf), 2-arg calls unchanged.
function isAncestor(keeper, ancSha, descSha, remoteIx) {
  if (!isFullSha(ancSha) || !isFullSha(descSha)) return false;
  if (ancSha === descSha) return true;
  const ancId = idOf(remoteIx, ancSha);
  const seen = new Set();
  const queue = [{ id: idOf(remoteIx, descSha), sha: descSha }];
  seen.add(queue[0].id);
  let head = 0;
  while (head < queue.length) {
    if (seen.size > WALK_CAP) break;
    for (const p of parentsVia(keeper, remoteIx, queue[head++])) {
      if (p.id == null) continue;
      if (p.id === ancId) return true;
      if (seen.has(p.id)) continue;
      seen.add(p.id);
      queue.push(p);
    }
  }
  return false;
}

//  commitTs(keeper, sha) → ron60 of the commit's AUTHOR time (committer
//  fallback), 0n when unreadable / no epoch.  Shared with subs.js so the
//  advanced-sub `mod` row stamps the sub-tip commit ts (SUBS-030) using the
//  SAME convention as the ahead/behind rows above.
function commitTs(keeper, sha) {
  let pc;
  try { pc = keeper.parseCommit(sha); } catch (e) { return 0n; }
  if (!pc) return 0n;
  const secs = identEpoch(pc.author || pc.committer || "");
  if (secs <= 0) return 0n;
  try { return ron.of(secs * 1000); } catch (e) { return 0n; }
}

module.exports = {
  aheadBehind: aheadBehind,
  isAncestor: isAncestor,
  ancestors: ancestors,
  topoSort: topoSort,
  identEpoch: identEpoch,
  subjectOf: subjectOf,
  commitTs: commitTs
};
