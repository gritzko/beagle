//  views/todo/todo.js — BE-038: the read-only ticket-board view.  `todo` shows
//  the open-ticket board (topics + one-liner titles), `todo GET` one topic's
//  list, `todo GET-001` the ticket page itself (thin `todo/GET/GET-001.mkd` or
//  fat `todo/GET/GET-001/README.mkd`).  Args route by SHAPE (bare / TOPIC /
//  TOPIC-123 — the `uc ucnum* "-" dgt+` key rule), never by path resolution;
//  a miss is ONE uniform line + throw (BE-003 spirit): `todo: <arg>: TODONONE`.
//
//  The ticket tree is be.todoRoot() (URI-016: `projectRoot()+"/todo"` — the
//  project root is DETECTED by a climb, never declared by an env var, and the
//  board is that ONE dir, not the first hit of a probe order).  List rows and in-page ticket keys
//  carry hidden context-less `O` click spells (`todo <KEY>`, BE-054 — U is now
//  addresses only) so a pager click re-enters the view IN the unchanged
//  context; `todo/done/` (closed tickets) never lists.
//
//  OPEN filter — TODO-004 supersedes the 2026-07-10 header-grep ruling.  The
//  state left the header for the [/meta/todo] META PAIRS: `Now:` carries
//  OPEN/DONE/DONT/STALE and `Sev:` the CRIT/HIGH/MED/LOW the priority marks
//  used to carry.  755 of the 878 live tickets carry a `Now:` and only 14 still
//  carry ANY header mark (none of them a state or a priority one — [JS],
//  [UMBRELLA], [SLOP] … are category tags), so the header grep had gone DEAD
//  and `Now: DONE` tickets boarded as open.  The pair is now the truth, read
//  through the [TODO-003] index (shared/metaidx.js, ONE find() per verb run);
//  the header mark stays a LEGACY fallback so nothing that was hidden becomes
//  visible — a closed mark still closes (verbs/done/done.js still writes
//  `[DONE]` into the header rather than `Now: DONE`, its own ticket), and a
//  ticket with neither pair nor mark reads open/normal.
//
//  TODO-004 arg grammar (ruling 2026-08-03) — an arg LINE, not a positional
//  call: a topic OR a ticket id, plus ANY NUMBER of `Key:Value` filters in ANY
//  order.  The classes are LEXICAL, case alone separates them:
//    `todo`                  the open board (topics, `Sev:`- then number-ordered)
//    `todo ABC`              one topic's open list          (all-caps word)
//    `todo ABC-123`          one ticket page                (`uc ucnum* "-" dgt+`)
//    `todo Now:OPEN`         one filter, every topic
//    `todo ABC Now:OPEN`     a topic AND a filter
//    `todo Who:gritzko Sev:HIGH`   two keys — they AND
//    `todo Now:OPEN Now:DONE`      one key twice — it ORs
//    `todo Rev:*`            PRESENCE: the key is there with any value
//    `todo Due:`             the key is not defined, or is empty
//  Spaces and colons SEPARATE, so a value carries neither: a SECOND colon in one
//  token is an error in plain words (never an OR shorthand), and a `Rev:`/`See:`
//  value (a URI) is presence-filterable only.  A bare key with no colon is an
//  error pointing at `Key:*`.  Refusals are plain words, never a bare code.
//
//  With no `Now:` in the query the listing hides `Now:DONE` and `Now:DONT`
//  ONLY — `Now:STALE` and pair-less tickets stay listed (fail-open, as an
//  unmarked header used to); ANY mention of `Now:` overrides that default, so
//  `todo Now:DONE` reaches the closed tickets.
//
//  A click REPLACES that key's filter and leaves the REST of the arg line
//  alone: on `todo ABC`, clicking `OPEN` gives `todo ABC Now:OPEN`, and
//  clicking `DONE` then gives `todo ABC Now:DONE` — not two filters, not a
//  fresh line.  The spell therefore carries the WHOLE arg line (argLineWith),
//  never a `todo(key,value)` call — that is what makes replacement and the
//  address-bar display expressible (BRO-025).  In-page meta pairs are the mkd
//  grammar's own `T` (`Key:`) + `S` (value) token pair and each half carries
//  its own spell — key → `Key:*`, value → `Key:value` — as context-less `O`
//  spells, so the pager stays arg-blind and the VERB resolves the arg.
//  TIME-SORT (TODO-004): a FLAT filter result is freshest-first, dirty by mtime
//  above committed by commit ts; the board and topic lists keep `Sev:` order.
//  Topic READMEs are landing pages, NEVER an index (they go stale);
//  `todo KEY` renders any page regardless — direct addressing always works.
//  Page reflinks resolve via the page's OWN refdef footer: a ticket-file
//  target re-enters `todo <KEY>`, any other in-tree page becomes the context-
//  less O spell `cat <meta-root-relative-path>` (right when the pager's context
//  tree IS the meta root; cross-tree authority is a pending ruling).
"use strict";

const pathlib = require("../../shared/util/path.js");
const join    = pathlib.join;
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const ticket  = require("../../shared/ticket.js");    // BRO-012: shared key scan
const SPELL   = require("../../shared/spell.js");      // BE-054: O-spell codec
const metaidx = require("../../shared/metaidx.js");    // TODO-003: the meta index

const EMPTY32 = new Uint32Array(0);
const EXTS = ["mkd", "md", "txt"];        // this board is .mkd-first
const CAP = 1 << 20;                       // 1 MiB page cap (tickets are small)

//  tok32 (dog/tok/TOK.h): [31..27] tag (A+n)  [23..0] end byte offset.
function tokPack(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }
const TAG_U = tagCode("U");
const TAG_F = tagCode("F");
const TAG_S = tagCode("S");
const TAG_N = tagCode("N");
//  BE-040 r3: the BE-041 house button pair — a visible 'Y' label + a hidden
//  'O' click spell (`_uriAt` follows the O verbatim; plain never emits them).
const TAG_Y = tagCode("Y");
const TAG_O = tagCode("O");
const TAG_B = tagCode("B");   // BRO-036: the elastic (pager-resizable) span

