//  get.js — `be get` as a loop HANDLER (JSQUE-009).  Converted from the
//  one-shot `main()` (JS-038/041) to `module.exports = handle(row, ctx)`:
//  the GET SEED resolves the remote ONCE (fetch/redirect + pinned target
//  tip/tree), writes the wtlog anchor, then FANS OUT the checkout as enqueued
//  rows — a per-dir Merkle-pruned reconcile that emits per-file write/merge
//  leaves + per-path delete rows + recursive subdir rows.  Output goes via
//  ctx.out (one flush at the loop edge).  A dirty-overlap PRE-PASS barrier
//  guards the seed; a del-sweep BARRIER folds the delete leaves (core/barrier.js
//  over the live queue ULog).  Pure JS over JABC + ./lib/* (libdog+abc).
//
//  ROW VOCABULARY (all under the `get` verb so this handler owns the tree):
//    get <remote>                  SEED: fetch/redirect, anchor, enqueue root
//    get <dir>/?<newTree>#<oldTree>    dir-reconcile: read both trees one level,
//                                  Merkle-prune (new==old → skip), fan out
//    get <path>?<newBlob>#<oldBlob>    write/merge LEAF: read own bytes, decide
//                                  clean-overwrite vs 3-way weave, write, emit
//    get <path>?#<oldBlob>         delete LEAF: removed entry (in old, not new)
//    get ::del-sweep               del-sweep BARRIER fold (back-scan the dels)
//
//  Usage:  be get <remote>      (one-shot fork still works via loop.cli)
//          jab be/loop.js get <remote>   (JSQUE-009 resident-loop handler)

"use strict";

//  JSQUE-009: sibling libs via relative require ("./lib/X.js"), resolved against
//  this module's own dir — robust under the resident loop (not argv[1]).
//  JSQUE-016: by-verb reorg — shared/ kernel, core/, view/ via ../../ .
const store    = require("../../shared/store.js");
const wire     = require("../../shared/wire.js");
const checkout = require("../../shared/checkout.js");
const relate   = require("../../shared/relate.js");
const ingest   = require("../../shared/ingest.js");
const pathlib  = require("../../shared/util/path.js");
const ulog     = require("../../shared/ulog.js");
const sha      = require("../../shared/util/sha.js");
const barrier  = require("../../core/barrier.js");
const be       = require("../../core/discover.js");
//  JAB-003: get emits a TRUE hunk (accumulated across dispatches, flushed once).
const hunkrows = require("../../shared/hunkrows.js");
const wtlog    = require("../../shared/wtlog.js");
const conflict = require("../../shared/conflict.js");
const submount = require("../../shared/submount.js");   // DIS-058 D2-D5 sub mount
const join = pathlib.join, dirname = pathlib.dirname;
const isFullSha = sha.isFullSha;

const writeWtlog = ulog.write;
const appendWtlog = ulog.append;

//  SUBS-041: cap the checkout sub-mount RECURSION depth.  A deep/cyclic gitlink
//  chain (a sub of a sub of a sub…) fans out unbounded; past this many descents,
//  stop mounting deeper (log a skip, leave the mount as-is, no crash).
const MAX_SUBMODULE_DEPTH = 8;

//  del-sweep fold-row URI marker.  Plain ASCII (no leading `:` / `?` / `#`):
//  a leading colon mis-frames the NEXT queue ULOG row (a URI-scheme parse
//  hazard — JSQUE-009 own-ticket), so the marker must be scheme-free.
const DELSWEEP = "del-sweep";
//  D5: a conflicting weave-merge leaf enqueues this sentinel; it dispatches
//  AFTER every leaf (tail-appended), so all files are materialised, then throws
//  → the loop maps it to the non-zero exit (markers already in the wt).
const CONFMARK = "merge-conflict";

//  --- remote URI → { local, cached, srcRoot, srcBe, proj, branch, pin } ---
//  No hand-rolled parsing: the URI binding splits scheme/host/path/query.
//  GET.mkd 5-slot map: Scheme=transport, Host=remote, Query=branch/sha,
//  Fragment=exact-commit PIN (D1).  A `//host` with NO scheme is a CACHED read
//  (D7) — the local store's remote-tracking tip, NO wire; only ssh:/be: open it.
function parseRemote(uri) {
  const u = new URI(uri);
  //  URI-009: route on slot PRESENCE (undefined = absent), not string-emptiness.
  //  A `//host` (authority present, NO scheme) is a CACHED read; a `file:`/scheme-
  //  less `.be` path is a LOCAL store; any scheme is a wire transport.  u.host is
  //  now the bare authority ("origin"), so the old `|| u.authority` (which leaked
  //  the `//` into the cached-host match) is gone.
  const hasScheme = u.scheme !== undefined;
  const hasAuth   = u.authority !== undefined;
  const scheme = u.scheme || "";
  const host = u.host || "";
  const authority = u.authority || "";
  const path = u.path || "";
  const query = u.query || "";
  const frag = u.fragment || "";           // D1: the exact-commit pin (no `?`)
  let proj = "", branch = "";
  if (query && query[0] === "/") {
    const segs = query.slice(1).split("/");
    proj = segs[0] || "";
    branch = segs.slice(1).join("/");
  } else if (query) {
    branch = query;
  }
  //  A `file:`/scheme-less LOCAL store path (ends in `.be` or holds one).  A
  //  scheme-less `//host` (authority, no store path) is the cached read below.
  const hasStorePath = path.replace(/\/+$/, "").slice(-3) === ".be" ||
                       (path !== "" && !hasAuth);
  const localish = (scheme === "file" && hasStorePath) ||
                   (!hasScheme && !hasAuth && hasStorePath) ||
                   (scheme === "keeper" && (host === "" || host === "local" ||
                                            host === "localhost"));
  //  D7 cached read: a Host with NO scheme (`//host?branch`) — read the local
  //  store's remote-tracking tip, never the wire.
  const cached = !localish && !hasScheme && hasAuth;
  let srcBe = path, srcRoot = path;
  if (localish) {
    srcBe = path.replace(/\/+$/, "");
    srcRoot = srcBe.replace(/\/\.be$/, "");
    if (srcRoot === srcBe) srcRoot = dirname(srcBe);
  }
  return { local: localish, cached, scheme, host, authority, srcRoot, srcBe,
           proj, branch, pin: frag, raw: uri };
}

function exists(p) { try { io.stat(p); return true; } catch (e) { return false; } }

