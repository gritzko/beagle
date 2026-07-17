//  post.js — `be post` as a loop HANDLER (JSQUE-012; was the JS-051 one-shot).
//  Converted from `main();` to `module.exports = handle(row, ctx)`: the commit
//  is a JOIN over the whole in-memory decision set (JSQUE-020: was a durable
//  back-scan barrier), built post-order (blobs->subtrees->root
//  tree->commit->ref-advance).  The refuse PRE-FLIGHT runs as a pre-loop gate
//  BEFORE the first store write (no orphans).  Output via ctx.out; sibling libs
//  via relative ./ requires (JSQUE-008).  Pure JS over libabc+libdog ONLY.
//
//  SCOPE — FF-or-refuse.  A non-FF advance throws POSTNOFF; the descendant
//  cascade is out of scope.  DIS-057: an in-scope `patch` row is now CONSUMED
//  (the unified classifier reads a patch-derived file as pat/mrg/cnf, the
//  consumer commits its merged content) — no more POSTSCOPE throw.
//  PARALLEL/RESUME follow-up: the keeper-pack idempotency guard (no double
//  pack-write on a re-run) is NOT built here.
//
//  Usage:  jab be/loop.js post '#msg' | post msg… | post -m msg  (SUT=loop)

"use strict";

//  JSQUE-008: sibling libs via relative require ("./lib/X.js"/"./core/X.js"),
//  resolved against this module's own dir — robust under the resident loop.
//  JSQUE-016: by-verb reorg — shared/ kernel + core/ via ../../ ; post's OWN
//  fold helpers (decide/commit) are siblings renamed fold-* (leaf-vs-fold).
const wtlog    = require("../../shared/wtlog.js");
const store    = require("../../shared/store.js");
const branchlib = require("../../shared/branch.js"); // DIS-061: the ONE branch codec
const decideM  = require("./fold-decide.js");
const commitM  = require("./fold-commit.js");
const conflict = require("../../shared/conflict.js");
const dag      = require("../../shared/dag.js");
const ulog     = require("../../shared/ulog.js");
const uri      = require("../../shared/uri.js");   // JAB-005: total arg parse
const pathlib  = require("../../shared/util/path.js");
//  BE-030: worktree fs paths go THROUGH resolve() — wtpath is the
//  resolve-backed, context-confined replacement for the old wtJoin.
const discover = require("../../core/discover.js");
const wtpath = discover.wtpath;
//  DIS-062: THE resolver (RULE ZERO) — a local wt-target's base is its chash.
const resolveHash = require("../../core/resolve_hash.js").resolve_hash;
const shalib   = require("../../shared/util/sha.js");
const subs     = require("../../shared/subs.js");     // DIS-058 D6: sub enum
const submount = require("../../shared/submount.js"); // DIS-072: pin track URI
const getverb  = require("../get/get.js");            // POST-026: reuse get merge
const wire     = require("../../shared/wire.js");      // GIT-013: wire push
const servelib = require("../../shared/serve.js");     // POST-028: self-push gate
const relate   = require("../../shared/relate.js");    // GIT-016: verdict spine
const ingest   = require("../../shared/ingest.js");    // GIT-016: remote-track saver
//  JAB-003: TRUE-hunk output via the shared columnar->hunk adapter (ctx.sink),
//  retiring the ctx.out columnar path for post.
const hunkrows = require("../../shared/hunkrows.js");
//  BRO-030: the unified quad reporter (wiki/Status.mkd) — THE post banner
//  speaks quad rows (shared/quad.js model, view/quadrender.js rows).
const quadlib    = require("../../shared/quad.js");
const quadrender = require("../../view/quadrender.js");
const ambient  = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const join = pathlib.join;   // BE-011: wtJoin confines wt-opens
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

//  --- POST.mkd URI-slot parse (DIS-054) ----------------------------------
//  POST.mkd's 5 URI slots ride the positional args; `new URI(arg)` splits each
//  (CLAUDE.md: never bypass the URI parser).  parseSlots scans the args and
//  returns the populated slots:
//    host       a Host slot is present (a wire PUSH target — GIT-013
//               pushRemote below).
//    hasQuery   a Query slot (`?branch`/`?..`/`?`/`?other`) is present.
//    query      the Query slot's target-branch token (raw, may be ""/".."/name).
//    narrow     the Path slot's narrow target (a `./path`/`dir/file`), or "".
//    fragment   a `#msg` riding the slot URI itself (`?other#msg`, `./p#msg`).
//  Precedence Host>Query>Path matches POST.mkd.  A bare-word message
//  (`fix the bug`) or a plain `#msg`/`-m` carries no slot, so the local-FF
//  commit path is untouched.
//
//  The bare `?` (trunk target) is special: its URI has an EMPTY query AND an
//  empty fragment, so it is detected by the raw `?`-prefixed form, not u.query.
function parseSlots(args) {
  const slots = { host: false, hostUri: "", wt: false, wtUri: "",
                  hasQuery: false, query: "", narrow: "", fragment: undefined };
  for (let a of args || []) {
    if (!a || a[0] === "#" || a[0] === "-") continue;   // #msg / -flag
    //  URI-015: a git scp-form remote recomposes to ssh:// (a Host slot below),
    //  else the total parse would degrade it to message prose.
    a = uri.fromGit(a);
    //  JAB-005: total parse — a non-URI arg (spaces) is free-form message
    //  text, not a slot; post's message picks it up.
    const u = uri.parse(a);
    if (typeof u === "string") continue;
    //  POST-028 (RULED 2026-07-16): a `file:` — or authority-less `be:` —
    //  target is a LOCAL store PUSH (Host slot, `jab receive-pack` serve);
    //  the query is the BRANCH want, the path a wt or a store shard.
    if (u.scheme === "file" || (u.scheme === "be" && !u.host)) {
      if (u.fragment !== undefined)
        throw "post `#msg` to a store target is not supported (commit first)";
      slots.host = true;
      slots.hostUri = a;
      if (a.indexOf("?") >= 0) { slots.hasQuery = true; slots.query = u.query || ""; }
      continue;
    }
    if (u.host) {
      //  DIS-062: a SCHEME-LESS authority is a LOCAL WORKTREE target (an
      //  explicit `//X[/sub]` operand, once loop.js's authorityRepo lets it
      //  through intact) — not a wire push; only ssh/https are Host below.
      if (!u.scheme) {
        slots.wt = true;
        slots.wtUri = a;
        continue;
      }
      //  GIT-013: a Host slot is a wire PUSH target (`ssh://host?br`,
      //  `https://host?br`).  Capture the raw URI + its branch query for postOne.
      slots.host = true;
      slots.hostUri = a;
      //  GIT-015 defect B: a `?` marker (even empty `?` = trunk) means a branch
      //  WAS selected; only a `?`-less host URI is the "no branch" refusal case.
      if (a.indexOf("?") >= 0) { slots.hasQuery = true; slots.query = u.query || ""; }
      continue;
    }
    //  Query slot: a non-empty query (`?branch`/`?..`/`?other`) OR the bare
    //  `?` trunk form (data is `?`, every slot empty — POST.mkd `?` row).
    if (u.query) {
      slots.hasQuery = true;
      slots.query = u.query;
      if (u.fragment) slots.fragment = u.fragment;
      continue;
    }
    if (isBareTrunkQuery(a, u)) {
      slots.hasQuery = true;
      slots.query = "";                     // trunk
      continue;
    }
    //  Path slot: a `./path`/`/abs`/`dir/file` narrow target — NOT a
    //  bare-word commit message (`fix`, `base`).
    if (isPathSlot(u.path)) {
      slots.narrow = u.path;
      if (u.fragment) slots.fragment = u.fragment;
    }
  }
  return slots;
}

