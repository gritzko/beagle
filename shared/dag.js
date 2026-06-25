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

const isFullSha = require("./util/sha.js").isFullSha;   // JSQUE-016: -> shared/util/

const WALK_CAP = 1 << 16;   // ~65k commits/side — matches the C anc cap order

//  Collect the ancestor SET of `root` (INCLUDING root itself) by a
//  bounded BFS over keeper.commitParents.  Returns a Set of 40-hex shas.
//  Unreadable / missing commits terminate that branch quietly (a shallow
//  shard may lack deep ancestors — the C walk is equally tolerant).
function ancestors(keeper, root) {
  const seen = new Set();
  if (!isFullSha(root)) return seen;
  const queue = [root];
  seen.add(root);
  let head = 0;
  while (head < queue.length) {
    if (seen.size > WALK_CAP) break;
    const sha = queue[head++];
    let parents;
    try { parents = keeper.commitParents(sha); } catch (e) { parents = undefined; }
    if (!parents) continue;
    for (const p of parents) {
      if (!isFullSha(p) || seen.has(p)) continue;
      seen.add(p);
      queue.push(p);
    }
  }
  return seen;
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

//  aheadBehind(keeper, curSha, tipSha) → { ahead, behind } (see header).
//  An equal cur/tip, a missing sha, or a no-divergence pair → both empty.
//  Lists are ordered newest-first by commit AUTHOR time (the C topo-sorts
//  then walks newest→oldest; commit time is the stable proxy with no
//  graf run index available in pure JS).
function aheadBehind(keeper, curSha, tipSha) {
  const out = { ahead: [], behind: [] };
  if (!isFullSha(curSha) || !isFullSha(tipSha)) return out;
  if (curSha === tipSha) return out;

  const ancCur = ancestors(keeper, curSha);
  const ancTip = ancestors(keeper, tipSha);

  const ahead = [], behind = [];
  for (const sha of ancCur) if (!ancTip.has(sha)) ahead.push(rowFor(keeper, sha));
  for (const sha of ancTip) if (!ancCur.has(sha)) behind.push(rowFor(keeper, sha));

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
function isAncestor(keeper, ancSha, descSha) {
  if (!isFullSha(ancSha) || !isFullSha(descSha)) return false;
  if (ancSha === descSha) return true;
  const seen = new Set();
  const queue = [descSha];
  seen.add(descSha);
  let head = 0;
  while (head < queue.length) {
    if (seen.size > WALK_CAP) break;
    const sha = queue[head++];
    let parents;
    try { parents = keeper.commitParents(sha); } catch (e) { parents = undefined; }
    if (!parents) continue;
    for (const p of parents) {
      if (!isFullSha(p)) continue;
      if (p === ancSha) return true;
      if (seen.has(p)) continue;
      seen.add(p);
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
