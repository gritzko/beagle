//  views/work/work.js — WORK-001: bare `work` renders the worktree FOREST.
//  Three TREE HUNKS (an empty hunk is ABSENT), Unicode box-drawing rails,
//  every work/ worktree hung under what it TRACKS (wtlog attachedBranch):
//   1. the TREE — the top wt as root, mounts recursed in .gitmodules order
//      (SOLID rails, BOLD in the pager), then each node's tracker wts as ONE
//      name-sorted run on DOTTED rails (`├┄┄`) so trackers never read as
//      subdirs (review ruling); a work wt is ONE row (no descent inside).
//   2. BRANCHES — `store → shard → [branch →] //worktree`, the branch level
//      OPTIONAL: a detached/branchless wt hangs directly under its shard node
//      (the //BLAME-001 form, official); shards resolve through the standard
//      resolver — never guessed, never string-carved (SUBS-054 et al).
//   3. FOREIGN — anchors outside the project's stores, wts under each.
//  WORK-004 wt row: `//KEY  [diff] [post]  [+N][-N]  <time5> #<hashlet8>
//  <subject≤30> [done] [dont]` — [diff]/[post] are fixed slots; the ahbeh
//  counts ARE buttons: `[+N]` mints bare `post` (advance the track, salad 'G'),
//  `[-N]` bare `get` (pull, salmon 'A'), each a `//KEY/: verb` O-invite in the
//  row ctx; a zero side shows nothing, two-digit clamp.  [done]/[dont] as before.
//  R2 rulings: the rails+name column pads RIGHT to KEYW with a DOTTED leader
//  (`//BE-043 ┄┄┄`) and the button slots are FIXED-width, so buttons, ahbeh,
//  time, hashlet and message all align at ONE column set down the whole view;
//  repo rows embolden the NAME only and share the ahbeh column (a mount reads
//  vs the PARENT's de-jure gitlink pin, the root vs its own tracked ref);
//  work/done/ (the done/dont discard root) is IGNORED entirely.
//  Counts are vs the TRACKED ref; keeper opened ONCE per (store,project),
//  ancestor closures memoized per tip.  Discovery = readdir of work/ gated on
//  a `.be` file (BE-043) + the context tree's mounts; ALL URI/track parsing
//  rides be.treeAt / wtlog / discover.wtdir — no string surgery on URIs.
//  Plain is chrome-free and style-free (rails are structure, they stay).
//  Miss = ONE uniform `work: <arg>: WORKNONE` line + throw (BE-003 spirit).
"use strict";

const pathlib    = require("../../shared/util/path.js");
const join       = pathlib.join;
const ambient    = require("../../shared/ambient.js");     // JAB-004: ctx→be bridge
const wtlog      = require("../../shared/wtlog.js");       // the ONE wtlog reader
const gitmodules = require("../../shared/gitmodules.js");  // PUT-004: mount decls
const branchlib  = require("../../shared/branch.js");      // SUBS-050: branch codec
const store      = require("../../shared/store.js");       // keeper open + shardDir
const dag        = require("../../shared/dag.js");         // ancestors/commitTs
const graf       = require("../../shared/graf.js");        // GRAF-001: ahbeh cache
const render     = require("../../view/render.js");        // dateCol (the 7-col form)
const navlib     = require("../../shared/nav.js");         // URI-011: nav spells
const todo       = require("../todo/todo.js");             // BE-038/043: ticket titles
const SPELL      = require("../../shared/spell.js");       // BRO-025: O-spell codec

//  tok32 (dog/tok/TOK.h): [31..27] tag (A+n)  [23..0] end byte offset.
function tokPack(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }
const TAG_U = tagCode("U"), TAG_S = tagCode("S"), TAG_O = tagCode("O");
//  WORK-004 palette slots: repo rows BOLD ('C'); [diff] yellow 'E', [post]
//  green 'W', [done]/[dont] blue 'Y'; ahbeh ahead 'G' (green slot, now salad-256),
//  behind 'A' (the NEW salmon slot) — proper 256-colour slots in view/bro.js.
const TAG_C = tagCode("C"), TAG_Y = tagCode("Y"), TAG_E = tagCode("E");
const TAG_W = tagCode("W"), TAG_G = tagCode("G"), TAG_A = tagCode("A");
//  WORK-010: the ticket-link `[?]` rides the cyan 'V' slot (view/bro.js THEME).
const TAG_V = tagCode("V");

//  --- fs probes ---------------------------------------------------------------
function isDir(p) { try { return io.stat(p).kind === "dir"; } catch (e) { return false; } }
function isReg(p) { try { return io.stat(p).kind === "reg"; } catch (e) { return false; } }

//  BE-043: the work root — be.workRoot() (`projectRoot()+"/work"`, URI-016).
function workDir() {
  if (typeof be === "undefined" || !be.workRoot) return null;
  const d = be.workRoot();
  if (!d || !isDir(d)) return null;
  return { root: be.projectRoot(), dir: d };
}

//  BE-043: the worktree names — every direct `work/` subdir owning a `.be` FILE
//  (the BE-044 gate), name-sorted; README pages and junk dirs never list.
function listWork(dir) {
  let names; try { names = io.readdir(dir); } catch (e) { return []; }
  const out = [];
  for (let nm of names) {
    const dirEnt = nm.length && nm[nm.length - 1] === "/";
    if (dirEnt) nm = nm.slice(0, -1);
    if (!nm || nm[0] === ".") continue;
    if (nm === "done") continue;           // r2: the discard root, never listed
    if (!dirEnt && !isDir(join(dir, nm))) continue;
    if (!isReg(join(join(dir, nm), ".be"))) continue;
    out.push(nm);
  }
  out.sort();
  return out;
}

