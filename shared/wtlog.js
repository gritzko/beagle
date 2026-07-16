//  wtlog.js — wtlog ULOG reader (JS-029).  Pure JS over the JABC ULOG
//  binding (abc.mmap("ULOG", …) drain cursor) + the URI binding + the
//  ron60 time codec + an abc.index for the ts stamp-set.  No C, no dog.
//  Mirrors sniff/AT.c (SNIFFAt* tip/baseline/boundary/scan helpers).
//
//  open(be) → reader where `be` is the object be.treeAt() returns (only
//  `.bePath` is used).  The reader drains every row ONCE into a JS array
//  (rows[]) — the ULOG family has no index and drains sequentially — and
//  exposes:
//    anchor()       → { ts, verb, uri:URI } | undefined   (row 0)
//    repo()         → row-0 URI string (the store anchor) | undefined
//    curTip()       → { branch, sha, ts }   latest get/post sha-tip
//    baselineTip()  → { branch, sha, ts }   latest get/post/patch sha-row
//    boundaries()   → { pd, patch }         pd = latest get/post ts;
//                                           patch = latest get / commit-all post ts
//    has(ts)        → bool                  ron60 stamp-set membership
//    patchTheirs()  → ["<40hex>", …]        in-scope patch rows' theirs shas
//    eachPutDelete(floorTs, cb)             put/del rows with ts > floor, oldest-first
//
//  A "tip" is the latest get/post row carrying a sha; the sha sits in the
//  URI FRAGMENT (`?<branch>#<sha>`, the canonical post/get shape) OR, for
//  legacy rows, in the QUERY `&`-chain (`?<branch>&<sha>`).  The branch is
//  the query's first non-sha chunk (LOCAL rows only — empty authority).

"use strict";

const shalib = require("./util/sha.js");   // JSQUE-016: sha.js -> shared/util/
const ulog = require("./ulog.js");
const branchlib = require("./branch.js");  // SUBS-050: the ONE branch codec
const isFullSha = shalib.isFullSha;

const GET = "get", POST = "post", PATCH = "patch",
      PUT = "put", DEL = "delete", REPO = "repo", CON = "con";   // STATUS-005: con

//  SUBS-050: title-strip a recorded branch query to the refs/divergence KEY —
//  the DOGQueryStripProject twin, now routed through the ONE branch codec (no
//  hand-rolled indexOf/slice).  `?/<project>/<branch>` → `<branch>`,
//  `?/<project>` → "", `?<branch>` (no leading /) → unchanged.
function stripProject(query) {
  return branchlib.key(branchlib.parse(query || "", ""));
}

//  DOGRefDrain over the `&`-chain: return { branch, sha } for one row's
//  query (project already stripped) + fragment.  Branch = first non-sha
//  chunk; sha = fragment if a full sha, else the first sha chunk in the
//  query.  `local` gates branch adoption (remote-fetch rows keep cur).
function refOf(u, local) {
  let branch = "", sha = "";
  if (isFullSha(u.fragment)) sha = u.fragment;
  const q = stripProject(u.query);
  if (q) for (const chunk of q.split("&")) {
    if (!chunk) continue;
    if (isFullSha(chunk)) { if (!sha) sha = chunk; }
    else if (!branch && local) branch = chunk;
  }
  return { branch: branch, sha: sha };
}

