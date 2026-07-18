//  quad.js — BRO-030: the unified quad status model (wiki/Status.mkd).
//  A path's whole story is FOUR relations against the ROOT tree, where
//  root = LCA(track, base) (dag.mergeBase) and the columns are, in order:
//    1 track  the tracked ref tip        (blue)
//    2 base   the wt's base commit       (green)
//    3 patch  the patched-in theirs      (yellow)
//    4 wt     the bytes on disk          (orange) — vs BASE, not root:
//             the wt column is the LOCAL-dirt axis (gritzko 2026-07-17),
//             so a cleanly committed change reads `.v..`, never `.v.v`.
//  Each column relates to root's tree entry as ONE char — the greppable
//  ASCII canon (gritzko's ruling; the tty render may substitute Unicode):
//    '.' same   'x' removed   'o' created   'v' advanced (different hash)
//  Styles ride the wt char: UPPERCASE = staged in plain ('X'/'O'/'V'),
//  bold on a tty; red = conflicted (plain spells it '!').
//  Commit ahead/behind is the SAME comparison one level up: a commit row's
//  quad marks 'o' in every column whose tip REACHES it (root never does) —
//  so a local unposted commit reads ".o..", an unabsorbed track commit
//  "o...", and a PATCH-absorbed one "o.o." (the STATUS-012 fix: absorbed
//  is patch-column ground, no longer a bare `miss`).
//
//  File lists are BUFFER-BACKED ULOGS (gritzko's ruling): each column's
//  tree listing is an abc.ram("ULOG") fed `<path>#<sha>` rows in lex tree
//  order (git tree walk order IS lex over full leaf paths), and the quad
//  falls out of a k-way cursor merge — no JS object maps for the trees.
//  The wt axis rides classify.classifyMerge's dirty rows (it owns the
//  stamp-set, staging and conflict knowledge); classify is wt-vs-base by
//  construction, so a path with no dirty row reads wt '.' (clean).
//
//  quadModel(inp) → { rows, commits, root, track, base, counts }  (pure)
//  quadOf(be, log, k) → quadModel over a live wt (ambient gather)
//  Degenerate roots (wiki/Status.mkd): detached ⇒ track = the pin (cur);
//  no track ⇒ track = base (all-'.'); no common ancestor ⇒ THROW — a
//  broken tree is an error to refuse, never a quad to render.

"use strict";

const dag      = require("./dag.js");
const classify = require("./classify.js");
const shalib   = require("./util/sha.js");
const isFullSha = shalib.isFullSha;

const CH = { same: ".", removed: "x", created: "o", advanced: "v" };

//  --- column ULOGs ------------------------------------------------------
//  treeUlog(k, commitSha) → an abc.ram("ULOG") of `<path>#<sha>` rows, one
//  per leaf (blobs AND gitlinks), fed in readTreeRecursive (lex) order with
//  a seq ts (the ULOG monotonic guard needs one; the ts is not data here).
//  A missing/unreadable commit yields an EMPTY ulog (a shallow shard reads
//  as all-removed rather than erroring — same tolerance as dag.js walks).
function treeUlog(k, commitSha) {
  const u = abc.ram("ULOG", 1 << 20);
  u._n = 0;
  if (!isFullSha(commitSha)) return u;
  let treeSha;
  try { treeSha = k.commitTree(commitSha); } catch (e) { treeSha = undefined; }
  if (!treeSha) return u;
  k.readTreeRecursive(treeSha, function (leaf) {
    u.feed("t", leaf.path + "#" + leaf.sha, BigInt(++u._n));
  });
  return u;
}

//  A pull cursor over a column ULOG: cur() → { path, sha } or null at end.
function cursor(u) {
  u.rewind();
  let live = u.next();
  function peel() {
    if (!live) return null;
    const s = u.uri, i = s.lastIndexOf("#");
    return { path: s.slice(0, i), sha: s.slice(i + 1) };
  }
  let cur = peel();
  return {
    cur: function () { return cur; },
    advance: function () { live = u.next(); cur = peel(); },
  };
}

//  patchUlog(k, theirsShas) → ONE merged column ulog over the absorbed
//  theirs trees, newest-wins on a path collision (wtlog order is oldest→
//  newest, so a LATER list entry shadows an earlier one) — the collapse
//  rule for several patched-in lines sharing the one yellow column.
function patchUlog(k, theirsShas) {
  const srcs = (theirsShas || []).map(function (sha) { return cursor(treeUlog(k, sha)); });
  const out = abc.ram("ULOG", 1 << 20);
  out._n = 0;
  for (;;) {
    let min = null;
    for (const c of srcs) {
      const v = c.cur();
      if (v && (!min || v.path < min)) min = v.path;
    }
    if (min === null) break;
    let sha = null;                      // last (newest) source wins the tie
    for (const c of srcs) {
      const v = c.cur();
      if (v && v.path === min) { sha = v.sha; c.advance(); }
    }
    out.feed("t", min + "#" + sha, BigInt(++out._n));
  }
  return out;
}