//  SUBS-041: a friendly diagnostic line to stderr (fd 2) — the runtime has no
//  console/log global; output goes via io.write of a utf8 buffer.
function warn(s) {
  try { const u = utf8.Encode(s + "\n"); const b = io.buf(u.length + 8);
        b.feed(u); io.write(2, b); } catch (e) {}
}

//  Newest tip sha recorded in an existing wtlog (the last `#<40hex>` row).
function oldTipOf(bePath) {
  let tip = "";
  ulog.each(bePath, function (log) {
    const u = new URI(log.uri);
    const f = u.fragment || "";          // URI-009: a clean sha, never `?`-prefixed
    if (isFullSha(f)) tip = f;
  });
  return tip;
}

//  GET-038: a local `file:` source may name a STORE (`<store>/.be`) OR a
//  WORKTREE (`<wt>` whose `.be` is a wtlog FILE redirecting to the real store).
//  A worktree is NOT a store: recording its path as the new wt's row-0 anchor
//  leaves `status`/`get` unable to read the baseline tree (the store has no
//  objects there) — every file then reads `unk`.  So resolve the source down to
//  the REAL store: when `<srcRoot>/.be` (or `<srcBe>` itself) is a FILE, follow
//  its row-0 `repo` redirect (be.repoFromBe / be.projectFromQuery, the same
//  DOGRepoFromBe split be.find uses) to the store dir + project, and record THAT
//  — never the worktree path.  A plain store source resolves to itself unchanged.
//  Returns { storeRoot, storeBe, proj } where storeBe is the real `<store>/.be`.
function resolveLocalSource(rem) {
  //  The source `.be` to probe: the path itself when it ends `.be`, else
  //  `<path>/.be`.  A worktree anchor is a regular FILE; a store `.be` is a dir.
  const srcBe = rem.srcBe;                       // path with trailing `.be` shed
  const beFile = (srcBe.slice(-3) === ".be") ? srcBe : join(srcBe, ".be");
  let kind; try { kind = io.stat(beFile).kind; } catch (e) { kind = undefined; }
  if (kind !== "reg")                            // a store (dir) or absent → as-is
    return { storeRoot: rem.srcRoot, storeBe: rem.srcBe, proj: rem.proj };

  //  Worktree source: read row 0 (the `repo|<storepath>` redirect) and split it
  //  to the real store dir + project — the same resolution be.find performs on a
  //  secondary wt anchor.
  let u0;
  ulog.each(beFile, function (log) { if (u0 === undefined) u0 = log.uri; });
  if (!u0)
    throw "be get: GETWTSRC worktree source " + srcBe +
          " has no store redirect — cannot resolve its store";
  const p = new URI(u0);
  const storeRoot = be.repoFromBe(p.path || "");
  const proj = rem.proj || be.projectFromQuery(p.query || "") ||
               be.projectFromPath(p.path || "");
  if (!storeRoot)
    throw "be get: GETWTSRC cannot resolve the store of worktree source " + srcBe;
  return { storeRoot: storeRoot, storeBe: join(storeRoot, ".be"), proj: proj };
}

//  --- the GET SEED: resolve the remote ONCE, anchor, return pinned coords --
//  D1: a Fragment pin (`?branch#<sha>`) resolves the EXACT commit, not the
//  branch tip — `rem.pin` (full or short hex) wins over resolveRef.
function seedLocal(rem, wt) {
  //  GET-038: resolve a worktree source down to its REAL store (the redirect
  //  target) before anchoring — anything else records a non-store path that
  //  status/get can't read the baseline from.
  const src = resolveLocalSource(rem);
  const k = store.open(src.storeRoot, src.proj);
  const tip = resolvePin(k, rem.pin) || k.resolveRef(rem.branch || "");
  if (!tip || !isFullSha(tip))
    throw "be get: cannot resolve " +
          (rem.pin ? "#" + rem.pin : (rem.branch || "trunk")) +
          " in " + src.storeBe;
  const bePath = join(wt, ".be");
  const fresh = !exists(bePath);
  const oldTip = fresh ? "" : oldTipOf(bePath);
  const redirect = "file:" + src.storeBe + "/?/" + src.proj;
  const tipRow = { verb: "get", uri: "?" + (rem.branch || "") + "#" + tip };
  if (fresh) writeWtlog(bePath, [{ verb: "get", uri: redirect }, tipRow]);
  else appendWtlog(bePath, [tipRow]);
  return { k, tip, oldTip, fresh, branch: rem.branch || "" };
}

//  D1/D2 short- or full-hex commit pin → 40-hex sha, or "" when none/unfound.
//  Reuses store.resolveHexAny (the any-object short-hex resolver, JAB-006).
function resolvePin(k, hex) {
  if (!hex) return "";
  if (isFullSha(hex)) return k.getObject(hex) ? hex : "";
  return k.resolveHexAny(hex) || "";
}

function seedRemote(rem, wt) {
  const beDir = join(wt, ".be");
  const fresh = !exists(beDir);
  const proj = rem.proj || "repo";
  const f = wire.fetch(rem.raw, rem.branch || "");
  const tip = f.want;
  if (!tip || !isFullSha(tip)) throw "be get: peer gave no tip";
  const branch = rem.branch || f.branch || "";
  const shard = join(beDir, proj);
  const wtl = join(beDir, "wtlog");
  let oldTip = "";
  if (fresh) {
    ingest.clone(f.pack, beDir, proj, tip, rem.raw);
    const anchor = "file:" + beDir + "/" + proj + "/";
    writeWtlog(wtl, [{ verb: "get", uri: anchor },
                     { verb: "get", uri: "?" + branch + "#" + tip }]);
  } else {
    oldTip = oldTipOf(wtl);
    ingest.add(f.pack, shard, rem.raw, tip);
    appendWtlog(wtl, [{ verb: "get", uri: "?" + branch + "#" + tip }]);
  }
  const k = store.open(wt, proj);
  return { k, tip, oldTip, fresh, branch };
}

