//  subs.js — submodule enumeration + classification + recursion (JS-033).
//  Pure JS over keeper.js (baseline-tree gitlink walk + sub-shard reader),
//  be.js (sub-mount discovery), wtlog.js (R4 sub-wt tip), dag.js (real
//  ancestry on the sub shard); no C, no dog — shares zero code with
//  sniff/SUBS.  Mirrors keeper/SUBS.c (KEEPSubsAt gitlink enum),
//  sniff/SUBS.c (SUBSDirty 3-axis classify, SNIFFSubIsMount/ReadTip) and
//  the SNIFF.exe.c CLASS_BASE_ONLY gitlink arm.
//
//  enumerate(repo, keeper, baselineTreeSha) → [ sub, … ] where each sub:
//    path     gitlink path in the baseline tree (the sub's wt-relative dir)
//    pin      R1 — the parent gitlink commit sha (the 160000 entry's sha)
//    mounted  bool — `<wt>/<path>/.be` is a regular file (a live mount)
//    bucket   "adv" | "ok"  — `adv` iff ADVANCED (R4 tip DESCENDS the pin)
//    stale    "" | "behind" | "diverged"  (the STALE axis — NOT adv)
//    r4       sub-wt tip sha (R4.base), "" when no mount / no tip
//    title    sub project shard title (for the sibling-shard open)
//    ts       ron60 of the sub-tip (r4) commit for the `adv` row date
//             (SUBS-030 stamps it native-side); 0n otherwise
//
//  Classification (SUBSDirty, 3 orthogonal axes — only DIRTY drives adv):
//    * ADVANCED (dirty): R4.base descends R1 (pin ⊑ r4, r4 ⋢ pin) — a
//      forward bump pending → `adv`.  SUBS-030 split the dirty verb: a
//      content EDIT reads `mod`, an advance-only sub reads the distinct
//      `adv` (Edited wins if both set).  The JS port has no Edited axis
//      yet (it stays inert — see below), so the only dirty case here is
//      advance-only → always `adv`, never `mod`.
//    * BEHIND  (stale):  r4 ⊑ pin (re-get needed) — NOT adv.
//    * DIVERGED (stale): cousins, neither descends — NOT adv.
//    * EQUAL pin: clean → `ok`.
//  The Edited axis (sub-local file edits ≠ R4.base) stays INERT here, the
//  same as native C until SUBS-027b — a noted residual gap (JS-034
//  candidate: it needs a recursive file-scan in the sub wt).  An unmounted
//  gitlink, or a mount whose shard can't be opened, stays `ok` (matches
//  the C `SNIFFSubIsMount` gate: only mounts are classified).

"use strict";

const join = require("./util/path.js").join;   // JSQUE-016: util libs -> shared/util/
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const wtpath = require("../core/discover.js").wtpath;
const isFullSha = require("./util/sha.js").isFullSha;

function statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }
function isFile(p) { return statKind(p) === "reg"; }
function lstatKind(p) { try { return io.lstat(p).kind; } catch (e) { return undefined; } }
//  GET-036: a sub mount point is a REAL directory; a SYMLINK there (the
//  `be/` self-locator `be -> .`, which follows to the wt's own `.be`)
//  is never a mount.  Mirror SNIFFSubIsMount's NOFOLLOW guard.
function isMountAt(subWt) {
  if (lstatKind(subWt) === "lnk") return false;
  const p = wtpath(subWt, ".be");
  //  SUBS-049: `.be` FILE OR a PRIMARY nested wt (`.be` DIR + wtlog), mirroring
  //  be.treeAt's anchor — parity with the FILE-anchored sub.
  return isFile(p) || (statKind(p) === "dir" && isFile(wtpath(subWt, ".be/wtlog")));
}

function libDir() {
  return (typeof __dirname !== "undefined" && __dirname) ? __dirname : ".";
}