//  --- the wt axis (classifyMerge buckets → presence + styles) ------------
//  Per dirty path: is the wt side PRESENT, is the relation STAGED (bold),
//  is it CONflicted (red).  A clean path has no row ⇒ wt mirrors base.
const WT_BUCKET = {
  put: { present: 1, staged: 1 }, "new": { present: 1, staged: 1 },
  mov: { present: 1, staged: 1 }, unk: { present: 1 },
  mod: { present: 1 }, pat: { present: 1 }, mrg: { present: 1 },
  con: { present: 1, con: 1 }, adv: { present: 1 },
  del: { present: 0, staged: 1 }, rmv: { present: 0, staged: 1 },
  mis: { present: 0 },
};

//  --- relations ----------------------------------------------------------
function rel(rootSha, colSha) {
  if (rootSha == null && colSha == null) return CH.same;     // absent in both
  if (rootSha == null) return CH.created;
  if (colSha == null) return CH.removed;
  return rootSha === colSha ? CH.same : CH.advanced;
}

//  --- the model ----------------------------------------------------------
//  quadModel({ k, base, track, patches, wtRows }) → the pure merge:
//    k        keeper reader (commitTree/readTreeRecursive/commitParents/…)
//    base     the wt's base commit sha (required; "" = empty history)
//    track    the tracked tip sha ("" / undefined ⇒ track := base)
//    patches  absorbed theirs commit shas, oldest→newest (may be empty)
//    wtRows   classifyMerge dirty rows [{ bucket, path, src?, dst?, ts }]
//  → { rows: [{ path, quad, staged, con, ts, src? }],   (lex by path)
//      commits: [{ quad, sha, hashlet, ts, subject }],  (newest-first)
//      root, track, base, counts: { perChar tallies } }
function quadModel(inp) {
  const k = inp.k;
  const base = isFullSha(inp.base) ? inp.base : "";
  let track = isFullSha(inp.track) ? inp.track : base;   // no track ⇒ all-'.'
  const patches = inp.patches || [];

  //  Root = LCA(track, base); root==base / root==track are the all-behind /
  //  all-ahead cases and need nothing special.  NO common ancestor between
  //  two real tips is a broken tree — refuse (wiki/Status.mkd ruling).
  let root = base;
  if (track && base && track !== base) {
    //  POST-031: a track pin with NO readable commit in this store (a re-
    //  cloned sub's stale parent pin) is not a real tip — the no-track rule.
    let tp; try { tp = k.commitParents(track); } catch (e) { tp = undefined; }
    if (tp == null) track = base;
    else {
      root = dag.mergeBase(k, track, base);
      if (!root) throw "broken tree: track and base share no common ancestor";
    }
  } else if (!base) root = track;        // empty-history wt (pre-first-post)

  //  The four tree columns as buffer-backed ULOG cursors (ruling above).
  const cRoot  = cursor(treeUlog(k, root));
  const cTrack = cursor(treeUlog(k, track));
  const cBase  = cursor(treeUlog(k, base));
  const cPatch = cursor(patchUlog(k, patches));

  //  The wt axis as a 5th (JS-array) cursor, lex by path; a move pair's dst
  //  row lands out of order at its src position, so sort defensively.
  const wtRows = (inp.wtRows || []).slice().sort(function (a, b) {
    return a.path < b.path ? -1 : a.path > b.path ? 1 : 0;
  });
  let wi = 0;

  const rows = [];
  const counts = { track: 0, base: 0, patch: 0, wt: 0, con: 0, staged: 0 };
  for (;;) {
    //  min path across the five cursors
    let min = null;
    for (const c of [cRoot, cTrack, cBase, cPatch]) {
      const v = c.cur();
      if (v && (min === null || v.path < min)) min = v.path;
    }
    if (wi < wtRows.length && (min === null || wtRows[wi].path < min)) min = wtRows[wi].path;
    if (min === null) break;

    const take = function (c) {
      const v = c.cur();
      if (v && v.path === min) { c.advance(); return v.sha; }
      return null;
    };
    const shaRoot = take(cRoot), shaTrack = take(cTrack);
    const shaBase = take(cBase), shaPatch = take(cPatch);
    let w = null;
    if (wi < wtRows.length && wtRows[wi].path === min) w = wtRows[wi++];

    const rTrack = rel(shaRoot, shaTrack);
    const rBase  = rel(shaRoot, shaBase);
    //  Patch column: absent-from-theirs is NOT a removal claim (a theirs tree
    //  simply not touching the path) — only a path theirs CARRIES relates.
    const rPatch = shaPatch == null ? CH.same : rel(shaRoot, shaPatch);
    //  wt vs BASE (the local-dirt axis): clean ⇒ '.'; dirty ⇒ presence vs
    //  base ('v' presumed when both present — bytes aren't hashed here).
    let rWt = CH.same, staged = false, con = false;
    if (w) {
      const wb = WT_BUCKET[w.bucket] || { present: 1 };
      rWt = wb.present ? (shaBase == null ? CH.created : CH.advanced)
                       : (shaBase == null ? CH.same : CH.removed);
      staged = !!wb.staged;
      con = !!wb.con;
    }

    const quad = rTrack + rBase + rPatch + rWt;
    if (quad === CH.same + CH.same + CH.same + CH.same && !staged && !con) continue;
    if (rTrack !== CH.same) counts.track++;
    if (rBase  !== CH.same) counts.base++;
    if (rPatch !== CH.same) counts.patch++;
    if (rWt    !== CH.same) counts.wt++;
    if (con) counts.con++;
    if (staged) counts.staged++;
    rows.push({ path: min, quad: quad, staged: staged, con: con,
                ts: w ? (w.ts || 0n) : 0n, src: w ? w.src : undefined });
  }

  //  --- commit rows (the same relations one level up) --------------------
  //  ahead = base-side commits (".o.."), behind = track-side ("o..."); a
  //  behind commit reachable from ANY absorbed theirs gains the patch '+'
  //  ("o.o.") — STATUS-012: absorbed is patch ground, not a bare miss.
  const commits = [];
  if (base && track && base !== track) {
    const ab = dag.aheadBehind(k, base, track);
    let absorbed = null;
    if (patches.length && ab.behind.length) {
      absorbed = new Set();
      for (const t of patches)
        for (const id of dag.ancestors(k, t)) absorbed.add(id);
    }
    for (const c of ab.ahead)
      commits.push({ quad: ".o..", sha: c.sha, hashlet: c.hashlet,
                     ts: c.ts, subject: c.subject });
    for (const c of ab.behind) {
      const inPatch = absorbed && c.sha && absorbed.has(c.sha);
      commits.push({ quad: inPatch ? "o.o." : "o...", sha: c.sha,
                     hashlet: c.hashlet, ts: c.ts, subject: c.subject });
    }
  }

  return { rows: rows, commits: commits, counts: counts,
           root: root, track: track, base: base };
}