//  A live mount (status.js isMount twin): `<wt>/<sub>/.be` FILE, or a PRIMARY
//  nested wt (`.be` DIR carrying a wtlog).
function isMount(wtRoot, subpath) {
  const p = join(join(wtRoot, subpath), ".be");
  let k; try { k = io.stat(p).kind; } catch (e) { return false; }
  if (k === "reg") return true;
  if (k !== "dir") return false;
  return isReg(join(p, "wtlog"));
}

//  --- the shard registry ------------------------------------------------------
//  WORK-001: keeper opened ONCE per (store,project); per-tip ancestor closures
//  and commit metas memoized — hundreds of wts share a handful of tips.
function normStore(p) { return String(p || "").replace(/\/+$/, ""); }
function shardKey(repo) { return normStore(repo.storePath) + "|" + (repo.project || ""); }

function registry() {
  const keepers = new Map(), ancs = new Map(), metas = new Map(), grafs = new Map();
  function keeperFor(repo) {
    const key = shardKey(repo);
    if (keepers.has(key)) return keepers.get(key);
    let k = null;
    try {
      const sd = store.shardDir(repo.storePath, repo.project);
      //  A named project whose shard dir is absent must NOT fall for shardDir's
      //  auto-detect guess (the //BLAME-001 `?/project` oddity) — no keeper.
      if (isDir(sd) && (!repo.project || pathlib.basename(sd) === repo.project))
        k = store.open(repo.storePath, repo.project);
    } catch (e) { k = null; }
    keepers.set(key, k);
    return k;
  }
  function ancestorsOf(repo, sha) {
    const key = shardKey(repo) + "#" + sha;
    if (ancs.has(key)) return ancs.get(key);
    const k = keeperFor(repo);
    const s = k ? dag.ancestors(k, sha) : new Set();
    ancs.set(key, s);
    return s;
  }
  //  WORK-011: the shard's graf ahbeh cache (GRAF-001), opened ONCE per shard
  //  beside the keeper — null when the shard has no keeper or graf.open throws.
  function grafFor(repo) {
    const key = shardKey(repo);
    if (grafs.has(key)) return grafs.get(key);
    let g = null;
    if (keeperFor(repo)) {
      try { g = graf.open(store.shardDir(repo.storePath, repo.project)); }
      catch (e) { g = null; }
    }
    grafs.set(key, g);
    return g;
  }
  //  meta(repo, sha) → { ts, subject } off the commit ("" / 0n when unreadable).
  function meta(repo, sha) {
    const key = shardKey(repo) + "#" + sha;
    if (metas.has(key)) return metas.get(key);
    const k = keeperFor(repo);
    let m = { ts: 0n, subject: "" };
    if (k && wtlog.isFullSha(sha)) {
      let pc; try { pc = k.parseCommit(sha); } catch (e) { pc = undefined; }
      if (pc) m = { ts: dag.commitTs(k, sha), subject: dag.subjectOf(pc.body || "") };
    }
    metas.set(key, m);
    return m;
  }
  //  counts(repo, cur, tip) → { ahead, behind } | null (vs the TRACKED ref).
  //  WORK-011: the counts come from the graf index (GRAF-001) — index-first,
  //  self-extending; the keeper closure diff stays only as the fallback (an
  //  absent/failed graf, which graf itself otherwise walks-and-caches).
  function counts(repo, cur, tip) {
    if (!wtlog.isFullSha(cur) || !wtlog.isFullSha(tip)) return null;
    if (cur === tip) return { ahead: 0, behind: 0 };
    const k = keeperFor(repo);
    if (!k) return null;
    const g = grafFor(repo);
    if (g) { try { return g.aheadBehind(k, cur, tip); } catch (e) {} }
    const a = ancestorsOf(repo, cur), b = ancestorsOf(repo, tip);
    let ahead = 0, behind = 0;
    for (const s of a) if (!b.has(s)) ahead++;
    for (const s of b) if (!a.has(s)) behind++;
    return { ahead: ahead, behind: behind };
  }
  return { keeperFor: keeperFor, meta: meta, counts: counts };
}

//  --- the context tree (block 1's skeleton) -----------------------------------
//  A node = the top wt or a live mount: { name, relpath, wt, repo, children,
//  wts }; children recurse in .gitmodules declaration order; `wts` collects
//  the node's tracker worktrees (trunk + pin merged, one name-sorted run).
function nodeAt(dir, name, relpath) {
  let repo; try { repo = be.treeAt(dir); } catch (e) { return null; }
  if (!repo || repo.wt !== dir) return null;
  const n = { name: name, relpath: relpath, wt: dir, repo: repo,
              children: [], wts: [], _tip: undefined };
  for (const p of gitmodules.paths(dir)) {
    if (!isMount(dir, p)) continue;
    const c = nodeAt(join(dir, p), p, relpath ? relpath + "/" + p : p);
    //  `subpath` = the mount path within its PARENT — the de-jure pin's key.
    //  WORK-007: `parent` is the pin holder — a slashless pin track hangs there.
    if (c) { c.subpath = p; c.parent = n; n.children.push(c); }
  }
  return n;
}

