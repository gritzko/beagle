//  post.js — `be post` as a loop HANDLER (JSQUE-012; was the JS-051 one-shot).
//  Converted from `main();` to `module.exports = handle(row, ctx)`: the commit
//  is a BARRIER (core/barrier.js) — a boundary marker + the decision leaf rows
//  + a fold row, back-scanned and folded post-order (blobs->subtrees->root
//  tree->commit->ref-advance).  The refuse PRE-FLIGHT runs as a pre-loop gate
//  BEFORE the first store write (no orphans).  Output via ctx.out; sibling libs
//  via relative ./ requires (JSQUE-008).  Pure JS over libabc+libdog ONLY.
//
//  SCOPE — FF-or-refuse (unchanged from JS-051).  A non-FF advance throws
//  POSTNOFF; an in-scope `patch` row throws POSTSCOPE; the descendant cascade
//  is out of scope.  PARALLEL/RESUME follow-up: the keeper-pack idempotency
//  guard (no double pack-write on a barrier re-fold) is NOT built here.
//
//  Usage:  jab be/loop.js post '#msg' | post msg… | post -m msg  (SUT=loop)

"use strict";

//  JSQUE-008: sibling libs via relative require ("./lib/X.js"/"./core/X.js"),
//  resolved against this module's own dir — robust under the resident loop.
const be       = require("./lib/be.js");
const wtlog    = require("./lib/wtlog.js");
const store    = require("./lib/store.js");
const decideM  = require("./lib/decide.js");
const commitM  = require("./lib/commit.js");
const conflict = require("./lib/conflict.js");
const dag      = require("./lib/dag.js");
const ulog     = require("./lib/ulog.js");
const pathlib  = require("./lib/path.js");
const shalib   = require("./lib/sha.js");
const barrier  = require("./core/barrier.js");
const join = pathlib.join;
const isFullSha = shalib.isFullSha;

