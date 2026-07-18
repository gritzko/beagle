//  patchscope.js — resolve the `be patch` scope + the ours/theirs/fork
//  commit triple (JS-052).  Mirrors sniff/PATCH.c resolve_ours /
//  resolve_target / resolve_cherry / resolve_parent_tip.  PATCH-015 scope
//  forms (DIS-030 overturned — URI bangs RETIRED):
//
//    `#<sha>`   NAMED  — cherry-pick ONE commit; theirs = the named commit,
//                        fork = parent(named) (the explicit-fork JOIN base).
//    `?<br>`    LINE   — absorb the WHOLE missing line; theirs = the branch tip,
//    (bare)              fork = LCA(cur, theirs).  Bare `patch` = the tracked ref.
//                        A non-empty path slot scopes the LINE to those paths.
//
//  Fork is the 3-way merge base.  For NAMED we take parent(named) exactly
//  like resolve_cherry; for LINE we take LCA(ours, theirs) — the most-recent
//  shared ancestor (a forgiving base, per PATCH.c's note).
//
//  URI-016: the URI->commit step is NOT patchscope's — it is resolve_hash's
//  ([/wiki/URI] §URI->hash step 5).  Only parents/fork/LCA/scope live here.
"use strict";

const shalib = require(__dirname + "/util/sha.js");   // JSQUE-016: -> shared/util/
const isFullSha = shalib.isFullSha;

//  URI-016: THE URI->commit step.  resolve_hash checks the object IS a commit on
//  every rung of step 5, so `#hashlet` (5.2) and `?hashlet` (5.4) resolve here
//  exactly as `type:` already sees them.  LAZY require: resolve_hash pulls
//  shared/*, and a top-level require from shared/ would close that cycle.
//  Returns the 40-hex `chash`, or undefined — the caller owns the refusal, so
//  resolve_hash's ok64 codes never leak into the view's PATCHFAIL dialect.
function chashOf(pin) {
  const rh = require(__dirname + "/../core/resolve_hash.js");
  const discover = require(__dirname + "/../core/discover.js");
  const ctx = discover.navCwd(discover.ctxDir());   // URI-016: derived off be.context
  let r; try { r = rh.resolve_hash(ctx, pin); } catch (e) { return undefined; }
  return r && isFullSha(r.chash) ? r.chash : undefined;
}

//  Parse the patch URI into { scope, branch, frag, paths }.  scope ∈
//  NAMED|LINE|TREE.  PATCH-015 (DIS-030 overturned): URI bangs are RETIRED, so
//  `?br` and bare `patch` BOTH absorb the WHOLE missing line (the old NEXT
//  one-commit scope is gone); a trailing `?br!` is ignored (bang shed), never a
//  distinct scope.  `#sha` (or bare `#`) → NAMED cherry.  A bare `<sha>`/ref
//  with no `?`/`#` is NAMED.  A non-empty path slot + `?ref` → a path-scoped
//  LINE (paths carries the scope; POST records no provenance for it).
function parseShape(arg) {
  const u = new URI(arg || "");
  //  PATCH-010: a scheme'd/authority-carrying arg is NEVER a cherry ref — it is
  //  a TREE source (`file:<path>` no-query | a `//WT` nav address) or refused.
  if (u.scheme !== undefined || u.authority !== undefined) {
    if (u.scheme === "file" && !u.query && !u.fragment && u.path &&
        (u.authority === undefined || u.authority === ""))
      return { scope: "TREE", branch: "", frag: "", paths: [], tree: u.path, nav: false };
    if (u.scheme === undefined)
      return { scope: "TREE", branch: "", frag: "", paths: [], tree: arg, nav: true };
    throw "PATCHFAIL: cannot patch from '" + arg +
          "' — supported: ?<br> | #<sha> | a worktree address";
  }
  let query = u.query || "";
  let frag = u.fragment || "";
  //  PATCH-015: shed a legacy trailing `!` on the query (bangs are retired) —
  //  `?br!` is just `?br`, the whole missing line, never a distinct scope.
  if (query.length && query[query.length - 1] === "!") query = query.slice(0, -1);
  //  URI-016: `pin` is the URI resolve_hash resolves — the SLOT the token was
  //  typed in decides the rung: `#x` is hashlet-only (5.1/5.2).
  if (frag && frag.length)
    return { scope: "NAMED", branch: "", frag: frag, paths: [],
             pin: URI.make(undefined, undefined, undefined, undefined, frag) };
  if (query.length) {
    //  strip a leading `/proj/` selector → branch path.
    let branch = query;
    if (branch[0] === "/") {
      const j = branch.indexOf("/", 1);
      branch = j < 0 ? "" : branch.slice(j + 1);
    }
    //  PATCH-015: a non-empty path slot scopes the LINE absorb to those paths.
    const paths = (u.path && u.path.length) ? [u.path] : [];
    return { scope: "LINE", branch: branch, frag: "", paths: paths };
  }
  //  No query, no fragment: a bare token is a named-commit pin (cherry).  It may
  //  be a branch ref OR a hex token, so it pins through the `?ref` slot — step
  //  5.3/5.4 resolves BOTH, which is exactly the old ladder's contract.
  if (u.path && u.path.length)
    return { scope: "NAMED", branch: "", frag: u.path, paths: [],
             pin: URI.make(undefined, undefined, undefined, u.path, undefined) };
  //  PATCH-015: bare `patch` — the whole missing line of the TRACKED ref.
  return { scope: "LINE", branch: "", frag: "", paths: [] };
}