//  R2: the node's de-jure gitlink pins (path → sha) off its own BASE tree —
//  a mount's ahbeh reads its live tip vs the PARENT's pin of it.
function nodePins(node, reg) {
  if (node._pins !== undefined) return node._pins;
  const pins = {};
  const k = reg.keeperFor(node.repo);
  const tip = nodeTip(node);
  if (k && wtlog.isFullSha(tip)) {
    let tree; try { tree = k.commitTree(tip); } catch (e) { tree = undefined; }
    if (tree) {
      try {
        k.readTreeRecursive(tree, function (l) { if (l.kind === "s") pins[l.path] = l.sha; });
      } catch (e) {}
    }
  }
  node._pins = pins;
  return pins;
}

//  The node's own cur tip sha (its wtlog), read once.
function nodeTip(node) {
  if (node._tip !== undefined) return node._tip;
  let sha = "";
  try { const cur = wtlog.open(node.repo).curTip(); sha = (cur && cur.sha) || ""; }
  catch (e) { sha = ""; }
  node._tip = sha;
  return sha;
}

function indexNodes(root) {
  const byShard = new Map(), byWt = new Map(), stores = new Set();
  (function walk(n) {
    const key = shardKey(n.repo);
    if (!byShard.has(key)) byShard.set(key, n);
    byWt.set(n.wt, n);
    stores.add(normStore(n.repo.storePath));
    for (const c of n.children) walk(c);
  })(root);
  return { byShard: byShard, byWt: byWt, stores: stores };
}

//  --- wt classification (WORK-007: link by the TRACKING edge) -----------------
//  resolveTrack(track) → the PARENT slot a URI-shaped track names, via be.wtdir
//  (+ the file: anchor fallback) — the SAME resolvers status/get use, NEVER a
//  regex.  Pin vs base is the track path's DIR-FORM (a trailing slash, the
//  discover.argRel idiom): `X/sub` (slashless) is X's gitlink PIN of sub, so it
//  hangs under X (sub's parent); `X/sub/` is sub's OWN base, under sub.  wtdir
//  COLLAPSES the slash (resolveInTree), so the pin/base bit is read off the
//  PARSED track path, not the fs resolver.  Returns { node } (a context mount/
//  root) | { wt } (another work wt — the recursive edge) | null (no live tree).
function resolveTrack(track, ix, byWorkWt) {
  let d = null;
  try { d = be.wtdir(track); } catch (e) { d = null; }
  if (!d) {
    //  A `file:` track names a tree by fs path (the JS-* anchor flavor).
    let u; try { u = uri._parse(track); } catch (e) { u = null; }
    if (u && u.scheme === "file" && u.path) {
      try { d = be.treeAt(u.path).wt; } catch (e) { d = null; }
    }
  }
  if (!d) return null;
  let dirForm = false;
  try { const up = uri._parse(track); const pp = (up && up.path) || "";
        dirForm = pp.length > 1 && pp[pp.length - 1] === "/"; } catch (e) {}
  //  A context mount/root?  A slashless PIN hangs under the mount's PARENT tree.
  let m = ix.byWt.get(d);
  if (!m) { try { m = ix.byWt.get(be.treeAt(d).wt); } catch (e) { m = null; } }
  //  WORK-012: a slashless PIN-form track hangs under the mount's PARENT but
  //  reads ahbeh vs the PARENT's de-jure gitlink pin (pinNode+subpath), NOT the
  //  parent's own tip; a dir-form track reads vs the mount's own live tip.
  if (m && !dirForm && m.parent)
    return { node: m.parent, pinNode: m.parent, subpath: m.subpath };
  if (m) return { node: m };
  //  Another work wt (the recursive edge) — hang directly under it.
  let w = byWorkWt.get(d);
  if (!w) { try { w = byWorkWt.get(be.treeAt(d).wt); } catch (e) { w = null; } }
  if (w) return { wt: w };
  return null;
}

//  WORK-007: place ONE work-wt node under what it TRACKS.  A URI track resolves
//  to a context tree node (block 1, pushed now) or another work wt (the edge is
//  RECORDED in parentWt, linked after the cycle pass); everything else anchors
//  by branch/detached/trunk/foreign.  A wt is a NODE (children[]), not a leaf,
//  so a wt tracking it nests recursively wherever it lands.
function placeWt(w, ix, byWorkWt, reg, branchMap, foreignMap) {
  const att = w.att, repo = w.repo;
  if (att.uriTrack && att.track) {
    const tgt = resolveTrack(att.track, ix, byWorkWt);
    if (tgt && tgt.node) {
      w.node = tgt.node;
      //  WORK-012: a pin-form track reads vs the PARENT's gitlink pin of the
      //  mount (unpinned → no counts, like a repo row's `mc = pin ? … : null`).
      if (tgt.pinNode) {
        const pin = nodePins(tgt.pinNode, reg)[tgt.subpath];
        w.counts = pin ? reg.counts(repo, w.sha, pin) : null;
      } else w.counts = reg.counts(repo, w.sha, nodeTip(tgt.node));
      tgt.node.wts.push(w);
      return;
    }
    if (tgt && tgt.wt && tgt.wt !== w) {
      w.parentWt = tgt.wt;
      w.counts = reg.counts(repo, w.sha, tgt.wt.sha);
      return;                                   // linked after breakCycles()
    }
    //  WORK-007 (default c): the tracked tree is gone — fall to the anchor shard
    //  with a plain-words mark, never silently to the store.
    w.mark = "tracked tree not found";
  }
  anchorWt(w, ix, reg, branchMap, foreignMap);
}