//  D7: a CACHED host read — `//host?branch` resets from the local store's
//  remote-tracking tip with NO network (GET.mkd pt 4: "uses the cached tip if
//  no scheme set").  Resolve the matching `eachRemote` row for host+branch off
//  the EXISTING repo's shard; append the wtlog tip row.  Only ssh:/be: (seedRemote)
//  ever opens the wire — a bare `//host` never does.
function seedCached(rem, wt) {
  const info = be.find(wt);
  const k = store.open(info.storePath, info.project);
  let tip = "";
  k.eachRemote(function (rt) {
    if (tip) return;
    const h = rt.host || "";
    if (h !== rem.host && h !== rem.authority) return;
    //  branch match: empty rem.branch = trunk (empty remote query).
    const rq = stripLeadRef(rt.query || "");
    if ((rem.branch || "") === rq) tip = rt.sha;
  });
  if (!tip || !isFullSha(tip))
    throw "be get: GETCACHE no cached tip for //" + rem.host +
          (rem.branch ? "?" + rem.branch : "") + " — fetch with ssh:/be: first";
  const bePath = info.bePath;
  const oldTip = oldTipOf(bePath);
  appendWtlog(bePath, [{ verb: "get",
                         uri: "?" + (rem.branch || "") + "#" + tip }]);
  return { k, tip, oldTip, fresh: false, branch: rem.branch || "" };
}

//  Strip a leading `?` / `/proj/` decoration off a remote-tracking ref query so
//  it compares as a bare branch (trunk = "").
function stripLeadRef(q) {
  if (q && q[0] === "?") q = q.slice(1);
  if (q && q[0] === "/") { const j = q.indexOf("/", 1); q = j < 0 ? "" : q.slice(j + 1); }
  return q;
}

//  --- per-level tree map: name → { sha, mode, isDir } --------------------
function treeMap(k, treeSha) {
  const m = {};
  if (!treeSha) return m;
  const entries = k.readTree(treeSha);
  if (!entries) return m;
  for (const e of entries)
    m[e.name] = { sha: e.sha, mode: e.mode, isDir: e.mode === 0o40000 };
  return m;
}

//  Map a git mode to checkout.js's leaf kind ("f"/"x"/"l"/"s").
function kindOf(mode) {
  if (mode === 0o160000) return "s";
  if (mode === 0o120000) return "l";
  if (mode === 0o100755) return "x";
  return "f";
}

//  --- URI shape classifiers for the fan-out rows -------------------------
//  A REMOTE seed carries a transport (scheme/authority) or a local store path
//  (a `file:`/scheme-less path ending `.be`).  A reconcile/leaf/fold row never
//  carries a remote — only pinned blob/tree shas (or the ::del-sweep marker).
//  A scheme-less `//host` (D7 cached) or `file:` store path is a remote seed
//  too; a bare `?ref`/`?<sha>`/`#~N`/`<path>` is an IN-REPO seed (D1-D4).
function isRemoteSeed(uri) {
  if (uri === DELSWEEP) return false;
  const u = new URI(uri);
  if (u.scheme || u.authority) return true;        // transport / file:// / //host
  const path = u.path || "";
  return path.replace(/\/+$/, "").slice(-3) === ".be";
}

//  --- the handler --------------------------------------------------------
//  Dispatch by row provenance: a FAN-OUT child (reconcile/leaf/fold) only
//  appears AFTER a seed pinned ctx._get, so `ctx._get` set ⇒ fan-out; unset ⇒
//  a top-level seed (remote clone/switch, cached host, or an in-repo form).
module.exports = function handle(row, ctx) {
  const uri = (row && row.uri) || "";
  if (uri === DELSWEEP) return delSweep(row, ctx);   // BARRIER fold row
  if (uri === CONFMARK) {                              // D5: post-leaf conflict gate
    flushGet(ctx);                                     // JAB-003: emit partial hunk before the loud exit
    throw "be get: GETCONF " + (ctx._get && ctx._get.conf || 1) +
          " file(s) merged with conflicts — resolve the markers";
  }
  if (ctx._get) return handleReconcileOrLeaf(uri, ctx);   // a fan-out child
  if (isRemoteSeed(uri)) return handleSeed(uri, ctx);     // remote/clone/cached
  return inRepoSeed(uri, ctx);                            // D1-D4 in-repo forms
};

//  SEED: resolve the remote once (resolution-at-entry), anchor the wtlog, emit
//  the `get ?<branch>#<hashlet>` banner + any pulled-commit rows, then run the
//  dirty-overlap PRE-PASS and ENQUEUE the root dir-reconcile.  Pins
//  { k, wt, tip, oldTip, kinds } on ctx so each fan-out child reuses the SAME
//  reader/wt without re-resolving (JSQUE-004).
function handleSeed(uri, ctx) {
  const wt = io.cwd();
  const rem = parseRemote(uri);
  const r = rem.cached ? seedCached(rem, wt)
          : rem.local  ? seedLocal(rem, wt)
          :              seedRemote(rem, wt);
  //  DIS-058 D4: the parent's SOURCE (this remote) so a gitlink leaf can fetch
  //  its child shard from the SAME source (project swapped to the sub title).
  r.source = rem;
  return fanoutWholeTree(ctx, r, wt, ctx.flags && ctx.flags.indexOf("--force") >= 0);
}

