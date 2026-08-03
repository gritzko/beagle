//  shared/lastcommit.js — LIST-001: the first-touch history attribution that the
//  `list:` view fuses onto its wt listing.  BRO-044 put a CACHE under it:
//
//    FAST PATH (shared/mtimeidx.js).  Every entry on screen is an OBJECT in the
//    scope tree — a blob for a file, a TREE for a directory — and the lane maps
//    `(hashlet60 | type:4) -> ron60` to the time of the oldest commit carrying
//    it.  A dir's tree sha changes whenever anything under it changes, so ONE
//    row answers a whole directory entry: the tree is never descended.  Rows the
//    lane does not have yet are filled by a walk BOUNDED BY WHAT IS ON SCREEN —
//    it stops as soon as the visible entries are attributed instead of dragging
//    to the bottom of history behind one ancient row.  The commit MESSAGE is not
//    in the lane, so the summary comes from a mainline walk that parses commits
//    ONLY (no tree diff) and opens a diff at the few commits whose time a
//    visible row actually names.
//
//    RAW PATH.  With no store shard to hold the lane, or with an explicit `cap`
//    (the LIST-001 ceiling repro), attribution is the original walk: tip→
//    first-parent newest-first, per commit diff its tree vs the mainline parent
//    via changedpaths.changedTrees, map each changed leaf to the IMMEDIATE scope
//    entry it lives under, halt when every entry is attributed or the ceiling
//    hits.  ONE walk, O(history × tree-diff).
//
//  Unattributed entries are simply ABSENT from the answer either way — the row
//  renders blank and sorts last, and attributes on a later, deeper query.
//  Rename-follow is OUT (a path appears/disappears only, per the ticket).
"use strict";

const changedpaths = require("./changedpaths.js");
const mtimeidx = require("./mtimeidx.js");
const shalib = require("./util/sha.js");
const dag = require("./dag.js");
const isFullSha = shalib.isFullSha;

//  LIST-001: mirror log.js LOG_MAX_WALK — the cyclic-DAG walk bound.  Entries
//  unattributed within the ceiling render blank (acceptable first cut).
const LIST_MAX_WALK = 1 << 16;

//  First-line commit summary (log.js firstLine twin): skip a leading CR/LF run,
//  take up to the next CR/LF.  `body` is the raw commit-object body string.
function summaryOf(body) {
  if (!body) return "";
  let i = 0;
  while (i < body.length && (body[i] === "\n" || body[i] === "\r")) i++;
  let j = i;
  while (j < body.length && body[j] !== "\n" && body[j] !== "\r") j++;
  return body.slice(i, j);
}

//  The mainline first parent (log.js mainlineParent twin): argmax(commitTs) —
//  the newest parent — so the diff is against the github-like first-parent line.
function mainlineParent(k, parents) {
  if (!parents || !parents.length) return undefined;
  if (parents.length === 1) return isFullSha(parents[0]) ? parents[0] : undefined;
  let best, bestTs = -1n;
  for (const p of parents) {
    if (!isFullSha(p)) continue;
    const ts = dag.commitTs(k, p);
    if (best === undefined || ts > bestTs) { best = p; bestTs = ts; }
  }
  return best;
}

//  The IMMEDIATE scope entry a changed leaf path belongs to: strip `scopePfx`
//  (dir form "" | "sub/"), then the FIRST segment is the entry name.  A path not
//  under the scope → null.  A leaf directly at the scope is that file's name; a
//  leaf deeper down attributes the containing immediate DIR.
function entryOf(scopePfx, leafPath) {
  if (scopePfx && leafPath.indexOf(scopePfx) !== 0) return null;
  const rel = leafPath.slice(scopePfx.length);
  if (!rel) return null;
  const slash = rel.indexOf("/");
  return slash < 0 ? rel : rel.slice(0, slash);
}

//  --- the cached path (BRO-044) -------------------------------------------
//  Descend `scopePfx` ("" | "sub/" | "a/b/") from the tip's ROOT tree to the
//  tree object the listing scope names; undefined when the scope is not a
//  committed directory (a brand-new dir in the wt, say).
function scopeTree(k, rootTree, scopePfx) {
  let sha = rootTree;
  for (const seg of String(scopePfx || "").split("/")) {
    if (!seg) continue;
    if (!isFullSha(sha)) return undefined;
    let ents;
    try { ents = k.readTree(sha); } catch (e) { ents = undefined; }
    if (!ents) return undefined;
    let next;
    for (const e of ents) if (e.name === seg && e.mode === 0o40000) { next = e.sha; break; }
    if (!next) return undefined;
    sha = next;
  }
  return isFullSha(sha) ? sha : undefined;
}

//  Each wanted entry name -> its lane row key, taken from the scope tree.  A
//  name the tip does not carry (untracked) has no object and so no row — it
//  renders blank, exactly as an unattributed row does.
function scopeKeys(k, tree, entries) {
  const out = {};
  let ents;
  try { ents = k.readTree(tree); } catch (e) { ents = undefined; }
  if (!ents) return out;
  const want = {};
  for (const n of entries) want[n] = 1;
  for (const e of ents) {
    if (!want[e.name] || !isFullSha(e.sha)) continue;
    out[e.name] = mtimeidx.objKey(e.sha, mtimeidx.typeOfMode(e.mode));
  }
  return out;
}

