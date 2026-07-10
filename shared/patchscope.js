//  patchscope.js — resolve the `be patch` scope + the ours/theirs/fork
//  commit triple (JS-052).  Mirrors sniff/PATCH.c resolve_ours /
//  resolve_target / resolve_cherry / resolve_parent_tip + the DIS-030 scope
//  forms:
//
//    `#<sha>`  NAMED  — cherry-pick ONE commit; theirs = the named commit,
//                       fork = parent(named) (the explicit-fork JOIN base).
//    `?<br>`   NEXT   — absorb ONE commit off the branch; theirs = the branch
//                       tip, fork = LCA(cur, theirs).  (Next-one selection is
//                       not modelled here — see the fidelity note below.)
//    `?<br>!`  WHOLE  — absorb the full stack; theirs = branch tip, fork =
//                       LCA(cur, theirs).
//
//  Fork is the 3-way merge base.  For NAMED we take parent(named) exactly
//  like resolve_cherry; for branch scopes we take LCA(ours, theirs) — the
//  most-recent shared ancestor (a forgiving base, per PATCH.c's note).
//
//  libabc+libdog ONLY: store.resolveRef + commit-graph walks over the reader
//  (no keeper/graf/sniff binding).
"use strict";

const shalib = require(__dirname + "/util/sha.js");   // JSQUE-016: -> shared/util/
const isFullSha = shalib.isFullSha;

//  Parse the patch URI into { scope, branch, frag }.  scope ∈
//  NAMED|NEXT|WHOLE.  `?br!` → WHOLE (shed the `!`); `?br` → NEXT; `#sha`
//  (or bare `#`) → NAMED.  A bare `<sha>`/ref with no `?`/`#` is NAMED.
function parseShape(arg) {
  const u = new URI(arg || "");
  //  PATCH-010: a scheme'd/authority-carrying arg is NEVER a cherry ref — it is
  //  a TREE source (`file:<path>` no-query | a `//WT` nav address) or refused.
  if (u.scheme !== undefined || u.authority !== undefined) {
    if (u.scheme === "file" && !u.query && !u.fragment && u.path &&
        (u.authority === undefined || u.authority === ""))
      return { scope: "TREE", branch: "", frag: "", tree: u.path, nav: false };
    if (u.scheme === undefined)
      return { scope: "TREE", branch: "", frag: "", tree: arg, nav: true };
    throw "PATCHFAIL: cannot patch from '" + arg +
          "' — supported: ?<br> | ?<br>! | #<sha> | a worktree address";
  }
  let query = u.query || "";
  let frag = u.fragment || "";
  //  A trailing `!` on the query is the WHOLE modifier (URI-002 debanger).
  let whole = false;
  if (query.length && query[query.length - 1] === "!") {
    whole = true;
    query = query.slice(0, -1);
  }
  if (frag && frag.length) return { scope: "NAMED", branch: "", frag: frag };
  if (query.length) {
    //  strip a leading `/proj/` selector → branch path.
    let branch = query;
    if (branch[0] === "/") {
      const j = branch.indexOf("/", 1);
      branch = j < 0 ? "" : branch.slice(j + 1);
    }
    return { scope: whole ? "WHOLE" : "NEXT", branch: branch, frag: "" };
  }
  //  No query, no fragment: a bare token is a named-commit pin (cherry).
  if (u.path && u.path.length) return { scope: "NAMED", branch: "", frag: u.path };
  return { scope: "NEXT", branch: "", frag: "" };
}

//  ours = the wt's current sha-tip (the cur branch's committed head).  Mirrors
//  resolve_ours (REFS tip of cur).  Falls back to the wtlog baseline sha.
function resolveOurs(wtl, reader) {
  const cur = wtl.curTip();
  const branch = (cur && cur.branch) || "";
  let sha = reader.resolveRef(branch);
  if (!sha && cur && cur.sha && isFullSha(cur.sha)) sha = cur.sha;
  if (!sha || !isFullSha(sha))
    throw "PATCHFAIL: cannot resolve ours (no cur tip)";
  return { sha: sha, branch: branch };
}