//  Pin ctx._get, run the dirty-overlap pre-pass, emit the banner + pulled-commit
//  rows, and ENQUEUE the root dir-reconcile + del-sweep fold.  Shared by the
//  remote/clone seed (handleSeed) AND the in-repo forms (inRepoSeed: pin /
//  detach / rewind / switch).  `force` (D6) makes the leaf clean-reset dirty
//  baselined paths instead of weave-merging them.
function fanoutWholeTree(ctx, r, wt, force) {
  const out = getOut(ctx);

  //  A re-get of the CURRENT commit (oldTip==tip), non-force: the wt may have
  //  drifted (a deleted tracked file, a dirty edit), so VISIT every path (no
  //  merkle-prune) with the REAL baseline = the new tree — the leaf then restores
  //  a missing file, weave-merges a dirty edit, and skips a clean one.  --force
  //  always re-materialises (discarding dirty bytes), so it prunes nothing too.
  const sameTip = !!(r.oldTip && r.oldTip === r.tip);
  const noPrune = !!force || (sameTip && !r.fresh);

  //  Pin the checkout coordinates on ctx; ctx.T0 is the cohort ts (one date
  //  column for the whole get).  `kinds` carries each leaf's tree-entry kind
  //  (the reconcile reads the mode; the queue round-trip drops it, so it rides
  //  ctx — set before the leaf is dispatched).  `dels` tallies the del-sweep.
  ctx._get = { k: r.k, wt: wt, tip: r.tip, oldTip: r.oldTip, fresh: r.fresh,
               branch: r.branch, ts: ctx.T0, kinds: {}, dels: 0, rows: [], head: null,
               force: !!force, noPrune: noPrune,
               //  DIS-058 D2-D5: the parent's source (for the same-source child
               //  fetch) + the store dir where sibling sub shards land + the
               //  parent shard title (the synthetic-branch parent token).
               //  GET-037: siblings land NEXT TO the parent shard, i.e. the
               //  parent shard's OWN parent dir — `dirname(r.k.shard)`.  For a
               //  remote clone that is `<wt>/.be`; for a LOCAL-store get (`<wt>/.be`
               //  is a redirect FILE, the real shard lives in the source store)
               //  it is the SOURCE store dir — using `<wt>/.be` would `mkdir`
               //  under the redirect file and crash with ENOTDIR (GET-037 repro).
               source: r.source || null,
               beDir: (r.k && r.k.shard) ? dirname(r.k.shard) : join(wt, ".be"),
               parentTitle: (r.k && r.k.project) || "" };

  //  Edge flush order (native get): pulled-commit `post` rows first (newest
  //  first, kept in push order), then file rows — new+upd interleaved lex,
  //  THEN del lex.  The fan-out arrives in queue order, so SORT at the flush.
  ctx.outSort = function (rows) { return sortGetRows(rows); };
  ctx._finalize = flushGet;   // JAB-003: flush the one get hunk after the queue drains

  //  Resolution-at-entry: pin the root NEW + OLD tree shas.  Old empty on a fresh
  //  clone OR a --force reset (re-write everything, discard dirty); else the real
  //  baseline tree (so the leaf has the blob to weave-merge a dirty edit).
  const newTree = r.k.commitTree(r.tip);
  if (!newTree) throw "be get: tip " + r.tip + " has no tree";
  const oldTree = (r.fresh || force) ? "" : r.k.commitTree(r.oldTip || "") || "";

  //  JSQUE-014: the dirty-overlap PRE-PASS barrier (SNIFFOVRL, GET.c:585-591)
  //  refuses BEFORE HUNKTableOpen — native emits NO banner on GETOVRL, so run
  //  the check ahead of any out.* push (the loop edge then flushes nothing).
  //  Refuse if a dirty wt file overlays a NEW target path with NO baseline to
  //  merge; a fresh clone has no baseline so it is a no-op (guards no-base only).
  //  D6 --force skips the refuse (it will clean-reset everything).
  if (!force) dirtyOverlapCheck(r.k, newTree, oldTree, wt, r.fresh);

  out.banner("get", "?" + (r.branch || "") + "#" + r.tip.slice(0, 8), ctx.T0);

  //  Pulled-commit rows (UPDATE only), newest-first; rendered above file rows.
  //  GIT-016: via the shared spine — from cur=oldTip the fetched tip is AHEAD, so
  //  the pulled commits are verdict.behind (pack persisted first → NO remote index).
  if (!r.fresh && r.oldTip && r.oldTip !== r.tip) {
    const v = relate.verdict(r.k, r.oldTip, r.tip);
    const pulled = v.behind;                        // rel exposed for a future FF gate (JGET-002)
    for (const c of pulled)
      out.row("?" + c.hashlet + (c.subject ? "#" + c.subject : ""), "post",
              c.ts, { _post: true });
  }

  //  ENQUEUE the root reconcile then the del-sweep fold row LAST (post-order:
  //  the fold sits after every delete leaf the reconcile fans out).
  return { enqueue: [{ verb: "get", uri: "?" + newTree + "#" + oldTree },
                     { verb: "get", uri: DELSWEEP }] };
}

//  --- IN-REPO seed (D1-D4): a top-level GET form over the EXISTING repo ----
//  Forms (each the whole arg as `uri`):
//    ?<sha>           D2 detach at a commit (full/short hex)  → wtlog `?<sha>`
//    ?#<sha> ?br#<sha> D1 pin: checkout the EXACT commit       → wtlog `?br#<sha>`
//    ?br ?./c ?       D3' branch/trunk switch                  → wtlog `?br#<tip>`
//    #~N              D3 rewind cur N first-parents            → wtlog `?br#<anc>`
//    <path>[?br]      D4 restore one file/subtree (scoped)
//    ! (empty)        D6 force-reset / FF current branch to tip
//  `be.find()` discovers the repo; cur is read from the wtlog; the reader is the
//  shared store.  A path form is path-scoped (restorePath); the rest reset the
//  whole wt via fanoutWholeTree.
function inRepoSeed(uri, ctx) {
  const info = (ctx && ctx.repo) || be.find(io.cwd());
  const wt = info.wt;
  const k = store.open(info.storePath, info.project);
  const wtl = wtlog.open(info);
  const cur = wtl.curTip();
  const curBranch = (cur && cur.branch) || "";
  const curSha = (cur && cur.sha) || "";

  //  D6 force (GET.mkd "be get!"): the `--force` flag (set by the cli `get!`
  //  shed) OR a trailing/lone `!` on the arg.  Force discards local edits — a
  //  CLEAN tree reset (the leaf clean-overwrites; the dirty-overlap pre-pass and
  //  the weave-merge are bypassed).
  let force = !!(ctx.flags && ctx.flags.indexOf("--force") >= 0);
  if (uri === "!") { uri = ""; force = true; }
  else if (uri.length > 1 && uri[uri.length - 1] === "!") {
    uri = uri.slice(0, -1); force = true;
  }

  //  URI-009: an ABSENT slot reads `undefined`, a present-but-empty one "".  So a
  //  bare `?` is query==="" (trunk switch) while a path arg is query===undefined —
  //  the leading-`?` forms tell apart by slot presence, NO raw-href inspection.
  //  Fragments never carry a leading `?`, so the old strip is gone too.
  const u = new URI(uri);
  let path = u.path || "";
  const query = u.query || "";
  const frag = u.fragment || "";
  //  Bare `be get` seeds a "." placeholder (loop.cli) — a whole-tree FF of the
  //  current branch, NOT a path restore of ".".  Shed it.
  if (path === "." && !query && !frag) path = "";

  //  D4: a PATH arg (a wt file/subtree) — scoped restore, NOT a whole-wt reset.
  //  `<path>` from cur's baseline; `<path>?br` from another branch's tip.
  if (path) return restorePath(ctx, k, wt, path, query, curSha, curBranch);

  //  D3 rewind: `#~N` — walk cur N first-parents; stay attached to cur's branch.
  if (frag && frag[0] === "~") {
    const n = parseInt(frag.slice(1), 10) || 1;
    const anc = firstParentBack(k, curSha, n);
    if (!isFullSha(anc))
      throw "be get: cannot rewind " + n + " from " + (curSha || "(none)");
    appendWtlog(info.bePath, [{ verb: "get",
                               uri: "?" + curBranch + "#" + anc }]);
    return fanoutWholeTree(ctx, { k, tip: anc, oldTip: curSha, fresh: false,
                                  branch: curBranch }, wt, force);
  }

  //  D2 detach: `?<sha>` (bare hex query, no fragment) — checkout detached, write
  //  the detached row (`?<40hex>`: sha in the QUERY, empty fragment).  A seed with
  //  a scheme/host already routed to handleSeed and a path arg returned above, so
  //  a non-empty fragment-less query here is unambiguously the leading-`?` form.
  if (query && !frag && (isFullSha(query) || isShortHex(query))) {
    const tip = resolvePin(k, query);
    if (!isFullSha(tip)) throw "be get: cannot resolve ?" + query;
    appendWtlog(info.bePath, [{ verb: "get", uri: "?" + tip }]);
    return fanoutWholeTree(ctx, { k, tip, oldTip: curSha, fresh: false,
                                  branch: "" }, wt, force);
  }

  //  D1 pin: `?#<sha>` / `?br#<sha>` — checkout the EXACT commit, attached to
  //  `?br` (empty br = trunk).  The fragment pin wins over the branch tip.
  if (frag && isShortOrFullHex(frag)) {
    const tip = resolvePin(k, frag);
    if (!isFullSha(tip)) throw "be get: cannot resolve ?" + query + "#" + frag;
    appendWtlog(info.bePath, [{ verb: "get", uri: "?" + query + "#" + tip }]);
    return fanoutWholeTree(ctx, { k, tip, oldTip: curSha, fresh: false,
                                  branch: query }, wt, force);
  }

  //  D3' branch/trunk switch (`?br`, `?`, `?./child`) OR a bare `!`/empty FF
  //  (reset the wt to the current/target branch tip).
  //  A non-empty query is the target branch; an empty/absent query (`?`, bare
  //  `be get`, `!`) folds to the current branch.  URI-009 makes the bare-`?` case
  //  query==="", so no `href === "?"` string match is needed any more.
  const branch = query;                          // "" = trunk / current branch
  const wantBranch = branch || curBranch;
  const tip = k.resolveRef(branch || "") ||
              (wantBranch === curBranch ? curSha : "");
  if (!isFullSha(tip))
    throw "be get: cannot resolve " + (branch ? "?" + branch : "current branch");
  appendWtlog(info.bePath, [{ verb: "get", uri: "?" + wantBranch + "#" + tip }]);
  return fanoutWholeTree(ctx, { k, tip, oldTip: curSha, fresh: false,
                                branch: wantBranch }, wt, force);
}

