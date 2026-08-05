//  views/todo/todo.js — BE-038: the read-only ticket-board view.  `todo` shows
//  the open-ticket board (topics + one-liner titles), `todo GET` one topic's
//  list, `todo Now:OPEN` a meta-pair filter listing.  Args route by SHAPE (bare
//  / TOPIC / `Key:Value` — the `uc ucnum* "-" dgt+` key rule stays LEXICAL),
//  never by path resolution; a miss is ONE uniform line + throw (BE-003
//  spirit): `todo: <arg>: TODONONE`.
//
//  TODO-011 (ruling gritzko 2026-08-04): `todo` is the TABULAR side ONLY.  The
//  ONE-TICKET PAGE moved out to its own view, `views/ticket/ticket.js` —
//  `ticket GET-001` renders it.  A ticket-id arg here no longer serves a page:
//  it refuses in plain words pointing at `ticket <KEY>` (pageRefusal below).
//  The two views share ONE home for everything they both need — this file: the
//  key/topic LEXER (shape/ticketKey/keyTopic), the board root (boardDir), the
//  page FILE ladder (pageFile), the byte read (readBytes), the arg-line parser
//  (parseArgs/argLineWith/spellWith) and the click classifiers (navSpell,
//  filterVal).  ticket.js requires them; nothing is implemented twice.
//  Every ticket-KEY click — a list row, a board inline value, an in-page
//  ticket-valued pair, an in-page bare key or `[KEY]` reflink — spells
//  `ticket <KEY>`; every FILTER / TOPIC click stays a `todo` spell.
//
//  The ticket tree is be.todoRoot() (URI-016: `projectRoot()+"/todo"` — the
//  project root is DETECTED by a climb, never declared by an env var, and the
//  board is that ONE dir, not the first hit of a probe order).  List rows
//  carry hidden context-less `O` click spells (TODO-011: `ticket <KEY>` for a
//  ticket row, `todo <TOPIC>` for a topic header — BE-054, U is now addresses
//  only) so a pager click re-enters IN the unchanged context; `todo/done/`
//  (closed tickets) never lists.
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
//    `todo ABC-123`          REFUSED — that page is `ticket ABC-123` (TODO-011)
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
//  TODO-008: a VALUE that lexes as a ticket id (`See: BE-050`) is a LINK — it
//  navigates to that ticket's PAGE (TODO-011: `ticket BE-050`), while the key
//  half keeps its presence filter, a `todo` spell.
//  TIME-SORT (TODO-004): a FLAT filter result is freshest-first, dirty by mtime
//  above committed by commit ts; the board and topic lists keep `Sev:` order.
//
//  TODO-005 grows the pager ROW to `<●> KEY ┄ [FILE] [COMMIT] <title> [done]`:
//  the `Sev:` bullet leads (CRIT red, HIGH orange, MED plain, LOW dim; a closed
//  ticket a filter still shows reads hollow `○`) and a matching `work/<KEY>*`
//  worktree (WORK-010's match run backwards) hangs TWO fixed-width BUTTON
//  FRAMES off the key column — the wt's whole verb surface, one click each:
//    `[ i nn nn nn  ✓]`  16 cols — the ` i` button opens `status //<wt>`, then
//        the three staging counts (changed → bare `put`, gone → bare `delete`,
//        untracked → `put +`) and the ` ✓` commit (`post 'KEY: <title>'`).
//        Each count slot is THREE-STATE: unstaged rows light the button, a
//        class with nothing left unstaged greys its STAGED count (no spell),
//        an empty class blanks.  The counts are the WHOLE tree's — bare
//        put/delete descend every mounted sub (SUBS-044), so a sub's buckets
//        fold into the same tally or the button would lie about its reach.
//    `[ ≡ nn nn]`        10 cols — the ` ≡` button opens `log //<wt>`, then TWO
//        FIXED sub-slots, POST position then GET position (a behind count must
//        never drift into the post column): ahead mints `post`, behind mints
//        `get`, and a DIVERGED `A⇄B` is ONE `patch` button over both slots.  The
//        commit frame stays TOP-repo — ahbeh is the wt's own line, not its subs'.
//  Every button is 2 CELLS carrying its tone as FOREGROUND over a VERY PALE
//  wash of that same tone — never an inversion — with a face of a count (sigil
//  + digit under ten, bare two digits above) or space+icon.  Both colours ride
//  the button's own hidden `O`, whose bytes open `#<pale><tone> ` (view/bro.js
//  whyBgAt: the WHY-001 prefix's bg then fg slots), so a button needs NO tok tag
//  of its own — the 32-code tag space is full — and the pager sheds the prefix
//  at the first space before firing.  Tones, faces and the ONE pale() the wash
//  is derived by live in view/theme.js, the ONE place an SGR value lives.  A
//  DISABLED button is plain grey fg with no wash.  The face IS the whole button:
//  every cell of it is painted and every cell of it clicks, no dead padding.
//  A frame DELIMITS its own columns, so nothing inside the brackets is
//  ┄-filled; dotted leaders live OUTSIDE them, and a wt-LESS row simply ┄-fills
//  the whole frames region so every title in the hunk lands at one column.
//  A ticket whose `Sub:` names a listed OPEN parent nests under it on work's
//  dotted rails.  Plain stays chrome-free (rails are structure, they stay);
//  the `Sev:` ORDER applies to both.
//  Topic READMEs are landing pages, NEVER an index (they go stale);
//  `ticket KEY` renders any page regardless — direct addressing always works.
"use strict";

const pathlib = require("../../shared/util/path.js");
const join    = pathlib.join;
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const ticket  = require("../../shared/ticket.js");    // BRO-012: shared key scan
const SPELL   = require("../../shared/spell.js");      // BE-054: O-spell codec
const metaidx = require("../../shared/metaidx.js");    // TODO-003: the meta index
const CACHE   = require("../../shared/cache.js");      // STATUS-019: the rev tree
//  CODE-028: work.js requires this module back, so the handle must be published
//  BEFORE that require; work.js publishes ITS handle the same way.
module.exports = todo;
const worklib = require("../work/work.js");
const wtlog   = require("../../shared/wtlog.js");
const mtimeidx = require("../../shared/mtimeidx.js");
const lastcommit = require("../../shared/lastcommit.js");
const classify = require("../../shared/classify.js");
const recurse = require("../../core/recurse.js");
const rh      = require("../../core/resolve_hash.js");
const store   = require("../../shared/store.js");
const CI      = require("../../shared/ci.js");

const EXTS = ["mkd", "md", "txt"];        // this board is .mkd-first
const CAP = 1 << 20;                       // 1 MiB page cap (tickets are small)