//  The bare `?` (trunk) Query form: the whole arg is exactly `?` (so the URI
//  has empty query AND empty fragment, no path/host) — POST.mkd `?` row
//  ("advance trunk to the wt hash").
function isBareTrunkQuery(arg, u) {
  return arg === "?" && !u.query && !u.fragment && !u.path && !u.host;
}

//  --- message (from the seed-pinned positional args, JSQUE-004) ----------
//  A `#msg` arg (leading `#` shed), `-m msg`, a slot-URI fragment
//  (`?other#msg`/`./p#msg`, passed as `slotFrag`), or bare trailing words
//  joined.  Empty -> POSTNOMSG.  A single trailing `!` (forget) is shed; `!!`
//  -> BANG.  A bare word that is itself a URI slot (`?br`, `./p`) is NOT a
//  message word — parseSlots owns it — so skip any arg the URI parser splits
//  into a non-empty query / host / path-slot.
function parseMessage(args, flags, slotFrag) {
  let msg, sawFrag = false;
  const words = [];
  const all = (flags || []).concat(args || []);
  for (let i = 0; i < all.length; i++) {
    const a = all[i];
    //  URI-011: the context URI (arg 0) rides every call; authorityRepo scopes
    //  it and BLANKS a bare `//WT` authority to "" — that emptied slot is NOT a
    //  message word (else `:post 'msg'` in a `//WT` view → " msg", a leading space).
    if (a === "") continue;
    if (a === "-m") { msg = all[++i]; sawFrag = true; continue; }
    if (a[0] === "-") continue;             // other flags
    if (a[0] === "#") { msg = a.slice(1); sawFrag = true; continue; }
    if (isSlotArg(a)) continue;             // a ?br/.//host/dir-path slot arg
    words.push(a);
  }
  if (msg == null && slotFrag != null) { msg = slotFrag; sawFrag = true; }
  if (msg == null && words.length) { msg = words.join(" "); sawFrag = true; }
  if (msg == null) return { msg: undefined };
  if (msg.length && msg[msg.length - 1] === "!") {
    msg = msg.slice(0, -1);
    if (msg.length && msg[msg.length - 1] === "!")
      throw "commit message may not end in `!`";
  }
  return { msg: msg, sawFrag: sawFrag };
}

//  Is `a` a URI-slot arg (Query / Host / Path) rather than a message word?
function isSlotArg(a) {
  if (!a || a[0] === "#" || a[0] === "-") return false;
  a = uri.fromGit(a);          // URI-015: a scp remote is a Host slot, not a word
  const u = uri.parse(a);                   // JAB-005: total — non-URI => not a slot
  if (typeof u === "string") return false;
  if (u.host || u.query) return true;
  if (a === "?") return true;               // bare trunk
  return isPathSlot(u.path);
}

//  A path arg is the POST.mkd Path slot (a `./path`/`/abs`/`dir/file`
//  narrow target) — NOT a bare-word commit message (`fix`, `base`).  The
//  slot marker is a path separator: a leading `./`/`../`/`/` or an embedded
//  `/`.  A separator-free word is a message and never narrows.
function isPathSlot(path) {
  if (!path) return false;
  return path[0] === "/" || path.indexOf("./") === 0 ||
         path.indexOf("../") === 0 || path.indexOf("/") >= 0;
}

//  --- refusal text (RULING 2026-07-16) ------------------------------------
//  POSTNONE/POSTNOFF are C error codes — the JS post never emits them.  A
//  refusal is plain text spelling the issue: 1 line, max 64 chars.  `refuse`
//  keeps the target's name/uri when it fits the budget, drops it otherwise.
const REFUSE_MAX = 64;
function refuse(named, bare) {
  return named.length <= REFUSE_MAX ? named : bare;
}
//  The "nothing to do" refusal texts postSubs swallows for a clean sub (the
//  former POSTNONE class); a real conflict / non-FF refusal must bubble up.
const NONE_MARKS = ["already at cur's tip", "already contains cur's tip",
                    "no cur tip to", "is this worktree",
                    "no changes since base"];
function isNoneRefusal(msg) {
  for (const m of NONE_MARKS) if (msg.indexOf(m) >= 0) return true;
  return false;
}

//  --- ref advance CAS ----------------------------------------------------
//  Resolve the branch's current REFS tip (expected-old), then conditionally
//  append the new tip — a divergence between resolve and set is a lost race.
function advanceRef(reader, shard, branchKey, expectedOld, newSha) {
  const cur = reader.resolveRef(branchKey || "");
  if ((cur || "") !== (expectedOld || ""))
    throw refuse("REFS for `?" + (branchKey || "") +
                 "` advanced concurrently, retry",
                 "REFS advanced concurrently, retry");
  store.set(shard, branchKey || "", newSha);
}

//  --- DIS-054 Query slot: target-branch resolution -----------------------
//  POST.mkd Query row: `?branch`/`?..`/`?`/`?.` selects the branch the post
//  advances.  `` (`?`) is trunk; `..` is cur's PARENT (dirname of cur's
//  branch); `.` is cur's own branch; anything else is the named branch.  A
//  relative `..` on trunk has no parent → refused (the spec's `?..` needs a
//  child to climb from).
function resolveTarget(query, curBranch, title) {
  if (query === "" || query === "/") return "";        // trunk
  //  GET-047: `.`/`..`/`./child`/`../sib` via the ONE dot-path convention
  //  (shared/branch.js resolveRel) — get's `?./child` speaks the same family.
  const rel = branchlib.resolveRel(query, curBranch);
  if (rel !== null) return rel;
  //  DIS-061: normalize a named/absolute query (`?/jab/.beagle`) to the stored
  //  refs KEY via the ONE branch codec — no bogus literal-keyed ref row.
  return branchlib.key(branchlib.parse(query, title || ""));
}

