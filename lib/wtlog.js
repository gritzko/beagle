//  wtlog.js — wtlog ULOG reader (JS-029).  Pure JS over the JABC ULOG
//  binding (abc.mmap("ULOG", …) drain cursor) + the URI binding + the
//  ron60 time codec + an abc.index for the ts stamp-set.  No C, no dog.
//  Mirrors sniff/AT.c (SNIFFAt* tip/baseline/boundary/scan helpers).
//
//  open(be) → reader where `be` is the object be.find() returns (only
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
//    eachPutDelete(floorTs, cb)             put/del rows with ts > floor, oldest-first
//
//  A "tip" is the latest get/post row carrying a sha; the sha sits in the
//  URI FRAGMENT (`?<branch>#<sha>`, the canonical post/get shape) OR, for
//  legacy rows, in the QUERY `&`-chain (`?<branch>&<sha>`).  The branch is
//  the query's first non-sha chunk (LOCAL rows only — empty authority).

"use strict";

const shalib = require("./sha.js");
const ulog = require("./ulog.js");
const isFullSha = shalib.isFullSha;

const GET = "get", POST = "post", PATCH = "patch",
      PUT = "put", DEL = "delete", REPO = "repo";

//  DOGQueryStripProject: `?/<project>/<branch>` → `<branch>`;
//  `?/<project>` → ""; `?<branch>` (no leading /) → unchanged.
function stripProject(query) {
  if (!query || query[0] !== "/") return query || "";
  const j = query.indexOf("/", 1);
  return j < 0 ? "" : query.slice(j + 1);
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
  //  Drain once into a plain JS row list (rule #4: no held native cursor).
  //  ts is kept as a BigInt; ron is the base64 stamp string.  `local` marks
  //  a host-less (authority-empty) row.
  const rows = [];
  ulog.each(be.bePath, function (log) {
    const u = new URI(log.uri);
    rows.push({ ts: log.time, ron: ron.encode(log.time), verb: log.verb,
                uri: u, local: (u.authority === "" || u.authority == null) });
  });

  //  Tip resolution: newest→oldest, only get/post (+patch for baseline)
  //  rows with a sha or a branch matter.  Returns { branch, sha, ts,
  //  query } where `query` is the matched tip row's RAW (project-stripped)
  //  URI query — the label native's status summary uses verbatim
  //  (`bu.query`): a named branch (`master`), a detached full sha
  //  (`<40hex>`, a `be get ?<sha>`), or empty (trunk).  `branch` is the
  //  PARSED branch (sha chunks dropped) — empty for a detached tip.
  function tip(withPatch) {
    let sha = "", ts = null, shaLocal = true, shaIdx = -1, branch = "",
        query = "";
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
                     query = stripProject(r.uri.query) || ""; break; }
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
    return { branch: branch, sha: sha, ts: ts, query: query };
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