//  --- arg SHAPE routing (BRO-023: a pure shape test, no fs probe) -----------
function ucnumRun(w, i) {
  while (i < w.length) {
    const c = w.charCodeAt(i);
    if ((c >= 65 && c <= 90) || (c >= 48 && c <= 57)) i++;
    else break;
  }
  return i;
}
//  "GET" → topic, "GET-001" → key, anything else → null.
function shape(w) {
  if (!w.length) return null;
  const c0 = w.charCodeAt(0);
  if (c0 < 65 || c0 > 90) return null;               // must open uppercase
  const run = ucnumRun(w, 0);
  if (run === w.length) return "topic";
  if (w[run] !== "-") return null;
  let j = run + 1;
  if (j === w.length) return null;
  while (j < w.length) {
    const c = w.charCodeAt(j);
    if (c >= 48 && c <= 57) j++;
    else return null;
  }
  return "key";
}
//  WORK-010: the BASE ticket key a name CARRIES — its leading `TOPIC-NNN`,
//  IGNORING any trailing suffix (a letter run or `-word`: `PIN-1b`, `URI-016-adv`,
//  `STATUS-008-f21` all → their base key).  "" when the name does not OPEN with a
//  key.  Same char rules as shape() (one parser), just tolerant of the tail.
function ticketKey(w) {
  if (!w.length) return "";
  const c0 = w.charCodeAt(0);
  if (c0 < 65 || c0 > 90) return "";                 // must open uppercase
  const run = ucnumRun(w, 0);
  if (run === w.length || w[run] !== "-") return "";
  let j = run + 1;
  if (j === w.length || w.charCodeAt(j) < 48 || w.charCodeAt(j) > 57) return "";
  while (j < w.length && w.charCodeAt(j) >= 48 && w.charCodeAt(j) <= 57) j++;
  return w.slice(0, j);                              // TOPIC-NNN, suffix dropped
}
function keyTopic(key) { return key.slice(0, key.indexOf("-")); }


//  --- the board root --------------------------------------------------------
//  URI-016: THE board dir is be.todoRoot() — `projectRoot()+"/todo"`, one dir,
//  no probe order.  → { root, dir } when it exists, null when it does not (no
//  repo, or a project with no ticket tree).  `dir` is todoRoot() itself; NEVER
//  join(root, "todo") again — todoRoot() already carries the `todo` segment.
//  `root` is the PROJECT root, the META tree (todo/, wiki/, meta/ live under
//  it) — page links re-anchor there.
function boardDir() {
  if (typeof be === "undefined" || !be.todoRoot) return null;
  const dir = be.todoRoot();
  if (!dir) return null;
  try { if (io.stat(dir).kind !== "dir") return null; } catch (e) { return null; }
  return { root: pathlib.dirname(dir), dir: dir };
}

//  --- fs probes -------------------------------------------------------------
function isDir(p)  { try { return io.stat(p).kind === "dir"; } catch (e) { return false; } }
function readBytes(full) {
  let st;
  try { st = io.lstat(full); } catch (e) { return null; }
  if (st.kind !== "reg") return null;
  if (st.size === 0) return new Uint8Array(0);
  const size = Number(st.size) < CAP ? Number(st.size) : CAP;
  let fd;
  try { fd = io.open(full, "r"); } catch (e) { return null; }
  try { const b = io.buf(size + 16); io.readAll(fd, b, size); return b.data().slice(); }
  catch (e) { return null; }
  finally { try { io.close(fd); } catch (e) {} }
}
//  A key's page file under the board dir: thin `TOPIC/KEY.<ext>` first, then
//  fat `TOPIC/KEY/README.<ext>`; null when absent.
function pageFile(dir, key) {
  const base = join(dir, join(keyTopic(key), key));
  for (const ext of EXTS) { const p = base + "." + ext; try { io.stat(p); return p; } catch (e) {} }
  for (const ext of EXTS) { const p = join(base, "README." + ext); try { io.stat(p); return p; } catch (e) {} }
  return null;
}
//  A page's TITLE = its first line, `#` markers + padding stripped.
function pageTitle(file) {
  const b = readBytes(file);
  if (!b || !b.length) return "";
  let nl = 0; while (nl < b.length && b[nl] !== 10) nl++;
  let s = utf8.Decode(b.slice(0, nl));
  let i = 0; while (i < s.length && s[i] === "#") i++;
  while (i < s.length && s[i] === " ") i++;
  return s.slice(i);
}

//  The header MARK's [ … ] span (the `[` and `]` char indices) — an UPPERCASE
//  `[…]` word right after the key, either side of the colon (`KEY [MARK]:` or
//  `KEY: [MARK] `); null when absent/malformed.  headerMark/stripMark share it.
function markSpan(key, title) {
  if (title.indexOf(key) !== 0) return null;
  let i = key.length;
  while (title[i] === " ") i++;
  if (title[i] === ":") { i++; while (title[i] === " ") i++; }
  if (title[i] !== "[") return null;
  let j = i + 1;
  while (j < title.length) {
    const c = title.charCodeAt(j);
    if (c >= 65 && c <= 90) j++;
    else break;
  }
  return (j > i + 1 && title[j] === "]") ? { i: i, j: j } : null;
}
//  The header MARK text (`OPEN`/`HIGH`/… ); "" when absent (both placements).
function headerMark(key, title) {
  const s = markSpan(key, title);
  return s ? title.slice(s.i + 1, s.j) : "";
}
//  WORK-008: the title with its [MARK] token stripped (both placements), colon
//  spacing normalized to `KEY: title`; a markless title passes through as-is.
function stripMark(key, title) {
  const s = markSpan(key, title);
  if (!s) return title;
  const before = title.slice(0, s.i).replace(/\s+$/, "");
  const after = title.slice(s.j + 1).replace(/^\s+/, "");
  const sep = before[before.length - 1] === ":" && after && after[0] !== ":" ? " " : "";
  return before + sep + after;
}
//  The LEGACY header mark still closes what it always closed (`done` writes
//  `[DONE]`, its own ticket) — nothing that was hidden may become visible.
const CLOSED = { DONE: true, DONT: true, STALE: true };   // header marks only
//  TODO-004 ruling 2026-08-03: the implicit `Now:` default hides DONE and DONT
//  ONLY.  `Now:STALE` is a live-but-superseded ticket and stays listed, exactly
//  as a pair-less ticket does — a default filter fails OPEN.
const HIDDEN = { DONE: true, DONT: true };           // the `Now:` pair default
const PRIO = { CRIT: 0, HIGH: 1, MED: 2, LOW: 3 };   // unmarked / unknown = 2
const STATES = ["OPEN", "DONE", "DONT", "STALE"];    // [/meta/todo] `Now:`
const PRIOS = ["CRIT", "HIGH", "MED", "LOW"];        // [/meta/todo] `Sev:`
const NOW = "Now", SEV = "Sev";