//  WORK-007: the anchor fallback — a wt with no live tracked tree lands by its
//  own store anchor: foreign hunk, branch tree, detached/trunk shard node.
function anchorWt(w, ix, reg, branchMap, foreignMap) {
  const att = w.att, repo = w.repo;
  const foreign = !ix.stores.has(normStore(repo.storePath));
  if (foreign) {
    if (!att.uriTrack && !att.detached)
      w.counts = reg.counts(repo, w.sha, refTip(reg, repo, att));
    groupOf(foreignMap, repo).wts.push(w);
    return;
  }
  if (att.detached || att.uriTrack) { groupOf(branchMap, repo).wts.push(w); return; }
  if (att.branch) {                                       // a named branch tree
    w.counts = reg.counts(repo, w.sha, refTip(reg, repo, att));
    let cur = groupOf(branchMap, repo);
    for (const seg of att.br.branch) {
      const kids = cur.branches || cur.kids;
      if (!kids.has(seg)) kids.set(seg, { wts: [], kids: new Map() });
      cur = kids.get(seg);
    }
    cur.wts.push(w);
    return;
  }
  //  Trunk: under the matching context tree node when mounted, else the shard.
  w.counts = reg.counts(repo, w.sha, refTip(reg, repo, att));
  const node = ix.byShard.get(shardKey(repo));
  if (node) { w.node = node; node.wts.push(w); return; }
  groupOf(branchMap, repo).wts.push(w);
}

//  WORK-007 (default b): break a track CYCLE (A→B→A) at the name-sorted first
//  node — cut its edge, mark it in plain words, re-anchor it by its own store so
//  the render graph stays a forest (no infinite descent).
function breakCycles(wts, ix, reg, branchMap, foreignMap) {
  for (const start of wts) {
    const seen = new Set();
    let n = start, hit = null;
    while (n) { if (seen.has(n)) { hit = n; break; } seen.add(n); n = n.parentWt; }
    if (!hit) continue;
    const members = []; let c = hit;
    do { members.push(c); c = c.parentWt; } while (c && c !== hit);
    members.sort(function (a, b) { return a.key < b.key ? -1 : a.key > b.key ? 1 : 0; });
    const victim = members[0];
    if (!victim.parentWt) continue;
    victim.parentWt = null;
    victim.counts = null;
    victim.mark = "tracks a cycle";
    anchorWt(victim, ix, reg, branchMap, foreignMap);
  }
}

//  The TRACKED ref's tip sha: the shard's trunk ("" query) or named branch.
function refTip(reg, repo, att) {
  const k = reg.keeperFor(repo);
  if (!k) return "";
  const key = att.branch ? branchlib.key(att.br) : "";
  let sha; try { sha = k.resolveRef(key); } catch (e) { sha = undefined; }
  return sha || "";
}

//  --- display helpers ---------------------------------------------------------
//  The store DIR of a storePath (`<store>/.be`, or the path itself when it IS
//  a `.be`), home-abbreviated for display (the sketch's `file:~/.be` form).
function storeDisp(storePath) {
  const p = normStore(storePath);
  const d = pathlib.basename(p) === ".be" ? p : p + "/.be";
  const home = io.getenv("HOME") || "";
  if (home && (d === home || d.indexOf(home + "/") === 0))
    return "~" + d.slice(home.length);
  return d;
}

//  A mount's annotation: its project title (when it differs from the mount's
//  basename) + ` ⇐ <store>` for a secondary wt into a shared store; "" else.
function mountAnnot(root, node) {
  const base = node.name.slice(node.name.lastIndexOf("/") + 1);
  const proj = node.repo.project || "";
  const colocated = normStore(node.repo.storePath) === node.wt;
  const parts = [];
  if (proj && proj !== base) parts.push(proj);
  if (!colocated) {
    let s = normStore(node.repo.storePath);
    s = pathlib.basename(s) === ".be" ? s : s + "/.be";
    const rooted = root && s.indexOf(root + "/") === 0;
    parts.push("⇐ " + (rooted ? s.slice(root.length + 1) : storeDisp(node.repo.storePath)));
  }
  return parts.join(" ");
}

function chars(s) { return Array.from(s).length; }
//  Review 2026-07-18: the wt-row subject clips at ~30 chars.
function trim30(s) {
  const a = Array.from(s || "");
  return a.length > 30 ? a.slice(0, 30).join("") : (s || "");
}

//  --- row assembly ------------------------------------------------------------
//  A hunk is a flat row list: a REPO row { rails, label, nav, bold, tail:{ts,
//  sha,subject,annot} }, a HEADER/BRANCH row { rails, label, tail:null|{text} },
//  or a WT row { rails, wt:{key,sha,ts,subject,counts,node} }.  Repo rows
//  share a padded label column; wt rows lay out per the review ruling.
function span(parts, spans, off, text, tag) {
  const b = utf8.Encode(text);
  parts.push(b);
  spans.push([tag, off + b.length]);
  return off + b.length;
}
function feed(sink, banner, parts, spans, off) {
  const body = new Uint8Array(off);
  let p = 0;
  for (const part of parts) { body.set(part, p); p += part.length; }
  const toks = new Uint32Array(spans.length);
  for (let i = 0; i < spans.length; i++) toks[i] = tokPack(spans[i][0], spans[i][1]);
  sink.feed(banner, body, toks, "", 0n);
}

