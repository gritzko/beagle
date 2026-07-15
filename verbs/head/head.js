//  head.js — `be head` as a loop HANDLER (GIT-016 T4).  HEAD is the READ-ONLY
//  peek ([HEAD.mkd]): it reports what a sync WOULD change — the ahead/behind
//  commit lists of local cur vs a remote/branch tip — WITHOUT touching history,
//  the worktree, OR the pack log.  It is GET's non-persisting twin: the fetched
//  connecting commits live ONLY in an in-memory wh128 commit->parent DAG (the T2
//  overlay), NEVER a `.keeper` pack-log file (that no-persist is the whole point
//  of T4; the pack log is GET/T5's job).  Pure JS over the shared spine.
//
//  FORMS:
//    be head                       BARE (≡ bare `be`): the STATUS check — cur vs
//                                  its parent/trunk (or cached remote) — no net,
//                                  no writes.  REUSES views/status/status.js.
//    be head ?branch               LOCAL: cur vs a LOCAL branch tip — ahead/behind
//                                  + changed FILE paths, all objects local (no net).
//    be head ssh://origin?branch   FETCH: advert -> resolve -> fetch the pack in
//                                  memory -> in-memory remote DAG -> verdict ->
//                                  report ahead/behind + changed paths -> update
//                                  remote-track ref.  No pack-log write.
//  BE-033: the scheme-less `//origin?branch` CACHED form is DROPPED — a `//X`
//  is ALWAYS a worktree (nav scopes or NAVNONEs it); remotes carry a scheme.
//  GIT-016: the changed-PATHS diff (T4-deferred) now lands via the shared
//  shared/changedpaths.js helper — cur's tree vs the tip's tree, listed per form.

"use strict";

const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const wire     = require("../../shared/wire.js");
const relate   = require("../../shared/relate.js");
const ingest   = require("../../shared/ingest.js");
const dag      = require("../../shared/dag.js");
const changed  = require("../../shared/changedpaths.js");   // GIT-016: paths diff
const status   = require("../../views/status/status.js");   // GIT-016: bare = status
const uriarg   = require("../../shared/uri.js");             // URI-015: scp → ssh://
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
//  JAB-004: head's arg is a whole REMOTE URI (like get) — a READ/query verb, so
//  it SELF-PARSES its URI (cat-style, NOT classifyArg) and reads be.repo (may be
//  null, repo-less) / be.sink off the global.  head peeks ONE target at a time;
//  the plain fn loops its args, one peek each.
function head() {
  //  Bare `be head` (zero args) is the STATUS check — one peek with no target.
  if (arguments.length === 0) return headOne("", null);
  for (let i = 0; i < arguments.length; i++) headOne(arguments[i], null);
}

//  JAB-004: peek ONE target — self-parse the remote-URI arg, read be.repo/be.sink
//  (fallback ctx for the legacy direct-handler test), then advertise→resolve→
//  verdict→report exactly as the legacy handler did.  A `head:` scheme prefix is
//  shed first (cat-style); NEVER routes through resolve.classifyArg / seed.
function headOne(arg, ctx) {
  const _be = (typeof be !== "undefined") ? be : null;
  const repo = (_be && _be.repo) || (ctx && ctx.repo) || null;

  let raw = String(arg || "");
  if (raw.indexOf("head:") === 0) raw = raw.slice(5);   // JAB-004: shed own scheme
  //  URI-015: git scp-form remote (`git@host:path`) recomposes to ssh:// here.
  const uri = (raw === ".") ? "" : uriarg.fromGit(raw); // loop's "." placeholder → bare
  const u = new URI(uri);
  const hasScheme = u.scheme !== undefined;
  const hasAuth   = u.authority !== undefined;
  const branch    = u.query || "";

  //  GIT-016: bare `be head` ≡ bare `be` — the local STATUS check (cur vs its
  //  parent/trunk, or its cached remote when cur IS the trunk).  No net, no
  //  writes: DELEGATE to the status view (do NOT reinvent status).  Plain path
  //  reads be off the global.
  if (!hasScheme && !hasAuth && !branch)
    return status();

  //  JAB-004: repo-less guard — head may run with be.repo=null (a fresh clone
  //  dir has no cur to compare); refuse cleanly instead of dereferencing null.
  const info = repo || be.treeAt(be.cwd());   // URI-011: context wt root, not raw cwd
  const k = store.open(info.storePath, info.project);
  const cur = wtlog.open(info).curTip();
  const curSha = (cur && cur.sha && isFullSha(cur.sha)) ? cur.sha : "";
  if (!curSha)
    throw "HEADNONE: no cur tip to compare (commit first, then `be head ssh://origin`)";

  //  BE-033: a scheme-less //authority is ALWAYS a worktree — nav scopes it or
  //  NAVNONEs upstream; head never reads it as a cached remote.
  if (!hasScheme && hasAuth)
    throw "NAVNONE: no worktree //" + (u.host || "");
  //  Dispatch by transport: any scheme is a wire fetch; a bare in-repo `?branch`
  //  is the LOCAL peek — cur vs a local branch tip, all objects local (no net).
  const res = hasScheme       ? peekFetch(k, uri, branch, curSha)
            : /* local ?br */   peekLocal(k, branch, curSha);
  report(ctx, uri || URI.make(undefined, undefined, undefined, branch, undefined),
         branch, res.rel, res.ahead, res.behind, res.tip, res.paths);
}
head.jab = "args";
module.exports = head;