//  --- the meta-pair index (TODO-003) -----------------------------------------
//  ONE metaidx.find() per verb RUN.  find() is the whole read: it scans the
//  ticket tree, indexes whatever is unindexed and answers, so there is nothing
//  to build first and no second scan to write here.  `false` = no answer (a
//  project with no store shard — the index lives in it): the board then falls
//  back to the legacy header mark and the meta ARG forms refuse in words.
//  Reset per invocation (be.now discipline) so a pager click that follows a
//  `done` re-render never reads a stale snapshot.
//  Keyed by the ticket's own FILE, never by its code: `todo/done/GET-9.mkd`
//  (the closed-ticket parking lot) carries the SAME code as a live
//  `todo/GET/GET-9.mkd`, and a code-keyed map would let the parked copy's
//  pairs answer for the live ticket.
let _snap = null, _snapErr = "";
function snapshot() {
  if (_snap !== null) return _snap;
  let r;
  try { r = metaidx.find({}); }
  catch (e) { _snapErr = String(e); return (_snap = false); }
  const byFile = Object.create(null);
  for (const t of r.tickets) byFile[t.file] = t;
  return (_snap = { byFile: byFile, list: r.tickets });
}
//  A narrowed query — the index does the filtering (presence / exact match).
//  Never memoized: it is the arg's OWN question, asked once.
function query(q) {
  try { return metaidx.find(q).tickets; }
  catch (e) { _snapErr = String(e); return null; }
}

//  metaidx packs a value once; comparing two packed values is comparing what
//  the index compares (despaced + decased, per-key normalizers) — never a
//  hand-rolled match.  The packing is memoized: the board asks it per ticket.
const _packed = Object.create(null);
function packed(key, word) {
  const k = key + "\u0000" + word;
  let v = _packed[k];
  if (v === undefined) v = _packed[k] = metaidx.packValue(key, word);
  return v;
}
//  meta[Key] = { lit, v } (the payload) -> the index's own row val.
function rowVal(mv) {
  return metaidx.packVal(mv.lit ? metaidx.VK_LIT : metaidx.VK_HASH, mv.v);
}
//  The FIRST vocabulary word a ticket's pair carries; "" when it carries none
//  (no pair at all, or a word outside the vocabulary — which reads normal so
//  the vocabulary can still grow).
function metaWord(meta, key, words) {
  const mv = meta && meta[key];
  if (!mv) return "";
  const v = rowVal(mv);
  for (const w of words) if (packed(key, w) === v) return w;
  return "";
}
//  A ticket FILE's meta, from the run's snapshot; null when it is not indexed.
function metaOf(file) {
  const s = snapshot();
  const e = s && s.byFile[file];
  return e ? e.meta : null;
}
//  The implicit `Now:` default over ONE ticket's meta: the pair hides DONE and
//  DONT, and a legacy closed header mark hides too.
function hiddenByDefault(meta, key, title) {
  return !!HIDDEN[metaWord(meta, NOW, STATES)] || !!CLOSED[headerMark(key, title)];
}
//  the same test for a board row, whose meta comes off the run's snapshot.
function isClosed(file, key, title) {
  return hiddenByDefault(metaOf(file), key, title);
}
//  PRIORITY: `Sev:` orders the list, the legacy header mark is the fallback.
function prioOf(file, key, title) {
  const w = metaWord(metaOf(file), SEV, PRIOS);
  if (w) return PRIO[w];
  const m = headerMark(key, title);
  return PRIO[m] !== undefined ? PRIO[m] : 2;
}
//  Topic then ticket NUMBER — the time sort's tie-break, and the whole order
//  when there is no repo to date the tickets against.
function byCode(a, b) {
  const at = keyTopic(a.key), bt = keyTopic(b.key);
  if (at !== bt) return at < bt ? -1 : 1;
  return parseInt(a.key.slice(a.key.indexOf("-") + 1), 10) -
         parseInt(b.key.slice(b.key.indexOf("-") + 1), 10);
}