//  BE-043 (merge ruling): the one-line post message IS the ticket page's own
//  title, via the todo board's pageFile/pageTitle; "" when no board/page.
function ticketTitle(key) {
  if (todo.shape(key) !== "key") return "";
  const board = todo.boardDir();
  if (!board) return "";
  const file = todo.pageFile(board.dir, key);
  //  WORK-008: strip the [OPEN]/[HIGH]/… status mark so the minted post message
  //  is the bare `KEY: title` commit convention (the board keeps it via headerMark).
  return file ? todo.stripMark(key, todo.pageTitle(file)) : "";
}

//  WORK-010: the `[?]` click-spell for a wt named after a ticket.  RULING
//  (gritzko 2026-07-19): an O-invite with EMPTY context, verb `todo`, the ticket
//  key as ARGUMENT — `//: todo TKT-123` (no wt context welded; the key is the
//  arg, not the context).  The name resolves through the SHARED todo parser:
//  its BASE ticket key (suffix-tolerant, `PIN-1b`→`PIN-1`, a page must exist), or
//  a bare TOPIC (the topic dir exists → `//: todo TOPIC`); "" for any other name.
function ticketLink(name) {
  const board = todo.boardDir();
  if (!board) return "";
  const key = todo.ticketKey(name);
  if (key)
    return todo.pageFile(board.dir, key) ? SPELL.mintOspell("//", "todo " + key) : "";
  if (todo.shape(name) === "topic")
    return isDir(join(board.dir, name)) ? SPELL.mintOspell("//", "todo " + name) : "";
  return "";
}

//  R2 fixed columns: the rails+name region pads to KEYW with a dotted leader;
//  the pager's button region is FIXED slots (WORK-010: "[?] " 4 + "[±] " 4 +
//  "[post] " 7; an absent button ┄-fills its slot), so ahbeh/time/hashlet/
//  message land at the SAME offsets on every row.  WORK-010 compacted [diff]→[±].
const KEYW = 32;                       // rails+name column width, test-pinned
const SLOT_HELP = 4, SLOT_DIFF = 4, SLOT_POST = 7;
const BTNW = SLOT_HELP + SLOT_DIFF + SLOT_POST;
//  WORK-004 ahbeh column: TEXT `+N`/`-N` right-aligns to 7 (plain + repo rows);
//  the pager's WT buttons `[+99][-99]` (+1 lead) widen it to 11 so the shared
//  #hashlet/message columns stay aligned across wt and repo rows.
const AHBEHW_TXT = 7, AHBEHW_BTN = 11;

//  WORK-006: a fixed-width dotted leader (the prefixSpans idiom — a breathing
//  space then a `┄` run) that closes up an absent button slot / a short subject
//  tail; n<2 degrades to a lone space so the grid never abuts.
function leader(n) { return n >= 2 ? " " + "┄".repeat(n - 1) : (n > 0 ? " " : ""); }

//  The rails+name column: label (+ its hidden nav), then the dotted leader
//  (` ┄┄┄`) out to KEYW; an over-long row degrades to one space (shifts right).
function prefixSpans(parts, spans, off, rails, label, ltag, nav) {
  if (rails) off = span(parts, spans, off, rails, TAG_S);
  off = span(parts, spans, off, label, ltag);
  if (nav) off = span(parts, spans, off, nav, TAG_U);
  const fill = KEYW - chars(rails) - chars(label);
  off = span(parts, spans, off, fill >= 2 ? " " + "┄".repeat(fill - 1) : " ", TAG_S);
  return off;
}

//  WORK-004: the shared ahbeh cell.  A pager WT row (btns+ctx): the counts
//  BECOME buttons — `[+N]` mints bare `post` (advance the track, salad 'G'),
//  `[-N]` bare `get` (pull, salmon 'A'), each a `//ctx/: verb` O-invite; a zero
//  side shows nothing, two-digit clamp; padded to AHBEHW_BTN.  Otherwise TEXT
//  `+N`/`-N` (plain + repo rows), NO trailing pad (dateCol leads the gap); the
//  RENDER clamps each side at 99 (the counts themselves stay unclamped).
function ahbehSpans(parts, spans, off, counts, btns, ctx) {
  const av = counts && counts.ahead ? Math.min(counts.ahead, 99) : 0;
  const bv = counts && counts.behind ? Math.min(counts.behind, 99) : 0;
  if (btns && ctx) {
    const aw = av ? 3 + String(av).length : 0, bw = bv ? 3 + String(bv).length : 0;
    off = span(parts, spans, off, leader(Math.max(1, AHBEHW_BTN - aw - bw)), TAG_S);
    if (av) { off = span(parts, spans, off, "[+" + av + "]", TAG_G);
              off = span(parts, spans, off, SPELL.mintOspell(ctx, "post"), TAG_O); }
    if (bv) { off = span(parts, spans, off, "[-" + bv + "]", TAG_A);
              off = span(parts, spans, off, SPELL.mintOspell(ctx, "get"), TAG_O); }
    return off;
  }
  const a = av ? "+" + av : "", b = bv ? "-" + bv : "";
  const w = btns ? AHBEHW_BTN : AHBEHW_TXT;
  const padw = Math.max(1, w - a.length - b.length);
  off = span(parts, spans, off, btns ? leader(padw) : " ".repeat(padw), TAG_S);
  if (a) off = span(parts, spans, off, a, TAG_G);
  if (b) off = span(parts, spans, off, b, TAG_A);
  return off;
}