//  GIT-016 LOCAL `?branch`: cur vs a LOCAL branch tip — resolve `?branch` to its
//  local ref sha (store.resolveRef), a pull-side verdict with NO remote index
//  (every connecting commit + tree is already local), and the changed FILE paths
//  (cur's tree vs the branch tip's tree, both read from the keeper).  No net.
function peekLocal(k, branch, curSha) {
  const tip = k.resolveRef(branch || "");
  if (!tip || !isFullSha(tip))
    throw "HEADREF: no local branch `?" + (branch || "") + "` to diff against";
  const v = relate.verdict(k, curSha, tip);
  const paths = changed.changedCommits(k, curSha, k, tip);
  return { rel: v.rel, ahead: v.ahead, behind: v.behind, tip: tip, paths: paths };
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
  //  GIT-016: changed paths — cur's tree from the keeper, the tip's tree from a
  //  TRANSIENT in-memory pack reader over the fetched bytes (never persisted).
  let paths = [];
  try { paths = changed.changedCommits(k, curSha, changed.packReader(f.pack), tip); }
  catch (e) { paths = []; }        // a truncated/thin fetch → skip paths, keep verdict
  //  Update the remote-tracking ref ONLY (reflog), never the pack log — this is
  //  the canonical cache refresh HEAD.mkd promises.
  ingest.saveRemoteRef(k.shard, uri, tip);
  return { rel: v.rel, ahead: v.ahead, behind: v.behind, tip: tip, paths: paths };
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

//  --- report ------------------------------------------------------------
//  HEAD is report-only: a `head:` banner naming the relationship + one row per
//  ahead / behind commit (newest-first, the aheadBehind order).  ahead rows are
//  local (`post`, they'd be sent); behind rows are remote (`miss`, they'd be
//  pulled).  An `eq` peek reports just the banner (nothing to sync).
//  GIT-016: the changed-PATHS diff (shared/changedpaths.js — cur's tree vs the
//  tip's tree) follows, one `chg` row per differing FILE path (lex order).
function report(ctx, uri, branch, rel, ahead, behind, tip, paths) {
  //  JAB-004: emit sink off global `be` (plain path), falling back to ctx (legacy).
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = (_be && _be.sink) || (ctx && ctx.sink) || null;
  if (!sink) return;
  //  Header row: the target ref (any `?ref`/`#pin` slot the uri already carries
  //  is shed) + the relation verb; the ahead/behind commit rows follow.
  //  URI-013 A5/B4: parse the uri and RE-COMPOSE via the URI class instead of a
  //  raw indexOf("?")/slice — the scheme/authority/path stay, the query is
  //  replaced by `?<branch>` and the fragment by `#<hashlet>` (byte-identical to
  //  the old `uri.slice(0,"?") + "?"+branch+"#"+tip` for every head URI form:
  //  slots feed verbatim, and a real head URI carries a `?query` whenever it has
  //  a `#pin`).
  const u = new URI(uri);
  const target = URI.make(u.scheme, u.authority, u.path,
                          branch || "", tip ? tip.slice(0, 8) : "");
  //  DIS-060: the banner carries the target ADDRESSING uri (`<remote>?<br>#<tip>`)
  //  directly — NEVER a phantom `head:` scheme ([Nav]).
  const out = hunkrows(sink, target);
  out.row(target, relVerb(rel), 0n);
  for (const c of ahead)
    out.row(URI.make(undefined, undefined, undefined, c.hashlet || "", c.subject ? c.subject : undefined),
            "post", c.ts);
  for (const c of behind)
    out.row(URI.make(undefined, undefined, undefined, c.hashlet || "", c.subject ? c.subject : undefined),
            "miss", c.ts);
  //  Changed FILE paths (cur's tree vs the tip's tree) — a `chg` row each.
  for (const p of (paths || [])) out.row(p, "chg", 0n);
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