//  --- ambient gather -----------------------------------------------------
//  quadOf(be, log, k[, opts]) → quadModel over a live wt: base/patches off
//  the wtlog, the track tip via resolve_hash (RULE ZERO — the ONE resolver;
//  detached ⇒ track = the pin = cur, per wiki/Status.mkd), the wt axis via
//  classifyMerge (opts.underNarrow forwarded).  `resolveTrack` is injectable
//  for tests; the default requires core/resolve_hash lazily (quadModel stays
//  usable with zero ambient globals).  STATUS-014: opts.trackPin (a full sha)
//  OVERRIDES track resolution — a recursed sub's track column is the parent's
//  track-tree gitlink pin, not the sub's self-ref row (base stays the sub's cur).
function quadOf(be, log, k, opts) {
  opts = opts || {};
  const cur = log.curTip();
  const base = (cur && isFullSha(cur.sha)) ? cur.sha : "";
  const att = log.attachedBranch();
  let track = "";
  //  STATUS-014: a RECURSED sub takes its track column from the PARENT's
  //  track-tree gitlink pin (opts.trackPin), NOT the sub's DIS-072 self-ref row.
  if (isFullSha(opts.trackPin)) track = opts.trackPin;
  else if (att.detached) track = base;   // detached: track = the pin (ruling)
  else if (att.track && opts.resolveTrack) track = opts.resolveTrack(att.track) || "";
  else if (att.track) {
    try {
      const discover = require("../core/discover.js");
      const rh = require("../core/resolve_hash.js")
        .resolve_hash(discover.navCwd(be.wt), att.track);
      const tip = rh.otype === "commit" ? rh.ohash : rh.chash;
      if (isFullSha(tip)) track = tip;
    } catch (e) { track = ""; }          // unresolvable track ⇒ all-'.' column
  }
  const patches = (typeof log.patchTheirs === "function") ? log.patchTheirs() : [];
  const m = classify.classifyMerge(be, log, k,
                                   opts.underNarrow ? { underNarrow: opts.underNarrow } : {});
  const model = quadModel({ k: k, base: base, track: track, patches: patches,
                            wtRows: m.rows });
  //  BRO-030: base-only gitlinks ride along for the status view's adv mapping
  //  (classifyMount is a view concern — the pure model stays sub-blind).
  model.gitlinks = m.gitlinks;
  return model;
}

module.exports = { quadModel: quadModel, quadOf: quadOf,
                   treeUlog: treeUlog, patchUlog: patchUlog,
                   CH: CH, WT_BUCKET: WT_BUCKET };