//  D4 single-file / subtree restore (GET.mkd pt 1, CLI `file.c` / `file.c?feat`).
//  `<path>` restores from cur's BASELINE tree; `<path>?br` from `?br`'s tip.  Only
//  the named path is touched — siblings are left as-is (path-scoped reconcile).
//  A FILE leaf restores that one blob (dirty → weave-merge / refuse, like the
//  whole-tree leaf); a DIR scopes the reconcile to that subtree (new vs the wt's
//  current on-disk state — which the reconcile reads as the OLD side via the
//  baseline tree's subtree, so a removed sibling under it is deleted).
function restorePath(ctx, k, wt, path, query, curSha, curBranch) {
  //  A named `be get <path>` is an explicit RESTORE — it OVERWRITES the file
  //  from the source (GET.mkd "restore one file from cur's baseline"), like git
  //  restore.  So the scoped leaves clean-reset (force) rather than weave-merge:
  //  the user named the path, they want the source bytes, not a 3-way merge.
  const force = true;
  //  Source commit: another branch's tip (`?br`) or cur's baseline.
  let srcSha = curSha;
  if (query) {
    srcSha = k.resolveRef(query) || resolvePin(k, query) || "";
    if (!isFullSha(srcSha)) throw "be get: cannot resolve ?" + query;
  }
  if (!isFullSha(srcSha)) throw "be get: no baseline to restore " + path + " from";
  const newTree = k.commitTree(srcSha);
  if (!newTree) throw "be get: tip " + srcSha + " has no tree";
  //  The OLD side (for the delete decision) is cur's baseline subtree — so a
  //  file present in the wt's baseline but absent in the source is removed.
  const oldTree = (curSha && curSha !== srcSha) ? k.commitTree(curSha) : newTree;

  const segs = path.replace(/\/+$/, "").split("/");
  const newEnt = k.descendPath(newTree, segs);
  const oldEnt = k.descendPath(oldTree, segs);
  if (!newEnt && !oldEnt) throw "be get: no such path " + path + " in source";

  //  Pin ctx like a seed so the reconcile/leaf fan-out reuses the reader/wt.
  //  noPrune: a named restore descends even an unchanged subtree (so a dirty/
  //  deleted file under it is reset), and force makes the leaf clean-overwrite.
  ctx._get = { k: k, wt: wt, tip: srcSha, oldTip: curSha, fresh: false,
               branch: curBranch, ts: ctx.T0, kinds: {}, dels: 0, rows: [], head: null,
               force: force, noPrune: true };
  ctx.outSort = function (rows) { return sortGetRows(rows); };
  ctx._finalize = flushGet;   // JAB-003: flush the one get hunk after the queue drains
  getOut(ctx).banner("get", "?" + (curBranch || "") + "#" + srcSha.slice(0, 8), ctx.T0);

  const enqueue = [];
  const newIsDir = newEnt && newEnt.kind === "tree";
  const oldIsDir = oldEnt && oldEnt.kind === "tree";
  if (newIsDir || oldIsDir) {
    //  Subtree: recurse a scoped dir-reconcile rooted at `path/`.
    enqueue.push({ verb: "get", uri: path.replace(/\/+$/, "") + "/?" +
                   (newIsDir ? newEnt.sha : "") + "#" + (oldIsDir ? oldEnt.sha : "") });
  } else {
    //  Single file/exe/symlink/gitlink leaf.
    const ent = newEnt || oldEnt;
    ctx._get.kinds[path] = kindOf(ent.mode);
    enqueue.push({ verb: "get", uri: path + "?" + (newEnt ? newEnt.sha : "") +
                   "#" + (oldEnt ? oldEnt.sha : "") });
  }
  enqueue.push({ verb: "get", uri: DELSWEEP });
  return { enqueue: enqueue };
}

//  D3: walk `n` FIRST-PARENT edges back from `sha` (the first listed parent at
//  each commit).  Returns "" if the chain runs out.
function firstParentBack(k, sha, n) {
  let cur = sha;
  for (let i = 0; i < n; i++) {
    if (!isFullSha(cur)) return "";
    let ps; try { ps = k.commitParents(cur); } catch (e) { ps = undefined; }
    if (!ps || !ps.length) return "";
    cur = ps[0];
  }
  return cur;
}