//  TODO-004 time sort: a flat listing is FRESHEST FIRST — dirty by fs mtime,
//  else by the blob's introducing commit (BRO-044's lane; mtime ties on clone):
//    1. resolve the repo owning the ticket tree + its tip; none -> byCode;
//    2. descend the tip's tree per path (memoized) to the blob it carries;
//    3. absent blob = untracked = dirty, no read;
//    4. wtlog-stamped mtime = clean (STATUS-011), else hash (classify.wtEqBase);
//    5. batch the clean rows' blobs to the lane, dirty rows never touch it;
//    6. order: dirty by mtime, committed by ts, unattributed last, byCode ties.
let _fresh = null;                                   // the per-run repo handle
function freshRepo(board) {
  if (_fresh !== null) return _fresh;
  _fresh = false;
  let t;
  try { t = require("../../core/resolve_hash.js").treeAt(board.dir); }
  catch (e) { return _fresh; }
  if (!t || !t.wt) return _fresh;
  let k, wtl, tip;
  try {
    k = require("../../shared/store.js").open(t.storePath, t.project);
    wtl = require("../../shared/wtlog.js").open(t);
    tip = (wtl.curTip() || {}).sha || "";
  } catch (e) { return _fresh; }
  if (!/^[0-9a-f]{40}$/.test(tip)) return _fresh;
  let rootTree;
  try { rootTree = k.commitTree(tip); } catch (e) { rootTree = undefined; }
  if (!rootTree) return _fresh;
  return (_fresh = { t: t, k: k, wtl: wtl, tip: tip, rootTree: rootTree,
                     trees: Object.create(null) });
}
//  The blob sha the TIP carries at a wt-relative path, "" when it carries none
//  (untracked).  Dir trees memoize: a topic of 40 tickets costs ONE readTree.
function tipBlob(f, rel) {
  const segs = pathlib.split(rel);
  if (!segs.length) return "";
  const name = segs[segs.length - 1], dir = segs.slice(0, -1).join("/");
  let tree = f.trees[dir];
  if (tree === undefined) {
    let d;
    try { d = f.k.descendPath(f.rootTree, segs.slice(0, -1)); } catch (e) { d = undefined; }
    tree = f.trees[dir] = (d && d.kind === "tree") ? d.sha : "";
  }
  if (!tree) return "";
  let ents;
  try { ents = f.k.readTree(tree); } catch (e) { ents = undefined; }
  if (!ents) return "";
  for (const e of ents)
    if (e.name === name && e.mode !== 0o40000) return e.sha;
  return "";
}
//  Date every row of a flat listing: `dirty` + `mtime`, or `ts` (the commit).
//  `rows` carry { file, mtime } off the index; the answer is the SAME array.
function dateRows(board, rows) {
  const f = freshRepo(board);
  if (!f || !rows.length) return rows;
  const mtimeidx = require("../../shared/mtimeidx.js");
  const lastcommit = require("../../shared/lastcommit.js");
  const classify = require("../../shared/classify.js");
  const pfx = f.t.wt + "/";
  const keys = new Set();
  for (const r of rows) {
    if (r.file.indexOf(pfx) !== 0) { r.dirty = true; continue; }   // outside the wt
    const rel = r.file.slice(pfx.length);
    const sha = tipBlob(f, rel);
    if (!sha) { r.dirty = true; continue; }        // untracked: dirty, no read
    //  STATUS-011: an mtime the wtlog itself stamped is verb-written, so the
    //  file is clean with NO content read; anything else is hashed.
    const stamped = !!r.mtime && typeof f.wtl.has === "function" && f.wtl.has(r.mtime);
    if (!stamped && !classify.wtEqBase(f.t.wt, rel, sha)) { r.dirty = true; continue; }
    r.dirty = false;
    r.key60 = mtimeidx.objKey(sha, mtimeidx.T_BLOB);
    keys.add(r.key60);
  }
  const ts = lastcommit.objTimes(f.k, f.tip, keys);
  for (const r of rows) if (!r.dirty) r.ts = ts.get(r.key60);
  return rows;
}
//  The order itself; every tie falls back to byCode, so a listing is stable.
function byFresh(a, b) {
  if (!a.dirty !== !b.dirty) return a.dirty ? -1 : 1;
  if (a.dirty) {
    const am = a.mtime, bm = b.mtime;
    if (am === undefined || bm === undefined) return byCode(a, b);
    return am > bm ? -1 : am < bm ? 1 : byCode(a, b);
  }
  const at = a.ts, bt = b.ts;
  if (at === undefined || bt === undefined)
    return at === bt ? byCode(a, b) : (at === undefined ? 1 : -1);
  return at > bt ? -1 : at < bt ? 1 : byCode(a, b);
}

//  List one topic dir's tickets: `KEY.<ext>` files + fat `KEY/` dirs whose key
//  matches the topic, priority- then numeric-sorted.  Returns
//  [{ key, title, mark }] — ALL tickets, open and closed alike.
//  TODO-001: the readdir entry ALREADY names the page — a thin `KEY.<ext>` is
//  taken verbatim (no probe), only a fat `KEY/` with no thin twin stats README.
function listTopic(dir, topic) {
  const tdir = join(dir, topic);
  let names; try { names = io.readdir(tdir); } catch (e) { return []; }
  const seen = new Map(), keys = [];
  for (let nm of names) {
    //  io.readdir marks a dir entry with a trailing "/" (a fat `KEY/` ticket).
    const dirEnt = nm.length && nm[nm.length - 1] === "/";
    if (dirEnt) nm = nm.slice(0, -1);
    let key = nm, ext = "";
    const dot = nm.indexOf(".");
    if (dot > 0) {
      if (dirEnt || EXTS.indexOf(nm.slice(dot + 1)) < 0) continue;
      key = nm.slice(0, dot);
      ext = nm.slice(dot + 1);
    }
    if (shape(key) !== "key" || keyTopic(key) !== topic) continue;   // README etc
    if (!ext && !dirEnt && !isDir(join(tdir, key))) continue;
    let e = seen.get(key);
    if (!e) { e = { exts: {}, fat: false }; seen.set(key, e); keys.push(key); }
    if (ext) e.exts[ext] = true; else e.fat = true;
  }
  const out = [];
  for (const key of keys) {
    //  pageFile's precedence verbatim: thin `KEY.<ext>` in EXTS order, then the
    //  fat `KEY/README.<ext>` — the ONE probe ladder left, and only for a fat key.
    const e = seen.get(key), base = join(tdir, key);
    let file = null;
    for (const x of EXTS) if (e.exts[x]) { file = base + "." + x; break; }
    if (!file && e.fat)
      for (const x of EXTS) { const p = join(base, "README." + x);
                              try { io.stat(p); file = p; break; } catch (er) {} }
    if (!file) continue;
    const title = pageTitle(file);
    out.push({ key: key, title: title, mark: headerMark(key, title),
                 prio: prioOf(file, key, title), closed: isClosed(file, key, title) });
  }
  out.sort(function (a, b) {
    const ap = a.prio, bp = b.prio;
    if (ap !== bp) return ap - bp;
    const an = parseInt(a.key.slice(a.key.indexOf("-") + 1), 10);
    const bn = parseInt(b.key.slice(b.key.indexOf("-") + 1), 10);
    return an - bn;
  });
  return out;
}