//  --- DIS-054 Query bare-advance (`?branch`/`?..`/`?`, no commit) ---------
//  POST.mkd: "advance ?branch / parent / trunk to the wt (cur) hash".  No new
//  commit — just FF-move the target branch's REFS tip to cur's tip.  Cur is
//  UNTOUCHED (its REFS row + the wtlog cur-tracking stay).  Refuses:
//    * no cur tip to advance to (a fresh, never-committed wt);
//    * target already AT (or ahead containing) cur's tip;
//    * target's tip is not an ancestor of cur (a non-FF advance).
//  An ABSENT target branch is CREATED at cur's tip (the FF-from-nothing case).
function advanceBranch(reader, wtl, info, ctx, target, curBranch, parent,
                       haveBaseline) {
  if (!haveBaseline || !parent)
    throw refuse("no cur tip to advance `?" + target + "` to",
                 "no cur tip to advance to");
  //  DIS-061: `post ?<cur's own branch>` is THE standard way to advance your
  //  branch to the wt base (uniform ruling) — FF below, no own-branch refusal.

  const tip = reader.resolveRef(target);
  const expectedOld = (tip && isFullSha(tip)) ? tip : "";
  if (expectedOld) {
    if (expectedOld === parent)
      throw refuse("`?" + target + "` is already at cur's tip",
                   "the target is already at cur's tip");
    //  POST-027: a target already PAST cur (cur ⊏ target) is the "already
    //  at/past it" refusal — checked BEFORE the FF gate (else dead arm).
    if (dag.isAncestor(reader, parent, expectedOld))
      throw refuse("`?" + target + "` already contains cur's tip",
                   "the target already contains cur's tip");
    //  FF only: cur must descend the target's tip.
    if (!dag.isAncestor(reader, expectedOld, parent))
      throw refuse("`?" + target + "` can not be fast-forwarded",
                   "the target can not be fast-forwarded");
  }
  //  Move ONLY the target branch's REFS row; cur's REFS + wtlog cur-tracking
  //  are left untouched (a bare advance makes no commit and does not retie).
  advanceRef(reader, reader.shard, target, expectedOld, parent);
  //  Banner: a `post` row naming the advanced branch at cur's hashlet.
  //  DIS-060: the banner carries a ref-only ADDRESSING uri (`?<target>#<hashlet>`,
  //  as get's banner already does), NEVER a phantom `post:` scheme ([Nav]).
  if (ctx && ctx.sink) {
    const stamp = ulog.nowAfter(wtlogTail(wtl));
    const refUri = URI.make(undefined, undefined, undefined, target, parent.slice(0, 8));
    const out = hunkrows(ctx.sink, refUri);
    out.row(refUri, "post", stamp);
    out.done();
  }
}

//  --- DIS-062 local worktree target (`//X[/sub]`) ------------------------
//  worktree.mkd: "all worktrees may get/post from/to each other directly" — a
//  scheme-less authority operand FF-advances that OTHER worktree's OWN base
//  (a wt is a branch of its own, DIS-076) to cur's tip; cur is untouched.
//  RULE ZERO: `resolveHash` (core/resolve_hash.js) is THE resolver — its
//  chash (step 5.5) IS the target's current base; NEVER wtlog.open(target)
//  by hand.  `discover.wtdir`/`discover.treeAt` (the SAME canonical routines
//  resolve_hash's own frame() calls) locate the target's on-disk anchor, only
//  to WRITE the advance row — not to re-derive the resolution.
function advanceWorktree(info, reader, ctx, targetUri, curTip, haveBaseline) {
  if (!haveBaseline || !curTip)
    throw refuse("no cur tip to advance `" + targetUri + "` to",
                 "no cur tip to advance to");
  const ctxUri = discover.navCwd(discover.ctxDir());
  const rh = resolveHash(ctxUri, targetUri);            // step 5.5: target's base
  const dir = discover.wtdir(targetUri);
  if (!dir) throw "WTNONE: `" + targetUri + "` anchors no local worktree";
  const target = discover.treeAt(dir);
  if (target.wt === info.wt)
    throw refuse("`" + targetUri + "` is this worktree",
                 "the target is this worktree");
  //  GIT-016 verdict spine (shared/relate.js) — the SAME local FF/ancestry
  //  read head.js's own DIS-062 peek uses; never a hand-rolled dag walk.
  const v = relate.verdict(reader, rh.chash, curTip);
  if (v.rel === "eq")
    throw refuse("`" + targetUri + "` is already at cur's tip",
                 "the target is already at cur's tip");
  if (v.rel === "ahead")
    throw refuse("`" + targetUri + "` already contains cur's tip",
                 "the target already contains cur's tip");
  if (v.rel === "unrelated")
    throw refuse("`" + targetUri + "` is unrelated to cur",
                 "the target is unrelated to cur");
  if (v.rel !== "behind") {
    //  diverged: the verdict spine already carries the ahead/behind rows —
    //  report the counts it provides (never a hand-rolled recount).
    const ab = " (" + v.ahead.length + " ahead, " + v.behind.length + " behind)";
    throw refuse("`" + targetUri + "` can not be fast-forwarded" + ab,
                 "the target can not be fast-forwarded" + ab);
  }
  //  POST-026: a `//WT` post FIRST materialises the dirties into the target wt —
  //  the SAME get merge fan-out `be get ?#<curTip>` runs, so a DIRTY target is
  //  3-way weave-merged (not clobbered) and an un-mergeable overlay / a conflict
  //  REFUSES (GETOVRL / GETCONF) BEFORE the base advances.  Reuse the get merge
  //  path (RULE ZERO — no hand-rolled checkout/dag): rh.chash is the target's
  //  current base = the merge OLD side / 3-way base, curTip the tree to reach.
  const targetK = store.open(target.storePath, target.project);
  getverb.mergeWorktreeTo({ info: target, k: targetK, tip: curTip,
                            oldTip: rh.chash, bePath: target.bePath });
  //  FF: append the advance row into the TARGET's OWN wtlog (a local receive);
  //  the branch it already tracks (if any) is preserved, only the tip moves.
  const targetWtl = wtlog.open(target);
  //  DIS-075: a DETACHED target stays detached — record `#<tip>` (query ABSENT),
  //  never trunk-shaped `?#<tip>`; an attached target keeps its branch query.
  const tatt = targetWtl.attachedBranch();
  const branch = tatt.branch || "";
  const stamp = ulog.nowAfter(wtlogTail(targetWtl));
  ulog.append(target.bePath,
              [{ verb: "post",
                 uri: URI.make(undefined, undefined, undefined,
                               tatt.detached ? undefined : branch, curTip),
                 ts: stamp }]);
  if (ctx && ctx.sink) {
    const refUri = URI.make(undefined, undefined, undefined, branch, curTip.slice(0, 8));
    const out = hunkrows(ctx.sink, refUri);
    out.row(refUri, "post", stamp);
    out.done();
  }
}