//  ours = the wt's current sha-tip (the cur branch's committed head).  Mirrors
//  resolve_ours.  PATCH-015: the wt's OWN cur sha is authoritative — a wt pinned
//  back (e.g. `get ?#t1`) has cur < the branch REF, and OURS is what the wt
//  actually holds, NOT resolveRef(branch) (which would read the ahead ref and
//  make the missing set spuriously empty).  Only fall back to the ref when cur
//  carries no sha.
function resolveOurs(wtl, reader) {
  const cur = wtl.curTip();
  const branch = (cur && cur.branch) || "";
  let sha = (cur && cur.sha && isFullSha(cur.sha)) ? cur.sha : reader.resolveRef(branch);
  if (!sha || !isFullSha(sha))
    throw "PATCHFAIL: cannot resolve ours (no cur tip)";
  return { sha: sha, branch: branch };
}

//  theirs/fork for a NAMED cherry: theirs = the named commit, fork = its
//  first parent.  Mirrors resolve_cherry.  URI-016: the ref->commit half is
//  resolve_hash's; the full-sha-or-branch ladder that lived here was a twin of
//  step 5 that refused every HASHLET the shard holds.
function resolveCherry(reader, frag, pin) {
  const thr = chashOf(pin || URI.make(undefined, undefined, undefined, undefined, frag));
  if (!thr)
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
//  (the VERB resolves the address + wtlog); fork = LCA, a whole-line absorb.
function resolveTree(theirs, branch, wtl, reader) {
  const ours = resolveOurs(wtl, reader);
  const fork = lca(reader, ours.sha, theirs);
  return { scope: "LINE", branch: branch || "", paths: [], ours: ours.sha,
           theirs: theirs, fork: fork };
}

//  PATCH-015: the `foster`/`picked` shas a commit body carries (headers git
//  does not understand — git.parseCommit drops them, so scan the raw header
//  block, which ends at the first blank line).
function commitHeaderShas(reader, sha, name) {
  let obj; try { obj = reader.getObject(sha); } catch (e) { return []; }
  if (!obj || obj.type !== "commit") return [];
  const out = [];
  const lines = utf8.Decode(obj.bytes).split("\n");
  for (const line of lines) {
    if (line === "") break;                    // header block ends at blank
    const sp = line.indexOf(" ");
    if (sp < 0 || line.slice(0, sp) !== name) continue;
    const v = line.slice(sp + 1).trim();
    if (isFullSha(v)) out.push(v);
  }
  return out;
}

//  PATCH-015 §Ancestor-skip: reachability closure of `root` via parent ∪ foster
//  edges, plus the `picked` shas seen along it.  `picked` is DEDUP-ONLY and
//  does NOT extend reachability (a later `?ref` still re-absorbs a picked line).
function reachClosure(reader, root) {
  const anc = Object.create(null), picked = Object.create(null);
  const stack = [root];
  while (stack.length) {
    const sha = stack.pop();
    if (!sha || anc[sha]) continue;
    anc[sha] = true;
    let parents = []; try { parents = reader.commitParents(sha) || []; } catch (e) {}
    for (const p of parents) if (!anc[p]) stack.push(p);
    for (const f of commitHeaderShas(reader, sha, "foster")) if (!anc[f]) stack.push(f);
    for (const pk of commitHeaderShas(reader, sha, "picked")) picked[pk] = true;
  }
  return { anc: anc, picked: picked };
}

//  PATCH-015: a cherry is a dedup no-op when its target is already reachable
//  from cur (parent ∪ foster) OR already recorded as a `picked` header there.
function alreadyPicked(reader, cur, target) {
  if (!cur || !target) return false;
  const c = reachClosure(reader, cur);
  return !!(c.anc[target] || c.picked[target]);
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
    const c = resolveCherry(reader, shape.frag, shape.pin);
    return { scope: "NAMED", branch: ours.branch, paths: [], ours: ours.sha,
             theirs: c.thr, fork: c.fork };
  }

  //  LINE scope (whole missing line): theirs = the branch tip.  PATCH-015: bare
  //  `patch` (empty branch) absorbs the TRACKED ref — the branch the wt's cur is
  //  attached to (resolveRef(cur.branch)), which may sit AHEAD of ours after a
  //  pin-back.  A named `?ref` resolves through resolve_hash (URI-016 step 5.3/4).
  let thr;
  if (shape.branch)
    thr = chashOf(URI.make(undefined, undefined, undefined, shape.branch, undefined));
  else
    thr = reader.resolveRef(ours.branch || "");
  if (!thr || !isFullSha(thr))
    throw "PATCHFAIL: cannot resolve target branch '?" + shape.branch + "'";
  const fork = lca(reader, ours.sha, thr);
  return { scope: "LINE", branch: shape.branch, paths: shape.paths || [],
           ours: ours.sha, theirs: thr, fork: fork };
}

module.exports = {
  parseShape: parseShape,
  resolveTree: resolveTree,
  resolveOurs: resolveOurs,
  resolveCherry: resolveCherry,
  lca: lca,
  ancestorSet: ancestorSet,
  commitHeaderShas: commitHeaderShas,
  reachClosure: reachClosure,
  alreadyPicked: alreadyPicked,
  resolve: resolve
};