//  short hex (6..39) — a `?<short>` detach prefix.
function isShortHex(s) { return /^[0-9a-f]{6,39}$/.test(s); }
//  any 6..40 hex — the pin / detach acceptance.
function isShortOrFullHex(s) { return /^[0-9a-f]{6,40}$/.test(s); }

//  Dirty-overlap PRE-PASS (SNIFFOVRL): on an UPDATE, a wt file present on disk
//  that the NEW tree introduces but the OLD baseline never carried, AND whose
//  bytes differ from the target, is an un-mergeable overlay → refuse.  A whole-
//  tree set difference (new \ old) intersected with dirty wt files — a barrier,
//  not a per-file job.  Fresh clone → no baseline → no overlap (no-op).
function dirtyOverlapCheck(k, newTree, oldTree, wt, fresh) {
  if (fresh || !oldTree) return;
  const oldPaths = {};
  k.readTreeRecursive(oldTree, function (l) {
    oldPaths[l.path] = l.sha;
    //  GET-039: a dir->leaf type-change (old `a/b` -> new `a`) IS baselined —
    //  mark every ancestor dir so the new leaf at `a` is treated as tracked
    //  content (reconcile + checkout drop the dir), not a dirty user overlay.
    for (let i = l.path.indexOf("/"); i >= 0; i = l.path.indexOf("/", i + 1))
      if (oldPaths[l.path.slice(0, i)] === undefined)
        oldPaths[l.path.slice(0, i)] = "";          // dir-prefix sentinel
  });
  const conflicts = [];
  k.readTreeRecursive(newTree, function (l) {
    if (oldPaths[l.path] !== undefined) return;     // had a baseline → mergeable
    const full = join(wt, l.path);
    if (!exists(full)) return;                       // no on-disk overlay
    const obj = k.getObject(l.sha);
    const bytes = obj ? obj.bytes : new Uint8Array(0);
    if (!checkout.leafUnchanged(full, { kind: kindOf(l.mode) }, bytes))
      conflicts.push(l.path);
  });
  if (conflicts.length)
    throw "be get: GETOVRL dirty wt overlays un-baselined target: " +
          conflicts.slice(0, 5).join(", ");
}

//  RECONCILE or LEAF: route on the URI shape.  A dir-reconcile row has an empty
//  path or a path ending `/`; a write/merge/delete leaf names a single file.
function handleReconcileOrLeaf(uri, ctx) {
  const u = new URI(uri);
  const path = u.path || "";
  const isDir = path === "" || path[path.length - 1] === "/";
  if (isDir) return reconcileDir(uri, ctx);
  return leaf(uri, ctx);
}

//  dir-reconcile: read the old + new trees ONE level, MERKLE-PRUNE unchanged
//  subtrees (old==new sha → skip the whole subtree), and emit per-entry rows:
//    a file/exe/symlink/gitlink leaf changed → `get <path>?<new>#<old>`,
//    a removed entry (old, not new)          → `get <path>?#<old>` (delete),
//    a changed subdir                        → recurse `get <sub>/?<new>#<old>`.
//  Each leaf's kind (the tree-entry mode) is stashed on ctx._get.kinds[path]
//  (the queue round-trip carries only the sha).  The new-vs-mod label is the
//  LEAF's call; the del-sweep collapse is the trailing BARRIER fold.
function reconcileDir(uri, ctx) {
  const g = ctx._get;
  const u = new URI(uri);
  const prefix = u.path || "";                 // "" (root) or "<dir>/"
  const newTree = u.query || "";               // URI-009: "" = empty side (no tree)
  const oldTree = u.fragment || "";            // URI-009: clean sha, never `?`-prefixed

  const nm = treeMap(g.k, newTree);
  const om = treeMap(g.k, oldTree);
  const enqueue = [];

  const names = {};
  for (const n in nm) names[n] = 1;
  for (const n in om) names[n] = 1;
  const sorted = Object.keys(names).sort();

  for (const name of sorted) {
    const ne = nm[name], oe = om[name];
    const rel = prefix + name;
    //  MERKLE-PRUNE: identical sha + dir-ness on both sides → unchanged; skip.
    //  D6 force / scoped RESTORE / same-tip re-get descend even unchanged
    //  subtrees (so a deleted or dirty wt file is reconciled, not skipped).
    if (!g.noPrune && ne && oe && ne.sha === oe.sha && ne.isDir === oe.isDir) continue;

    if (ne && ne.isDir) {                       // changed/new subdir → recurse
      enqueue.push({ verb: "get",
                     uri: rel + "/?" + ne.sha + "#" + (oe && oe.isDir ? oe.sha : "") });
      if (oe && !oe.isDir) { g.kinds[rel] = kindOf(oe.mode);   // stale file → del
        enqueue.push({ verb: "get", uri: rel + "?#" + oe.sha }); }
      continue;
    }
    if (ne) {                                   // a (changed) file/exe/lnk/sub
      g.kinds[rel] = kindOf(ne.mode);
      enqueue.push({ verb: "get",
                     uri: rel + "?" + ne.sha + "#" + (oe && !oe.isDir ? oe.sha : "") });
      if (oe && oe.isDir)                        // old dir → recurse to delete
        enqueue.push({ verb: "get", uri: rel + "/?#" + oe.sha });
      continue;
    }
    //  removed entry (old, not new).
    if (oe && oe.isDir)
      enqueue.push({ verb: "get", uri: rel + "/?#" + oe.sha });
    else { g.kinds[rel] = kindOf(oe.mode);
      enqueue.push({ verb: "get", uri: rel + "?#" + oe.sha }); }
  }
  return { enqueue: enqueue };
}