function open(be) {
  //  SUBS-050: the shard [Title] the caller knows (repo.project) — re-heads a
  //  title-stripped relative-dotted row when parsing it back to a Branch.
  const title = (be && be.project) || "";
  //  Drain once into a plain JS row list (rule #4: no held native cursor).
  //  ts is kept as a BigInt; ron is the base64 stamp string.  `local` marks
  //  a host-less (authority-empty) row.
  const rows = [];
  ulog.each(be.bePath, function (log) {
    const u = new URI(log.uri);
    rows.push({ ts: log.time, ron: ron.encode(log.time), verb: log.verb,
                uri: u, local: (u.authority === "" || u.authority == null) });
  });

  //  SUBS-050: a recorded branch query → its parsed Branch.  An ABSOLUTE row
  //  (`/<title>/…`) carries its own title head; any other shape re-heads with
  //  the shard title, using the already sha-stripped `strippedBranch` (drops a
  //  legacy `&<sha>` tail / a detached bare sha → trunk).
  function parseBranch(rawQuery, strippedBranch) {
    const raw = rawQuery || "";
    const brStr = (raw[0] === "/") ? raw : (strippedBranch || "");
    return branchlib.parse(brStr, title);
  }

  //  Tip resolution: newest→oldest, only get/post (+patch for baseline)
  //  rows with a sha or a branch matter.  Returns { branch, sha, ts,
  //  query } where `query` is the matched tip row's RAW (project-stripped)
  //  URI query — the label native's status summary uses verbatim
  //  (`bu.query`): a named branch (`master`), a detached full sha
  //  (`<40hex>`, a `be get ?<sha>`), or empty (trunk).  `branch` is the
  //  PARSED branch (sha chunks dropped) — empty for a detached tip.
  function tip(withPatch) {
    let sha = "", ts = null, shaLocal = true, shaIdx = -1, branch = "",
        query = "", rawQuery = "";
    for (let i = rows.length - 1; i >= 0; i--) {
      const r = rows[i];
      const isGP = (r.verb === GET || r.verb === POST);
      const ok = withPatch ? (isGP || r.verb === PATCH) : isGP;
      if (!ok) continue;
      const ref = refOf(r.uri, r.local);
      //  Skip a bare/tip-less store anchor (no query AND no fragment):
      //  matches SNIFFAtBaseline/CurTip's empty-q-and-f skip.
      if (!ref.sha && !r.uri.query && !r.uri.fragment) continue;
      if (ref.sha) { sha = ref.sha; ts = r.ts; shaLocal = r.local;
                     shaIdx = i; branch = ref.branch;
                     query = stripProject(r.uri.query) || "";
                     //  JAB-004: the matched tip row's VERBATIM (un-stripped)
                     //  query — the label native's status summary prints
                     //  (`bu.query`).  For a secondary-wt sub anchor
                     //  (`?/<project>[/<branch>]`) native keeps the WHOLE
                     //  `/project/branch` raw (NOT project-stripped); a primary
                     //  wt's query is empty either way.  `query` (stripped)
                     //  stays the branch-resolution key (divergence/refOf).
                     rawQuery = r.uri.query || ""; break; }
    }
    //  A sha-row that was a remote fetch (non-local) doesn't move cur's
    //  branch — walk further back for the latest LOCAL get/post branch.
    if (sha && !shaLocal && !branch) {
      for (let i = shaIdx - 1; i >= 0; i--) {
        const r = rows[i];
        if (r.verb !== GET && r.verb !== POST) continue;
        if (!r.local) continue;
        const q = stripProject(r.uri.query);
        if (q) for (const chunk of q.split("&")) {
          if (chunk && !isFullSha(chunk)) { branch = chunk; break; }
        }
        if (branch) break;
      }
    }
    //  SUBS-050: the parsed Branch (title comes from the shard) alongside the
    //  raw fields existing callers still read during the conversion.
    return { branch: branch, sha: sha, ts: ts, query: query,
             rawQuery: rawQuery, br: parseBranch(rawQuery, branch) };
  }

  //  attachedBranch — the SINGLE source of truth for "what branch is this wt
  //  on" (DIS-057).  A wt is attached per the RECENTMOST `get` record (NOT
  //  get/post/patch): `?master`, `?` (trunk), `?branch#sha`, `?#sha` are all
  //  ATTACHED; a bare-hash record `#<sha>` (DIS-075) or a legacy `?<sha>` is
  //  DETACHED.  A legacy detached POST wrote trunk-shaped `?#<sha>` — from the
  //  record alone that is UNRESOLVABLE, so attachment stays anchored on the
  //  recentmost GET record (DIS-059), never a post row.
  //  Returns { branch, detached, rawQuery, sha }.  status's label,
  //  post's detach guard + curBranch, and divergence all route through THIS so
  //  they cannot disagree (status said trunk while post said detached).
  function attachedBranch() {
    for (let i = rows.length - 1; i >= 0; i--) {
      const r = rows[i];
      if (r.verb !== GET) continue;
      const ref = refOf(r.uri, r.local);
      if (!ref.sha && !ref.branch) continue;     // store/project anchor pins nothing
      const q = stripProject(r.uri.query) || "";
      //  DIS-075: the canonical detach record is `#<sha>` — LOCAL row, query slot
      //  ABSENT, sha in the fragment; legacy `?<sha>` (sha in the query) reads on.
      const detached = !ref.branch &&
            (q.split("&").some(function (c) { return isFullSha(c); }) ||
             (r.local && r.uri.query === undefined && isFullSha(r.uri.fragment)));
      return { branch: ref.branch || "", detached: detached,
               rawQuery: r.uri.query || "", sha: ref.sha || "",
               br: parseBranch(r.uri.query, ref.branch) };
    }
    return { branch: "", detached: false, rawQuery: "", sha: "",
             br: branchlib.parse("", title) };
  }

  //  A `post` at index idx is commit-all iff no put/delete lies between
  //  its pd boundary (most recent get/post strictly before it) and itself.
  function postIsCommitAll(idx) {
    for (let j = idx - 1; j >= 0; j--) {
      const v = rows[j].verb;
      if (v === PUT || v === DEL) return false;        // selective
      if (v === GET || v === POST) return true;        // pd boundary, none seen
    }
    return true;                                       // nothing before → commit-all
  }

  return {
    rows: rows,

    anchor: function () {
      if (!rows.length) return undefined;
      const r = rows[0];
      if (r.verb !== GET && r.verb !== REPO) return undefined;
      return { ts: r.ts, verb: r.verb, uri: r.uri };
    },

    repo: function () {
      const a = this.anchor();
      return a ? a.uri.href : undefined;
    },

    //  Cur tip: latest get/post sha-row (skips patch).  SNIFFAtCurTip.
    curTip: function () { return tip(false); },

    //  Baseline tip: latest get/post/patch sha-row.  SNIFFAtBaseline.
    baselineTip: function () { return tip(true); },

    //  attachedBranch: the ONE attach/detach reader (recentmost GET record,
    //  DIS-057) — status label, post detach-guard + curBranch, divergence.
    attachedBranch: attachedBranch,

    //  pd boundary  = ts of the latest get/post row (SNIFFAtLastPostTs:
    //                 the floor for "put/delete since last post").
    //  patch boundary = ts of the latest get OR commit-all post row
    //                 (SNIFFAtPatchFloorTs).  null when none.
    boundaries: function () {
      let pd = null, patch = null;
      for (let i = rows.length - 1; i >= 0; i--) {
        const r = rows[i];
        if (r.verb !== GET && r.verb !== POST) continue;
        if (pd === null) pd = r.ts;
        if (patch === null) {
          if (r.verb === GET || postIsCommitAll(i)) { patch = r.ts; break; }
        }
      }
      return { pd: pd, patch: patch };
    },

    //  has(ts): ron60 stamp-set membership.  v1 = a JS Set of the row
    //  stamps (ron base64 strings) built from the drained rows; ts may be
    //  a BigInt, a Number, or a ron base64 string.  (JS-034 notes a
    //  libdog ULOG-index leaf if random access ever gets hot.)
    has: function (ts) {
      if (!this._set) {
        const s = new Set();
        for (const r of rows) s.add(r.ron);
        this._set = s;
      }
      const key = (typeof ts === "string") ? ts : ron.encode(BigInt(ts));
      return this._set.has(key);
    },

    //  patchFloor(): the patch-scope floor ts — the latest get / commit-all
    //  post ts (boundaries().patch).  A `patch` row with ts strictly greater is
    //  in scope (its theirs tree is un-posted).  null when no floor (DIS-057).
    patchFloor: function () { return this.boundaries().patch; },

    //  patchTheirs(): the THEIRS commit shas of every IN-SCOPE patch row, oldest
    //  -first (DIS-057 RULING 2026-06-29).  A patch row's URI pins the absorbed
    //  (theirs) commit in the fragment (`#<sha>`, NAMED) OR the query (`?<sha>`/
    //  `?<sha>!`, NEXT/WHOLE) — refOf() reads either form back to a 40-hex sha.
    //  These trees are the classifier's SEPARATE 4th input (the patched-in
    //  trees), NEVER folded into the OURS baseline (which stays curTip).  Empty
    //  when no patch row is in scope, so the whole axis is a no-op otherwise.
    patchTheirs: function () {
      const floor = this.patchFloor();
      const out = [];
      for (const r of rows) {
        if (r.verb !== PATCH) continue;
        if (floor != null && r.ts <= floor) continue;
        const ref = refOf(r.uri, r.local);
        if (ref.sha && isFullSha(ref.sha)) out.push(ref.sha);
      }
      return out;
    },

    //  STATUS-005: paths named by durable `con <path>` rows (a merge left
    //  markers there); append-only — status re-scans wt bytes for liveness.
    conPaths: function () {
      const s = new Set();
      for (const r of rows) {
        if (r.verb !== CON) continue;
        const p = (r.uri && r.uri.path) || "";
        if (p) s.add(p);
      }
      return s;
    },

    //  eachPutDelete(floorTs, cb): every put/delete row with ts strictly
    //  greater than floorTs, oldest-first (SNIFFAtScanPutDelete).  floorTs
    //  may be a BigInt, Number, or ron base64 string; cb gets the row.
    eachPutDelete: function (floorTs, cb) {
      let floor = 0n;
      if (floorTs != null) {
        floor = (typeof floorTs === "string") ? ron.decode(floorTs)
                                              : BigInt(floorTs);
      }
      for (const r of rows) {
        if (r.ts <= floor) continue;
        if (r.verb !== PUT && r.verb !== DEL) continue;
        cb(r);
      }
    }
  };
}

module.exports = { open: open, isFullSha: isFullSha, refOf: refOf,
                   stripProject: stripProject };