//  WORK-005: the age fade — the row's default-fg darkens by the day.  Age =
//  be.now - d.ts (both ron60); grey level = min(floor(age/24h), 8), channel =
//  level*0x11 (fresh #000000 .. week+ #888888).  Baked as a row-leading bare
//  `#rrggbb` O token; view/bro.js paintWhyRow tints the row's default cells.
function fadeHex(ts) {
  const now = (typeof be !== "undefined" && be.now) || 0n;
  let days = 0;
  if (ts && ts !== 0n && now) {
    try { days = Math.floor((render.ronToMs(now) - render.ronToMs(ts)) / 86400000); }
    catch (e) { days = 0; }
  }
  //  Perceptual ramp: #000/#111/#222 are indistinguishable fg shades, so day 1
  //  jumps straight to #222 and each further day adds #111, capping at #888.
  const lvl = days <= 0 ? 0 : Math.min(days + 1, 8);
  const h = (lvl * 0x11).toString(16).padStart(2, "0");
  return "#" + h + h + h;
}

//  The wt row: `//KEY ┄┄┄  [?] [±] [post]  [+N][-N]  <time5> #<hashlet8>
//  <subject≤30> [done] [dont]` — buttons pager-only, everything else content.
//  WORK-010: [?] (`//: todo TKT` invite) + [±] (compact diff) lead the buttons.
function wtSpans(parts, spans, off, rails, d, btns) {
  const ctx = "//" + d.key;
  //  WORK-005: pager-only leading fade marker; the plain path stays chrome-free.
  if (btns) off = span(parts, spans, off, fadeHex(d.ts), TAG_O);
  off = prefixSpans(parts, spans, off, rails, ctx, TAG_S, "status " + ctx);
  off = span(parts, spans, off, " ", TAG_S);
  if (btns) {
    //  WORK-010 RULING: [?] then [±] LEAD the button run.  [?] is the O-invite
    //  `//: todo TKT` (nav to the ticket page / topic list) on a ticket-/topic-
    //  named wt, else the slot ┄-pads (WORK-006); [±] is the compact [diff] face.
    const link = ticketLink(d.key);
    if (link) {
      off = span(parts, spans, off, "[?]", TAG_V);
      off = span(parts, spans, off, link, TAG_O);
      off = span(parts, spans, off, " ", TAG_S);
    } else off = span(parts, spans, off, leader(SLOT_HELP), TAG_S);
    off = span(parts, spans, off, "[±]", TAG_E);
    off = span(parts, spans, off, "diff " + ctx, TAG_U);
    off = span(parts, spans, off, " ", TAG_S);
    const title = ticketTitle(d.key);
    if (title) {
      off = span(parts, spans, off, "[post]", TAG_W);
      off = span(parts, spans, off, SPELL.mintOspell(ctx, "post '" + title + "'"), TAG_O);
      off = span(parts, spans, off, " ", TAG_S);
    } else off = span(parts, spans, off, leader(SLOT_POST), TAG_S);
  }
  off = ahbehSpans(parts, spans, off, d.counts, btns, ctx);
  off = span(parts, spans, off, render.dateCol(d.ts || 0n), TAG_S);
  off = span(parts, spans, off, "#" + (d.sha ? d.sha.slice(0, 8) : "........"), TAG_S);
  const subj = trim30(d.subject);
  if (btns) {
    //  WORK-006: pad the subject to the 30-col trim width with a ┄ leader so
    //  [done]/[dont] land at ONE column on every wt row (breathing-space idiom).
    off = span(parts, spans, off, " " + subj, TAG_S);
    const pad = 30 - chars(subj);
    if (pad > 0) off = span(parts, spans, off, leader(pad), TAG_S);
    off = span(parts, spans, off, " ", TAG_S);
    off = span(parts, spans, off, "[done]", TAG_Y);
    off = span(parts, spans, off, SPELL.mintOspell(ctx, "done ."), TAG_O);
    off = span(parts, spans, off, " ", TAG_S);
    off = span(parts, spans, off, "[dont]", TAG_Y);
    off = span(parts, spans, off, SPELL.mintOspell(ctx, "dont ."), TAG_O);
  } else if (subj) off = span(parts, spans, off, " " + subj, TAG_S);  // plain: no pad
  //  WORK-007 (defaults b/c): a cycle / dead-track wt carries a plain-words mark.
  if (d.mark) off = span(parts, spans, off, " (" + d.mark + ")", TAG_S);
  return off;
}

function emitRows(sink, rows, btns) {
  const parts = [], spans = [];
  let off = 0;
  for (const r of rows) {
    if (r.wt) {
      off = wtSpans(parts, spans, off, r.rails, r.wt, btns);
    } else if (r.tail && r.tail.sha !== undefined) {
      //  A repo row: the NAME alone bold (r2); the button region pads blank so
      //  its ahbeh/time/hashlet/message share the wt rows' offsets exactly.
      off = prefixSpans(parts, spans, off, r.rails, r.label,
                        r.bold ? TAG_C : TAG_S, r.nav);
      off = span(parts, spans, off, btns ? " " + " ".repeat(BTNW) : " ", TAG_S);
      off = ahbehSpans(parts, spans, off, r.counts, btns, null);
      const t = r.tail;
      let rest = render.dateCol(t.ts || 0n) +
                 "#" + (t.sha ? t.sha.slice(0, 8) : "........");
      if (t.subject) rest += " " + t.subject;
      if (t.annot) rest += "  " + t.annot;
      off = span(parts, spans, off, rest, TAG_S);
    } else {
      //  A store/shard/branch header row: bare label (+ the `remote` mark).
      if (r.rails) off = span(parts, spans, off, r.rails, TAG_S);
      off = span(parts, spans, off, r.label, TAG_S);
      if (r.tail && r.tail.text) off = span(parts, spans, off, "  " + r.tail.text, TAG_S);
    }
    off = span(parts, spans, off, "\n", TAG_S);
  }
  feed(sink, "work", parts, spans, off);
}