//  DIS-074: the TRACK = the recentmost `get` record (worktree.mkd:50-54); the
//  row selection mirrors wtlog.attachedBranch, which hides the row's authority.
function trackRow(wtl) {
  for (let i = wtl.rows.length - 1; i >= 0; i--) {
    const r = wtl.rows[i];
    if (r.verb !== "get") continue;
    const ref = wtlog.refOf(r.uri, r.local);
    if (!ref.sha && !ref.branch) continue;
    return r;
  }
  return undefined;
}

//  POST-026: does `targetUri` (a `//WT[/sub]` wt-target) resolve to THIS
//  worktree?  A MOUNTED sub's DIS-072 re-attach records its track as the
//  parent-mount address `//<wt>/<subpath>` (submount.trackUri) — which resolves
//  to the sub's OWN on-disk dir.  Detect the self-reference via the SAME
//  discover anchor routines advanceWorktree itself uses (RULE ZERO — no
//  hand-rolled path compare), so advanceTrack can fall through to the branch arm.
function selfWorktree(info, targetUri) {
  let dir;
  try { dir = discover.wtdir(targetUri); } catch (e) { return false; }
  if (!dir) return false;
  let t;
  try { t = discover.treeAt(dir); } catch (e) { return false; }
  return !!t && t.wt === info.wt;
}

//  DIS-074: bare-post ADVANCE arm — the implied target is the track: a `//X`
//  row (DIS-063 clone) -> advanceWorktree, else the attached branch (advanceBranch).
function advanceTrack(info, wtl, reader, ctx, curBranch, parent, haveBaseline) {
  const tr = trackRow(wtl);
  if (tr && tr.uri.scheme === undefined && tr.uri.authority) {
    //  DIS-074: shed the recorded `#rev` pin — resolve_hash step 5.5 must read
    //  the target's CURRENT base, not the base it had at clone time.
    const t = URI.make(undefined, tr.uri.authority, tr.uri.path || undefined,
                       undefined, undefined);
    //  POST-026: a mounted sub tracks its parent-mount `//<wt>/<subpath>`, which
    //  IS the sub's own dir — a bare post inside a clean-but-ahead sub must NOT
    //  self-refuse (`… is this worktree`).  Fall through to the sub's
    //  OWN branch (trunk) FF — worktree.mkd:53-54, the DIS-074 track advance.
    if (!selfWorktree(info, t))
      return advanceWorktree(info, reader, ctx, t, parent, haveBaseline);
  }
  return advanceBranch(reader, wtl, info, ctx, curBranch, curBranch, parent,
                       haveBaseline);
}

//  --- GIT-013 wire PUSH (Host slot) --------------------------------------
//  FF-push cur's tip to a remote branch over the JS wire (shared/wire.js).
//  `remoteUri` is the raw Host-slot arg; `branch` the be-side target (empty =
//  trunk → refs/heads/main); `tip` cur's 40-hex tip.  Flow: open receive-pack
//  advert (read the remote's old sha), enforce FF (remote tip must be an
//  ancestor of `tip` in the LOCAL DAG) BEFORE any wire write, build the thin
//  pack of objects the remote lacks via `keeper upload-pack`, then push.
function pushRemote(info, reader, ctx, remoteUri, branch, tip, hasQuery) {
  if (!tip || !isFullSha(tip))
    throw "no cur tip to push (commit first, then `be post ssh://host`)";
  //  GIT-019: ssh/local push runs on ONE receive-pack session — advertise once,
  //  verdict off that advert, send on the SAME fds.  http is stateless (no
  //  session): fall back to the classic advertRefs-inside-relate + wire.push.
  const cls = wire.classify(remoteUri, "receive-pack");
  const isHttp = !!cls.http;
  //  POST-028: a LOCAL target that resolves (wt-or-shard, GET-038 redirect) to
  //  this wt's own anchor shard is a SELF-push — refuse before any spawn.
  if (!isHttp && !cls.ssh) {
    let tgt; try { tgt = servelib.serveStore(remoteUri).shard; } catch (e) { tgt = ""; }
    if (tgt && tgt === store.shardDir(info.storePath, info.project))
      throw refuse("`" + remoteUri + "` is this worktree's own store",
                   "the target is this worktree's own store");
  }
  let session = null;
  if (!isHttp) {
    try { session = wire.pushSession(remoteUri); }
    catch (e) { throw (e && e.msg) ? e.msg : e; }
  }
  const sessAdv = session ? session.adv : undefined;

  //  GIT-016: the advertise->resolve->verdict spine (shared/relate.js) does the
  //  POSTNOREF gate + GIT-015 ref resolution + the LOCAL-DAG FF verdict.  A
  //  {code,msg} throw is re-raised as the SAME string message post always used.
  //  GIT-019: feed the session advert in so relate does NOT re-advertise.
  let rel;
  try { rel = relate.relate(reader, remoteUri, branch, tip, hasQuery,
                            undefined, sessAdv); }
  catch (e) { if (session) session.close(); throw (e && e.msg) ? e.msg : e; }
  const wireRef = rel.wireRef, old = rel.old, adv = rel.adv;

  //  FF gate (POST stays FF-only), behaviour-preserving over the spine verdict.
  //  GIT-019: a refusal flush-closes the session first (clean no-op exit).
  if (old) {
    if (rel.verdict.eq) {
      if (session) session.close();
      throw refuse("remote `" + wireRef + "` is already at cur's tip",
                   "the remote is already at cur's tip");
    }
    //  GIT-016: honest non-FF refusal — post is FF-only, so no force hint; the
    //  remote ancestry is unknown here (no fetch, verdict has no counts) so no
    //  diverged/unrelated guess and no ahead/behind report.
    if (!rel.verdict.ff) {
      if (session) session.close();
      throw refuse("remote `" + wireRef + "` can not be fast-forwarded",
                   "the remote can not be fast-forwarded");
    }
  }

  //  Build the thin pack (objects the remote lacks) from the local store via
  //  keeper's pack serve: want=tip, have=the remote's advertised tips.
  const serve = info.storePath + "?/" + (info.project || "");
  const haves = adv.refs.map(r => r.sha).filter(isFullSha);
  //  JS-100: a buildPushPack/send throw past the FF gate must not leak the
  //  receive-pack child — session.close() is the idempotent done-guarded reaper.
  try {
    const pack = wire.buildPushPack(serve, tip, haves);
    const updates = [{ ref: wireRef, neu: tip, old: old }];
    //  GIT-019: send on the SAME session (ssh/local); http stays the stateless
    //  GET-advert-then-POST-pack shape.
    if (session) session.send(updates, pack);
    else wire.push(remoteUri, updates, pack);
  } catch (e) { if (session) session.close(); throw e; }
  //  GIT-016: SAVE the just-advanced remote tip as a remote-tracking refs row
  //  (ingest.saveRemoteRef, the get/clone row shape) — the reflog bookkeeping.
  ingest.saveRemoteRef(reader.shard, remoteUri, tip);

  //  DIS-060: the banner carries the remote ADDRESSING uri (`<remote>?<br>#<tip>`)
  //  directly — a transport-schemed / ref uri, NEVER a phantom `post:` ([Nav]).
  if (ctx && ctx.sink) {
    const stamp = ulog.nowAfter(0n);
    //  GIT-015: remoteUri already carries the `?branch` slot (POSTNOREF gate
    //  guarantees it) — set only the `#tip` pin via the parse, never re-add
    //  `?branch` (that doubled it to `?main?main`).  URI-013 B10: compose the
    //  fragment through the URI class off remoteUri's own slots.
    const ru = new URI(remoteUri);
    const target = URI.make(ru.scheme, ru.authority, ru.path, ru.query, tip.slice(0, 8));
    const out = hunkrows(ctx.sink, target);
    out.row(target, "post", stamp);
    //  DIS-060: the change set across the pushed range (old..tip) — add/mod/del
    //  rows like the local post (was blank: only the confirmation row shown).
    for (const c of rangeChanges(reader, old, tip)) out.row(c.path, c.verb, 0n);
    out.done();
  }
}

