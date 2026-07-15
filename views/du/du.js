//  views/du/du.js — BRO-026: the `du` store disk-usage VIEW.  A read-only,
//  ARG-LESS verb: bare `du` (or `du .`) resolves the STORE backing the current
//  repo and tables EVERY shard in it — the per-project dirs a sharded store
//  hosts ([/blog/git]).  Motivation: tens of forked / rm-rf'd worktrees share
//  ONE store; it accretes packs + indexes with zero visibility.  One row per
//  shard (packs, indexes, other, total) + a store TOTAL row.
//
//  STORE RESOLUTION (never `$HOME/.be`): be.treeAt() → { storePath, project };
//  store.shardDir(storePath, project) is THIS repo's shard, and its PARENT is
//  the store ROOT that hosts every shard (a colocated primary store's `<wt>/
//  .be`, or the redirect target of a secondary wt's `.be` file — the GET-038
//  resolveLocalSource kin).  du readdirs that root; each subdir is a shard.
//
//  SHARD LAYOUT (flat, one readdir per shard, NO recursion below it): each
//  entry classifies by suffix — `NNNNNNNNNN.keeper` pack logs (count + bytes);
//  `*.keeper.idx` / `*.graf.idx` / `*.spot.idx` LSM indexes (index bytes);
//  everything else — `refs`, `wtlog`, misc — OTHER bytes.  CAVEAT (BRO-026):
//  the read-only io.readdir binding HIDES dotfiles, so the DOTTED store files
//  (`.refs.idx`, `.lock*`) are invisible to the scan and never tallied — a
//  negligible loss (locks are 0 bytes, `.refs.idx` a few KB) and the only
//  honest read-only option (shelling out to system `du` is forbidden).
//  Non-dirs at the store root and unreadable entries/shards skip SILENTLY.
//
//  OUTPUT: rows sorted by total size desc (name tie-break), the current repo's
//  own shard marked `*`, a final TOTAL row.  Pager mode (ambient.format() !=
//  "plain") aligns columns with human K/M/G sizes; PLAIN mode emits one
//  tab-separated raw-byte row per shard for scripting (`name<TAB>packs<TAB>
//  keeperBytes<TAB>idxBytes<TAB>otherBytes<TAB>total[<TAB>*]`).  Both flow
//  through the shared hunkrows sink (JAB-003) — S-tagged text, plain==color.
//
//  BE-003 miss discipline: only the bare / `.` arg is legal; `du <anything>` →
//  ONE uniform `du: <arg>: DUNONE` line + throw (jab maps it to exit != 0).
"use strict";

const store    = require("../../shared/store.js");    // shardDir resolution
const pathlib  = require("../../shared/util/path.js"); // join/dirname/basename
const ambient  = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const hunkrows = require("../../shared/hunkrows.js");   // JAB-003: columnar→HUNK

const join = pathlib.join;

//  --- shard-file classification ---------------------------------------------
//  A flat shard entry's suffix decides its byte bucket.  `.keeper` first (a
//  pack log); the three LSM index suffixes next; ELSE "other" — note `.refs.idx`
//  ends `.idx` but is NOT an LSM index suffix, so it (correctly) lands in other.
function endsWith(s, suf) {
  return s.length >= suf.length && s.slice(s.length - suf.length) === suf;
}
//  (Dotted names never reach here in a live scan — io.readdir hides them — but
//  classify() is total over any name for the unit test's sake.)
function classify(name) {
  if (endsWith(name, ".keeper")) return "keeper";
  if (endsWith(name, ".keeper.idx") || endsWith(name, ".graf.idx") ||
      endsWith(name, ".spot.idx")) return "idx";
  return "other";
}

//  --- fs tally --------------------------------------------------------------
//  tallyShard(dir) → { keepers, keeperBytes, idxBytes, otherBytes, total } | null.
//  ONE readdir + a stat per regular file; subdirs (flat shards have none) and
//  unreadable entries skip silently; an unreadable shard dir → null (skipped).
function tallyShard(dir) {
  let names;
  try { names = io.readdir(dir); } catch (e) { return null; }   // unreadable shard
  let keepers = 0, kb = 0, ib = 0, ob = 0;
  for (const nm of names) {
    if (nm.length && nm[nm.length - 1] === "/") continue;       // subdir → skip
    let sz;
    try {
      const st = io.stat(join(dir, nm));
      if (st.kind !== "reg") continue;                          // non-file → skip
      sz = Number(st.size);
    } catch (e) { continue; }                                   // unreadable → skip
    const c = classify(nm);
    if (c === "keeper") { keepers++; kb += sz; }
    else if (c === "idx") ib += sz;
    else ob += sz;
  }
  return { keepers: keepers, keeperBytes: kb, idxBytes: ib, otherBytes: ob,
           total: kb + ib + ob };
}

//  scanStore(root, curName) → the shard rows, total-desc (name tie-break).  Each
//  trailing-"/" entry of the store root is a shard dir (non-dirs skip silently);
//  `curName` (this repo's shard basename) flags the `cur` row for the `*` mark.
function scanStore(root, curName) {
  let names;
  try { names = io.readdir(root); } catch (e) { names = []; }
  const rows = [];
  for (const nm of names) {
    if (!nm.length || nm[nm.length - 1] !== "/") continue;      // non-dir → skip
    const base = nm.slice(0, -1);
    if (base === "" || base === "." || base === "..") continue;
    const t = tallyShard(join(root, base));
    if (!t) continue;                                           // unreadable → skip
    t.name = base; t.cur = (base === curName);
    rows.push(t);
  }
  rows.sort(function (a, b) {
    if (b.total !== a.total) return b.total - a.total;
    return a.name < b.name ? -1 : a.name > b.name ? 1 : 0;      // stable tie-break
  });
  return rows;
}