//  TODO-004: one topic's LISTING = its ticket files that are not CLOSED — the
//  `Now:` pair first, the legacy header mark as the fallback.  No README index.
function openTickets(dir, topic) {
  const files = listTopic(dir, topic).filter(function (t) { return !t.closed; });
  return { topic: topic, tickets: files };
}
//  The board's topics: every UPPERCASE-shaped subdir with >=1 ticket ("done" —
//  the closed-ticket parking lot — and lowercase/mixed dirs never list).
function listTopics(dir) {
  let names; try { names = io.readdir(dir); } catch (e) { return []; }
  const out = [];
  for (let nm of names) {
    const dirEnt = nm.length && nm[nm.length - 1] === "/";
    if (dirEnt) nm = nm.slice(0, -1);
    if (nm === "done" || shape(nm) !== "topic") continue;
    if (!dirEnt && !isDir(join(dir, nm))) continue;
    const g = openTickets(dir, nm);
    if (g.tickets.length) out.push(g);
  }
  out.sort(function (a, b) { return a.topic < b.topic ? -1 : a.topic > b.topic ? 1 : 0; });
  return out;
}

//  --- hunk building (the ls.js emitHunk model) -------------------------------
//  Append one text span + tag; returns the new offset.
function span(parts, spans, off, text, tag) {
  const b = utf8.Encode(text);
  parts.push(b);
  spans.push([tag, off + b.length]);
  return off + b.length;
}
//  One list row: `<indent><KEY><rest>\n` with the KEY an `F` token.  BE-054:
//  the pager row (`btn`) follows the KEY with the hidden context-less `O` nav
//  `todo <KEY>` (verb clicks are O) — pager-ONLY chrome, so the plain path
//  emits no click token (an O in a plain hunk would trip the why-plain cursor).
//  BE-040 r3: `btn` also grows the BE-041 button tail — ` ` sep, visible Y
//  `[done]`, hidden O `done KEY` — AFTER the nav O so the title click navigates.
//  TODO-004: every key the ARG LINE names shows its value INLINE as ` [value]`
//  right after the key (where the old `[MARK]` used to read), in arg order, and
//  each bracket is its OWN click — the whole arg line with THAT key's filter
//  replaced.  A ticket missing the key shows no bracket, and a bare board
//  (no filter in the line) shows none at all.
function titleRow(parts, spans, off, indent, key, title, btn, vals) {
  const rest = title.indexOf(key) === 0 ? title.slice(key.length) : " " + title;
  if (indent) off = span(parts, spans, off, indent, TAG_S);
  off = span(parts, spans, off, key, TAG_F);
  if (!btn) {
    for (const v of vals || []) off = span(parts, spans, off, " [" + v.text + "]", TAG_S);
    return span(parts, spans, off, rest + "\n", TAG_S);
  }
  off = span(parts, spans, off, SPELL.mintOspell("", "todo " + key), TAG_O);
  for (const v of vals || []) {
    off = span(parts, spans, off, " [", TAG_S);
    off = span(parts, spans, off, v.text, TAG_N);
    //  a value carrying a colon is not expressible as a filter arg — that
    //  bracket simply does not click (its key half still offers `Key:*`).
    if (v.spell) off = span(parts, spans, off, SPELL.mintOspell("", v.spell), TAG_O);
    off = span(parts, spans, off, "]", TAG_S);
  }
  //  BRO-036: the title is the ONE elastic `B` span — the pager …-cuts / pads
  //  it to the live width in no-wrap mode; bytes stay verbatim.
  off = span(parts, spans, off, rest, TAG_B);
  off = span(parts, spans, off, " ", TAG_S);
  off = span(parts, spans, off, "[done]", TAG_Y);
  off = span(parts, spans, off, "done " + key, TAG_O);
  off = span(parts, spans, off, "\n", TAG_S);
  return off;
}
function feed(sink, banner, parts, spans, off) {
  const body = new Uint8Array(off);
  let p = 0;
  for (const part of parts) { body.set(part, p); p += part.length; }
  const toks = new Uint32Array(spans.length);
  for (let i = 0; i < spans.length; i++) toks[i] = tokPack(spans[i][0], spans[i][1]);
  sink.feed(banner, body, toks, "", 0n);
}

//  The board / one topic, as ONE hunk of title rows.  BE-040 r3: `btns` puts a
//  `[done]` button on every OPEN list row (pager-only; plain passes false).
function emitList(sink, banner, groups, headers, btns) {
  const parts = [], spans = [];
  let off = 0;
  for (const g of groups) {
    if (headers) {                       // topic header row, itself a target
      off = span(parts, spans, off, g.topic, TAG_N);
      //  BE-054: pager-only O nav (plain stays chrome-free — see titleRow).
      if (btns) off = span(parts, spans, off, SPELL.mintOspell("", "todo " + g.topic), TAG_O);
      off = span(parts, spans, off, "\n", TAG_S);
    }
    for (const t of g.tickets)
      off = titleRow(parts, spans, off, headers ? "  " : "", t.key, t.title, btns, t.vals);
    if (!g.tickets.length)               // an explicit `todo TOPIC`, all closed
      off = span(parts, spans, off, (headers ? "  " : "") +
        (g.note || "(no open tickets in todo/" + g.topic + "/)") + "\n", TAG_S);
  }
  feed(sink, banner, parts, spans, off);
}

//  --- page links --------------------------------------------------------------
//  The page's reflink DEFINITIONS: `[name]: <target> …` footer lines → a
//  name→target map (char-scan, one line each; a URL target stays inert later).
function refdefs(text) {
  const map = {};
  for (const line of text.split("\n")) {
    if (line[0] !== "[") continue;
    const rb = line.indexOf("]");
    if (rb <= 1 || line[rb + 1] !== ":") continue;
    let i = rb + 2;
    while (i < line.length && (line[i] === " " || line[i] === "\t")) i++;
    let j = i;
    while (j < line.length && line[j] !== " " && line[j] !== "\t") j++;
    if (j > i) map[line.slice(1, rb)] = line.slice(i, j);
  }
  return map;
}
function isReg(p) { try { return io.stat(p).kind === "reg"; } catch (e) { return false; } }
//  A link TARGET (refdef path, or an inline `/pocket/Page` shortcut) → its
//  click spell (BE-054: minted O at the splice): a ticket file (`KEY.<ext>`
//  basename) re-enters `todo KEY`; any
//  other page resolves against the page's dir, re-anchors META-ROOT-relative
//  and opens as `cat <rel>` (extensionless shortcuts probe `.mkd/.md/.txt`).
//  Scheme'd targets (http:, mailto:) and NAVESCAPE climbs stay inert.  No
//  absolute fs path ever reaches a token; the spell text is all we compose.
function targetSpell(board, pageDirRel, target) {
  if (!target || target.indexOf(":") >= 0) return null;
  const base = pathlib.basename(target);
  const dot = base.lastIndexOf(".");
  const stem = dot > 0 ? base.slice(0, dot) : base;
  const ext = dot > 0 ? base.slice(dot + 1) : "";
  if (ext && EXTS.indexOf(ext) >= 0 && shape(stem) === "key" && pageFile(board.dir, stem))
    return "todo " + stem;
  let rel;
  try { rel = pathlib.resolveInTree(target[0] === "/" ? "" : pageDirRel, target); }
  catch (e) { return null; }                            // NAVESCAPE → no link
  if (!rel) return null;
  const abs = join(board.root, rel);
  if (isReg(abs)) return "cat " + rel;
  if (!ext) for (const e2 of EXTS) if (isReg(abs + "." + e2)) return "cat " + rel + "." + e2;
  return null;
}