//  DIS-060: the files changed across a pushed range (old..tip) — a NET tree diff
//  (add/mod/del), so the remote-post hunk shows the change set like the local
//  post + git-on-push.  old "" (fresh remote) → tip's first-parent as the base.
function rangeChanges(k, oldSha, tipSha) {
  const base = (oldSha && isFullSha(oldSha)) ? oldSha
             : (k.commitParents(tipSha) || [])[0];
  const oldMap = {};
  const ot = base && isFullSha(base) ? k.commitTree(base) : null;
  if (ot) k.readTreeRecursive(ot, function (l) { oldMap[l.path] = l.sha; });
  const rows = [], seen = {};
  const tt = k.commitTree(tipSha);
  if (tt) k.readTreeRecursive(tt, function (l) {
    seen[l.path] = 1;
    if (!(l.path in oldMap)) rows.push({ path: l.path, verb: "add" });
    else if (oldMap[l.path] !== l.sha) rows.push({ path: l.path, verb: "mod" });
  });
  for (const p in oldMap) if (!(p in seen)) rows.push({ path: p, verb: "del" });
  rows.sort(function (a, b) { return a.path < b.path ? -1 : a.path > b.path ? 1 : 0; });
  return rows;
}

//  JSQUE-012: `be post` as a loop HANDLER.  The wt path rides the ROW; the
//  message + flags are seed-pinned and ride ctx (ctx.args/ctx.flags — the
//  queue round-trip carries only ts/verb/uri); output goes through ctx.out
//  (one flush at the loop edge).  No process.argv, no self-run tail.
//
//  DIS-058 D6/D7/D9 (POST-ORDER sub recursion): the handler is `postTree` — it
//  first commits any dirty/advanced MOUNTED sub (post-order: child hash known
//  before the parent commits), records the child's new commit into the parent's
//  gitlink (a synthesised `put <sub>#<newsha>` bump → the existing fold-decide
//  gitlink-add path), then runs the single-repo body `postOne` on the parent.
//  A `--nosub` flag (out of D-scope but cheap) suppresses the descent.
//  JAB-004: plain-args POST — args ARE the commit MESSAGE (ride ctx.args verbatim,
//  no classifyArg to URI-split a `T: log: msg`).
function post() {
  const _be = (typeof be !== "undefined") ? be : null;
  const repo = (_be && _be.repo) || be.treeAt();
  //  Message + slots ride ctx.args as PLAIN args (put's synthetic-ctx pattern);
  //  `-m` and the like ride ctx.flags off be (loop split them out of args).
  const argv = [];
  for (let i = 0; i < arguments.length; i++) argv.push(String(arguments[i]));
  const ctx = {
    repo: repo, sink: _be && _be.sink,
    args: argv, flags: (_be && _be.flags) || [],
    //  POST-029: the run-start stamp (be.now, minted afresh per verb invocation)
    //  — ONE ts reused for the parent's and every folded sub's post row + the
    //  BE-011 mtime restamps; never re-sampled per repo.
    now: (_be && _be.now != null) ? BigInt(_be.now) : ron.now(),
  };
  return postTree(repo, ctx, { uri: argv.length ? argv[0] : "" });
}
post.jab = "args";
post._pushRemote = pushRemote;   // JS-100: test hook (child-leak repro)
module.exports = post;

//  postTree(info, ctx, row): recurse mounted subs (post-order), then postOne.
function postTree(info, ctx, row) {
  const flags = (ctx && ctx.flags) || [];
  //  GIT-015: a Host-slot wire push (`post ssh://host?ref`) is a single-repo wire
  //  op — it must NOT fan out over submodules (a sub's cur is not the super's
  //  tip; pushing it to the super's remote is a spurious non-FF → POSTNOFF).
  const isPush = parseSlots((ctx && ctx.args) || []).host;
  if (!isPush && flags.indexOf("--nosub") < 0) {
    //  POST-026: decide the recursion MODE ONCE over the WHOLE mounted tree
    //  (parent + every mounted descendant): ANY active put/delete anywhere ⇒
    //  SELECTIVE (commit only what's staged); zero ⇒ POST-ALL.  Computed at the
    //  OUTERMOST level and threaded down via ctx (the sub fan-out Object.assigns
    //  it), so no level recomputes its own mode (the per-repo `anyStaged` bug).
    if (ctx._selective === undefined) ctx._selective = anyPutDeep(info);
    postSubs(info, ctx);
  }
  return postOne(info, ctx, row);
}

//  SUBS-042: anyStaged(wtl) — the [Dirty] `anyPd` selective test: any in-scope
//  (since the last get/post) put/delete row? (the classify.js anyPd predicate).
function anyStaged(wtl) {
  let any = false;
  wtl.eachPutDelete(wtl.boundaries().pd, function () { any = true; });
  return any;
}