//  tok32 (dog/tok/TOK.h): [31..27] tag (A+n)  [23..0] end byte offset.
function tokPack(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }
const TAG_U = tagCode("U");
const TAG_F = tagCode("F");
const TAG_S = tagCode("S");
const TAG_N = tagCode("N");
//  BE-040 r3: a button is a visible face + a hidden 'O' click spell (`_uriAt`
//  follows the O verbatim; plain never emits them).  TODO-005 retired the 'Y'
//  label slot — every button now rides its class tag + its O's truecolor pair.
const TAG_O = tagCode("O");
const TAG_B = tagCode("B");   // BRO-036: the elastic (pager-resizable) span
//  TODO-005 palette slots (view/bro.js THEME): the sev bullet is red 'M' (CRIT),
//  salmon 'A' (HIGH), default 'S' (MED), gray 'D' (LOW + the hollow closed ○).
//  The frames need NO tag slot: a button's colour rides its own `O`
//  (view/theme.js BTN), and 'D' greys a disabled button and the brackets.
const TAG_M = tagCode("M"), TAG_A = tagCode("A"), TAG_D = tagCode("D");

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
//  TODO-005: ONE read per ticket yields BOTH halves of its head (the TODO-001
//  budget): the TITLE (first line, `#` markers + padding stripped) and the
//  `Key: value` META pairs DIRECTLY under it — the run ends at the first line
//  that is not a pair, so no body line can ever forge one.  The pairs the INDEX
//  answers (`Now:`/`Sev:`) come off metaOf, not this scan; the scan is for what
//  a packed row cannot give back — `Sub:`, a ticket CODE the render must read.
const META_PAIR = /^([A-Z][a-z][a-z0-9]): (.*)$/;
function pageHead(file) {
  const out = { title: "", meta: {} };
  const b = readBytes(file);
  if (!b || !b.length) return out;
  let p = 0, n = 0;
  while (p < b.length) {
    let nl = p; while (nl < b.length && b[nl] !== 10) nl++;
    const line = utf8.Decode(b.slice(p, nl));
    if (n === 0) {
      let i = 0; while (i < line.length && line[i] === "#") i++;
      while (i < line.length && line[i] === " ") i++;
      out.title = line.slice(i);
    } else {
      const m = META_PAIR.exec(line);
      if (!m) break;
      if (out.meta[m[1]] === undefined) out.meta[m[1]] = m[2];
    }
    p = nl + 1; n++;
  }
  return out;
}
//  A page's TITLE alone (the work view's post-message read).
function pageTitle(file) { return pageHead(file).title; }

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
//  TODO-005: the sev BULLET's colour slot, indexed by that same PRIO number.
const PRIO_TAG = [TAG_M, TAG_A, TAG_S, TAG_D];       // red, orange, plain, dim
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
  try { t = rh.treeAt(board.dir); }
  catch (e) { return _fresh; }
  if (!t || !t.wt) return _fresh;
  let k, wtl, tip;
  try {
    k = store.open(t.storePath, t.project);
    wtl = wtlog.open(t);
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
    //  TODO-005: the ONE read hands back the title AND the head's raw pairs —
    //  `Sub:` is a ticket CODE the family nesting must read, and the index's
    //  packed payload is a match token, never a render source.
    const head = pageHead(file);
    const title = head.title;
    //  TODO-006: the row names its FILE — the line memo's key (a parked
    //  `todo/done/KEY.mkd` carries the same code, so a code key would collide).
    out.push({ key: key, title: title, mark: headerMark(key, title), meta: head.meta,
                 file: file,
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
//  TODO-006 r3: the HEAD block's memo — the topic DIR is its witness, so ONE
//  ticket edit re-reads THAT topic's heads and every other topic replays its
//  rows (title, prio, closed, meta) with no file read at all.  The rows are the
//  same objects a later render re-derives `wt`/`rails`/`stat` on.
function openTickets(dir, topic) {
  const tdir = join(dir, topic);
  const rv = _memoOn ? CACHE.rev(tdir) : 0;
  const had = _memoOn ? _topicMemo.get(tdir) : undefined;
  if (had && had.rev === rv) return { topic: topic, tickets: had.tickets };
  const files = listTopic(dir, topic).filter(function (t) { return !t.closed; });
  if (_memoOn) _topicMemo.set(tdir, { rev: rv, tickets: files });
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

//  --- the wt side (TODO-005) --------------------------------------------------
//  views/work/work.js requires THIS module (WORK-010 ticket titles); the
//  reverse edge is the top-level `worklib` handle above (CODE-028).
//  Everything below rides work's own machinery verbatim.

//  Per-render-run state (the TODO-001 memo discipline, reset by every run):
//  the wt index, work's shard registry (keeper+graf opened ONCE per shard), the
//  per-wt status read and the per-TRACK tip (a hundred wts track the same
//  `///be/` — resolving it once is the difference between 2 s and 40 s).
//  A board with NO wt-having ticket opens nothing.
let _wtix, _reg = null, _wtc = new Map(), _tips = new Map();
//  STATUS-019: the ACROSS-run memos, keyed by the shared/cache.js rev of the wt
//  (resp. the sub boundary) — a spot whose rev stands still cannot have changed.
//  TODO-006 r3: one memo PER BLOCK, each with its own witness — the FILE counts
//  by rev(wtDir), the ticket HEADS by rev(topic dir), the ahbeh pair by the tips
//  fingerprint (_ciTips).  They hold numbers and strings, nothing live.
let _fileRev = new Map(), _subRev = new Map(), _ciTips = new Map();
//  TODO-006: the per-run wtlog reads behind the tips FINGERPRINT (below).
let _tipc = new Map();
function runReset() {
  _wtix = undefined; _reg = null; _wtc = new Map(); _tips = new Map();
  _tipc = new Map(); _boardStill = false;
  //  TODO-006: the memos exist only while a watcher is live (cache.js ruling 3)
  //  — one that went away, or came back, invalidates every rev they hold.
  const on = CACHE.stats().live;
  if (on !== _memoOn) {
    _fileRev.clear(); _subRev.clear(); _ciTips.clear();
    _topicMemo.clear(); _boardMemo.clear();
  }
  _memoOn = on;
}
function runClose() { if (_reg) { try { _reg.close(); } catch (e) {} } _reg = null; }

//  TODO-005: the exact reverse of WORK-010's `[?]` — every `work/` wt name maps
//  to the BASE ticket key it carries (suffix-tolerant: `work/PIN-1b` → `PIN-1`),
//  and the name-sorted FIRST wt of a key wins the slot (listWork arrives sorted).
function wtIndex() {
  if (_wtix !== undefined) return _wtix;
  _wtix = new Map();
  const work = worklib;
  let wd = null; try { wd = work.workDir(); } catch (e) { wd = null; }
  if (wd) for (const nm of work.listWork(wd.dir)) {
    const k = ticketKey(nm);
    if (k && !_wtix.has(k)) _wtix.set(k, { name: nm, dir: join(wd.dir, nm) });
  }
  return _wtix;
}

//  TODO-005: a classify.js BUCKET → the frame slot it feeds, split by AXIS.
//  UNSTAGED (still on the wt side: dirty/patched/merged/conflicted content,
//  untracked adds, on-disk deletions) is what a staging button would ACT on, so
//  it lights the slot; STAGED (a row the wtlog already carries) only greys the
//  count — there is nothing left to stage.  `ok` is clean, it tallies nowhere.
const UN_COL = { mod: "chg", pat: "chg", mrg: "chg", con: "chg",
                 unk: "add", mis: "del" };
const ST_COL = { put: "chg", "new": "add", mov: "add", del: "del", rmv: "del" };

//  TODO-005 r2: fold ONE repo's classify counts into the frame's tallies.
function foldCounts(r, repo, log, k) {
  const c = classify.classifyMerge(repo, log, k, {}).counts;
  for (const b in UN_COL) r.un[UN_COL[b]] += c[b] || 0;
  for (const b in ST_COL) { const n = c[b] || 0; r.st[ST_COL[b]] += n; r.staged += n; }
}
//  TODO-005 r2 (BUG): the FILE counts are the WHOLE TREE's.  Bare `put` and bare
//  `delete` both descend every mounted sub (SUBS-044, put's bareStageSubs and
//  delete's bareSweepSubs), so a button that stages `//WT` acts on the subs too
//  and its count must say so — a top-repo-only tally under-reported every mount.
//  ONE classifyMerge per live mount, `.gitmodules` order, depth-first over the
//  SAME recurse.walk spine those folds use (never a second mount scanner).  The
//  COMMIT frame stays top-repo: ahbeh is the wt's own line, not its subs'.
//  STATUS-019: a sub's tallies are a pure function of its own subtree, so a
//  boundary whose rev stands still REPLAYS them instead of paying classifyMerge.
function blankFold() {
  return { un: { chg: 0, add: 0, del: 0 }, st: { chg: 0, add: 0, del: 0 }, staged: 0 };
}
function addFold(r, d) {
  for (const c in d.un) r.un[c] += d.un[c];
  for (const c in d.st) r.st[c] += d.st[c];
  r.staged += d.staged;
}
function foldSubs(r, repo) {
  recurse.walk(repo, "", function (subRepo) {
    const rv = CACHE.rev(subRepo.wt);
    const had = _subRev.get(subRepo.wt);
    if (had && had.rev === rv) { addFold(r, had.d); return; }
    const d = blankFold();
    try {
      const sk = _reg.keeperFor(subRepo);
      if (sk) foldCounts(d, subRepo, wtlog.open(subRepo), sk);
    } catch (e) { /* an unreadable sub simply tallies nothing */ }
    foldSubs(d, subRepo);                       // then its grandchildren
    _subRev.set(subRepo.wt, { rev: rv, d: d });
    addFold(r, d);
  });
}

//  TODO-005: ONE status read per wt-having ticket (plus one per mounted sub) —
//  classify.classifyMerge, THE wt differ status itself reads through (no
//  parallel differ, [reuse-libdog]); the quad's four TREE columns are never
//  asked for, each is a full tree walk and this cell only needs the wt axis.
//  Ahbeh is work's registry (the WORK-011 graf cache) against work's own track
//  edge, memoized per track.  An unreadable wt yields null and the row's slots
//  blank out — never an error row.
//  TODO-006: the TIPS FINGERPRINT — the wt's cur tip plus its RESOLVED track
//  tip, the status.js `state` precedent for what the watcher cannot witness: a
//  post or fetch from a second wt rewrites refs under an unwatched `.be/`, so
//  no rev moves while the ahbeh counts (and the patch arg) do.  The wtlog reads
//  it pays are kept per run, so the miss path below opens the log ONCE.
function tipsOf(w) {
  let e = _tipc.get(w.dir);
  if (e !== undefined) return e;
  e = { tips: "?", repo: null, log: null, cur: null, att: null, tip: "" };
  try {
    const work = worklib;
    if (!_reg) _reg = work.registry();
    e.repo = be.treeAt(w.dir);
    e.log = wtlog.open(e.repo);
    e.cur = e.log.curTip();
    e.att = e.log.attachedBranch();
    const tk = (e.att.uriTrack && e.att.track) || "";
    if (tk && _tips.has(tk)) e.tip = _tips.get(tk);
    else { e.tip = work.trackTip(_reg, e.repo, e.att); if (tk) _tips.set(tk, e.tip); }
    e.tips = ((e.cur && e.cur.sha) || "") + "|" + e.tip + "|" + (e.att.detached ? "!" : "") +
             (e.att.track || "") + "?" + (e.att.branch || "");
  } catch (er) { e.tips = "?"; }
  _tipc.set(w.dir, e);
  return e;
}
//  TODO-006 r3 (RULING 2026-08-04): the wt row is TWO BLOCKS, TWO WITNESSES.
//  A post to the MAIN TREE moves every wt's tips at once — splitting them is
//  what keeps it from re-classifying 88 worktrees to move one ahbeh pair.
function fileStat(w, tp) {
  //  STATUS-019: the wt's rev stands still ⇒ no file under it moved ⇒ the whole
  //  read (classifyMerge + foldSubs) replays from the last run's record.
  const rv = CACHE.rev(w.dir);
  const kept = _fileRev.get(w.dir);
  if (kept && kept.rev === rv) return kept.f;
  let f = null;
  try {
    const repo = tp.repo, log = tp.log;
    const k = repo && _reg ? _reg.keeperFor(repo) : null;
    if (k && log) {
      f = { un: { chg: 0, add: 0, del: 0 }, st: { chg: 0, add: 0, del: 0 },
            staged: 0, dirty: false };
      foldCounts(f, repo, log, k);
      foldSubs(f, repo);
      f.dirty = (f.un.chg + f.un.add + f.un.del) > 0;
    }
  } catch (e) { f = null; }
  _fileRev.set(w.dir, { rev: rv, f: f });
  return f;
}
//  The COMMIT block: the ahbeh pair and the patch FORM its button spells, both
//  measured against the very tips the fingerprint names — its whole witness.
function commitStat(w, tp) {
  const kept = _ciTips.get(w.dir);
  if (kept && kept.tips === tp.tips) return kept.c;
  let c = null;
  try {
    if (tp.repo && _reg) {
      const cur = tp.cur, att = tp.att;
      //  TODO-005 r2: the diverged button's patch ARG must name the SAME tip the
      //  ahbeh counts measured, and absorb the whole missing LINE (not one
      //  commit).  `#<sha>` does NOT: patchscope reads a fragment as the NAMED
      //  scope — a CHERRY-PICK with fork = parent(named).  The LINE forms are
      //  `?<branch>` (a named-branch wt), the TRACK ADDRESS itself (PATCH-010
      //  TREE source: theirs = the addressed wt's cur tip, fork = LCA — exactly
      //  what work.trackTip measured), and BARE `patch` (PATCH-015: the whole
      //  missing line of the tracked ref, which is what refTip measured).
      //  null = no form names that tip, so the pair greys.
      c = { counts: null,
            patch: att.detached ? null
                 : att.uriTrack ? ((att.track && att.track.slice(0, 2) === "//") ? att.track : null)
                 : att.branch ? "?" + att.branch : "" };
      c.counts = _reg.counts(tp.repo, (cur && cur.sha) || "", tp.tip);
    }
  } catch (e) { c = null; }
  _ciTips.set(w.dir, { tips: tp.tips, c: c });
  return c;
}
//  The row's numbers, one object per run — fileFrame and commitFrame render the
//  tok array off it EVERY render (there is no second, cached renderer).
function wtStat(w) {
  if (_wtc.has(w.dir)) return _wtc.get(w.dir);
  const tp = tipsOf(w);                 // the fingerprint opens the repo + log
  const f = fileStat(w, tp), c = commitStat(w, tp);
  const r = f ? { un: f.un, st: f.st, staged: f.staged, dirty: f.dirty,
                  counts: c ? c.counts : null, patch: c ? c.patch : null } : null;
  _wtc.set(w.dir, r);
  return r;
}

//  --- family nesting (TODO-005) -----------------------------------------------
//  work.js's dotted tracker rails: a `Sub:` child is NOT a subdir, so it hangs
//  on `├┄┄`, never the solid mount rails.
const RAIL = { mid: "├┄┄ ", last: "└┄┄ ", bar: "│   ", gap: "    " };
//  One list's tickets → the same list re-ordered as a FOREST: a ticket whose
//  `Sub:` names a LISTED OPEN parent follows it on dotted rails (`t.rails`),
//  a parent-less / closed-parent / cross-list `Sub:` renders flat.  A `Sub:`
//  CYCLE is cut at the name-sorted first member (work.js breakCycles), so the
//  walk below can never descend forever.  A row with no head scan (the TODO-004
//  filter listing, whose rows are index hits) simply has no parent.
function nest(tickets) {
  const by = new Map();
  for (const t of tickets) by.set(t.key, t);
  for (const t of tickets) {
    const p = (t.meta && t.meta.Sub) ? String(t.meta.Sub).trim() : "";
    const par = (p && p !== t.key) ? by.get(p) : undefined;
    t.parent = (par && !par.closed) ? par : null;
  }
  for (const t of tickets) {
    const seen = new Set();
    let n = t, hit = null;
    while (n) { if (seen.has(n)) { hit = n; break; } seen.add(n); n = n.parent; }
    if (!hit) continue;
    const mem = []; let c = hit;
    do { mem.push(c); c = c.parent; } while (c && c !== hit);
    mem.sort(function (a, b) { return a.key < b.key ? -1 : a.key > b.key ? 1 : 0; });
    mem[0].parent = null;
  }
  const kids = new Map(), roots = [];
  for (const t of tickets) {
    if (!t.parent) { roots.push(t); continue; }
    if (!kids.has(t.parent.key)) kids.set(t.parent.key, []);
    kids.get(t.parent.key).push(t);
  }
  const out = [];
  (function walk(list, prefix, top) {
    for (let i = 0; i < list.length; i++) {
      const t = list[i], last = i === list.length - 1;
      t.rails = top ? "" : prefix + (last ? RAIL.last : RAIL.mid);
      out.push(t);
      const ks = kids.get(t.key);
      if (ks) walk(ks, top ? "" : prefix + (last ? RAIL.gap : RAIL.bar), false);
    }
  })(roots, "", true);
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
//  `ticket <KEY>` (verb clicks are O) — pager-ONLY chrome, so the plain path
//  emits no click token (an O in a plain hunk would trip the why-plain cursor).
//  BE-040 r3: `btn` also grows the BE-041 button tail — ` ` sep, visible Y
//  `[done]`, hidden O `done KEY` — AFTER the nav O so the title click navigates.
//  TODO-004: every key the ARG LINE names shows its value INLINE as ` [value]`
//  right after the key (where the old `[MARK]` used to read), in arg order, and
//  each bracket is its OWN click — the whole arg line with THAT key's filter
//  replaced.  A ticket missing the key shows no bracket, and a bare board
//  (no filter in the line) shows none at all.
//  TODO-005 fixed columns (the work.js R2 discipline): the rails+bullet+key
//  region pads to KEYW with a dotted leader, so every KEY — board row or nested
//  one — lands at ONE column and the two button frames behind it line up down
//  the wt-having rows.  An over-long region (a wide `[value]` run) degrades to
//  one space, like work's.
const KEYW = 18;
//  TODO-005 the two BUTTON FRAMES.  Every button is 2 cells and every slot is
//  exactly one button wide, buttons parted by a 1-cell gap; a frame DELIMITS
//  its own columns, so nothing inside the brackets is ┄-filled (an absent slot
//  is plain SPACES) and `┄` leaders live only OUTSIDE.
//  CI-004 (ruling 2026-08-04): the STAGING surface ends in "test it" and the
//  HISTORY surface ends in "commit it" — the ` ∞` run button takes the file
//  frame's last slot and the post ` ✓` moves to the commit frame's last, which
//  is why COMMIT grew 10 → 13.
//    FILE   `[ i nn nn nn  ∞]`  16 = 1 + 2 + (1+2)*4 + 1   (status ~ - + run)
//    COMMIT `[ ≡ nn nn  ✓]`     13 = 1 + 2 + (1+2)*3 + 1   (log, post, get, ci)
const FRAMEW_FILE = 16, FRAMEW_COMMIT = 13, BTNW = 2;
//  a DIVERGED `A⇄B` is ONE button over both ahbeh sub-slots and the gap between
//  them — 5 cells, which `12⇄34` fills exactly (each side clamps to 2 digits).
const PAIRW = BTNW * 2 + 1;
//  the WHOLE frames region incl. the space that leads each frame — the width a
//  wt-LESS row ┄-fills so its title lands at the hunk's one title column.
const FRAMESW = 1 + FRAMEW_FILE + 1 + FRAMEW_COMMIT;
//  TODO-005 r2: button colours + faces are THEME data (view/theme.js is the ONE
//  place an SGR value lives — the DIFF_WASH precedent), never local literals.
const THEME = require("../../view/theme.js");
const BTN = THEME.BTN, FACE = THEME.BTN_FACE;
//  the legacy 16-palette FALLBACK slot per button (view/theme.js BTN_TAG) — the
//  face rides it and its O overrides with truecolor, so a lost prefix degrades
//  to the class colour, never to grey (grey is the DISABLED signal).
function btnTag(name) { return tagCode(THEME.BTN_TAG[name]); }
function chars(s) { return Array.from(s).length; }

//  TODO-005: the pager row's mark, key and title are separate COLUMNS now — the
//  elastic `B` field is the BARE title (the [MARK] became the bullet, the key
//  its own column); plain keeps the verbatim header line.
function bareTitle(key, title) {
  const s = stripMark(key, title);
  if (s.indexOf(key) !== 0) return s;
  let i = key.length;
  if (s[i] === ":") i++;
  while (s[i] === " ") i++;
  return s.slice(i);
}

//  TODO-005 r2: a button's hidden `O`.  The bytes are the BRO-025 three-part
//  invite behind a LEADING `#<pale><tone> ` — BOTH slots of the WHY-001 colour
//  prefix (view/bro.js whyBgAt: bg then fg), the wash derived from the tone by
//  the ONE theme.pale().  So a single token spells the button's whole look AND
//  its click, and the button needs no tok tag of its own (the 32-code tag space
//  is full).  The pager's _uriAt sheds the prefix at the first space.
function btnSpell(ctx, spell, fg) {
  return THEME.pale(fg) + fg + " " + SPELL.mintOspell(ctx, spell);
}

//  TODO-005 r2: emit ONE button — the face on its legacy FALLBACK tag, then the
//  O that overrides it with the truecolor pair.  The face IS the whole button: every cell of
//  it is washed and coloured and every cell of it clicks, so there is no dead
//  padding to mis-hit.  A face is always BTNW cells (2 digits, or space+icon).
//  CI-004: `tone` overrides the slot's own colour (the run button wears its
//  verdict); the FALLBACK tag stays the slot's, so a lost O still reads as one.
function btnCell(parts, spans, off, face, name, ctx, spell, tone) {
  off = span(parts, spans, off, face, btnTag(name));
  return span(parts, spans, off, btnSpell(ctx, spell, tone || BTN[name]), TAG_O);
}
//  A DISABLED button is plain grey fg — no wash, no spell.
function greyCell(parts, spans, off, face) { return span(parts, spans, off, face, TAG_D); }
//  TODO-005: INFO, not a button — the class colour with NO wash and NO spell
//  (fg-only `##rrggbb ` O; the empty spell makes a click fall through to the row).
function infoCell(parts, spans, off, face, name) {
  off = span(parts, spans, off, face, btnTag(name));
  return span(parts, spans, off, "#" + BTN[name] + " ", TAG_O);
}
//  An empty slot: plain spaces, no fill character (frames delimit their columns).
function blankCell(parts, spans, off, w) { return span(parts, spans, off, " ".repeat(w), TAG_S); }
//  A count FACE is ALWAYS exactly 2 cells (BTNW): the class SIGIL + the digit
//  under ten (`~3`, `+2`, `-9`), the bare two digits from ten up (`10`, `47`),
//  the count clamped at 99.  Colour carries the class; the sigil is what keeps
//  a single-digit count from reading as a bare number.
function countFace(sigil, n) {
  const v = Math.min(n, 99);
  return v < 10 ? sigil + v : String(v);
}

//  TODO-005: ONE count slot, the THREE-STATE rule (grey overruled 2026-08-03:
//  a nonzero count ALWAYS wears its class colour) — `un` rows left to stage
//  light the BUTTON (wash + O spell, count = unstaged), a wholly STAGED class
//  keeps the colour but sheds the wash and the spell (info, not a button),
//  an empty class blanks.  Only the wash says "clickable".
function countSlot(parts, spans, off, sigil, un, st, name, ctx, spell) {
  if (un > 0) return btnCell(parts, spans, off, countFace(sigil, un), name, ctx, spell);
  if (st > 0) return infoCell(parts, spans, off, countFace(sigil, st), name);
  return blankCell(parts, spans, off, BTNW);
}
const ZERO3 = { chg: 0, add: 0, del: 0 };
//  TODO-005: the FILE frame — the wt's staging surface, `[ i nn nn nn  ✓]`.
//  The ` i` status button always lights (the wt exists); an unreadable wt (`stat`
//  null) leaves the four action slots blank rather than making an error row.
//  The counts are the WHOLE tree's, mounted subs folded in (foldSubs) — bare
//  put/delete stage those too, so the button that runs them must say so.
function fileFrame(parts, spans, off, t) {
  const s = t.stat, ctx = "//" + t.wt.name;
  const un = (s && s.un) || ZERO3, st = (s && s.st) || ZERO3;
  off = span(parts, spans, off, "[", TAG_D);
  //  The status link — EMPTY ctx (the wt is the ARG; status's own arg0
  //  authority re-anchors the pager to `//<wt>/` on the click).
  off = btnCell(parts, spans, off, FACE.status, "status", "", "status //" + t.wt.name);
  off = span(parts, spans, off, " ", TAG_S);
  off = countSlot(parts, spans, off, "~", un.chg, st.chg, "chg", ctx, "put");
  off = span(parts, spans, off, " ", TAG_S);
  off = countSlot(parts, spans, off, "-", un.del, st.del, "del", ctx, "delete");
  off = span(parts, spans, off, " ", TAG_S);
  off = countSlot(parts, spans, off, "+", un.add, st.add, "add", ctx, "put +");
  off = span(parts, spans, off, " ", TAG_S);
  //  CI-004: the staging surface ends in TEST IT (the ✓ moved to the commit
  //  frame) — always lit, since every wt can be asked to run its default stuff.
  return span(parts, spans, runSlot(parts, spans, off, t), "]", TAG_D);
}
//  CI-004 (ruling 2026-08-04): the RUN button.  The spell is the PLAIN VIEW
//  spell `ci` on the row's wt context — views/ci/ci.js, pushed by the pager
//  through the ordinary machinery, no pager-local dispatch.  Face and tone come
//  from the verdict MEMO, so the button un-tints itself the moment rev(wtDir)
//  moves; an unreadable CI leg simply leaves the plain button.
//  TODO 11: the FACE says in flight (live state); the COLOUR is the REMEMBERED
//  verdict, read COLD off the status map — greyish never-ran, red failed, green
//  ok — so a fresh pager over an untouched tree still shows the last result.
function runSlot(parts, spans, off, t) {
  let live = null, last = null;
  try { live = (CI.row(t.wt.dir) || {}).state || null;
        last = CI.status(t.wt.dir); }
  catch (e) { live = null; last = null; }
  //  The wt is the ARG and the context is EMPTY — the ` i`/` ≡` view buttons'
  //  own form, so the pushed view records `ci //<wt>` and `r`/back replay it.
  return btnCell(parts, spans, off, live === "run" ? FACE.runb : FACE.run,
                 "run", "", "ci //" + t.wt.name,
                 live === "run" ? BTN.run : THEME.BTN_RUN[last || "none"]);
}
//  CI-004: the commit ✓, now the HISTORY surface's last slot.  Lit + spelled
//  while ANY row is staged (WORK-008's minted `KEY: <title>` message), blank
//  otherwise (no grey ✓ — 2026-08-03).
function ciSlot(parts, spans, off, t, ctx) {
  const s = t.stat;
  if (!(s && s.staged > 0)) return blankCell(parts, spans, off, BTNW);
  return btnCell(parts, spans, off, FACE.ci, "ci", ctx,
                 "post '" + t.key + ": " + bareTitle(t.key, t.title) + "'");
}
//  TODO-005 r2: the COMMIT frame — `[ ≡ nn nn]`.  The ahbeh sub-slots are
//  POSITIONAL and fixed: the POST position first, the GET position second, so a
//  behind count never drifts into the post column (position, sigil and colour
//  all say the class).  Ahead-only fills the left and
//  blanks the right, behind-only the reverse; a DIVERGED pair is ONE patch
//  button over both slots and their gap, right-aligned in its 5 cells (grey and
//  dead when no patch form names the tracked tip — wtStat's `patch`); in sync,
//  blank.
function commitFrame(parts, spans, off, t) {
  const s = t.stat, ctx = "//" + t.wt.name, c = s && s.counts;
  const a = c && c.ahead ? Math.min(c.ahead, 99) : 0;
  const b = c && c.behind ? Math.min(c.behind, 99) : 0;
  off = span(parts, spans, off, "[", TAG_D);
  off = btnCell(parts, spans, off, FACE.log, "log", "", "log //" + t.wt.name);
  off = span(parts, spans, off, " ", TAG_S);
  if (a && b) {
    const face = (a + "⇄" + b).padStart(PAIRW, " ");
    const arg = s.patch;                       // null = no form names that tip
    off = arg === null ? greyCell(parts, spans, off, face)
        : btnCell(parts, spans, off, face, "patch", ctx,
               "patch" + (arg ? " '" + arg + "'" : ""));
    off = span(parts, spans, off, " ", TAG_S);
    return span(parts, spans, ciSlot(parts, spans, off, t, ctx), "]", TAG_D);
  }
  if (a) off = btnCell(parts, spans, off, countFace("+", a), "post", ctx, "post");
  else off = blankCell(parts, spans, off, BTNW);
  off = span(parts, spans, off, " ", TAG_S);
  if (b) off = btnCell(parts, spans, off, countFace("-", b), "get", ctx, "get");
  else off = blankCell(parts, spans, off, BTNW);
  off = span(parts, spans, off, " ", TAG_S);
  //  CI-004: ...and the history surface ends in COMMIT IT.
  return span(parts, spans, ciSlot(parts, spans, off, t, ctx), "]", TAG_D);
}

//  TODO-005: the trailing DONE/DONT panel — ONE frame, TWO live buttons.  ` ✓`
//  closes the ticket (its head's `Now:` pair becomes DONE and its worktree, if
//  it has one, moves to the work/done/ discard root); ` ✗` shelves it the same
//  way as DONT.  Frame conventions throughout: dim brackets, a dim gap, and
//  each 2-cell face its OWN click zone.  Both mint the ROW's spell form
//  (`done KEY` / `dont KEY`), context-less — the KEY is the argument, and the
//  verb resolves the page and the worktree itself.  7 cols: 1 + 2 + 1 + 2 + 1.
const PANELW = 1 + BTNW + 1 + BTNW + 1;
function donePanel(parts, spans, off, key) {
  off = span(parts, spans, off, "[", TAG_D);
  off = btnCell(parts, spans, off, FACE.done, "done", "", "done " + key);
  off = span(parts, spans, off, " ", TAG_S);
  off = btnCell(parts, spans, off, FACE.dont, "dont", "", "dont " + key);
  return span(parts, spans, off, "]", TAG_D);
}

//  TODO-005 [go]: a wt-LESS row's frames region.  A ticket whose head carries
//  `Rep:` — the repo it relates to, a (usually relative) repo URI — offers the
//  ONE creating action on the board: MINT `work/<KEY>` from that repo.  The
//  button sits at the LEFT EDGE of the region in its own frame — dim brackets,
//  the 2-cell face the only live part — and the rest keeps its ┄ leader,
//  so titles stay at the hunk's one column either way; no `Rep:` and the region
//  is pure leader.  The spell is context-LESS: the wt does not exist yet, so
//  there is no `//<wt>/` to run in — the verb resolves the destination itself.
function goSlot(parts, spans, off, t) {
  const rep = (t.meta && t.meta.Rep) ? String(t.meta.Rep).trim() : "";
  if (!rep) return span(parts, spans, off, "┄".repeat(FRAMESW), TAG_S);
  //  WORK-016: the ROW owns the button's breathing space, exactly as the space
  //  that leads the file frame does — so the region is the same width either way.
  //  The BRACKETS are frame chrome like every other frame's: dim, and OUTSIDE
  //  the click zone.  Only the 2-cell face is the button.
  off = span(parts, spans, off, " ", TAG_S);
  off = span(parts, spans, off, "[", TAG_D);
  off = btnCell(parts, spans, off, FACE.go, "go", "", "work " + t.key + " " + rep);
  off = span(parts, spans, off, "]", TAG_D);
  return span(parts, spans, off, "┄".repeat(FRAMESW - 3 - BTNW), TAG_S);
}

//  `t` is one listTopic / todoFilter row: { key, title, prio, closed, vals?,
//  rails?, wt?, stat? }.  TODO-005: the pager row leads with the sev BULLET and
//  hangs the two button frames off a wt (a wt-less row ┄-fills that region);
//  plain stays the bare header line on its rails.  `cols` is the HUNK's verdict
//  — fixed columns exist to ALIGN something, so a listing where NO ticket owns a
//  worktree drops the leader and the whole region, and the row is bullet + key
//  + title (33 dead ┄ cells would eat a narrow terminal and starve the title).
function titleRow(parts, spans, off, indent, t, btn, cols) {
  const key = t.key, title = t.title, vals = t.vals;
  const lead = indent + (t.rails || "");
  const rest = title.indexOf(key) === 0 ? title.slice(key.length) : " " + title;
  if (lead) off = span(parts, spans, off, lead, TAG_S);
  if (!btn) {
    off = span(parts, spans, off, key, TAG_F);
    for (const v of vals || []) off = span(parts, spans, off, " [" + v.text + "]", TAG_S);
    return span(parts, spans, off, rest + "\n", TAG_S);
  }
  //  TODO-005: the sev bullet leads — solid `●` in the `Sev:` colour, hollow
  //  `○` (dim) for a closed ticket a `Now:` filter still shows.
  const prio = t.prio !== undefined ? t.prio : 2;
  off = span(parts, spans, off, t.closed ? "○" : "●",
             t.closed ? TAG_D : PRIO_TAG[prio]);
  off = span(parts, spans, off, " ", TAG_S);
  off = span(parts, spans, off, key, TAG_F);
  //  TODO-011: a ticket KEY lands on the ticket PAGE — its own view now.
  off = span(parts, spans, off, SPELL.mintOspell("", "ticket " + key), TAG_O);
  let vw = 0;
  for (const v of vals || []) {
    off = span(parts, spans, off, " [", TAG_S);
    off = span(parts, spans, off, v.text, TAG_N);
    //  a value carrying a colon is not expressible as a filter arg — that
    //  bracket simply does not click (its key half still offers `Key:*`).
    if (v.spell) off = span(parts, spans, off, SPELL.mintOspell("", v.spell), TAG_O);
    off = span(parts, spans, off, "]", TAG_S);
    vw += chars(v.text) + 3;
  }
  if (cols) {
    const fill = KEYW - chars(lead) - 2 - chars(key) - vw;
    off = span(parts, spans, off, fill >= 2 ? " " + "┄".repeat(fill - 1) : " ", TAG_S);
    //  TODO-005 r2 (RULING): the two BUTTON FRAMES on a wt-having row, and the
    //  DOTTED LEADER STRAIGHT THROUGH the frames region on a wt-less one — the
    //  region is a fixed column set again, so every title in the hunk aligns
    //  (r1's "the frames vanish and the title moves left" is reversed).  The
    //  leader is `┄` FILL ONLY (WORK-016): the row owns the breathing space, so
    //  a wt-less row's fill joins the KEYW leader into ONE uninterrupted run.
    if (t.wt) {
      off = span(parts, spans, off, " ", TAG_S);
      off = fileFrame(parts, spans, off, t);
      off = span(parts, spans, off, " ", TAG_S);
      off = commitFrame(parts, spans, off, t);
    } else off = goSlot(parts, spans, off, t);
  }
  //  BRO-036: the title is the ONE elastic `B` span — the pager …-cuts / pads
  //  it to the live width in no-wrap mode; bytes stay verbatim.
  off = span(parts, spans, off, " ", TAG_S);
  off = span(parts, spans, off, bareTitle(key, title), TAG_B);
  off = span(parts, spans, off, " ", TAG_S);
  off = donePanel(parts, spans, off, key);
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

//  --- TODO-006 r3: the STRUCTURED per-row cache -------------------------------
//  RULING (gritzko 2026-08-04, supersedes r2's rendered-line splice): cache the
//  NUMBERS and the HEAD FIELDS — never the rendered bytes — and run titleRow /
//  fileFrame / commitFrame over them EVERY render.  Each BLOCK has its OWN
//  witness and drops alone:
//    * the ticket HEADS   — rev(the topic dir)     (_topicMemo, openTickets)
//    * the FILE counts    — rev(the wt dir)        (_fileRev, fileStat)
//    * the ahbeh + patch  — the tips fingerprint   (_ciTips, commitStat)
//  Above them ONE board-wide short-circuit: a still rev(be.todoRoot()) proves no
//  ticket changed anywhere, so listTopics / listTopic / metaidx and every head
//  read are skipped and the sort + `Sub:` nesting run off the cached rows.
//  All of it is pager-only and lives only while a watcher is live (ruling 3).
let _boardMemo = new Map();     // board dir + arg line → { rev, groups }
let _topicMemo = new Map();     // topic dir → { rev, tickets }
let _memoOn = false;            // a watcher is live: the memos may be used
let _boardStill = false;        // this run's rev(todoRoot) stood still
//  THE render: the row's numbers first, then the ONE titleRow over them — the
//  frames are re-emitted every render, off the cached counts (the r3 ruling).
function renderRow(parts, spans, off, indent, t, btn, cols) {
  if (t.wt) t.stat = wtStat(t.wt);
  return titleRow(parts, spans, off, indent, t, btn, cols);
}
//  THE one-shot board check: a standstill reuses the whole row set —
//  no listTopics, no listTopic, no metaidx find, no ticket read at all.
function boardGroups(board, line, build) {
  if (!_memoOn) return build();
  const rv = CACHE.rev(board.dir);
  const key = board.dir + "\u0000" + line;
  const had = _boardMemo.get(key);
  if (had && had.rev === rv) { _boardStill = true; return had.groups; }
  const g = build();
  _boardMemo.set(key, { rev: rv, groups: g });
  return g;
}

//  The board / one topic, as ONE hunk of title rows.  BE-040 r3: `btns` puts a
//  `[done]` button on every OPEN list row (pager-only; plain passes false).
function emitList(sink, banner, groups, headers, btns) {
  const parts = [], spans = [];
  let off = 0;
  //  TODO-005 pass 1: `Sub:` families nest in BOTH modes (rails are structure,
  //  the work.js rule); the wt LOOKUP is pager-only and runs over the WHOLE
  //  hunk first, because its verdict decides whether the fixed column set
  //  exists at all (titleRow's `cols`) and the columns align hunk-wide.
  const lists = [];
  let cols = false;
  for (const g of groups) {
    const rows = nest(g.tickets);
    if (btns) for (const t of rows) {
      t.wt = wtIndex().get(t.key) || null;
      //  TODO-005: the region exists when ANY row has something to put in it —
      //  a worktree's two frames, or a `Rep:` row's [go] mint button.
      if (t.wt || (t.meta && t.meta.Rep)) cols = true;
    }
    lists.push(rows);
  }
  for (let gi = 0; gi < groups.length; gi++) {
    const g = groups[gi];
    if (headers) {                       // topic header row, itself a target
      off = span(parts, spans, off, g.topic, TAG_N);
      //  BE-054: pager-only O nav (plain stays chrome-free — see titleRow).
      if (btns) off = span(parts, spans, off, SPELL.mintOspell("", "todo " + g.topic), TAG_O);
      off = span(parts, spans, off, "\n", TAG_S);
    }
    //  TODO-006: every row goes through the LINE memo — a hit splices its stored
    //  bytes, a miss falls through to the wt read and the one titleRow.
    for (const t of lists[gi])
      off = renderRow(parts, spans, off, headers ? "  " : "", t, btns, cols);
    if (!g.tickets.length)               // an explicit `todo TOPIC`, all closed
      off = span(parts, spans, off, (headers ? "  " : "") +
        (g.note || "(no open tickets in todo/" + g.topic + "/)") + "\n", TAG_S);
  }
  feed(sink, banner, parts, spans, off);
}

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
//  TODO-011: the parser is SHARED with views/ticket/ticket.js and stays purely
//  GRAMMATICAL — it classes, it does not decide which view serves what.  Each
//  verb then refuses the classes it does not serve (todo: a key subject →
//  `ticket <KEY>`; ticket: a topic / a filter → `todo …`).  `scheme` is the
//  DIS-060 spell form the caller answers to (`todo:GET-1` / `ticket:GET-1`).
function parseArgs(argv, scheme) {
  const filters = [], toks = [];
  let subject = null;
  for (let i = 0; i < argv.length; i++) {
    const w = unscheme(argv[i], scheme);
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
        return { err: "'" + key + ":' is not a meta key — a capital and two" +
                 " lowercase letters or digits, like Now: or On1:" };
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
  //  TODO-011: "a ticket id takes no filters" was a check HERE until the page
  //  split; it is a per-VIEW refusal now (each view points at the other), so
  //  the parser stays purely grammatical and neither message is duplicated.
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
//  TODO-011: a FILTER click is table business and stays a `todo` spell wherever
//  it is rendered — the board's inline `[value]` brackets AND the ticket view's
//  in-page meta block both mint through THIS one function.
function spellWith(a, key, val) { return "todo " + argLineWith(a, key, val); }

//  TODO-008: a rendered value that lexes as a TICKET ID (the grammar's own
//  `key` class) clicks to that ticket's page — its LEXICAL class alone decides,
//  so an unregistered key's ticket-shaped value navigates too, and a dangling
//  code gets the `ticket` view's own miss line.  null = no jump, filter as
//  before.  TODO-011: THE one home for the nav spell — the board's inline
//  `[value]` bracket and the ticket view's in-page meta block both call it, so
//  the page's verb name can never drift between the two render sites.
function navSpell(raw) {
  const t = String(raw == null ? "" : raw).trim();
  return shape(t) === "key" ? "ticket " + t : null;
}

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
      //  TODO-008: an inline ticket-shaped value jumps too — same class test.
      const nav = navSpell(t), fv = nav ? null : filterVal(t);
      vals.push({ text: t, spell: nav || (fv === null ? null : spellWith(a, k, fv)) });
    }
    //  TODO-005: a filter row wears the same bullet — its `Sev:` colour, hollow
    //  when the line's own `Now:` reached a closed ticket.  No head scan here
    //  (these rows are index hits), so `Sub:` nesting stays a board concern.
    rows.push({ key: e.code, title: title, vals: vals,
                file: e.file, mtime: e.mtime,
                prio: prioOf(e.file, e.code, title),
                closed: hiddenByDefault(e.meta, e.code, title) });
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
//  TODO-011: `scheme` names the form the CALLING view answers to, so the one
//  parse serves `todo:…` and `ticket:…` alike; it defaults to this view's own.
function unscheme(arg, scheme) {
  let w = String(arg == null ? "" : arg);
  const sc = scheme || "todo";
  if (w.indexOf(":") >= 0) {
    try { const p = uri._parse(w); if (p.scheme === sc) w = p.path || ""; } catch (e) {}
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
  const a = parseArgs(argv, "todo");
  if (a.err) bad(a.err);
  //  TODO-011: the one-ticket PAGE is its own view now — `todo` is the tabular
  //  side (board / topic list / meta-pair filters) and refuses a ticket id in
  //  plain words, naming the view that does serve it.  RULING default: refuse,
  //  never silently re-route — the arg line IS the address bar, and a `todo`
  //  line that answers with a page would lie about where the reader is.
  //  ...and a filter cannot narrow a page either, so ONE refusal covers both
  //  `todo ABC-123` and `todo ABC-123 Now:OPEN`.
  if (a.subject && a.subject.kind === "key")
    bad("'" + a.subject.w + "' names one ticket page, and this is the board — " +
        "write 'ticket " + a.subject.w + "' for the page, or 'todo " +
        keyTopic(a.subject.w) +
        (a.filters.length ? " " + a.toks.filter(function (t) {
             return t.indexOf(":") > 0; }).join(" ") : "") +
        "' for the topic's list");
  //  a TOPIC that is no dir keeps the historic uniform miss line, filter or not.
  if (a.subject && a.subject.kind === "topic" && !isDir(join(board.dir, a.subject.w)))
    miss(a.subject.w, "TODONONE");
  //  TODO-005: the wt memos live for ONE run; DOG-027 — release every keeper +
  //  graf slot the count cells opened, even on a thrown miss.
  runReset();
  try {
    if (a.filters.length) { todoFilter(board, a, mode, sink); return; }
    if (!a.subject) {
      emitList(sink, "todo", boardGroups(board, "todo", function () {
                 return listTopics(board.dir); }), true, mode !== "plain");
      return;
    }
    if (a.subject.kind === "topic") {
      const w = a.subject.w;
      emitList(sink, "todo " + w, boardGroups(board, "todo " + w, function () {
                 return [openTickets(board.dir, w)]; }), false, mode !== "plain");
      return;
    }
    //  TODO-011: a ticket id reaches here only via the refusal above.
  } finally { runClose(); }
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
//  TODO-005: expose the row layer for the repro test (head scan, wt match,
//  family nesting, the two button frames, the emitted row itself).
module.exports.pageHead = pageHead;
module.exports.nest = nest;
module.exports.emitList = emitList;
module.exports.wtIndex = wtIndex;
module.exports.runReset = runReset;
module.exports.runClose = runClose;
module.exports.fileFrame = fileFrame;
module.exports.commitFrame = commitFrame;
module.exports.FRAMEW = { file: FRAMEW_FILE, commit: FRAMEW_COMMIT, region: FRAMESW };
//  TODO-011: THE shared ground views/ticket/ticket.js reads this module for —
//  the ticket tree's lexer, its file ladder, its byte read, the arg-line
//  grammar and the two click classifiers.  The ticket view implements NONE of
//  them a second time; it adds only the mkd PAGE rendering the board has no
//  use for.  (shape/ticketKey/pageFile/boardDir/pageTitle are exported above.)
module.exports.keyTopic = keyTopic;
module.exports.readBytes = readBytes;
module.exports.EXTS = EXTS;
module.exports.parseArgs = parseArgs;
module.exports.argLineWith = argLineWith;
module.exports.spellWith = spellWith;
module.exports.navSpell = navSpell;
module.exports.filterVal = filterVal;