//  LEAF: a single-path effect.  `get <path>?<new>#<old>` writes/weaves the new
//  blob (the leaf reads its OWN on-disk bytes to label new-vs-mod and to decide
//  clean-overwrite vs 3-way weave); `get <path>?#<old>` deletes a removed path.
//  Gitlink (submodule) leaves are recorded-only (mount recursion is a follow-up).
function leaf(uri, ctx) {
  const g = ctx._get, out = getOut(ctx);
  const u = new URI(uri);
  const rel = u.path || "";
  const newSha = u.query || "";          // URI-009: "" = present-empty = delete leaf
  const oldSha = u.fragment || "";       // URI-009: clean sha, never `?`-prefixed
  const full = join(g.wt, rel);
  const kind = g.kinds[rel] || "f";

  if (!newSha) {                                // delete LEAF (removed path)
    try { io.unlink(full); out.row(rel, "del", g.ts); g.dels++; } catch (e) {}
    return;
  }
  //  DIS-058 D2-D5: a gitlink leaf is MOUNTED + recursed (pre-order): fetch the
  //  child shard from the same source, clone it as a sibling shard, write the
  //  sub wtlog anchor, check out the pinned commit, then descend into the sub's
  //  OWN gitlinks.  `newSha` IS the parent gitlink pin (the 160000 entry sha).
  if (kind === "s") return mountGitlink(g, rel, newSha, out);

  const obj = g.k.getObject(newSha);
  if (!obj) return;                             // unresolved → skip
  const bytes = obj.bytes;
  const existed = exists(full);
  //  Skip if the on-disk path already matches the target (preserves dirty bytes
  //  equal to the target; emits no row — native get only reports what moved).
  if (existed && checkout.leafUnchanged(full, { kind: kind }, bytes)) return;

  //  D5 (DATA SAFETY): a DIRTY baselined file (on-disk differs from BOTH the old
  //  baseline blob AND the target) must be 3-WAY MERGED — re-apply the user's
  //  uncommitted edit onto the new tree — NOT clean-overwritten.  Only regular
  //  files merge (symlink/exec/gitlink stay clean-reset).  --force (D6) and the
  //  clean (un-edited) case clean-overwrite.  An un-baselined dirty overlay with
  //  no base to merge refuses loudly (the whole-tree pre-pass also catches it).
  if (existed && !g.force && kind === "f") {
    const onDisk = readWt(full);
    const baseBytes = oldSha ? blobOf(g.k, oldSha) : null;
    const dirty = onDisk != null && (baseBytes == null || !bytesEq(onDisk, baseBytes));
    if (dirty && baseBytes == null)
      throw "be get: GETOVRL dirty wt overlays un-baselined target: " + rel;
    if (dirty) {                                 // real local edit → weave-merge
      const merged = weave3(baseBytes, onDisk, bytes, extOf(rel));
      checkout.materialise(g.wt, rel, { kind: kind }, merged);
      if (conflict.hasConflictMarker(merged)) {
        g.conf = (g.conf || 0) + 1;
        out.row(rel, "cnf", g.ts);   // DIS-057: conf→cnf

        //  D5: defer a loud non-zero exit until every file is materialised — the
        //  CONFMARK sentinel dispatches after all tail-appended leaves.
        return { enqueue: [{ verb: "get", uri: CONFMARK }] };
      }
      out.row(rel, "mrg", g.ts);
      return;
    }
  }
  //  Clean (un-edited), forced, or non-mergeable kind → clean-overwrite.
  checkout.materialise(g.wt, rel, { kind: kind }, bytes);
  out.row(rel, existed ? "upd" : "new", g.ts);
}

//  DIS-058 D2-D5 (pre-order sub mount): mount the gitlink at `rel` pinned at
//  `pin`, emit a `get <sub>?<hashlet>` row, then DESCEND into the freshly-
//  mounted sub's OWN gitlinks (a sub of a sub) — depth-first, pre-order (the
//  parent's files + this sub's files are already on disk before the grandchild
//  mounts).  Reuses the same parent SOURCE for the same-source child fetch, so
//  a grandchild fetches `<store>?/<gtitle>` off the same store too.  A loud
//  SUBFETCH/SUBPIN throw on a truly-unreachable child (never a silent mis-record).
function mountGitlink(g, rel, pin, out) {
  //  GET-037: an undeclared (.gitmodules) 160000 gitlink is not a real sub — never
  //  fetch/mount/recurse it (e.g. the `be` gitlink pins our own commit; mounting it crashes).
  if (!isDeclaredSub(g.wt, rel)) {
    return;                                       // undeclared gitlink → skip
  }
  const m = submount.mount({
    wt: g.wt, beDir: g.beDir, subpath: rel, pin: pin,
    source: g.source, parentTitle: g.parentTitle, parentBranch: g.branch,
  });
  out.row(rel, "new", g.ts);                    // the mounted sub leaf row
  //  Pre-order recurse: descend the mounted sub's pin tree for nested gitlinks.
  //  SUBS-041: this top-level mount is depth 0; each descent increments.
  recurseSubMounts(g, rel, m, out, 0);
}

//  GET-037: YES iff `<subpath>` is declared in `<wt>/.gitmodules` (the SUBS-043 gate);
//  an undeclared 160000 gitlink (e.g. the `be` entry) is never mounted/recursed.
function isDeclaredSub(wt, subpath) {
  if (submount.gitmodulesUrl(wt, subpath)) return true;
  const decl = require("../../core/recurse.js").gitmodulesOrder(wt);
  for (const p of decl) if (p === subpath) return true;
  return false;
}

//  Walk a just-mounted sub's pin tree for `160000` gitlinks and mount each
//  (grandchildren).  The grandchild's subpath is parent-relative (`<rel>/<sp>`);
//  its source is the SAME parent source (the same store serves every shard),
//  its synthetic-branch parent token is this sub's title.  In-process (not the
//  queue) so the parent's source coords stay in scope.
function recurseSubMounts(g, rel, m, out, depth) {
  //  SUBS-041: bound the descent — a deep/cyclic gitlink chain past the cap
  //  stops here (a friendly stderr skip, leave the mount as-is, no fan-out crash).
  if (depth >= MAX_SUBMODULE_DEPTH) {
    warn("be get: SUBS-041 sub-mount depth cap " + MAX_SUBMODULE_DEPTH +
         " reached at " + rel + " — not descending deeper");
    return;
  }
  const tree = m.k.commitTree(m.tip);
  if (!tree) return;
  const links = [];
  m.k.readTreeRecursive(tree, function (l) {
    if (l.kind === "s") links.push({ path: l.path, pin: l.sha });
  });
  const subWt = join(g.wt, rel);                 // the mounted sub's wt on disk
  for (const l of links) {
    const sp = rel + "/" + l.path;
    //  GET-037: gate the grandchild on the sub's own `.gitmodules` (SUBS-043); an
    //  undeclared gitlink is skipped, never recursed.
    if (!isDeclaredSub(subWt, l.path)) {
      continue;
    }
    const cm = submount.mount({
      wt: g.wt, beDir: g.beDir, subpath: sp, pin: l.pin,
      source: g.source, parentTitle: m.project, parentBranch: m.branch,
    });
    out.row(sp, "new", g.ts);
    recurseSubMounts(g, sp, cm, out, depth + 1);
  }
}