//  --- the three hunks ---------------------------------------------------------
//  Review 2026-07-18: SOLID rails for real subdirs (mounts/branches), DOTTED
//  (`├┄┄`) for tracker wt rows — same characters in plain (structure).
const RAIL = { mid: "├── ", last: "└── ", dmid: "├┄┄ ", dlast: "└┄┄ ",
               bar: "│   ", gap: "    " };
function childRails(prefix, isLast, dotted) {
  return prefix + (dotted ? (isLast ? RAIL.dlast : RAIL.dmid)
                          : (isLast ? RAIL.last : RAIL.mid));
}
function deeper(prefix, wasLast) { return prefix + (wasLast ? RAIL.gap : RAIL.bar); }

function wtRow(rails, r, reg) {
  const m = reg.meta(r.repo, r.sha);
  return { rails: rails,
           wt: { key: r.key, sha: r.sha, ts: m.ts || r.ts, subject: m.subject,
                 counts: r.counts, node: r.node, mark: r.mark || "" } };
}

//  WORK-007: emit a tracker wt row then RECURSE into its own tracker children
//  (wts tracking THIS wt) — one deeper dotted rail run.  `prefix` is the parent's
//  child-prefix; `isLast` positions this row among its siblings.
function pushWt(out, prefix, isLast, w, reg) {
  out.push(wtRow(childRails(prefix, isLast, true), w, reg));
  const kids = w.children || [];
  const p = deeper(prefix, isLast);
  for (let i = 0; i < kids.length; i++)
    pushWt(out, p, i === kids.length - 1, kids[i], reg);
}

//  Block 1: the tree — per node: the mounts first (.gitmodules order, solid
//  bold), then ALL tracker wts as ONE name-sorted dotted run (review ruling;
//  listWork arrives sorted, pushes keep order).
function treeRows(root, reg, out, projRoot) {
  const m0 = reg.meta(root.repo, nodeTip(root));
  //  R2: the root's ahbeh reads vs its OWN tracked ref (when it has one).
  let rc = null;
  try {
    const att = wtlog.open(root.repo).attachedBranch();
    if (!att.uriTrack && !att.detached)
      rc = reg.counts(root.repo, nodeTip(root), refTip(reg, root.repo, att));
  } catch (e) { rc = null; }
  out.push({ rails: "", label: root.name, nav: navlib.navLink("status", ""),
             bold: true, counts: rc,
             tail: { ts: m0.ts, sha: nodeTip(root), subject: m0.subject } });
  (function walk(node, prefix) {
    const kids = [];
    for (const c of node.children) kids.push({ node: c });
    for (const r of node.wts) kids.push({ wt: r });
    for (let i = 0; i < kids.length; i++) {
      const last = i === kids.length - 1;
      if (kids[i].wt) {
        pushWt(out, prefix, last, kids[i].wt, reg);        // WORK-007: recurse
        continue;
      }
      const c = kids[i].node;
      const m = reg.meta(c.repo, nodeTip(c));
      //  R2: a mount's ahbeh = its live tip vs the PARENT's de-jure pin of it
      //  (the bare-`sub` pin, [dejure-vs-defacto]); empty when unpinned.
      const pin = nodePins(node, reg)[c.subpath];
      const mc = pin ? reg.counts(c.repo, nodeTip(c), pin) : null;
      out.push({ rails: childRails(prefix, last, false), label: c.name,
                 nav: navlib.navLink("status", c.relpath), bold: true, counts: mc,
                 tail: { ts: m.ts, sha: nodeTip(c), subject: m.subject,
                         annot: mountAnnot(projRoot, c) } });
      walk(c, deeper(prefix, last));
    }
  })(root, "");
}

//  Blocks 2/3 share the store grouping: Map storePath → Map project → group.
function groupOf(map, repo) {
  const sp = normStore(repo.storePath), proj = repo.project || "";
  if (!map.has(sp)) map.set(sp, new Map());
  const shards = map.get(sp);
  if (!shards.has(proj)) shards.set(proj, { wts: [], branches: new Map() });
  return shards.get(proj);
}
function sortedKeys(map) { return Array.from(map.keys()).sort(); }