//  A ticket page: raw .mkd bytes; non-plain modes tokenize with the mkd
//  grammar and splice a hidden context-less `O` after every RESOLVABLE link
//  token (BE-054, cat.js withLinks model, board-scoped): a bare/`[KEY]` ticket
//  key → `todo KEY`; a `[ref]`/`[/pocket/Page]` reflink → its refdef target's
//  spell (todo/cat).  The page's OWN key gets no self-link.
function emitPage(sink, board, key, file, mode, a) {
  const bytes = readBytes(file);
  if (bytes == null) return false;
  let body = bytes, toks = EMPTY32;
  if (mode !== "plain") {
    try { toks = tok.parse(bytes, "mkd"); } catch (e) { toks = EMPTY32; }
    if (toks.length) {
      const pfx = board.root + "/";
      const rel = file.indexOf(pfx) === 0 ? file.slice(pfx.length) : "";
      const linked = pageLinks(board, key, pathlib.dirname(rel), bytes, toks, a);
      body = linked.body; toks = linked.toks;
    }
  }
  sink.feed("todo " + key, body, toks, "", 0n);
  return true;
}
function pageLinks(board, selfKey, pageDirRel, body, toks, a) {
  const defs = refdefs(utf8.Decode(body));
  const us = new Array(toks.length);
  const sta = new Array(toks.length);
  let extra = 0, nlinks = 0, prev = 0;
  for (let i = 0; i < toks.length; i++) { us[i] = null; sta[i] = prev; prev = tokEnd(toks[i]); }
  const word = (i) => utf8.Decode(body.slice(sta[i], tokEnd(toks[i])));
  //  BE-054: the verb click is a context-less O (empty ctx = "here").
  const mint = (spell) => (spell && spell !== "todo " + selfKey)
      ? utf8.Encode(SPELL.mintOspell("", spell)) : null;
  const keySpell = (w) => (shape(w) === "key" && w !== selfKey && pageFile(board.dir, w))
      ? "todo " + w : null;
  //  A bare ticket key is an `F` token — it links on its own.
  for (let i = 0; i < toks.length; i++)
    if (tokTagL(toks[i]) === "F" && tokEnd(toks[i]) > sta[i]) us[i] = mint(keySpell(word(i)));
  //  TODO-004: the META-PAIR block, read through the ONE shared matcher
  //  (metaidx.metaBlock — the mkd tokenizer's `T` = the line-opening `Key:`,
  //  plus the indent + "directly under the header" rulings of 2026-08-03).
  //  The earlier claim here — that a `T` never fires on an INDENTED word, so
  //  the view needed no second grammar — was FALSE: `T` is indent-tolerant,
  //  while the index's line regex was anchored at column 0, so TODO-003's
  //  four-space `Now:`/`Sev:` rendered and clicked but matched nothing.  One
  //  matcher now answers both, so they cannot drift again.  Each half gets its
  //  own whole-ARG-LINE spell: the key → `Key:*` (every ticket carrying it),
  //  the value → `Key:value`.  The page's own id is the arg line here, so both
  //  resolve against its TOPIC (argLineWith).
  for (const p of metaidx.metaBlock(body, toks)) {
    if (metaidx.codeOf(p.key) === null) continue;  // not a registered pair shape
    us[p.ki] = mint(spellWith(a, p.key, "*"));
    if (p.vi >= toks.length) continue;
    const fv = filterVal(p.value);
    if (fv !== null) us[p.vi] = mint(spellWith(a, p.key, fv));
  }
  //  DOG-024: a span is markup + text, so a reflink is the token RUN `[` … `]`,
  //  not one `G` token — read the label from between the brackets and give every
  //  token of the run the spell (a click anywhere on the link re-enters).
  for (let i = 0; i < toks.length; i++) {
    if (tokTagL(toks[i]) !== "G" || word(i) !== "[") continue;
    let j = i + 1;
    while (j < toks.length && !(tokTagL(toks[j]) === "G" && word(j) === "]")
           && word(j).indexOf("\n") < 0) j++;
    if (j >= toks.length || tokTagL(toks[j]) !== "G") continue;   // unclosed
    const label = utf8.Decode(body.slice(tokEnd(toks[i]), sta[j]));
    let spell = keySpell(label);
    if (!spell && defs[label] !== undefined) spell = targetSpell(board, pageDirRel, defs[label]);
    else if (!spell && label[0] === "/") spell = targetSpell(board, pageDirRel, label);
    const u = mint(spell);
    if (u) for (let k = i; k <= j; k++) if (!us[k]) us[k] = u;
    i = j;
  }
  for (let i = 0; i < toks.length; i++) if (us[i]) { extra += us[i].length; nlinks++; }
  if (!nlinks) return { body: body, toks: toks };
  const out = new Uint8Array(body.length + extra);
  const ntoks = new Uint32Array(toks.length + nlinks);
  let op = 0, oi = 0;
  prev = 0;
  for (let i = 0; i < toks.length; i++) {
    const end = tokEnd(toks[i]);
    for (let p = prev; p < end; p++) out[op++] = body[p];
    ntoks[oi++] = tokPack((toks[i] >>> 27) & 0x1f, op);
    if (us[i]) { out.set(us[i], op); op += us[i].length; ntoks[oi++] = tokPack(TAG_O, op); }
    prev = end;
  }
  return { body: out, toks: ntoks };
}
function tokTagL(w) { return String.fromCharCode(65 + ((w >>> 27) & 0x1f)); }
function tokEnd(w) { return w & 0xffffff; }

