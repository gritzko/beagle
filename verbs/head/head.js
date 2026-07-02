//  head.js — `be head` as a loop HANDLER (GIT-016 T4).  HEAD is the READ-ONLY
//  peek ([HEAD.mkd]): it reports what a sync WOULD change — the ahead/behind
//  commit lists of local cur vs a remote/branch tip — WITHOUT touching history,
//  the worktree, OR the pack log.  It is GET's non-persisting twin: the fetched
//  connecting commits live ONLY in an in-memory wh128 commit->parent DAG (the T2
//  overlay), NEVER a `.keeper` pack-log file (that no-persist is the whole point
//  of T4; the pack log is GET/T5's job).  Pure JS over the shared spine.
//
//  FORMS (GIT-016 T4 primary):
//    be head ssh://origin?branch   FETCH: advert -> resolve -> fetch the pack in
//                                  memory -> in-memory remote DAG -> verdict ->
//                                  report ahead/behind -> update remote-track ref.
//    be head //origin?branch       CACHED: no network — read the remote-tracking
//                                  tip (store.eachRemote) and report vs cur.
//  DEFERRED (say so): bare `be head` (cur vs trunk) and local `?br` diffs — the
//  network peek is T4's contract; the in-repo status peek is a follow-up.

"use strict";

const be       = require("../../core/discover.js");
const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const wire     = require("../../shared/wire.js");
const relate   = require("../../shared/relate.js");
const ingest   = require("../../shared/ingest.js");
const dag      = require("../../shared/dag.js");
const shalib   = require("../../shared/util/sha.js");
const hunkrows = require("../../shared/hunkrows.js");
const isFullSha = shalib.isFullSha;
const hashlet60FromBytes = shalib.hashlet60FromBytes;
const hexDecode = hex.decode;

//  GIT-016: wh128 commit type + key rule (WHIFFKeyPack(T_COMMIT, hashlet60)) —
//  the SAME low-nibble type + hashlet key store.js/ingest.js/dag.js use.
const T_COMMIT = 1;
function keyFor(h60) { return (h60 << 4n) | BigInt(T_COMMIT); }
function h60(sha) { return hashlet60FromBytes(hexDecode(sha)); }

//  --- the handler --------------------------------------------------------
//  head's arg is a whole REMOTE URI (like get) — it rides the row verbatim; the
//  handler resolves the remote at entry.  A repo MUST exist (cur is the compare
//  baseline); a fresh clone-target has no cur, so head refuses cleanly there.
module.exports = function handle(row, ctx) {
  const uri = (row && row.uri) || "";
  const info = (ctx && ctx.repo) || be.find(io.cwd());
  const k = store.open(info.storePath, info.project);
  const cur = wtlog.open(info).curTip();
  const curSha = (cur && cur.sha && isFullSha(cur.sha)) ? cur.sha : "";
  if (!curSha)
    throw "HEADNONE: no cur tip to compare (commit first, then `be head //origin`)";

  const u = new URI(uri);
  const hasScheme = u.scheme !== undefined;
  const hasAuth   = u.authority !== undefined;
  //  A `//host` (authority, NO scheme) is the CACHED read; any scheme is a wire
  //  fetch.  A bare in-repo `?br` (no host/scheme) is DEFERRED (see header).
  if (!hasScheme && !hasAuth)
    throw "HEADLOCAL: local `be head` / `?br` peek is deferred — use a transport " +
          "scheme (`//origin` cached, `ssh://origin` fetch)";
  const cached = !hasScheme && hasAuth;
  const branch = u.query || "";

  const res = cached ? peekCached(k, u, branch, curSha)
                     : peekFetch(k, uri, branch, curSha);
  report(ctx, uri, branch, res.rel, res.ahead, res.behind, res.tip);
};

//  CACHED `//origin?branch`: the remote-tracking tip from store.eachRemote (no
//  wire).  cur vs that tip is a LOCAL-object verdict — the connecting commits
//  are already in the store (a prior get/head/push fetched them), so NO remote
//  index is needed.  Missing cache -> HEADCACHE (fetch with ssh:/be: first).
function peekCached(k, u, branch, curSha) {
  let tip = "";
  k.eachRemote(function (rt) {
    if (tip) return;
    const h = rt.host || "";
    if (h !== (u.host || "") && h !== (u.authority || "")) return;
    const rq = stripLeadRef(rt.query || "");
    if ((branch || "") === rq) tip = rt.sha;
  });
  if (!tip || !isFullSha(tip))
    throw "HEADCACHE: no cached tip for //" + (u.host || u.authority) +
          (branch ? "?" + branch : "") + " — fetch with ssh:/be: first";
  const v = relate.verdict(k, curSha, tip);
  return { rel: v.rel, ahead: v.ahead, behind: v.behind, tip: tip };
}

