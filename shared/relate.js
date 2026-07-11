//  relate.js — GIT-016: the ONE graph-only advertise -> resolve -> verdict
//  spine the remote verbs share.  It advertises the peer's ref tips
//  (wire.advertRefs), resolves the target ref (the GIT-013/015 pushRemote
//  branch -> refs/heads/X rule with the absolute-marker + empty-segment
//  guards), and returns the LOCAL-object verdict via dag.isAncestor /
//  dag.aheadBehind (over `keeper`, plus an OPTIONAL remote wh128 index).  NO
//  content transfer lives here — that is a per-verb step AFTER the verdict.
//
//  resolveRef(branch) -> "refs/heads/X" | throws {code,msg} (POSTNOREF/POSTREF)
//  relate(keeper, remoteUri, branch, tip, hasQuery, remoteIx?) ->
//    { wireRef, old, adv, verdict }  where
//      wireRef  the resolved remote ref
//      old      the peer's advertised sha for wireRef (""/absent = new ref)
//      adv      the raw advert ({ refs:[{sha,name}] }) — for a per-verb pack
//      verdict  { eq, ff }  — push-side ({eq, ff(ahead), non-ff}); `ff` is a
//               LOCAL parent-walk from cur (tip) seeking old.  behind is UNKNOWN
//               on the push side (remote ancestry not walked) and NOT guessed.

"use strict";

const wire   = require("./wire.js");
const dag    = require("./dag.js");
const shalib = require("./util/sha.js");
const isFullSha = shalib.isFullSha;

//  GIT-016 (was pushRemote): branch -> refs/heads/X.  GIT-015 defect A strips a
//  leading absolute-marker `/` (`?/project` -> project); an empty path segment
//  (`refs/heads//…`, trailing `/`) is a bad target -> POSTREF (never sent).
//  SUBS-050: NOT routed through branch.wireRef — test/put/wire-refguard pins the
//  RAW-string semantics (`/project` -> project; `dev/`/`a//b` -> POSTREF), which
//  the codec's segment split/merge collapses away; preserved per the no-test-edit
//  rule (see the ticket's wire-ref note; serve.js/wire.js DO use the codec, over
//  already-clean title-stripped keys).
function resolveRef(branch) {
  const bare = (branch && branch[0] === "/") ? branch.slice(1) : branch;
  const wireRef = "refs/heads/" + ((bare && bare !== "main") ? bare : "main");
  if (/\/\//.test(wireRef) || wireRef[wireRef.length - 1] === "/")
    throw { code: "POSTREF",
            msg: "POSTREF: empty ref segment in `" + wireRef + "` — bad branch target" };
  return wireRef;
}

//  GIT-016: advertise -> resolve -> verdict (see header).  `hasQuery` false =>
//  no branch selected (GIT-015 defect B) -> POSTNOREF.  The FF gate is a LOCAL
//  walk from cur (tip) seeking the advertised old; a remote wh128 index, when
//  present, lets that walk cross remote-only commits (pull side).
//  GIT-019: `adv` is INJECTABLE (mirrors `remoteIx`) — a caller holding a live
//  push session's advert feeds it in; when absent we advertise internally (pull).
function relate(keeper, remoteUri, branch, tip, hasQuery, remoteIx, adv) {
  if (!hasQuery)
    throw { code: "POSTNOREF",
            msg: "POSTNOREF: no branch selected — use `?/PROJ` to pick a trunk" };
  const wireRef = resolveRef(branch);
  if (!adv) adv = wire.advertRefs(remoteUri, "receive-pack");
  const cur = adv.refs.find(function (r) { return r.name === wireRef; });
  const old = cur ? cur.sha : "";

  const verdict = { eq: false, ff: false };
  if (old && isFullSha(old)) {
    if (old === tip) verdict.eq = true;
    else verdict.ff = dag.isAncestor(keeper, old, tip, remoteIx);
  } else {
    verdict.ff = true;               // no remote tip => FF from nothing
  }
  return { wireRef: wireRef, old: old, adv: adv, verdict: verdict };
}

//  GIT-016 (pull side): the FULL verdict of local `cur` vs remote `tip`, with
//  the remote commits present in `remoteIx` (an abc.index("wh128",{mem}) of
//  commit->parent edges).  Derived from dag.aheadBehind + dag.isAncestor:
//    eq         cur === tip
//    ahead      cur descends tip (tip is an ancestor of cur) — get can FF back,
//               post can advance; only local commits are ahead
//    behind     tip descends cur (cur is an ancestor of tip) — get can FF forward
//    diverged   both lists non-empty AND a common base exists
//    unrelated  both lists non-empty AND no common base (disjoint histories)
//  Returns { rel, ahead:[rows], behind:[rows] } — the SAME row shape
//  dag.aheadBehind yields (sha/hashlet/ts/subject).
function verdict(keeper, cur, tip, remoteIx) {
  if (!isFullSha(cur) || !isFullSha(tip))
    return { rel: "unrelated", ahead: [], behind: [] };
  if (cur === tip) return { rel: "eq", ahead: [], behind: [] };
  const ab = dag.aheadBehind(keeper, cur, tip, remoteIx);
  const ahead = ab.ahead, behind = ab.behind;
  if (!behind.length) return { rel: "ahead",  ahead: ahead, behind: behind };
  if (!ahead.length)  return { rel: "behind", ahead: ahead, behind: behind };
  //  Both diverge: a common base exists iff tip and cur share an ancestor.  The
  //  cheapest present test is either side's ancestor reachable from the other's
  //  base — isAncestor both ways is false here (neither descends the other), so
  //  probe for a shared ancestor via aheadBehind's own closures: if cur & tip
  //  had NO common ancestor, EVERY ancestor of each would appear in ahead/behind.
  const base = hasCommonBase(keeper, cur, tip, remoteIx);
  return { rel: base ? "diverged" : "unrelated", ahead: ahead, behind: behind };
}

//  GIT-016: do cur & tip share ANY ancestor commit? — intersect the two
//  ancestor id-sets (dag.ancestors, which already crosses the remote index).
//  A non-empty intersection = a common base = diverged (not unrelated).
function hasCommonBase(keeper, cur, tip, remoteIx) {
  const ac = dag.ancestors(keeper, cur, remoteIx);
  const at = dag.ancestors(keeper, tip, remoteIx);
  for (const id of ac) if (at.has(id)) return true;
  return false;
}

module.exports = { relate: relate, resolveRef: resolveRef, verdict: verdict };