//  --- the ARG LINE (TODO-004, ruling 2026-08-03) -----------------------------
//  parseArgs(argv) -> { subject, filters, toks } | { err }
//    subject   { kind: "topic"|"key", w }  or null (the board)
//    filters   [{ key, kind: "eq"|"any"|"none", val }] in arg order
//    toks      the arg line VERBATIM, in order — the click spells rewrite IT
//  Steps, one per token:
//    1. a token holding a `:` is a FILTER — split at the FIRST colon;
//    2. a SECOND colon in it is an error (a colon separates; repeat the key
//       to OR instead), and a key outside the `[A-Z][a-z][a-z]` shape is one too;
//    3. value `*` = presence, "" = not-defined-or-empty, else exact;
//    4. else a `TOPIC-123` / `TOPIC` shape is the SUBJECT — only one per line;
//    5. else a bare meta KEY is an error pointing at `Key:*`;
//    6. else the word is of no known class and the line refuses in plain words.
function parseArgs(argv) {
  const filters = [], toks = [];
  let subject = null;
  for (let i = 0; i < argv.length; i++) {
    const w = unscheme(argv[i]);
    if (w === "" || w === ".") continue;
    const ci = w.indexOf(":");
    if (ci > 0) {
      const key = w.slice(0, ci), val = w.slice(ci + 1);
      if (val.indexOf(":") >= 0)
        return { err: "'" + w + "' carries two colons — a colon separates, so a" +
                 " filter is one Key:Value; repeat the key to widen it, like " +
                 key + ":" + val.slice(0, val.indexOf(":")) + " " + key + ":" +
                 val.slice(val.indexOf(":") + 1) };
      if (metaidx.codeOf(key) === null)
        return { err: "'" + key + ":' is not a meta key — a key is three letters," +
                 " capital first, like Now: or Who:" };
      filters.push({ key: key, val: val,
                     kind: val === "*" ? "any" : val === "" ? "none" : "eq" });
      toks.push(w);
      continue;
    }
    const s = shape(w);
    if (s) {
      if (subject)
        return { err: "'" + subject.w + "' and '" + w + "' — one topic or one" +
                 " ticket id at a time, plus any number of Key:Value filters" };
      subject = { kind: s, w: w };
      toks.push(w);
      continue;
    }
    if (metaidx.codeOf(w) !== null)
      return { err: "'" + w + "' is a meta key with no value — write " + w +
               ":VALUE to match one, " + w + ":* for any value, or " + w +
               ": for the tickets that lack it" };
    return { err: "'" + w + "' is not a ticket code, a topic or a Key:Value" +
             " filter — try ABC-123, ABC or Now:OPEN" };
  }
  //  A ticket id names ONE page; a page cannot be narrowed by a filter.
  if (subject && subject.kind === "key" && filters.length)
    return { err: "'" + subject.w + "' names one ticket page — a Key:Value" +
             " filter needs a topic (" + keyTopic(subject.w) + ") or none" };
  return { subject: subject, filters: filters, toks: toks };
}

//  The arg line with KEY's filter REPLACED by `Key:val` — every other token
//  stays exactly where it was, and a key the line does not yet carry is
//  APPENDED.  A ticket-id subject becomes its TOPIC: a page takes no filters,
//  and the topic is what the id already carries, so clicking a pair on
//  `todo ABC-123` opens `todo ABC Now:OPEN` rather than an unrenderable line.
function argLineWith(a, key, val) {
  const out = [];
  let done = false;
  for (const t of a.toks) {
    const ci = t.indexOf(":");
    if (ci > 0 && t.slice(0, ci) === key) {
      if (!done) { out.push(key + ":" + val); done = true; }
      continue;                                    // an OR'd repeat collapses
    }
    out.push(shape(t) === "key" ? keyTopic(t) : t);
  }
  if (!done) out.push(key + ":" + val);
  return out.join(" ");
}
//  the whole-arg-line click spell (BRO-025: never a `todo(key,value)` call).
function spellWith(a, key, val) { return "todo " + argLineWith(a, key, val); }

//  A rendered value -> the filter ARG that matches it, or null when none does.
//  Spaces and colons SEPARATE, so a spaced value rides its DESPACED index form
//  (the index compares that form, so the filter still matches exactly); a value
//  carrying a colon — a `Rev:`/`See:` URI — is not expressible at all and only
//  its key's `Key:*` presence click remains.
function filterVal(raw) {
  const t = String(raw == null ? "" : raw).trim();
  if (t === "") return null;
  if (!/[\s:]/.test(t)) return t;
  const n = metaidx.normalize(t);
  return (n && n.indexOf(":") < 0) ? n : null;
}

//  --- the filter query -------------------------------------------------------
//  ONE clause against ONE ticket's packed meta.  `packed(key,"")` is the value
//  an EMPTY pair packs to, so presence (`Key:*`) and absence (`Key:`) are the
//  two sides of the same comparison — no second grammar, no raw re-read.
function clauseHolds(meta, key, cl) {
  const mv = meta && meta[key];
  const v = mv ? rowVal(mv) : undefined;
  const empty = packed(key, "");
  if (cl.kind === "none") return v === undefined || v === empty;
  if (v === undefined) return false;
  if (cl.kind === "any") return v !== empty;
  return v === packed(key, cl.val);
}
//  Repeating a key ORs its clauses; different keys AND (the loop in todoFilter).
function anyHolds(meta, key, clauses) {
  for (const cl of clauses) if (clauseHolds(meta, key, cl)) return true;
  return false;
}