//  FETCH `ssh://origin?branch`: advert -> resolve the target ref -> fetch the
//  connecting pack IN MEMORY -> parse its commit objects into an in-memory wh128
//  remote DAG (commit->parent edges) -> pull-side verdict -> UPDATE the remote-
//  tracking ref.  NO pack-log write: the pack bytes are wrapped by git.pack.over
//  (no file) and dropped at return; only the edge index (+ the ref row) persists.
function peekFetch(k, uri, branch, curSha) {
  //  Resolve the target ref name the SAME way the push side does (GIT-015).
  let wireRef;
  try { wireRef = relate.resolveRef(branch); }
  catch (e) { throw (e && e.msg) ? e.msg.replace(/^POST/, "HEAD") : e; }

  //  want = the branch tip.  GIT-016: fetch with NO haves — the same full-branch
  //  fetch get's proven path uses (a `have`-driven thin pack trips wire.fetch's
  //  multi-ACK `ready` scan, which get sidesteps too).  The pack is never
  //  persisted, so a slightly larger in-memory DAG is free; the verdict is exact.
  const f = wire.fetch(uri, branch || "");
  const tip = f.want;
  if (!tip || !isFullSha(tip)) throw "HEADNOTIP: peer advertised no usable ref";

  //  Build the in-memory remote DAG from the fetched pack (no file, no packlog).
  const remoteIx = commitEdges(f.pack);
  const v = relate.verdict(k, curSha, tip, remoteIx);
  //  Update the remote-tracking ref ONLY (reflog), never the pack log — this is
  //  the canonical cache refresh HEAD.mkd promises.
  ingest.saveRemoteRef(k.shard, uri, tip);
  return { rel: v.rel, ahead: v.ahead, behind: v.behind, tip: tip };
}

//  GIT-016: parse the fetched pack's COMMIT records into an in-memory wh128
//  index of commit->parent hashlet edges (the T2 remote-DAG overlay).  Wrap the
//  pack bytes with git.pack.over (in-memory, NO file), walk each record, and for
//  every commit put one edge per parent — REUSING git.parseCommit + the same
//  WHIFFKeyPack key store.js/ingest.js use.  Non-commit records are skipped.
function commitEdges(packBytes) {
  const ix = abc.index("wh128", { mem: 1 << 16 });
  const log = ingest.packLogBytes(packBytes);        // strip the 20-byte trailer
  const pk = git.pack.over(log);
  pk.buffer.watermark = log.byteLength;
  pk.rewind();
  const offsets = [];
  while (pk.next()) offsets.push(pk.offset);
  for (const off of offsets) {
    pk.seek(off);
    if (pk.type !== "commit") continue;              // only commit->parent edges
    let bytes;
    try {
      const out = io.buf((pk.size || 0) * 4 + 256);
      pk.seek(off); pk.resolve(out); bytes = out.data();
    } catch (e) { continue; }
    let pc; try { pc = git.parseCommit(bytes); } catch (e) { continue; }
    const child = store.frameSha("commit", bytes);   // the commit's own sha
    const ch = h60(child);
    for (const p of (pc.parents || []))
      if (isFullSha(p)) ix.put(keyFor(ch), h60(p));   // child -> parent edge
  }
  ix.flush();
  return ix;
}

//  Strip a leading `?`/`/proj/` off a remote-tracking ref query (bare branch).
function stripLeadRef(q) {
  if (q && q[0] === "?") q = q.slice(1);
  if (q && q[0] === "/") { const j = q.indexOf("/", 1); q = j < 0 ? "" : q.slice(j + 1); }
  return q;
}

//  --- report ------------------------------------------------------------
//  HEAD is report-only: a `head:` banner naming the relationship + one row per
//  ahead / behind commit (newest-first, the aheadBehind order).  ahead rows are
//  local (`post`, they'd be sent); behind rows are remote (`miss`, they'd be
//  pulled).  An `eq` peek reports just the banner (nothing to sync).
//  GIT-016: the changed-PATHS diff (fetch remote trees/blobs transiently, diff
//  vs cur's tree) is DEFERRED — the ahead/behind graph core ships solid first.
function report(ctx, uri, branch, rel, ahead, behind, tip) {
  if (!(ctx && ctx.sink)) return;
  //  Header row: the target ref (any `?ref`/`#pin` slot the uri already carries
  //  is shed) + the relation verb; the ahead/behind commit rows follow.
  const q = uri.indexOf("?"), base = q >= 0 ? uri.slice(0, q) : uri;
  const target = base + "?" + (branch || "") + "#" + (tip ? tip.slice(0, 8) : "");
  const out = hunkrows(ctx.sink, "head:" + target);
  out.row(target, relVerb(rel), 0n);
  for (const c of ahead)
    out.row("?" + (c.hashlet || "") + (c.subject ? "#" + c.subject : ""),
            "post", c.ts);
  for (const c of behind)
    out.row("?" + (c.hashlet || "") + (c.subject ? "#" + c.subject : ""),
            "miss", c.ts);
  out.done();
}

//  Map the pull-side relation to a report verb column: eq/ahead/behind reuse
//  the get/post/miss columns; diverged/unrelated get their own honest labels.
function relVerb(rel) {
  if (rel === "eq") return "get";
  if (rel === "ahead") return "post";
  if (rel === "behind") return "miss";
  if (rel === "diverged") return "dvg";
  return "unr";                                       // unrelated
}
