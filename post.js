//  post.js — `be post` as a pure-JS extension (JS-051).  Reproduces native
//  `be post '#msg'` on the LOCAL path output-equivalently: classify the
//  staged change-set into keep/unlink/add decisions, build the git tree +
//  commit object, write a keeper pack-log (+ idx), FF-advance the ref,
//  append the `post` row + restamp the files, and print the `post:` banner.
//  Pure JS over JABC + bin/lib/* (libabc+libdog ONLY; no keeper/graf/sniff
//  binding).
//
//  SCOPE — FF-or-refuse.  We do NOT touch PATCH.  A non-FF advance throws
//  POSTNOFF (native rebases via graf, unbindable here); an in-scope `patch`
//  row throws (the absorbed-"theirs" 5th merge input is out of scope); the
//  descendant cascade is ignored.  All refuse-capable checks (detached,
//  conflict, empty-commit, FF) run BEFORE the pack is opened — no orphans.
//
//  Usage:  be post '#msg'   |   be post msg…   |   be post -m msg

"use strict";

const self = process.argv[1];
const here = self.slice(0, self.lastIndexOf("/"));
const be       = require(here + "/lib/be.js");
const wtlog    = require(here + "/lib/wtlog.js");
const store    = require(here + "/lib/store.js");
const decideM  = require(here + "/lib/decide.js");
const commitM  = require(here + "/lib/commit.js");
const conflict = require(here + "/lib/conflict.js");
const dag      = require(here + "/lib/dag.js");
const ulog     = require(here + "/lib/ulog.js");
const pathlib  = require(here + "/lib/path.js");
const render   = require(here + "/lib/render.js");
const shalib   = require(here + "/lib/sha.js");
const join = pathlib.join;
const isFullSha = shalib.isFullSha;
const dateCol = render.dateCol, verbCol = render.verbCol,
      writeStdout = render.writeStdout;

//  --- author identity from <store>/.be/config (TOML) ---------------------
//  Mirror SNIFF.exe.c: `[user] name/email` → `<name> <<email>>`.  The store
//  config is bootstrapped from `git config --global` on the first post; we
//  only READ it.  Falls back to the legacy sniff sentinel when absent.
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

//  --- epoch seconds from a ron60 stamp -----------------------------------
//  Native: at_ts_of_ron60 = mktime(RONToTime(stamp), isdst=-1) — a LOCAL-tz
//  calendar split, then mktime → epoch seconds (sub-second dropped).  We
//  decode the same ron60 fields (ulog.ronToMs layout: YY M DD hh mm ss lll)
//  and feed them to a LOCAL-time Date (JS Date(y,mon,d,…) is local, == mktime).
function epochSecOf(stamp) {
  const r = BigInt(stamp);
  const d = (k) => Number((r >> BigInt(k * 6)) & 63n);
  const yy = d(9) * 10 + d(8);
  const mon = d(7), day = d(6) * 10 + d(5);
  const hh = d(4), mm = d(3), ss = d(2);
  const dt = new Date(2000 + yy, mon - 1, day, hh, mm, ss, 0);   // local tz
  return Math.floor(dt.getTime() / 1000);
}

//  --- message (CLI) ------------------------------------------------------
//  Native folds trailing words into a `#frag`; `-m <msg>` is the legacy
//  form.  We accept either: a `#msg` argument (leading `#` shed), `-m msg`,
//  or bare trailing words joined by a space.  Empty → POSTNOMSG.  Trailing
//  `!` = the forget modifier — out of scope for JS-051's FF path (foster);
//  we shed it and ignore (a literal `!!`-ending message is the BANG case,
//  refused).  Reuse (`#` with empty msg) is unsupported here (POSTNOMSG).
function parseMessage(argv) {
  let msg, sawFrag = false;
  const words = [];
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "-m") { msg = argv[++i]; sawFrag = true; continue; }
    if (a[0] === "-") continue;             // other flags
    if (a[0] === "#") { msg = a.slice(1); sawFrag = true; continue; }
    words.push(a);
  }
  if (msg == null && words.length) { msg = words.join(" "); sawFrag = true; }
  if (msg == null) return { msg: undefined };
  //  forget modifier: a single trailing `!` (DOG_BANG_FRAG).  Out of scope
  //  for the FF path; shed it.  A surviving trailing `!` is the BANG refuse.
  if (msg.length && msg[msg.length - 1] === "!") {
    msg = msg.slice(0, -1);
    if (msg.length && msg[msg.length - 1] === "!")
      throw "POSTBANG: commit message may not end in `!`";
  }
  return { msg: msg, sawFrag: sawFrag };
}

//  --- ref advance CAS ----------------------------------------------------
//  Resolve the branch's current REFS tip (expected-old), then conditionally
//  append the new tip — store.set is the append, the CAS is the resolve +
//  the FF pre-flight already passed, so a divergence between resolve and
//  set is a lost race (POSTNOFF-class; we report it, don't force).
function advanceRef(reader, shard, branchKey, expectedOld, newSha) {
  const cur = reader.resolveRef(branchKey || "");
  if ((cur || "") !== (expectedOld || ""))
    throw "POSTNOFF: REFS for `?" + (branchKey || "") +
          "` advanced concurrently — retry";
  store.set(shard, branchKey || "", newSha);
}