//  D5 3-blob weave merge (GRAFMerge3Bytes twin): build the OURS and THEIRS
//  weaves INDEPENDENTLY off a shared base (each base→side), then WEAVEMerge them
//  — the shared base tokens carry the SAME base commit-id so they coincide and
//  dedup, and each side's edit diffs against the base (NOT sequentially).  Render
//  the ours/theirs scopes with conflict fences: disjoint edits coexist cleanly,
//  a divergent region gets the standard conflict markers.
const _W3_BASE = "0000000000000001", _W3_OURS = "0000000000000002",
      _W3_THRS = "0000000000000003", _W3_MRG = "0000000000000004";
function weave3(base, ours, theirs, ext) {
  base = base || new Uint8Array(0);
  if (bytesEq(ours, theirs)) return ours;        // same edit both sides
  if (bytesEq(ours, base)) return theirs;        // only theirs changed
  if (bytesEq(theirs, base)) return ours;        // only ours changed
  const wo0 = abc.ram("WEAVE", 1 << 18), wo1 = abc.ram("WEAVE", 1 << 18);
  const wt0 = abc.ram("WEAVE", 1 << 18), wt1 = abc.ram("WEAVE", 1 << 18);
  wo0.fold(null, base, ext, _W3_BASE);  wo1.fold(wo0, ours,   ext, _W3_OURS);
  wt0.fold(null, base, ext, _W3_BASE);  wt1.fold(wt0, theirs, ext, _W3_THRS);
  const wm = abc.ram("WEAVE", 1 << 19);
  wm.merge(wo1, wt1, _W3_MRG);
  const oScope = wm.scope([_W3_BASE, _W3_OURS]);
  const tScope = wm.scope([_W3_BASE, _W3_THRS]);
  const out = io.buf(1 << 20);
  wm.merged([oScope, tScope], out);
  return out.data().slice();

}

//  Read a wt file's bytes (null on error); the dirty-vs-clean compare input.
function readWt(full) {
  let ls; try { ls = io.lstat(full); } catch (e) { return null; }
  if (ls.kind !== "reg") return null;
  let fd; try { fd = io.open(full, "r"); } catch (e) { return null; }
  try {
    const b = io.buf((ls.size || 0) + 16);
    io.readAll(fd, b, ls.size);
    const d = b.data().slice();
    io.close(fd);
    return d;
  } catch (e) { try { io.close(fd); } catch (e2) {} return null; }
}

//  Blob bytes for a sha (empty Uint8Array when missing), for the merge base/theirs.
function blobOf(k, sha) {
  if (!sha) return new Uint8Array(0);
  const obj = k.getObject(sha);
  return obj ? obj.bytes : new Uint8Array(0);
}

function bytesEq(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

//  File extension (the weave tokenizer's language key); no dot → "" (generic).
function extOf(path) {
  const slash = path.lastIndexOf("/");
  const base = slash < 0 ? path : path.slice(slash + 1);
  const dot = base.lastIndexOf(".");
  return dot <= 0 ? "" : base.slice(dot + 1);
}

//  del-sweep BARRIER (get_drain_unlinks, GET.c:668): the trailing fold over the
//  delete leaves.  Back-scans the live queue ULog from this fold row to the
//  newest delete-bearing reconcile boundary, RE-READING the `get <p>?#<old>`
//  delete rows in range (core/barrier.js) — a durable, idempotent aggregate.
//  JS has no rmdir leaf (checkout.js note), so the empty-dir collapse is a
//  no-op; the per-path `del` rows ARE the durable effect (already emitted).
function delSweep(row, ctx) {
  const q = ctx && ctx.queue;
  if (q && q.path && row.offset != null) {
    //  Fold ALL rows before this fold row, counting the delete leaves
    //  (`get <p>?#<old>`: empty query + a fragment).  A sentinel markerVerb that
    //  never appears makes seekBack miss, so the fold spans the whole queue head
    //  → here (core/barrier.js).  Report-only: JS has no rmdir leaf, so the
    //  empty-dir collapse is a no-op; the per-path `del` rows are the effect.
    const res = barrier.fold(q.path, row.offset, "::root", function (acc, r) {
      const u = new URI(r.uri);
      if ((u.query || "") === "" && (u.fragment || "") !== "") acc++;   // a delete
      return acc;
    }, 0);
    ctx._get.delSwept = res.acc;
  }
}

//  Edge flush comparator (native get layout): pulled-commit `post` rows first
//  in push order (newest-first by author ts), then file rows — new+upd
//  interleaved lex by path, THEN del lex by path.  A stable partition keeps the
//  post block put; the file groups sort by uri.
//  JAB-003: get's output spans MANY dispatches (seed banner + post rows, then a
//  per-leaf row each), so accumulate descriptors on ctx._get and flush ONE
//  sorted hunk at the DELSWEEP terminal — a per-call adapter can't span these.
function getOut(ctx) {
  return {
    banner: function (verb, uri, ts) { ctx._get.head = { verb: verb, uri: uri, ts: ts }; },
    row: function (uri, verb, ts, tag) {
      (ctx._get.rows || (ctx._get.rows = [])).push(
        { uri: uri, verb: verb, ts: ts, _post: !!(tag && tag._post) });
    },
  };
}

//  Flush the accumulated rows as ONE get hunk (uri `get:<head>`): the header row
//  first, then sortGetRows (post, new/upd lex, del lex).  Idempotent guard.
function flushGet(ctx) {
  const g = ctx && ctx._get;
  if (!g || !g.head || !ctx.sink || g._flushed) return;
  g._flushed = true;
  const out = hunkrows(ctx.sink, "get:" + g.head.uri);
  out.row(g.head.uri, g.head.verb, g.head.ts);
  for (const r of sortGetRows(g.rows || [])) out.row(r.uri, r.verb, r.ts);
  out.done();
}

function sortGetRows(rows) {
  const post = [], nu = [], del = [];
  for (const r of rows) {
    if (r._post) post.push(r);
    else if (r.verb === "del") del.push(r);
    else nu.push(r);
  }
  const byUri = function (a, b) { return a.uri < b.uri ? -1 : a.uri > b.uri ? 1 : 0; };
  nu.sort(byUri); del.sort(byUri);
  return post.concat(nu, del);
}

//  jab injects module.exports for a required module; the loop requires this as
//  the `get` verb handler.  No self-run tail (JSQUE-009: handler, not main()).