//  POST-026: the WHOLE-TREE put/delete probe — does `info` OR any MOUNTED
//  descendant carry an active (in-scope, since the last get/post) put/delete
//  row?  Two roles: (1) the GLOBAL mode decision at the top of the recursion
//  (ANY put/delete anywhere ⇒ selective, zero ⇒ post-all); (2) the per-sub
//  transitive selection probe (SUBS-052 nesting-blind fix: a change staged
//  >=2 mounts down is still seen).  An out-of-band `adv` descendant DOES NOT
//  pull a sub in — only a real put/delete does (the old `s.bucket === "adv"`
//  shortcut was the bug: an adv-only sub was force-committed in selective mode).
function anyPutDeep(info) {
  if (anyStaged(wtlog.open(info))) return true;
  let reader;
  try { reader = store.open(info.storePath, info.project); } catch (e) { return false; }
  const baseTip = wtlog.open(info).curTip();
  const baseTree = (baseTip && baseTip.sha && isFullSha(baseTip.sha))
        ? reader.commitTree(baseTip.sha) : "";
  if (!baseTree) return false;
  for (const s of subs.enumerate(info, reader, baseTree)) {
    if (!s.mounted) continue;
    let subInfo;
    try { subInfo = be.treeAt(wtpath(info.wt, s.path)); } catch (e) { continue; }
    if (anyPutDeep(subInfo)) return true;
  }
  return false;
}

//  SUBS-042: does an in-scope parent put/delete target this sub (its dir or a
//  file under it)? — a gitlink bump or staged sub-internal change keeps it in scope.
function subInParentScope(wtl, subPath) {
  const pfx = subPath + "/";
  let hit = false;
  wtl.eachPutDelete(wtl.boundaries().pd, function (r) {
    const p = (r.uri && r.uri.path) || "";
    if (p === subPath || p.indexOf(pfx) === 0) hit = true;
  });
  return hit;
}

//  postSubs: walk the parent's MOUNTED gitlink subs in `.gitmodules` order and,
//  for each one that is dirty/advanced, RECURSE a post into it FIRST (so its new
//  commit hash exists), then synthesise a `put <subpath>#<newtip>` gitlink bump
//  in the PARENT wtlog so the parent's commit records the advance (D7 auto).
//  SUBS-042: in selective mode commit only what's staged — skip a sub with nothing
//  in scope; commit-all mode descends into every dirty sub as before.
function postSubs(info, ctx) {
  const reader = store.open(info.storePath, info.project);
  const wtl = wtlog.open(info);
  //  POST-026: the GLOBAL tree-wide mode, decided once and threaded via ctx
  //  (postTree above), NOT the old per-repo `anyStaged(wtl)`.
  const selective = ctx._selective;
  const baseTip = wtl.curTip();
  const baseTree = (baseTip && baseTip.sha && isFullSha(baseTip.sha))
        ? reader.commitTree(baseTip.sha) : "";
  if (!baseTree) return;
  //  Enumerate the baseline gitlinks + classify each mount (pin vs sub cur tip).
  const subList = subs.enumerate(info, reader, baseTree);
  for (const s of subList) {
    if (!s.mounted) continue;                       // unmounted gitlink → skip
    const subWt = wtpath(info.wt, s.path);
    let subInfo;
    try { subInfo = be.treeAt(subWt); } catch (e) { continue; }

    //  POST-026 selective gate: in SELECTIVE mode a sub is posted IFF its
    //  gitlink is `put` OR a file under it is put/deleted — the parent-scope
    //  probe (gitlink/file put/delete in the parent wtlog) OR the sub's own
    //  TRANSITIVE put/delete scan (anyPutDeep, incl. mounted descendants).  An
    //  out-of-band `adv` sub with NO put under it is LEFT ALONE (no `bucket ===
    //  "adv"` force-include).  POST-ALL mode (selective false) descends every
    //  mounted sub as before.
    if (selective &&
        !subInParentScope(wtl, s.path) &&
        !anyPutDeep(subInfo))
      continue;

    //  The sub's cur tip BEFORE the recursive post.
    const subWtl0 = wtlog.open(subInfo);
    const cur0 = subWtl0.curTip();
    const tip0 = (cur0 && cur0.sha && isFullSha(cur0.sha)) ? cur0.sha : "";

    //  Recurse a post into the sub (it descends into ITS subs first).  A
    //  POSTNONE (no changes in this sub) is swallowed — a clean sub needs no
    //  child commit; its pin stays put.  Any other refusal propagates (a real
    //  conflict in a child must not be silently dropped).
    //  DIS-074: mark the fan-out call — a clean sub keeps its swallowed
    //  POSTNONE and never bare-advances ITS OWN track as a side effect.
    //  POST-027: thread the sub's mount path down the fan-out (accumulated
    //  across nesting: `dog`, `dog/abc`) — postOne spices the message with it.
    const spice = (ctx._spice ? ctx._spice + "/" : "") + s.path;
    try { postTree(subInfo, Object.assign({}, ctx, { _sub: true, _spice: spice }),
                   { uri: subWt }); }
    catch (e) {
      const msg = "" + e;
      if (!isNoneRefusal(msg)) throw e;             // a real refusal bubbles up
    }

    //  The sub's NEW cur tip AFTER the post.  If it advanced (a child commit
    //  landed), record the bump into the PARENT gitlink (D7) so postOne commits
    //  the new pin.  Also bump an already-ADVANCED sub (pin ⊏ sub tip) even when
    //  this run made no new commit (the sub was committed out-of-band).
    const subWtl1 = wtlog.open(subInfo);
    const cur1 = subWtl1.curTip();
    const tip1 = (cur1 && cur1.sha && isFullSha(cur1.sha)) ? cur1.sha : "";
    const newTip = tip1 || tip0;
    if (newTip && isFullSha(newTip) && newTip !== s.pin) {
      //  Synthesise the manual `put <subpath>#<newtip>` bump (SUBS-019 / D7):
      //  a wtlog put row whose fragment is the new sub commit — fold-decide's
      //  gitlink-bump branch turns it into a `160000` add on the parent tree.
      //  `ulog.append` reads the live tail + stamps strictly past it, so a
      //  second sub's bump never collides with the first's stamp.  postOne
      //  re-opens the wtlog, so it sees this just-appended bump row.
      ulog.append(info.bePath, [{ verb: "put", uri: URI.make(undefined, undefined, s.path, undefined, newTip) }]);
      //  DIS-072: the parent's post RE-ATTACHES the child at the new pin — a fresh
      //  `get //WT/path/to/sub#<pin>` row in the CHILD's wtlog; no ref is written.
      ulog.append(subInfo.bePath,
                  [{ verb: "get", uri: submount.trackUri(info.wt, s.path, newTip) }]);
    }
  }
}