//  `todo ABC Now:OPEN Now:DONE Who:gritzko` — the flat filter listing.
//  Steps:
//    1. group the clauses by key, keeping arg order;
//    2. hand the INDEX every key that appears ONCE with an exact/presence value
//       (an OR'd key and a `Key:` absence cannot narrow through it, and its
//       absent-key early-out answers those without opening a ticket);
//    3. drop what the arg line's topic, the board layout or a remaining clause
//       excludes, then apply the implicit `Now:` default unless the line
//       mentions `Now:` at all;
//    4. read each surviving ticket's RAW pairs for the inline values (the row
//       payload is a match token, never a render source);
//    5. DATE every surviving row (dirty -> fs mtime, clean -> the lane's commit
//       time) and order FRESHEST FIRST, byCode breaking ties; emit ONE hunk.
function todoFilter(board, a, mode, sink) {
  const byKey = Object.create(null), order = [];
  for (const f of a.filters) {
    if (!byKey[f.key]) { byKey[f.key] = []; order.push(f.key); }
    byKey[f.key].push(f);
  }
  const q = {};
  for (const k of order) {
    const cl = byKey[k];
    if (cl.length !== 1) continue;                       // OR: post-filtered
    if (cl[0].kind === "eq") q[k] = cl[0].val;
    else if (cl[0].kind === "any") q[k] = true;
  }
  const hits = query(q);
  if (hits === null) return noIndex();
  const topic = a.subject ? a.subject.w : null;
  const rows = [];
  for (const e of hits) {
    //  the board's OWN addressing must reach it: a parked `todo/done/KEY.mkd`
    //  and any off-layout page the index scanned are not board rows.
    if (topic && keyTopic(e.code) !== topic) continue;
    if (pageFile(board.dir, e.code) !== e.file) continue;
    let all = true;
    for (const k of order) if (!anyHolds(e.meta, k, byKey[k])) { all = false; break; }
    if (!all) continue;
    const title = pageTitle(e.file);
    if (!byKey[NOW] && hiddenByDefault(e.meta, e.code, title)) continue;
    const raw = metaidx.pairs(e.file);
    const vals = [];
    for (const k of order) {
      const t = raw[k] !== undefined ? String(raw[k]).trim() : "";
      if (t === "") continue;
      const fv = filterVal(t);
      vals.push({ text: t, spell: fv === null ? null : spellWith(a, k, fv) });
    }
    rows.push({ key: e.code, title: title, vals: vals,
                file: e.file, mtime: e.mtime });
  }
  //  TODO-004 time-sort: freshest first — dirty (fs mtime) above committed
  //  (introducing-commit time, off BRO-044's lane), byCode breaking every tie.
  //  With no repo to date against, NOTHING is dated and byFresh IS byCode.
  dateRows(board, rows);
  rows.sort(byFresh);
  const line = a.toks.join(" ");
  emitList(sink, "todo " + line, [{ topic: topic || line, tickets: rows,
    note: "(no ticket matches " + line + ")" }], false, mode !== "plain");
}

//  --- the verb ---------------------------------------------------------------
//  BE-003 spirit: ONE uniform miss line, then throw (jab maps it to exit!=0).
function miss(arg, code) { io.log("todo: " + arg + ": " + code + "\n"); throw code; }
//  TODO-004: the arg-line refusals speak plain words (the ticket/topic miss
//  keeps its historic TODONONE line — the BE-038 golden pins it).
function bad(why) {
  io.log("todo: " + why + "\n");
  throw "todo: unknown argument";
}
//  The meta-pair index lives in the project store; without one the board still
//  browses (the legacy header mark), but a filter cannot be answered.
function noIndex() {
  io.log("todo: " + (_snapErr || "the meta-pair index is unavailable") + "\n");
  throw "todo: no meta index";
}

//  DIS-060: tolerate the scheme'd `todo:GET-1` spell form via ONE parse.
function unscheme(arg) {
  let w = String(arg == null ? "" : arg);
  if (w.indexOf(":") >= 0) {
    try { const p = uri._parse(w); if (p.scheme === "todo") w = p.path || ""; } catch (e) {}
  }
  return w;
}

//  JAB-004: PLAIN verb (`.jab="args"`) reads its args off `be`.
//  TODO-004 (ruling 2026-08-03): the whole arg LINE is ONE question — a topic
//  or a ticket id plus any number of `Key:Value` filters, in any order.  One
//  hunk comes out, and its banner IS the arg line the address bar shows.
function todo() {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  if (!sink) return;
  const board = boardDir();
  if (!board) miss("todo/", "TODONONE");
  const mode = ambient.format();
  _snap = null; _snapErr = "";        // ONE index snapshot per verb invocation
  _fresh = null;                      // ...and ONE repo handle for the time sort
  const argv = [];
  for (let i = 0; i < arguments.length; i++) argv.push(String(arguments[i]));
  const a = parseArgs(argv);
  if (a.err) bad(a.err);
  //  a TOPIC that is no dir keeps the historic uniform miss line, filter or not.
  if (a.subject && a.subject.kind === "topic" && !isDir(join(board.dir, a.subject.w)))
    miss(a.subject.w, "TODONONE");
  if (a.filters.length) { todoFilter(board, a, mode, sink); return; }
  if (!a.subject) {
    emitList(sink, "todo", listTopics(board.dir), true, mode !== "plain");
    return;
  }
  if (a.subject.kind === "topic") {
    emitList(sink, "todo " + a.subject.w, [openTickets(board.dir, a.subject.w)],
             false, mode !== "plain");
    return;
  }
  //  Direct addressing ALWAYS works — open or closed, the page renders.
  const file = pageFile(board.dir, a.subject.w);
  if (!file || !emitPage(sink, board, a.subject.w, file, mode, a))
    miss(a.subject.w, "TODONONE");
}
todo.jab = "args";
module.exports = todo;
//  BE-038: expose the internals for the repro test (the ls.js/log.js model).
module.exports.shape = shape;
//  WORK-010: the work view reads the BASE ticket key off a (maybe suffixed) wt name.
module.exports.ticketKey = ticketKey;
module.exports.listTopics = listTopics;
module.exports.pageFile = pageFile;
//  BE-043: the work board reuses the board root + the page-title read.
module.exports.boardDir = boardDir;
module.exports.pageTitle = pageTitle;
//  WORK-008: the work view strips the status mark from the minted post title.
module.exports.stripMark = stripMark;
//  TODO-004 time-sort: the dating pass and its comparator, for the golden.
module.exports.dateRows = dateRows;
module.exports.byFresh = byFresh;
module.exports.byCode = byCode;