//  --- author identity from <store>/.be/config (TOML) ---------------------
//  Mirror SNIFF.exe.c: `[user] name/email` -> `<name> <<email>>`.  READ only.
function readConfigValue(text, section, key) {
  const lines = text.split("\n");
  let inSec = false;
  for (let raw of lines) {
    const line = raw.replace(/#.*$/, "").trim();
    if (!line) continue;
    const h = /^\[(.+)\]$/.exec(line);
    if (h) { inSec = (h[1].trim() === section); continue; }
    if (!inSec) continue;
    const kv = /^([A-Za-z0-9_.-]+)\s*=\s*"(.*)"\s*$/.exec(line);
    if (kv && kv[1] === key) return kv[2];
  }
  return undefined;
}

function authorIdent(storePath) {
  let text = "";
  try {
    const p = join(join(storePath, ".be"), "config");
    const st = io.stat(p);
    const fd = io.open(p, "r");
    try {
      const b = io.buf(st.size + 16);
      io.readAll(fd, b, st.size);
      text = utf8.Decode(b.data());
    } finally { io.close(fd); }
  } catch (e) { text = ""; }
  const name = readConfigValue(text, "user", "name");
  const email = readConfigValue(text, "user", "email");
  if (!name && !email) return "sniff <sniff@dogs>";
  let s = "";
  if (name) s += name + " ";
  s += "<" + (email || "") + ">";
  return s;
}

//  --- epoch seconds from a ron60 stamp (LOCAL-tz mktime, like native) -----
function epochSecOf(stamp) {
  const r = BigInt(stamp);
  const d = (k) => Number((r >> BigInt(k * 6)) & 63n);
  const yy = d(9) * 10 + d(8);
  const mon = d(7), day = d(6) * 10 + d(5);
  const hh = d(4), mm = d(3), ss = d(2);
  const dt = new Date(2000 + yy, mon - 1, day, hh, mm, ss, 0);   // local tz
  return Math.floor(dt.getTime() / 1000);
}

//  --- message (from the seed-pinned positional args, JSQUE-004) ----------
//  A `#msg` arg (leading `#` shed), `-m msg`, or bare trailing words joined.
//  Empty -> POSTNOMSG.  A single trailing `!` (forget) is shed; `!!` -> BANG.
function parseMessage(args, flags) {
  let msg, sawFrag = false;
  const words = [];
  const all = (flags || []).concat(args || []);
  for (let i = 0; i < all.length; i++) {
    const a = all[i];
    if (a === "-m") { msg = all[++i]; sawFrag = true; continue; }
    if (a[0] === "-") continue;             // other flags
    if (a[0] === "#") { msg = a.slice(1); sawFrag = true; continue; }
    words.push(a);
  }
  if (msg == null && words.length) { msg = words.join(" "); sawFrag = true; }
  if (msg == null) return { msg: undefined };
  if (msg.length && msg[msg.length - 1] === "!") {
    msg = msg.slice(0, -1);
    if (msg.length && msg[msg.length - 1] === "!")
      throw "POSTBANG: commit message may not end in `!`";
  }
  return { msg: msg, sawFrag: sawFrag };
}

//  --- ref advance CAS ----------------------------------------------------
//  Resolve the branch's current REFS tip (expected-old), then conditionally
//  append the new tip — a divergence between resolve and set is a lost race.
function advanceRef(reader, shard, branchKey, expectedOld, newSha) {
  const cur = reader.resolveRef(branchKey || "");
  if ((cur || "") !== (expectedOld || ""))
    throw "POSTNOFF: REFS for `?" + (branchKey || "") +
          "` advanced concurrently — retry";
  store.set(shard, branchKey || "", newSha);
}

//  JSQUE-012: `be post` as a loop HANDLER.  The wt path rides the ROW; the
//  message + flags are seed-pinned and ride ctx (ctx.args/ctx.flags — the
//  queue round-trip carries only ts/verb/uri); output goes through ctx.out
//  (one flush at the loop edge).  No process.argv, no self-run tail.
module.exports = function handle(row, ctx) {
  const args  = (ctx && ctx.args)  || [];
  const flags = (ctx && ctx.flags) || [];
  const out   = ctx && ctx.out;
  const m = parseMessage(args, flags);
  const force = flags.indexOf("--force") >= 0;

  const info = (ctx && ctx.repo) || be.find((row && row.uri) || undefined);
  const wtl = wtlog.open(info);
  const reader = store.open(info.storePath, info.project);

  //  ===== PRE-FLIGHT GATE (refuse before the first store write) ==========
  //  All refuse-capable checks run here, BEFORE the commit barrier opens —
  //  a refusal leaves the store byte-identical (POST-017 all-or-nothing).

  //  1. Detached guard (DIS-009): a `?<sha>` cur-tip has no branch.
  const cur = wtl.curTip();
  if (cur && cur.query && cur.query.length === 40 && isFullSha(cur.query) &&
      (!cur.sha || cur.query === cur.sha) && !curHasFragment(wtl)) {
    throw "POSTDET: refusing on detached wt — re-attach (be get ?<branch>)";
  }

  //  2. Parent / branch resolve (cur's branch is the commit's branch).
  const branchKey = (cur && cur.branch) || "";
  const parent = (cur && cur.sha && isFullSha(cur.sha)) ? cur.sha : undefined;
  const haveBaseline = !!(cur && cur.sha);

  //  3. Classify the change-set into keep/unlink/add decisions.
  const dres = decideM.decide(info, wtl, reader);
  if (dres.hasPatch)
    throw "POSTSCOPE: a `patch` row is in scope — absorbed-patch trees are " +
          "out of scope for the JS FF post (use native `be post`)";

  //  4. FF pre-flight (POSTNOFF): a REFS tip != parent must be an ancestor.
  let expectedOld = "";
  if (haveBaseline && parent) {
    const tip = reader.resolveRef(branchKey || "");
    if (tip && isFullSha(tip)) {
      expectedOld = tip;
      if (tip !== parent && !dag.isAncestor(reader, tip, parent))
        throw "POSTNOFF: branch `?" + (branchKey || "") + "` advanced — " +
              "non-FF post refused (reconcile with native `be patch`)";
    }
  }

  //  5. Conflict pre-scan (POST-017): a tracked `add` carrying a complete
  //  WEAVE conflict triple aborts before any store write.  `--force` skips.
  if (!force) {
    for (const d of dres.decisions) {
      if (d.verb !== "add") continue;
      const bytes = commitM.readAddBytes(info.wt, d);
      if (bytes && conflict.hasConflictMarker(bytes))
        throw "POSTCFLCT: conflict marker in tracked file " + d.path +
              " (re-run with --force to override)";
    }
  }

  //  6. Empty-commit refuse (POSTNONE): the new root tree equals baseline's.
  //  Pre-build the tree (no store write yet) to compare; the barrier re-folds
  //  the SAME decisions durably below.
  const pre = commitM.buildTree(dres.decisions);
  const rootTreeSha = pre.rootTreeSha || commitM.EMPTY_TREE_SHA;
  if (haveBaseline && dres.haveBase && dres.baseTreeSha &&
      rootTreeSha === dres.baseTreeSha)
    throw "POSTNONE: no changes since base";

  //  7. Message resolution (after empty-commit so a no-op reports POSTNONE).
  if (m.msg == null || m.msg === "")
    throw "POSTNOMSG: a commit message is required (`be post '#msg'`)";

  //  ===== COMMIT BARRIER (core/barrier.js: marker + back-scan fold) ======
  //  The commit needs the WHOLE decision set at once (root tree depends on
  //  every leaf), so it is a JOIN, not a per-unit job.  Emit a boundary
  //  marker + one leaf row per decision + a `commit` fold row into a scratch
  //  barrier ULOG (post-order: the fold sits after all its inputs), then
  //  back-scan (marker, here) and fold the leaves bottom-up into the tree +
  //  commit + ref-advance.  Durable re-read => idempotent over the range.
  const tail = wtlogTail(wtl);
  const stamp = ulog.nowAfter(tail);
  const author = authorIdent(info.storePath);

  const bpath = barrierPath(info);
  const leaves = dres.decisions.map(function (d) {
    //  Each leaf is a branch-free decision row `<verb> path[?<old>]#<sha>`.
    let uri = d.path;
    if (d.verb === "add") uri += (d.oldSha ? "?" + d.oldSha : "") + "#" + d.sha;
    else if (d.verb === "keep") uri += "#" + d.sha;
    return { verb: d.verb, uri: uri };
  });
  barrier.emit(bpath, "postmark", "?" + branchKey, leaves,
               "commit", "?" + branchKey + "#" + stamp.toString());
  const foldOff = lastFoldOffset(bpath, "commit");

  //  The fold folds the (marker, here) leaf range into the commit.  We carry
  //  the already-classified decisions (the durable leaf rows mirror them);
  //  the fold re-reads the range to confirm the count, then builds the tree
  //  post-order (blobs->subtrees->root) + the commit object.
  const folded = barrier.fold(bpath, foldOff, "postmark",
    function (acc, leaf) { acc.count++; return acc; }, { count: 0 });
  if (folded.count !== leaves.length)
    throw "POSTFOLD: barrier range mismatch (" + folded.count + " != " +
          leaves.length + ")";

  //  Build the tree (post-order bodies) + the commit object from the folded
  //  decision set, then drop the scratch barrier ULOG.
  const tb = commitM.buildTree(dres.decisions);
  cleanupBarrier(bpath);

  const commit = commitM.buildCommit({
    treeSha: tb.rootTreeSha || commitM.EMPTY_TREE_SHA,
    parents: parent ? [parent] : [],
    author: author,
    epochSec: epochSecOf(stamp),
    message: m.msg
  });

  //  Write the keeper pack-log (+ idx) — the FIRST store mutation.  PARALLEL
  //  follow-up: an idempotency guard (skip a re-pack on a barrier re-fold).
  commitM.writePack(reader.shard, info.wt,
                    commit.body, tb.rootTreeSha, tb.bodies, dres.decisions);

  //  Advance the ref (resolve expected-old + conditional store.set).
  advanceRef(reader, reader.shard, branchKey, expectedOld, commit.sha);

  //  Append the `post` row (`?<branch>#<sha>`) at the stamp, then restamp
  //  every `add` file so it reads clean under the new baseline.
  ulog.append(info.bePath,
              [{ verb: "post", uri: "?" + (branchKey || "") + "#" + commit.sha,
                 ts: stamp }]);
  for (const d of dres.decisions) {
    if (d.verb !== "add") continue;
    try { io.setMtime(join(info.wt, d.path), stamp); } catch (e) {}
  }

  //  The `post:` banner (POST-018) via ctx.out: a commit confirmation row,
  //  then the per-file change rows (add/mod/del), matching native's table.
  emitBanner(out, commit.sha, m.msg, dres.decisions, stamp);
  //  Commit barrier leaf: no further fan-out.
};

//  --- barrier scratch ULOG (the commit JOIN) ----------------------------
//  A primary `.be/` dir hosts `.be/post.barrier`; a secondary `.be` FILE has
//  no dir, so scratch under /tmp keyed by bePath (unlinked after the fold).
function barrierPath(info) {
  const bp = info.bePath || "";
  if (bp.slice(-6) === "/wtlog") return bp.slice(0, -6) + "/post.barrier";
  const key = bp.split("/").join("_");
  return "/tmp/.bepost.barrier." + (io.getenv("USER") || "x") + "." + key;
}

//  The offset of the newest `verb` row in the barrier ULOG (the fold row).
function lastFoldOffset(path, verb) {
  let off = -1;
  ulog.each(path, function (log) { if (log.verb === verb) off = log.offset; });
  return off;
}

function cleanupBarrier(path) {
  try { io.unlink(path); } catch (e) {}
}

//  Whether the cur-tip row carries a non-empty fragment (a trunk-state
//  `?#<sha>` is NOT detached — only a bare `?<sha>` query is).
function curHasFragment(wtl) {
  for (let i = wtl.rows.length - 1; i >= 0; i--) {
    const r = wtl.rows[i];
    if (r.verb !== "get" && r.verb !== "post") continue;
    return !!(r.uri.fragment && r.uri.fragment.length);
  }
  return false;
}

//  The wtlog tail ts (for the monotonic stamp bump) — the last row's ts.
function wtlogTail(wtl) {
  return wtl.rows.length ? wtl.rows[wtl.rows.length - 1].ts : 0n;
}

//  Banner via ctx.out: the `post post:` header, the commit row
//  `post ?<hashlet8>#<subject>`, then per-file `<verb> <path>` rows (ts=0n →
//  blank-date column, like native).  add->`add`, modify->`mod`, unlink->`del`.
function emitBanner(out, sha, message, decisions, stamp) {
  out.raw(render0(out, "post", "post:", stamp));      // dated header
  const subject = subjectOf(message);
  out.row("?" + sha.slice(0, 8) + (subject ? "#" + subject : ""), "post", stamp);
  for (const d of decisions) {
    let v;
    if (d.verb === "unlink") v = "del";
    else if (d.verb === "add") v = d.oldSha ? "mod" : "add";
    else continue;                          // keep rows are not reported
    out.row(d.path, v, 0n);                  // ts=0n → blank-date column
  }
}

//  The header line `<date7> post post:` — emit.row would columnise `post:` as
//  a uri; we want the dated header verbatim, so render it with the same
//  dateCol/verbCol the sink uses and push via out.raw (status's framing path).
function render0(out, verb, text, ts) {
  const render = require("./lib/render.js");
  return render.dateCol(ts) + " " + render.verbCol(verb) + " " + text;
}

function subjectOf(msg) {
  let i = 0;
  while (i < msg.length && (msg[i] === "\n" || msg[i] === "\r")) i++;
  let j = i;
  while (j < msg.length && msg[j] !== "\n" && msg[j] !== "\r") j++;
  return msg.slice(i, j);
}