//  Block 2: `store → shard → [branch →] //worktree` — the branch level only
//  where a branch is tracked; branchless wts hang right under the shard.
function branchRows(map, reg, out) {
  for (const sp of sortedKeys(map)) {
    const shards = map.get(sp);
    out.push({ rails: "", label: "file:" + storeDisp(sp), tail: null });
    const projs = sortedKeys(shards);
    for (let pi = 0; pi < projs.length; pi++)
      emitShard(shards.get(projs[pi]), projs[pi], "",
                pi === projs.length - 1, reg, out);
  }
}
function emitShard(g, proj, prefix, last, reg, out) {
  let p = prefix;
  if (proj) {                                // the shard level (skipped when flat)
    out.push({ rails: childRails(prefix, last, false), label: proj, tail: null });
    p = deeper(prefix, last);
  }
  const kids = [];
  for (const seg of sortedKeys(g.branches)) kids.push({ seg: seg, b: g.branches.get(seg) });
  for (const r of g.wts) kids.push({ wt: r });
  for (let i = 0; i < kids.length; i++) {
    const isLast = i === kids.length - 1;
    if (kids[i].wt) { pushWt(out, p, isLast, kids[i].wt, reg); continue; }
    emitBranch(kids[i].seg, kids[i].b, p, isLast, reg, out);
  }
}
function emitBranch(seg, b, prefix, last, reg, out) {
  out.push({ rails: childRails(prefix, last, false), label: seg, tail: null });
  const p = deeper(prefix, last);
  const kids = [];
  for (const s of sortedKeys(b.kids)) kids.push({ seg: s, b: b.kids.get(s) });
  for (const r of b.wts) kids.push({ wt: r });
  for (let i = 0; i < kids.length; i++) {
    const isLast = i === kids.length - 1;
    if (kids[i].wt) { pushWt(out, p, isLast, kids[i].wt, reg); continue; }
    emitBranch(kids[i].seg, kids[i].b, p, isLast, reg, out);
  }
}

//  Block 3: one `file:<store> [?/<proj>]  remote` header per foreign shard.
function foreignRows(map, reg, out) {
  for (const sp of sortedKeys(map)) {
    const shards = map.get(sp);
    for (const proj of sortedKeys(shards)) {
      out.push({ rails: "",
                 label: "file:" + storeDisp(sp) + (proj ? " ?/" + proj : ""),
                 tail: { text: "remote" } });
      const wts = shards.get(proj).wts;
      for (let i = 0; i < wts.length; i++)
        pushWt(out, "", i === wts.length - 1, wts[i], reg);
    }
  }
}

//  --- the forest --------------------------------------------------------------
function emitForest(sink, board, btns) {
  let root = null;
  if (board.root) root = nodeAt(board.root, pathlib.basename(board.root), "");
  if (!root) miss("work/", "WORKNONE");
  const ix = indexNodes(root);
  const reg = registry();
  const branchMap = new Map(), foreignMap = new Map();

  //  WORK-007 pass 1: a NODE per work/ wt (work wts are forest nodes too), read
  //  once (attach edge + cur tip), indexed by dir for the recursive track lookup.
  const byWorkWt = new Map();
  const wts = [];
  for (const name of listWork(board.dir)) {
    const dir = join(board.dir, name);
    let repo; try { repo = be.treeAt(dir); } catch (e) { continue; }
    let att, cur;
    try { const log = wtlog.open(repo); att = log.attachedBranch(); cur = log.curTip(); }
    catch (e) { continue; }                              // an unreadable wt
    const w = { key: name, dir: dir, repo: repo, att: att,
                sha: (cur && cur.sha) || "", ts: (cur && cur.ts) || 0n,
                counts: null, node: null, children: [], parentWt: null, mark: "" };
    byWorkWt.set(dir, w);
    wts.push(w);
  }
  //  Pass 2: each wt under its track edge (recursive), then cut cycles and LINK
  //  the surviving work-wt edges — the render graph is a forest by construction.
  for (const w of wts) placeWt(w, ix, byWorkWt, reg, branchMap, foreignMap);
  breakCycles(wts, ix, reg, branchMap, foreignMap);
  for (const w of wts) if (w.parentWt) w.parentWt.children.push(w);

  const rows1 = [];
  treeRows(root, reg, rows1, board.root);
  emitRows(sink, rows1, btns);
  if (branchMap.size) {
    const rows2 = [];
    branchRows(branchMap, reg, rows2);
    emitRows(sink, rows2, btns);
  }
  if (foreignMap.size) {
    const rows3 = [];
    foreignRows(foreignMap, reg, rows3);
    emitRows(sink, rows3, btns);
  }
}

//  --- the verb ----------------------------------------------------------------
//  BE-003 spirit: ONE uniform miss line, then throw (jab maps it to exit!=0).
function miss(arg, code) { io.log("work: " + arg + ": " + code + "\n"); throw code; }

function workOne(arg, board, mode, sink) {
  //  DIS-060 kin: tolerate the scheme'd `work:` spell form via ONE parse.
  let w = String(arg == null ? "" : arg);
  if (w.indexOf(":") >= 0) {
    try { const p = uri._parse(w); if (p.scheme === "work") w = p.path || ""; } catch (e) {}
  }
  if (w !== "" && w !== ".") miss(w, "WORKNONE");        // the forest takes no arg
  emitForest(sink, board, mode !== "plain");
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its args reading `be`.
function work() {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  if (!sink) return;
  const board = workDir();
  if (!board) miss("work/", "WORKNONE");
  const mode = ambient.format();
  const argv = arguments.length ? arguments : [""];
  for (let i = 0; i < argv.length; i++) workOne(argv[i], board, mode, sink);
}
work.jab = "args";
module.exports = work;
//  WORK-001: expose the internals for the repro test (the todo.js model).
module.exports.workDir = workDir;
module.exports.listWork = listWork;
//  WORK-011: expose the shard registry for the graf-parity test.
module.exports.registry = registry;