//  The lane holds the TIME, never the message.  Resolve the few commits those
//  times name: walk the mainline newest-first parsing commits ONLY, and open a
//  changed-objects diff at a commit ONLY when a visible row carries its exact
//  time — so the tree work is O(entries on screen), not O(history).  A time
//  shared by two commits is disambiguated by that diff (the object must really
//  be introduced there), never by the clock alone.
function summarize(k, tip, byName, tsOf, ceil) {
  const out = {};
  const wantTs = new Set();
  let remaining = 0;
  for (const name in byName) { wantTs.add(tsOf[name]); remaining++; }
  if (!remaining) return out;

  let sha = tip;
  for (let n = 0; n < ceil && remaining > 0; n++) {
    if (!isFullSha(sha)) break;
    const pc = k.parseCommit(sha);
    if (!pc) break;
    const parents = k.commitParents(sha) || [];
    const parent = mainlineParent(k, parents);
    const ts = dag.commitTs(k, sha);
    if (wantTs.has(ts)) {
      const here = new Set();
      changedpaths.changedObjectsCommits(k, parent || "", k, sha, function (o) {
        if (o.sha && isFullSha(o.sha)) here.add(mtimeidx.objKey(o.sha, o.type));
      });
      const summary = summaryOf(pc.body || "");
      for (const name in byName) {
        if (out[name] || tsOf[name] !== ts || !here.has(byName[name])) continue;
        out[name] = { summary: summary, ts: ts, sha: sha };
        remaining--;
      }
    }
    if (!parent) break;
    sha = parent;
  }
  return out;
}

//  BRO-044: attribution through the lane.  Returns the same name → { summary,
//  ts, sha } map the raw walk does, or null when the lane cannot serve this
//  scope (no committed scope tree) and the caller should fall back.
function cachedCommits(k, tip, scopePfx, entries) {
  const rootTree = k.commitTree(tip);
  if (!rootTree) return null;                  // no tip tree: leave it to the walk
  //  A scope the tip does not carry holds NOTHING committed, so no entry under
  //  it can be attributed — answer blank without touching history.  That is the
  //  case the raw walk pays a whole history for and still comes back empty.
  const tree = scopeTree(k, rootTree, scopePfx);
  if (!tree) return {};
  const byName = scopeKeys(k, tree, entries);

  const keys = new Set();
  for (const name in byName) keys.add(byName[name]);
  if (!keys.size) return {};

  const ix = mtimeidx.openIndex(k.shard);
  const tsOf = {};
  try {
    let missing = false;
    for (const name in byName) {
      const v = ix.get(byName[name]);
      if (v === undefined || v === null) { missing = true; continue; }
      tsOf[name] = v;
    }
    if (missing) {
      const f = mtimeidx.fill(ix, k, tip, keys, { parentOf: mainlineParent });
      for (const name in byName) {
        if (tsOf[name] !== undefined) continue;
        const v = f.rows.get(byName[name]);
        if (v !== undefined) tsOf[name] = v;
      }
    }
  } finally { try { ix.close(); } catch (e) {} }

  const hit = {};
  for (const name in byName) if (tsOf[name] !== undefined) hit[name] = byName[name];
  return summarize(k, tip, hit, tsOf, LIST_MAX_WALK);
}

//  LIST-001: attribute each name in `entries` (immediate file/dir names, RELATIVE
//  to `scopePfx`) its last-touch commit, walking from `tip`.  Returns a plain map
//  name → { summary, ts, sha }; unattributed names are simply absent (blank age).
//  `cap` overrides the ceiling (tests) AND selects the RAW walk: the ceiling is
//  the LIST-001 repro knob, so a capped call must exercise the walk itself, not
//  a lane that a previous call filled.  Uncapped calls ride the lane whenever
//  the store has a shard to hold it (shared/mtimeidx.js).
function lastCommits(k, tip, scopePfx, entries, cap) {
  if (!(cap && cap > 0) && k && mtimeidx.hasShard(k.shard)) {
    const cached = cachedCommits(k, tip, scopePfx, entries);
    if (cached) return cached;
  }
  return walkCommits(k, tip, scopePfx, entries, cap);
}

//  LIST-001: the RAW bounded walk (see the header) — the fallback and the
//  ceiling repro.
function walkCommits(k, tip, scopePfx, entries, cap) {
  const want = {};                       // name → 1, entries still unattributed
  for (const n of entries) want[n] = 1;
  let remaining = entries.length;
  const out = {};
  const ceil = cap && cap > 0 ? cap : LIST_MAX_WALK;

  let sha = tip;
  for (let n = 0; n < ceil && remaining > 0; n++) {
    if (!isFullSha(sha)) break;
    const pc = k.parseCommit(sha);
    if (!pc) break;                      // missing/non-commit → walk breaks clean
    const parents = k.commitParents(sha) || [];
    const parent = mainlineParent(k, parents);
    //  Changed leaves of THIS commit vs its mainline parent (a root commit
    //  diffs vs the empty tree → every leaf it introduces).
    const changed = changedpaths.changedCommits(k, parent || "", k, sha);
    if (changed.length) {
      const summary = summaryOf(pc.body || "");
      const ts = dag.commitTs(k, sha);
      for (const leaf of changed) {
        const name = entryOf(scopePfx, leaf);
        if (name == null || !want[name]) continue;   // out of scope / already done
        out[name] = { summary: summary, ts: ts, sha: sha };
        delete want[name]; remaining--;
        if (remaining === 0) break;
      }
    }
    if (!parent) break;                  // root commit → stop
    sha = parent;
  }
  return out;
}

module.exports = { lastCommits: lastCommits, walkCommits: walkCommits,
                   cachedCommits: cachedCommits, summaryOf: summaryOf,
                   mainlineParent: mainlineParent, entryOf: entryOf,
                   scopeTree: scopeTree, scopeKeys: scopeKeys,
                   LIST_MAX_WALK: LIST_MAX_WALK };