//  Classify one mounted sub against its pin (R1) using REAL ancestry on
//  the sub's OWN shard.  Returns { bucket, stale, r4, title, ts }.  Mirrors
//  SUBSDirty: open the sibling sub shard (be.treeAt on the sub wt resolves
//  store+title), read R4 (sub-wt cur tip), then compare by ancestry.
function classifyMount(parentRepo, subPath, pin) {
  const wtlog = require(libDir() + "/wtlog.js");
  const store = require(libDir() + "/store.js");
  const dag   = require(libDir() + "/dag.js");

  const res = { bucket: "ok", stale: "", r4: "", title: "", ts: 0n };
  const subWt = join(parentRepo.wt, subPath);

  //  Resolve the sub mount: be.treeAt on the sub wt dir reads the `<sub>/.be`
  //  secondary-wt anchor's row-0 redirect → the sub's store + project
  //  (title).  A non-mount (no anchor) throws → stays `ok`.
  let subRepo;
  try { subRepo = be.treeAt(subWt); } catch (e) { return res; }
  res.title = subRepo.project || "";

  //  R4.base — the sub wt's actually-checked-out commit (its cur tip).
  let r4 = "";
  try {
    const subLog = wtlog.open(subRepo);
    const cur = subLog.curTip();
    if (cur && cur.sha && isFullSha(cur.sha)) r4 = cur.sha;
  } catch (e) { /* no tip */ }
  res.r4 = r4;

  //  No R1 pin or no R4 tip — the pin-relationship axis doesn't apply;
  //  clean on the dirty axis (the Edited axis is inert here — see header).
  if (!isFullSha(pin) || !r4) return res;

  //  EQUAL pin: clean (the cheap string short-circuit, like SUBSDirty).
  if (r4 === pin) return res;

  //  Open the sub's OWN shard (a sibling project shard in the SAME store)
  //  and walk ancestry there.  A genuinely unresolvable shard → keep the
  //  conservative Advanced signal (a non-equal pin we can't verify is
  //  treated as forward), matching SUBSDirty's HOMEProjectExists fallback.
  let subK;
  try { subK = store.open(subRepo.storePath, subRepo.project); }
  catch (e) { res.bucket = "adv"; return res; }
  //  Guard: the shard must actually hold the pin/tip commits to decide
  //  ancestry; if neither resolves, fall back to the conservative adv.
  const pinAnc = dag.isAncestor(subK, pin, r4);   // pin ⊑ r4 → r4 descends → ahead
  const r4Anc  = dag.isAncestor(subK, r4, pin);   // r4  ⊑ pin → behind

  if (pinAnc && !r4Anc) {                                             // ADVANCED
    res.bucket = "adv"; res.stale = "";
    //  SUBS-030: the gitlink `adv` row carries the sub-tip commit ts.
    res.ts = dag.commitTs(subK, r4);
  }
  else if (r4Anc && !pinAnc) { res.bucket = "ok"; res.stale = "behind"; }
  else {
    //  Neither descends the other: a diverged pin (cousins) OR a shard
    //  that hasn't fetched one side (ancestry undecidable).  SUBSDirty
    //  treats undecidable-and-unresolvable as conservative Advanced, but
    //  a RESOLVABLE diverged pin is STALE (not adv).  We can't cheaply
    //  tell the two apart in pure JS; mirror the resolvable-diverged =
    //  STALE branch (NOT adv) since the shard opened — the conservative
    //  Advanced only fires when the shard itself is missing (above).
    res.bucket = "ok"; res.stale = "diverged";
  }
  return res;
}

//  enumerate(repo, keeper, baselineTreeSha) — walk the baseline tree for
//  160000 gitlinks, classify each, return the sub list in tree decl order
//  (readTreeRecursive yields lex/tree order; the C KEEPSubsAt emits in
//  decl order — close enough for the status row sort, which re-sorts by
//  path).  `repo` is be.treeAt()'s result (wt + storePath + project).
function enumerate(repo, keeperReader, baselineTreeSha) {
  const subs = [];
  if (!baselineTreeSha) return subs;
  const links = [];
  keeperReader.readTreeRecursive(baselineTreeSha, function (leaf) {
    if (leaf.kind === "s") links.push({ path: leaf.path, pin: leaf.sha });
  });
  for (const l of links) {
    const subWt = wtpath(repo.wt, l.path);
    const mounted = isMountAt(subWt);
    let cls = { bucket: "ok", stale: "", r4: "", title: "", ts: 0n };
    if (mounted) cls = classifyMount(repo, l.path, l.pin);
    subs.push({
      path: l.path, pin: l.pin, mounted: mounted,
      bucket: cls.bucket, stale: cls.stale, r4: cls.r4, title: cls.title,
      ts: cls.ts
    });
  }
  return subs;
}

//  mountWtDir(repo, subPath) → absolute sub-mount wt dir (for recursion).
function mountWtDir(repo, subPath) { return wtpath(repo.wt, subPath); }

module.exports = {
  enumerate: enumerate,
  classifyMount: classifyMount,
  mountWtDir: mountWtDir,
  isFullSha: isFullSha
};