//  theirs/fork for a NAMED cherry: theirs = the named commit, fork = its
//  first parent.  Mirrors resolve_cherry.
function resolveCherry(reader, frag) {
  let thr = frag;
  if (!isFullSha(thr)) {
    const r = reader.resolveRef(frag);
    if (r && isFullSha(r)) thr = r;
  }
  if (!isFullSha(thr))
    throw "PATCHFAIL: cannot resolve cherry ref '" + frag + "'";
  const parents = reader.commitParents(thr);
  if (!parents || !parents.length)
    throw "PATCHFAIL: cherry-pick of root commit unsupported";
  return { thr: thr, fork: parents[0] };
}

//  Ancestor closure of `root` (first-parent + merge parents), as a Set of
//  shas.  Mirrors dag.ancestors but local so patchscope has no dag cycle.
function ancestorSet(reader, root) {
  const seen = Object.create(null);
  const stack = [root];
  while (stack.length) {
    const sha = stack.pop();
    if (!sha || seen[sha]) continue;
    seen[sha] = true;
    let parents;
    try { parents = reader.commitParents(sha); } catch (e) { parents = undefined; }
    if (!parents) continue;
    for (const p of parents) if (!seen[p]) stack.push(p);
  }
  return seen;
}

//  LCA(a, b): the most-recent common ancestor — the first commit in b's
//  ancestor walk that is also reachable from a.  Returns undefined when the
//  two share no history (a flat peer branch); the caller drops to no base.
function lca(reader, a, b) {
  if (a === b) return a;
  const ancA = ancestorSet(reader, a);
  //  BFS from b in ts order is overkill; a DFS that returns the first
  //  b-ancestor present in ancA is the merge base for a linear/DAG history.
  const seen = Object.create(null);
  const stack = [b];
  while (stack.length) {
    const sha = stack.pop();
    if (!sha || seen[sha]) continue;
    seen[sha] = true;
    if (ancA[sha]) return sha;
    let parents;
    try { parents = reader.commitParents(sha); } catch (e) { parents = undefined; }
    if (!parents) continue;
    for (const p of parents) if (!seen[p]) stack.push(p);
  }
  return undefined;
}

//  PATCH-010: triple for a TREE source — theirs = the addressed wt's cur tip
//  (the VERB resolves the address + wtlog); fork = LCA, a WHOLE-style absorb.
function resolveTree(theirs, branch, wtl, reader) {
  const ours = resolveOurs(wtl, reader);
  const fork = lca(reader, ours.sha, theirs);
  return { scope: "WHOLE", branch: branch || "", ours: ours.sha,
           theirs: theirs, fork: fork };
}

//  Resolve the full ours/theirs/fork triple + scope for a patch arg.
//  Returns { scope, branch, ours, theirs, fork } (40-hex shas; fork may be
//  undefined when the branches share no history → empty merge base).
function resolve(arg, wtl, reader) {
  const shape = parseShape(arg);
  //  PATCH-010: a TREE address needs the verb's fs/wtlog resolution — see patch.js.
  if (shape.scope === "TREE")
    throw "PATCHFAIL: tree source '" + arg + "' must be resolved by the verb";
  const ours = resolveOurs(wtl, reader);

  if (shape.scope === "NAMED") {
    const c = resolveCherry(reader, shape.frag);
    return { scope: "NAMED", branch: ours.branch, ours: ours.sha,
             theirs: c.thr, fork: c.fork };
  }

  //  Branch scope (NEXT / WHOLE): theirs = the branch tip.
  let thr = reader.resolveRef(shape.branch);
  if (!thr && isFullSha(shape.branch)) thr = shape.branch;
  if (!thr || !isFullSha(thr))
    throw "PATCHFAIL: cannot resolve target branch '?" + shape.branch + "'";
  const fork = lca(reader, ours.sha, thr);
  return { scope: shape.scope, branch: shape.branch, ours: ours.sha,
           theirs: thr, fork: fork };
}

module.exports = {
  parseShape: parseShape,
  resolveTree: resolveTree,
  resolveOurs: resolveOurs,
  resolveCherry: resolveCherry,
  lca: lca,
  ancestorSet: ancestorSet,
  resolve: resolve
};