//  postOne(info, ctx, row): the single-repo commit body (the former handler).
function postOne(info, ctx, row) {
  const args  = (ctx && ctx.args)  || [];
  const flags = (ctx && ctx.flags) || [];   // JAB-004: kept for parseMessage (msg flags)
  const force = ambient.force();   // JAB-004: force off be

  const wtl = wtlog.open(info);
  const reader = store.open(info.storePath, info.project);

  //  ===== PRE-FLIGHT GATE (refuse before the first store write) ==========
  //  All refuse-capable checks run here, BEFORE the commit barrier opens —
  //  a refusal leaves the store byte-identical (POST-017 all-or-nothing).

  //  0. URI-slot parse (DIS-054): the POST.mkd Host/Query/Path slots ride the
  //  args.  Host (push) is the GIT-013 wire push (pushRemote).  Query (?branch)
  //  retargets the advance; Path (./path) narrows the commit — both REAL below.
  const slots = parseSlots(args);
  const m = parseMessage(args, flags, slots.fragment);
  //  POST-027: a recursed sub's commit SPICES the parent's message with the
  //  sub mount path (`<msg> [dog]`, nested `[dog/abc]`) — POST.mkd row 2.
  if (ctx && ctx._spice && m.msg) m.msg = m.msg + " [" + ctx._spice + "]";

  //  1. Attachment (DIS-057): the SAME wtlog.attachedBranch reader `status`
  //  uses — `?#<sha>` (trunk pinned at a sha) is ATTACHED, only a bare `?<sha>`
  //  is detached.  A detached post is ALLOWED (Bug #2): it commits on top of
  //  cur and HOPS to the next hash — the FF pre-flight and the branch-ref
  //  advance below are gated on `!att.detached`, so no branch ref moves; only
  //  the cur wtlog row advances.  (`cur` = curTip for the parent sha.)
  const cur = wtl.curTip();
  const att = wtl.attachedBranch();

  //  2. Parent / branch resolve.  The COMMIT's branch is the attached branch
  //  (the recentmost GET record, same reader); a Query slot retargets it
  //  (`?other` → the commit lands on `other`).  The parent of the new commit
  //  stays cur's tip (a FF commit on top of cur), whichever branch published to.
  const curBranch = att.branch || "";
  const parent = (cur && cur.sha && isFullSha(cur.sha)) ? cur.sha : undefined;
  const haveBaseline = !!(cur && cur.sha);

  //  DIS-057 RULING 2026-06-29: an in-scope `patch` row's THEIRS commit becomes a
  //  MERGE PARENT of the absorb (the merged/take-theirs bytes ride the wt; the
  //  commit records the second parent so the absorb is a real merge in the DAG).
  //  base=ours did NOT change this — the absorb's parents are ours-tip (parent,
  //  below) + each in-scope theirs.  De-dup + drop any that equal ours-tip.
  const theirsParents = [];
  if (typeof wtl.patchTheirs === "function") {
    const seen = {};
    for (const tsha of wtl.patchTheirs()) {
      if (!isFullSha(tsha) || tsha === parent || seen[tsha]) continue;
      seen[tsha] = 1; theirsParents.push(tsha);
    }
  }

  //  DIS-054 Query slot: resolve the target branch the post advances.  No
  //  Query → cur's branch (the unchanged local-FF path).  A Query target is
  //  `` (trunk, `?`), the parent (`?..`), cur's own (`?.`), or a named branch.
  const target = slots.hasQuery
        ? resolveTarget(slots.query, curBranch, (att.br && att.br.title) || "")
        : curBranch;

  //  DIS-062 local worktree target (`//X[/sub]`): FF-advance that OTHER
  //  worktree's base to cur's tip.  No local commit, cur untouched.
  if (slots.wt) {
    advanceWorktree(info, reader, ctx, slots.wtUri, parent, haveBaseline);
    return;                                  // no commit, no fan-out
  }

  //  GIT-013 Host slot (`ssh://host?br` / `https://host?br`):
  //  FF-push cur's existing tip to the remote branch.  No local commit — this
  //  ships what cur already points at (the descendant cascade / commit-then-
  //  push is out of scope).  The FF gate + pack build live in pushRemote.
  if (slots.host) {
    pushRemote(info, reader, ctx, slots.hostUri, target, parent, slots.hasQuery);
    return;                                  // no commit, no fan-out
  }

  //  DIS-054 Query bare-advance (`?branch`/`?..`/`?` with NO commit content):
  //  FF-advance the target branch's REFS tip to cur's tip — no new commit, cur
  //  untouched.  A message present makes it a cross-branch COMMIT instead (the
  //  commit path below).  Fires before the change-set classify (it commits
  //  nothing) and before the no-msg guard (a bare advance needs no message).
  if (slots.hasQuery && (m.msg == null || m.msg === "") && !slots.narrow) {
    advanceBranch(reader, wtl, info, ctx, target, curBranch, parent,
                  haveBaseline);
    return;                                  // no commit, no fan-out
  }

  //  POST-027: POST.mkd row 1 exception — a message-less post (bare / `#`) with
  //  an ACTIVE absorbed patch COMMITS, reusing the theirs commit's own message.
  if ((m.msg == null || m.msg === "") && !slots.narrow && theirsParents.length)
    m.msg = commitMessage(reader, theirsParents[theirsParents.length - 1]);

  //  POST-026: a message-less `post` is the ADVANCE op — FF the tracked branch
  //  (append `?#<curtip>` to <project>/refs), regardless of uncommitted changes.
  if ((m.msg == null || m.msg === "") && !slots.narrow && !att.detached)
    return advanceTrack(info, wtl, reader, ctx, curBranch, parent, haveBaseline);

  const branchKey = target;

  //  3. Classify the change-set into keep/unlink/add decisions.  A Path slot
  //  (DIS-054) narrows the classify to that path — out-of-scope paths keep
  //  baseline, so only the named path's change lands in the commit.
  //  DIS-057: post CONSUMES an in-scope `patch` row's theirs tree (POST-005
  //  subsumed).  The unified classifier reads a patch-derived file as pat/mrg/
  //  cnf and the consumer commits its merged content; a `cnf` (conflict-marked)
  //  file is still caught by the POSTCFLCT pre-scan below.  No more POSTSCOPE.
  const dres = decideM.decide(info, wtl, reader, slots.narrow || undefined);

  //  DIS-076: a commit never touches any branch REF (WT-only motion), so the
  //  old commit-time POSTNOFF gate (checked branchKey's ref vs parent) is
  //  gone — that FF check now lives solely in advanceBranch's explicit arm.

  //  5. Conflict pre-scan (POST-017): a tracked `add` carrying a complete
  //  WEAVE conflict triple aborts before any store write.  `--force` skips.
  if (!force) {
    for (const d of dres.decisions) {
      if (d.verb !== "add") continue;
      const bytes = commitM.readAddBytes(info.wt, d);
      if (bytes && conflict.hasConflictMarker(bytes)) {
        const named = "conflict marker in tracked file " + d.path;
        throw refuse(named + " (--force overrides)",
                     refuse(named,
                            "conflict marker in a tracked file (--force overrides)"));
      }
    }
  }

  //  6. Empty-commit refuse (POSTNONE): the new root tree equals baseline's.
  //  Pre-build the tree (no store write yet) to compare; the commit re-builds
  //  the SAME decisions below.
  const pre = commitM.buildTree(dres.decisions);
  const rootTreeSha = pre.rootTreeSha || commitM.EMPTY_TREE_SHA;
  //  A message-bearing post that produces baseline's tree has nothing to commit
  //  (a message-less bare post already took the ADVANCE arm above).
  if (haveBaseline && dres.haveBase && dres.baseTreeSha &&
      rootTreeSha === dres.baseTreeSha)
    throw "no changes since base";

  //  7. A committing post (message / narrow / detached) requires a message.
  if (m.msg == null || m.msg === "")
    throw "a commit message is required (`be post '#msg'`)";

  //  BRO-030: snapshot the quad BEFORE the commit — the wt-column dirt is what
  //  this post absorbs into base, so the PRE state is the report.
  let preQuad = null;
  try { preQuad = quadlib.quadOf(info, wtl, reader); } catch (e) {}

  //  JSQUE-020: the commit is a JOIN over the WHOLE decision set held in
  //  memory (dres.decisions); the former durable back-scan barrier only
  //  re-derived a leaf count, so assert it in-memory instead.
  //  POST-029: the run-start ts (ctx.now = be.now) is the single source of
  //  truth for the post row ts AND the BE-011 mtime restamps below.
  const stamp = (ctx && ctx.now != null) ? BigInt(ctx.now)
        : ulog.nowAfter(wtlogTail(wtl));
  const author = authorIdent(info.storePath);

  const leaves = dres.decisions.map(function (d) {
    //  Each leaf is a branch-free decision row `<verb> path[?<old>]#<sha>`.
    //  URI-013 B10: compose path + optional `?old` query + `#sha` fragment via
    //  the URI class (byte-identical: slots feed verbatim, an absent old => no `?`).
    let uri = d.path;
    if (d.verb === "add")
      uri = URI.make(undefined, undefined, d.path, d.oldSha ? d.oldSha : undefined, d.sha);
    else if (d.verb === "keep")
      uri = URI.make(undefined, undefined, d.path, undefined, d.sha);
    return { verb: d.verb, uri: uri };
  });
  if (leaves.length !== dres.decisions.length)
    throw "leaf/decision count mismatch (" + leaves.length + " != " +
          dres.decisions.length + ")";

  //  Build the tree (post-order bodies) + the commit object from the
  //  in-memory decision set.
  const tb = commitM.buildTree(dres.decisions);

  const commit = commitM.buildCommit({
    treeSha: tb.rootTreeSha || commitM.EMPTY_TREE_SHA,
    parents: (parent ? [parent] : []).concat(theirsParents),
    author: author,
    epochSec: epochSecOf(stamp),
    message: m.msg
  });

  //  Write the keeper pack-log (+ idx) — the FIRST store mutation.  PARALLEL
  //  follow-up: an idempotency guard (skip a re-pack on a re-run).
  commitM.writePack(reader.shard, info.wt,
                    commit.body, tb.rootTreeSha, tb.bodies, dres.decisions);

  //  DIS-076 (2026-07-15 ruling): a commit is a WT-only motion — it NEVER
  //  touches any ref, no exceptions.  A ref is born only via explicit `post ?`.

  //  Append the `post` row (`?<branch>#<sha>`) at the stamp, then restamp
  //  every `add` file so it reads clean under the new baseline.
  //  DIS-075: a DETACHED commit records the bare-hash shape `#<sha>` (query slot
  //  ABSENT) — never trunk-shaped `?#<sha>`; an explicit `?target` still records.
  const recQuery = (att.detached && !slots.hasQuery) ? undefined : (branchKey || "");
  //  POST-029: a same-ms collision (ts<=tail) bumps the row — restamp with the
  //  ASSIGNED ts append returns, so the mtime always equals the row's real ts.
  const assigned = ulog.append(info.bePath,
              [{ verb: "post", uri: URI.make(undefined, undefined, undefined, recQuery, commit.sha),
                 ts: stamp }]);
  const rowTs = (assigned && assigned.length) ? assigned[0] : stamp;
  for (const d of dres.decisions) {
    if (d.verb !== "add") continue;
    try { io.setMtime(wtpath(info.wt, d.path), rowTs); } catch (e) {}   // BE-011
  }

  //  BRO-030: the quad report is THE post banner (wiki/Status.mkd "not a
  //  flag"); a `--quad` flag is tolerated as a no-op.
  emitQuadBanner(ctx, info, branchKey, m.msg, preQuad, dres.decisions);
  //  Commit leaf: no further fan-out.
}