//  --- rendering -------------------------------------------------------------
//  human(n): raw bytes under 1K, else K/M/G/T with one decimal below 10 (the
//  `du -h` feel — `4.0K`, `5.5M`, `44M`, `222M`).
function human(n) {
  n = Number(n);
  if (n < 1024) return String(n);
  const units = ["K", "M", "G", "T"];
  let u = -1, v = n;
  do { v /= 1024; u++; } while (v >= 1024 && u < units.length - 1);
  const s = v >= 10 ? String(Math.round(v)) : (Math.round(v * 10) / 10).toFixed(1);
  return s + units[u];
}

//  renderPlain(rows) → tab-separated raw-byte lines + a TOTAL row.  The current
//  shard trails a `\t*` (the six data fields stay positionally stable for cut/awk).
function renderPlain(rows) {
  const out = [];
  let tk = 0, tkb = 0, tib = 0, tob = 0, tt = 0;
  for (const r of rows) {
    out.push(r.name + "\t" + r.keepers + "\t" + r.keeperBytes + "\t" +
             r.idxBytes + "\t" + r.otherBytes + "\t" + r.total + (r.cur ? "\t*" : ""));
    tk += r.keepers; tkb += r.keeperBytes; tib += r.idxBytes;
    tob += r.otherBytes; tt += r.total;
  }
  out.push("TOTAL\t" + tk + "\t" + tkb + "\t" + tib + "\t" + tob + "\t" + tt);
  return out;
}

function padRight(s, w) { while (s.length < w) s += " "; return s; }
function padLeft(s, w)  { while (s.length < w) s = " " + s; return s; }

//  renderPager(rows) → aligned, human-size lines: a `<mark> SHARD` left block,
//  then right-aligned PACKS/KEEPER/INDEX/OTHER/TOTAL columns; header + TOTAL row.
function renderPager(rows) {
  const head  = ["", "SHARD", "PACKS", "KEEPER", "INDEX", "OTHER", "TOTAL"];
  const body  = [];
  let tk = 0, tkb = 0, tib = 0, tob = 0, tt = 0;
  for (const r of rows) {
    body.push([r.cur ? "*" : "", r.name, String(r.keepers), human(r.keeperBytes),
               human(r.idxBytes), human(r.otherBytes), human(r.total)]);
    tk += r.keepers; tkb += r.keeperBytes; tib += r.idxBytes;
    tob += r.otherBytes; tt += r.total;
  }
  const total = ["", "TOTAL", String(tk), human(tkb), human(tib), human(tob), human(tt)];
  const all = [head].concat(body, [total]);
  const w = [];
  for (let c = 0; c < 7; c++) {
    let m = 0;
    for (const row of all) if (row[c].length > m) m = row[c].length;
    w[c] = m;
  }
  function fmt(row) {
    let s = padRight(row[0], w[0]) + " " + padRight(row[1], w[1]);
    for (let c = 2; c < 7; c++) s += "  " + padLeft(row[c], w[c]);
    return s;
  }
  const lines = [fmt(head)];
  for (const row of body) lines.push(fmt(row));
  lines.push(fmt(total));
  return lines;
}

//  --- the verb --------------------------------------------------------------
//  BE-003 spirit: ONE uniform miss line, then throw.
function miss(arg, code) { io.log("du: " + arg + ": " + code + "\n"); throw code; }

//  JAB-004: PLAIN verb — args ride `arguments`, ambient repo/sink ride `be`.
function du() {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  if (!sink) return;

  //  Arg discipline: only bare / `.` (tolerate the scheme'd `du:` spell form).
  for (let i = 0; i < arguments.length; i++) {
    let w = String(arguments[i] == null ? "" : arguments[i]);
    if (w.indexOf(":") >= 0) {
      try { const p = uri._parse(w); if (p.scheme === "du") w = p.path || ""; }
      catch (e) {}
    }
    if (w !== "" && w !== ".") miss(w, "DUNONE");
  }

  //  Resolve THIS repo's store (be.treeAt throws when repo-less → nonzero exit).
  const repo = (_be && _be.repo) || ((_be && _be.treeAt) ? _be.treeAt() : null);
  if (!repo) miss(".", "DUNONE");
  const shard   = store.shardDir(repo.storePath, repo.project);
  const root    = pathlib.dirname(shard);        // the store root = shard's parent
  const curName = pathlib.basename(shard);

  const rows = scanStore(root, curName);
  const mode = ambient.format();
  const out  = hunkrows(sink, "du");             // no scheme → all lines are text
  const lines = (mode === "plain") ? renderPlain(rows) : renderPager(rows);
  for (const ln of lines) out.raw(ln);
  out.done();
}
du.jab = "args";
module.exports = du;

//  BRO-026: expose internals for the byte-exact repro test (the ls.js/log.js
//  model todo.js follows) — the fixture drives scan/tally/render directly.
module.exports.classify = classify;
module.exports.tallyShard = tallyShard;
module.exports.scanStore = scanStore;
module.exports.renderPlain = renderPlain;
module.exports.renderPager = renderPager;
module.exports.human = human;