function main() {
  const argv = process.argv.slice(2);
  const m = parseMessage(argv);

  const wt = io.cwd();
  const info = be.find(wt);
  const wtl = wtlog.open(info);
  const reader = store.open(info.storePath, info.project);

  //  1. Detached guard (DIS-009): a `?<sha>` cur-tip (40-hex query, empty
  //  fragment) has no branch to record against — refuse.
  const cur = wtl.curTip();
  if (cur && cur.query && cur.query.length === 40 && isFullSha(cur.query) &&
      (!cur.sha || cur.query === cur.sha) && !curHasFragment(wtl)) {
    throw "POSTDET: refusing on detached wt — re-attach (be get ?<branch>)";
  }

  //  2. Parent / branch resolve.  The cur tip's branch is the commit's
  //  branch (empty = trunk); its sha is the single parent.
  const baseTip = wtl.baselineTip();
  const branchKey = (cur && cur.branch) || "";
  const parent = (cur && cur.sha && isFullSha(cur.sha)) ? cur.sha : undefined;
  const haveBaseline = !!(cur && cur.sha);

  //  3. Classify the change-set.
  const dres = decideM.decide(info, wtl, reader);
  if (dres.hasPatch)
    throw "POSTSCOPE: a `patch` row is in scope — absorbed-patch trees are " +
          "out of scope for the JS FF post (use native `be post`)";

  //  4. FF pre-flight: when the branch has a REFS tip != parent, it must be
  //  an ancestor of parent (FF) — else native rebases via graf, which we
  //  refuse (POSTNOFF).  Runs BEFORE the pack open.
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
  const force = argv.indexOf("--force") >= 0;
  if (!force) {
    for (const d of dres.decisions) {
      if (d.verb !== "add") continue;
      const bytes = commitM.readAddBytes(info.wt, d);
      if (bytes && conflict.hasConflictMarker(bytes))
        throw "POSTCFLCT: conflict marker in tracked file " + d.path +
              " (re-run with --force to override)";
    }
  }

  //  6. Build trees.  Empty / all-unlink → empty tree.
  const tb = commitM.buildTree(dres.decisions);
  const rootTreeSha = tb.rootTreeSha || commitM.EMPTY_TREE_SHA;

  //  7. Empty-commit refuse (POSTNONE): the new root tree equals the
  //  baseline's tree → nothing to record.  Skip on a fresh repo.
  if (haveBaseline && dres.haveBase && dres.baseTreeSha &&
      rootTreeSha === dres.baseTreeSha)
    throw "POSTNONE: no changes since base";

  //  8. Message resolution (after the empty-commit refuse so a no-op post
  //  reports POSTNONE, not POSTNOMSG).  No reuse path in JS-051.
  if (m.msg == null || m.msg === "")
    throw "POSTNOMSG: a commit message is required (`be post '#msg'`)";

  //  9. Commit object (single per-commit stamp drives the author epoch AND
  //  the post row + file restamps — all in lockstep, like native).
  const tail = wtlogTail(wtl);
  const stamp = ulog.nowAfter(tail);
  const author = authorIdent(info.storePath);
  const commit = commitM.buildCommit({
    treeSha: rootTreeSha,
    parents: parent ? [parent] : [],
    author: author,
    epochSec: epochSecOf(stamp),
    message: m.msg
  });

  //  10. Write the keeper pack-log (+ idx) — the FIRST store mutation.
  commitM.writePack(reader.shard, info.wt,
                    commit.body, tb.rootTreeSha, tb.bodies, dres.decisions);

  //  11. Advance the ref (resolve expected-old + conditional store.set).
  advanceRef(reader, reader.shard, branchKey, expectedOld, commit.sha);

  //  12. Append the `post` row (`?<branch>#<sha>`) at the stamp, then
  //  restamp every `add` file so it reads clean under the new baseline.
  ulog.append(info.bePath,
              [{ verb: "post", uri: "?" + (branchKey || "") + "#" + commit.sha,
                 ts: stamp }]);
  for (const d of dres.decisions) {
    if (d.verb !== "add") continue;
    try { io.setMtime(join(info.wt, d.path), stamp); } catch (e) {}
  }

  //  13. The `post:` banner (POST-018): a commit confirmation row, then the
  //  per-file change rows (add/mod/del), matching native's HUNK table.
  emitBanner(commit.sha, m.msg, dres.decisions, stamp);
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

//  Banner: `<ts> post post:` headerless, then the commit row
//  `<ts> post ?<hashlet8>#<subject>` and per-file `<ts> <verb> <path>` rows.
//  add → `add`, modify (oldSha present) → `mod`, unlink → `del`.  Files in
//  decision (lex) order, matching native's post_walk_decisions.
function emitBanner(sha, message, decisions, stamp) {
  const dc = dateCol(stamp);
  const blank = dateCol(0n);                 // file rows carry ts=0 (native)
  let body = dc + " " + verbCol("post") + " post:\n";
  const subject = subjectOf(message);
  body += dc + " " + verbCol("post") + " ?" + sha.slice(0, 8) +
          (subject ? "#" + subject : "") + "\n";
  for (const d of decisions) {
    let v;
    if (d.verb === "unlink") v = "del";
    else if (d.verb === "add") v = d.oldSha ? "mod" : "add";
    else continue;                          // keep rows are not reported
    body += blank + " " + verbCol(v) + " " + d.path + "\n";
  }
  writeStdout(body);
}

function subjectOf(msg) {
  let i = 0;
  while (i < msg.length && (msg[i] === "\n" || msg[i] === "\r")) i++;
  let j = i;
  while (j < msg.length && msg[j] !== "\n" && msg[j] !== "\r") j++;
  return msg.slice(i, j);
}

main();