//  The wtlog tail ts (for the monotonic stamp bump) — the last row's ts.
function wtlogTail(wtl) {
  return wtl.rows.length ? wtl.rows[wtl.rows.length - 1].ts : 0n;
}

//  POST-027: a commit's own message — the bytes past the header's blank line,
//  trailing newlines shed (buildCommit re-adds the one).  The reuse source.
function commitMessage(reader, sha) {
  const o = reader.getObject(sha);
  if (!o || !o.bytes) return "";
  const t = utf8.Decode(o.bytes);
  const i = t.indexOf("\n\n");
  return i < 0 ? "" : t.slice(i + 2).replace(/\n+$/, "");
}

//  BRO-030: the quad banner — PRE-post quad rows for the committed set
//  (post turned the wt dirt into base-'v'; the PRE state IS what got absorbed).
function emitQuadBanner(ctx, info, branchKey, message, pre, decisions) {
  if (!(ctx && ctx.sink)) return;
  const subject = subjectOf(message);
  const out = hunkrows(ctx.sink,
    URI.make(undefined, undefined, undefined, branchKey || "", subject ? subject : undefined));
  //  BRO-030: the touched set = the banner's own add/unlink decisions (keep
  //  rows are not reported) — the paths this commit actually carried.
  const touched = {};
  for (const d of decisions)
    if (d.verb === "add" || d.verb === "unlink") touched[d.path] = 1;
  if (pre) for (const r of pre.rows)
    if (touched[r.path]) out.raw(quadrender.fileRow(r, false));
  //  BRO-030: fresh readers post-commit — model.commits now reads the new
  //  local commit as ".+.." against the (unmoved) track.
  try {
    const m2 = quadlib.quadOf(info, wtlog.open(info),
                              store.open(info.storePath, info.project));
    for (const c of m2.commits) out.raw(quadrender.commitRow(c, false));
  } catch (e) {}
  out.done();
}

function subjectOf(msg) {
  let i = 0;
  while (i < msg.length && (msg[i] === "\n" || msg[i] === "\r")) i++;
  let j = i;
  while (j < msg.length && msg[j] !== "\n" && msg[j] !== "\r") j++;
  return msg.slice(i, j);
}
