//  metaidx.js — TODO-003: the meta-pair index, ONE `kv64` lane in the store
//  shard.  The whole surface is LAZY: a caller asks `find(query)` for a
//  filtered ticket list and the index brings ITSELF up to date to serve it —
//  scan the fs, index whatever is unindexed, answer from the index.  There is
//  no separate "build" verb for a caller to forget.  [/todo/TODO/TODO-004] (the
//  arg/click/sort layer) is built on this one call.
//
//  ROW (kv64: a KEYED lane, so a re-put OVERWRITES — a pair is a mutable cell)
//    key = path_hl:40 | key_code:20 | kind:4        the wh128 field split
//    val = vkind:4    | payload:60                  (`dog/WHIFF.h`)
//  Path-major: one ticket's rows are ONE contiguous block, so
//  `prefix(path_hl<<24n, 24, cb)` reads a whole ticket's meta in a pass.
//  `key_code` is the 3-char meta key VERBATIM at 6 bits/char (18 of the 20
//  bits) — `ron.decode("Sta")`, injective, so a meta key never collides and a
//  ticket's pairs sort stably.  The two spare `key_code` bits carry the ONE
//  reserved code, CODE_HEAD (the per-ticket block header).
//
//  The value is stored LITERALLY when its normalized form fits 60 bits of
//  ron60 (it then ranges and compares exactly), else as its hashlet60 (equals
//  only).  Normalization is despaced + decased — the payload is a MATCH TOKEN,
//  never a render source; the board still reads titles and bodies off the file.
//
//  The SWEEP owns staleness (there is no confirming read left with the value in
//  the row):
//    1. readdir `todo/`, classify + stat every ticket, hash its path;
//    2. re-lex every file whose mtime is AT OR PAST the mark (`>=`: a redundant
//       re-lex costs one read, a missed edit gives a wrong answer);
//    3. reconcile that ticket's block — put what the lex found, TOMBSTONE what
//       vanished (kv64 has no delete, so a tombstone is a row);
//    4. tombstone the whole block of a ticket that died (a header row of THIS
//       wt whose path is no longer on disk);
//    5. write the mark row LAST.  Intermediate seals must NOT advance it: a
//       crash mid-sweep costs a redundant re-lex, never a file marked done
//       that was never lexed.
//  The mark is the MAX mtime OBSERVED in the scan (ruling 2026-08-03) — a clock
//  is never consulted, so fs/process clock skew cannot exist.  Worktrees SHARE
//  a shard, so `path_hl` hashes the wt-qualified (absolute) path, every block
//  carries its owning wt's code, and the mark row is per-wt.
"use strict";

const pathlib = require("./util/path.js");
const join = pathlib.join;
const sha = require("./util/sha.js");        // hashlet60FromBytes — never hand-rolled
const store = require("./store.js");         // shardDir — never compose a shard by hand
const ticketlib = require("./ticket.js");    // the SHARED ticket-code tokenizer

//  The lane's runs live beside `keeper.idx` in the store shard.
const IDX_EXT = ".meta.idx";
//  Rows put between two commits must fit ONE 4 KB memtable page (256 kv64
//  rows) — the shared/ingest.js `idxWriter` discipline.
const IDX_BATCH = 200;
//  Ticket pages are small; the same 1 MiB page cap views/todo/todo.js reads under.
const PAGE_CAP = 1 << 20;

//  --- field split ----------------------------------------------------------
const PHL_BITS = 40n, CODE_BITS = 20n, KIND_BITS = 4n;
const MARK_PHL = (1n << PHL_BITS) - 1n;      // reserved: sorts ABOVE every block
const CODE_HEAD = (1n << CODE_BITS) - 1n;    // reserved: the block header row
const CODE_MAX = (1n << 18n) - 1n;           // the widest verbatim 3-char code

const KIND_BLOCK = 0x0n;                     // a row inside a ticket's block
const KIND_MARK = 0xFn;                      // the per-wt mark sentinel

const VK_LIT = 0x0n;                         // payload = ron60 of the value
const VK_HASH = 0x1n;                        // payload = hashlet60 of the value
const VK_MARK = 0x2n;                        // payload = the max mtime (ron60)
const VK_HEAD = 0xEn;                        // payload = the owning wt's code
const VK_TOMB = 0xFn;                        // the pair/file is GONE

function packKey(phl, code, kind) {
  return (phl << (CODE_BITS + KIND_BITS)) | (code << KIND_BITS) | kind;
}
function packVal(vkind, payload) { return (vkind << 60n) | payload; }
function valKind(v) { return v >> 60n; }
function valPayload(v) { return v & ((1n << 60n) - 1n); }
function keyPhl(k) { return k >> (CODE_BITS + KIND_BITS); }
function keyCode(k) { return (k >> KIND_BITS) & ((1n << CODE_BITS) - 1n); }
function keyKind(k) { return k & ((1n << KIND_BITS) - 1n); }

//  40-bit path hashlet over the WT-QUALIFIED (absolute) path — the top 40 bits
//  of the sha, i.e. the shared hashlet60 with its low 20 dropped.  MARK_PHL is
//  reserved for the sentinel, so the 1-in-2^40 path that lands on it is nudged
//  one down (it is a hash either way, and a real duplicate is caught below).
function pathHl(abs) {
  const h = sha.hashlet60FromBytes(sha1(utf8.Encode(abs))) >> 20n;
  return h === MARK_PHL ? h - 1n : h;
}
//  A worktree's 20-bit code — the top 20 bits of the same sha.  Blocks and the
//  mark row carry it, so wt A's sweep never marks or tombstones wt B's rows.
function wtCode(wt) { return sha.hashlet60FromBytes(sha1(utf8.Encode(wt))) >> 40n; }

//  The 3-char meta key VERBATIM at 6 bits/char — RON64 IS that alphabet, so
//  `ron.decode` IS the packing (no hand-rolled char math).  null when the key
//  is not the `[A-Z][a-z][a-z0-9]` shape or would not round-trip.
function codeOf(key) {
  if (!/^[A-Z][a-z][a-z0-9]$/.test(key)) return null;
  let c;
  try { c = ron.decode(key); } catch (e) { return null; }
  if (ron.encode(c) !== key || c > CODE_MAX) return null;
  return c;
}
//  code -> the 3-char key (the answer maps rows back to key names).
function keyOf(code) { try { return ron.encode(code); } catch (e) { return null; } }

//  --- value normalization + packing ----------------------------------------
//  Despaced and decased (ruling 2026-08-02): the payload is a match token.
function normalize(v) { return String(v).replace(/\s+/g, "").toLowerCase(); }

//  Per-key normalizers.  `Due: 2026-08-03` becomes the ron60 DATE stamp, so
//  deadlines sort and range; everything else takes the generic path.
const NORM = {
  Due: function (raw) {
    const m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(String(raw).trim());
    if (!m) return null;
    const t = Date.UTC(+m[1], +m[2] - 1, +m[3]);
    if (t !== t) return null;
    return packVal(VK_LIT, ron.of(t));
  }
};

//  A meta-pair value -> its row val.  Literal when the normalized form fits 60
//  bits of ron60 (it round-trips through the codec), else its hashlet60.
function packValue(key, raw) {
  const nrm = NORM[key];
  if (nrm) { const v = nrm(raw); if (v !== null) return v; }
  const s = normalize(raw);
  if (s !== "") {
    let n = null;
    try { n = ron.decode(s); } catch (e) { n = null; }
    if (n !== null && ron.encode(n) === s) return packVal(VK_LIT, n);
  }
  return packVal(VK_HASH, sha.hashlet60FromBytes(sha1(utf8.Encode(s))));
}

//  --- the fs scan ----------------------------------------------------------
//  A name is a ticket code when the SHARED tokenizer (`tok.parse` with the mkd
//  grammar, the `uc ucnum* "-" dgt+` rule) fuses the WHOLE name into one `F`.
const CODE_MEMO = Object.create(null);          // one tok.parse per distinct name
function isCode(name) {
  let v = CODE_MEMO[name];
  if (v !== undefined) return v;
  const ks = ticketlib.scanKeys(name);
  v = ks.length === 1 && ks[0].lo === 0 && ks[0].key === name;
  CODE_MEMO[name] = v;
  return v;
}
//  Scope ruling: a thin ticket is `<KEY>.mkd`; a fat ticket is the DIR `<KEY>/`
//  holding `README.mkd`/`README.md`.  A fat ticket's IDENTITY is the dir, its
//  indexed + stat'ed path is the README, and its ATTACHED files never index.
const THIN_EXT = ".mkd";
const README = { "README.mkd": true, "README.md": true };

//  Classify one readdir-relative entry -> { id, rel } or null.  `id` is the
//  ticket's identity (dir for fat, file for thin), `rel` the indexed path.
function classify(rel) {
  const segs = pathlib.split(rel);
  if (!segs.length) return null;
  const last = segs[segs.length - 1];
  for (let i = 0; i < segs.length - 1; i++) {
    if (!isCode(segs[i])) continue;
    //  inside a fat ticket: ONLY its README indexes, everything else is an
    //  attachment (and a nested `KEY.mkd` attachment is not a ticket).
    return (i === segs.length - 2 && README[last])
      ? { id: pathlib.merge(segs.slice(0, i + 1)), rel: rel, code: segs[i] }
      : null;
  }
  if (last.length <= THIN_EXT.length || last.slice(-THIN_EXT.length) !== THIN_EXT)
    return null;
  const stem = last.slice(0, -THIN_EXT.length);
  return isCode(stem) ? { id: rel, rel: rel, code: stem } : null;
}

//  Every ticket under `todo` -> [{ id, rel, code, file, mtime, phl }],
//  path-sorted.  `file` is ABSOLUTE (the wt-qualified path `path_hl` hashes).
//  `hash` overrides `pathHl` — the golden forces a 40-bit collision that way
//  (one is expected per ~1.5M tickets, so it cannot be planted with real sha).
function scan(todo, hash) {
  const phlOf = hash || pathHl;
  let names;
  try { names = io.readdir(todo, { recursive: true }); }
  catch (e) { throw "meta index: cannot read the ticket tree " + todo + " (" + e + ")"; }
  const out = [];
  for (const nm of names) {
    if (nm.length && nm[nm.length - 1] === "/") continue;      // dirs trail "/"
    const c = classify(nm);
    if (!c) continue;
    const file = join(todo, c.rel);
    let st;
    try { st = io.stat(file); } catch (e) { continue; }        // raced away
    if (st.kind !== "reg") continue;
    out.push({ id: c.id, rel: c.rel, code: c.code, file: file,
               mtime: st.mtime, phl: phlOf(file) });
  }
  out.sort(function (a, b) { return a.id < b.id ? -1 : a.id > b.id ? 1 : 0; });
  return out;
}

//  --- the meta-pair extractor (ONE grammar, ONE matcher) --------------------
//  TODO-004 fix 2026-08-03: the pair is read off the TOKENIZER, not a line
//  regex.  `tok.parse(bytes,"mkd")` already isolates a pair — `T` is the
//  line-opening `Key:` and the next token is the value to end of line — so the
//  house grammar decides, and the view that RENDERS a pair and the index that
//  MATCHES one can no longer disagree.  This is [/todo/DOG/DOG-026] (leaf kinds
//  off the tokenizer) arriving early for this one shape.
//
//  The regex it replaces was anchored at column 0, so an INDENTED header —
//  `todo/TODO/TODO-003.mkd` lines 3-4 — rendered and clicked but indexed as
//  nothing, and its own `Sev: HIGH` click answered "no ticket matches".
//
//  Two rulings shape the reading, and BOTH are needed — indent-tolerance alone
//  is wrong:
//    1. INDENT (ruling 2026-08-03): a meta header is PLAINLY indented — column
//       0 or exactly four spaces, nothing else.  The tokenizer fires `T` at any
//       indent, so the leading `R` run is checked here.
//    2. SCOPE ([/meta/todo]: "ticket meta goes in `Key: value` meta pairs
//       directly under the header"): the ticket's OWN meta is the pair block
//       standing directly under the header — one leading non-pair line (the
//       header itself) is skipped, blank lines pass, and the FIRST other
//       construct ends the block.  Without this the four-space `Fix:`/`Msg:`
//       pairs buried in a WIP bullet (POST-023, CODE-019, POST-031, BLAME-006,
//       PATCH-004, BRO-005) would answer `Fix:*` as if they were ticket meta.
//  Each key at most once ([/meta/todo]): the FIRST occurrence wins.  `Due\: …`
//  escapes a literal key and falls out of the `T` shape by itself.
//  TODO-009: 3rd char may be a digit (DOG-026 grammar) — `On1:`, `On2:` … .
const KEYRE = /^([A-Z][a-z][a-z0-9]):$/;
const INDENT = "    ";                       // the ONE legal indent (or none)

function tokTag(w) { return String.fromCharCode(65 + ((w >>> 27) & 0x1f)); }
function tokEnd(w) { return w & 0xffffff; }

//  metaBlock(bytes, toks) -> [{ key, value, ki, vi }] — the ticket's own meta
//  pairs in file order.  `ki`/`vi` are the TOKEN INDICES of the `Key:` half and
//  of the value half, so views/todo/todo.js can hang a click spell on each
//  without re-deriving the grammar.  Pure: it reads, it never opens a file.
//  The walk reads BYTES and decodes only the two tokens of a pair it keeps: a
//  ticket is a whole page of tokens and the block ends within a few lines of
//  the top, so decoding every token up front cost the sweep more than the parse.
function metaBlock(bytes, toks) {
  const out = [];
  if (!toks || !toks.length) return out;
  const sta = (i) => (i ? tokEnd(toks[i - 1]) : 0);
  const txt = (i) => utf8.Decode(bytes.slice(sta(i), tokEnd(toks[i])));
  const hasNl = (i) => {
    for (let p = sta(i), e = tokEnd(toks[i]); p < e; p++) if (bytes[p] === 10) return true;
    return false;
  };
  const isIndent = (i) => {                       // the ONE legal indent run
    const s = sta(i), e = tokEnd(toks[i]);
    if (e - s !== INDENT.length) return false;
    for (let p = s; p < e; p++) if (bytes[p] !== 32) return false;
    return true;
  };
  const inked = (i) => {                          // any non-whitespace byte?
    for (let p = sta(i), e = tokEnd(toks[i]); p < e; p++) if (bytes[p] > 32) return true;
    return false;
  };
  let i = 0, header = false;
  while (i < toks.length) {
    //  one LINE = the tokens up to and including the one carrying the newline.
    let end = i;
    while (end < toks.length - 1 && !hasNl(end)) end++;
    //  the legal indents: nothing, or the ONE four-space `R` run.
    const k = (tokTag(toks[i]) === "R" && isIndent(i)) ? i + 1 : i;
    const m = k <= end && tokTag(toks[k]) === "T" ? KEYRE.exec(txt(k)) : null;
    if (m && k < end) {
      let v = "";
      for (let t = k + 1; t <= end; t++) v += txt(t);
      out.push({ key: m[1], value: v.replace(/\n+$/, "").trim(), ki: k, vi: k + 1 });
    } else {
      let blank = true;
      for (let t = i; t <= end; t++) if (inked(t)) { blank = false; break; }
      if (!blank) {
        //  the ticket's `#   KEY: title` header — skipped ONCE, and only while
        //  no pair has been read; anything else ENDS the block.
        if (header || out.length) break;
        header = true;
      }
    }
    i = end + 1;
  }
  return out;
}

function readBytes(file) {
  let st;
  try { st = io.lstat(file); } catch (e) { return null; }
  if (st.kind !== "reg") return null;
  if (st.size === 0n || Number(st.size) === 0) return new Uint8Array(0);
  const size = Math.min(Number(st.size), PAGE_CAP);
  let fd;
  try { fd = io.open(file, "r"); } catch (e) { return null; }
  try { const b = io.buf(size + 16); io.readAll(fd, b, size); return b.data().slice(); }
  catch (e) { return null; }
  finally { try { io.close(fd); } catch (e) {} }
}

//  TODO-004: one file -> { <Key>: "<raw value>" } in file order — the DISPLAY
//  reading.  The row payload is a MATCH TOKEN (despaced, decased, maybe a
//  hash), so a view that RENDERS a pair must read it off the file; this is
//  that read, and `lex` packs THIS, so the pair grammar lives in ONE place.
function pairs(file) {
  const bytes = readBytes(file);
  const out = {};
  if (bytes === null || !bytes.length) return out;
  let toks;
  try { toks = tok.parse(bytes, "mkd"); } catch (e) { return out; }
  for (const p of metaBlock(bytes, toks))
    if (out[p.key] === undefined) out[p.key] = p.value;
  return out;
}

//  One file -> { <code>: <row val> } for every meta pair it carries.
function lex(file) {
  const raw = pairs(file);
  const out = {};
  for (const key in raw) {
    const code = codeOf(key);
    if (code === null || out[code] !== undefined) continue;
    out[code] = packValue(key, raw[key]);
  }
  return out;
}

//  --- the index handle -----------------------------------------------------
//  A batching writer over the lane.  A seal NEVER carries the mark — that row
//  is the sweep's LAST write (see the header).
function idxWriter(ix) {
  let n = 0;
  return {
    put: function (k, v) { ix.put(k, v); if (++n >= IDX_BATCH) { ix.commit(); n = 0; } },
    seal: function () { if (n) { ix.commit(); n = 0; } }
  };
}

//  Open the lane in `shard`.  `abc.index` io.mkdir()s its dir, so never
//  CONJURE a shard: a missing one is a repo without a store, said in words.
//  There is NO marker audit here (the shared/ingest.js keeper.idx family needs
//  one): a run this family seals is complete BY CONSTRUCTION — the mark row is
//  the sweep's last write, so a run left by a crashed sweep holds only rows
//  that are true, and the un-advanced mark makes the next sweep re-lex them.
function openIndex(shard) {
  let kind = null;
  try { kind = io.stat(shard).kind; } catch (e) { kind = null; }
  if (kind !== "dir")
    throw "meta index: there is no store shard at " + shard +
          " — the ticket index lives in the project store";
  return abc.index("kv64", { dir: shard, ext: IDX_EXT });
}

//  ONE merged pass over the whole stack -> { blocks, codes, marks }.
//  `blocks[phl] = { wt, rows: {code: val} }` with tombstones already dropped;
//  `codes` is every live key code in the lane (the absent-key early-out);
//  `marks[wtcode] = mtime`.  The lane is path-major, so a key column is not
//  contiguous and this full scan is what a key query rides — it costs far less
//  than the fs sweep that precedes it.
function readAll(ix) {
  const blocks = Object.create(null), codes = Object.create(null),
        marks = Object.create(null);
  const c = ix.seek(0n);
  while (c.next()) {
    const k = c.key, v = c.val;
    if (keyKind(k) === KIND_MARK) {
      if (valKind(v) === VK_MARK) marks[keyCode(k)] = valPayload(v);
      continue;
    }
    if (keyKind(k) !== KIND_BLOCK) continue;
    const phl = keyPhl(k), code = keyCode(k);
    let b = blocks[phl];
    if (!b) b = blocks[phl] = { wt: null, rows: Object.create(null) };
    if (code === CODE_HEAD) {
      b.wt = valKind(v) === VK_TOMB ? null : valPayload(v);
      continue;
    }
    if (valKind(v) === VK_TOMB) { delete b.rows[code]; continue; }
    b.rows[code] = v;
    codes[code] = true;
  }
  return { blocks: blocks, codes: codes, marks: marks };
}

//  --- the sweep ------------------------------------------------------------
//  Reconcile ONE ticket's block: put what the lex found (a re-put overwrites,
//  an identical value is not re-put), tombstone every code that vanished, and
//  refresh the header row that names the owning wt.
function reconcile(w, t, pairs, block, wt, rec) {
  const old = block ? block.rows : Object.create(null);
  for (const code in pairs) {
    if (old[code] === pairs[code]) continue;
    w.put(packKey(t.phl, BigInt(code), KIND_BLOCK), pairs[code]);
    rec.put++;
  }
  for (const code in old) {
    if (pairs[code] !== undefined) continue;
    w.put(packKey(t.phl, BigInt(code), KIND_BLOCK), packVal(VK_TOMB, 0n));
    rec.tombed++;
  }
  if (!block || block.wt !== wt) {
    w.put(packKey(t.phl, CODE_HEAD, KIND_BLOCK), packVal(VK_HEAD, wt));
    rec.put++;
  }
  if (block) block.rows = pairs; else block = { wt: wt, rows: pairs };
  block.wt = wt;
  return block;
}

//  Tombstone a whole block — a file this wt indexed and that is gone (or that
//  a path-hash collision just took out of the index).
function killBlock(w, phl, block, rec) {
  for (const code in block.rows) {
    w.put(packKey(BigInt(phl), BigInt(code), KIND_BLOCK), packVal(VK_TOMB, 0n));
    rec.tombed++;
  }
  w.put(packKey(BigInt(phl), CODE_HEAD, KIND_BLOCK), packVal(VK_TOMB, 0n));
  rec.tombed++;
  block.rows = Object.create(null);
  block.wt = null;
}

//  sweep(ix, tickets, wt, opts) -> { blocks, codes, direct, rec }.
//  `direct` holds the tickets a path-hash COLLISION took out of the index —
//  two live paths on one `path_hl`, so both are dropped and read directly.
function sweep(ix, tickets, wt, opts) {
  const rec = { files: tickets.length, lexed: 0, skipped: 0, put: 0, tombed: 0,
                collided: 0, mark: 0n, maxMtime: 0n, cold: false };
  const state = readAll(ix);
  const blocks = state.blocks;
  const mark = state.marks[wt] !== undefined ? state.marks[wt] : 0n;
  rec.mark = mark;
  rec.cold = mark === 0n;

  //  the COMPLETE path list is in hand, so a 40-bit collision is DETECTED.
  const live = Object.create(null), byPhl = Object.create(null);
  for (const t of tickets) {
    live[t.phl] = true;
    (byPhl[t.phl] || (byPhl[t.phl] = [])).push(t);
    if (t.mtime > rec.maxMtime) rec.maxMtime = t.mtime;
  }
  const direct = [], collided = Object.create(null);
  for (const phl in byPhl) {
    if (byPhl[phl].length < 2) continue;
    collided[phl] = true;
    for (const t of byPhl[phl]) { t.pairs = lex(t.file); direct.push(t); rec.collided++; }
  }

  const w = idxWriter(ix);
  let n = 0;
  for (const t of tickets) {
    if (collided[t.phl]) continue;
    if (t.mtime < mark) { rec.skipped++; continue; }         // `>=` re-lexes
    const pairs = lex(t.file);
    blocks[t.phl] = reconcile(w, t, pairs, blocks[t.phl], wt, rec);
    for (const code in pairs) state.codes[code] = true;
    rec.lexed++;
    //  the crash-mid-sweep golden: abort AFTER an intermediate seal and BEFORE
    //  the mark row.  Production never passes this.
    if (opts && opts._crashAfter !== undefined && ++n >= opts._crashAfter) {
      w.seal();
      throw "meta index: injected sweep fault after " + n + " tickets";
    }
  }
  //  a block THIS wt owns whose file is gone (or just collided out) dies.
  for (const phl in blocks) {
    const b = blocks[phl];
    if (b.wt !== wt) continue;                               // another wt's block
    if (live[phl] && !collided[phl]) continue;
    killBlock(w, phl, b, rec);
  }
  w.seal();
  //  LAST, and only when it moves: an intermediate seal must never carry it.
  if (rec.maxMtime !== mark) {
    ix.put(packKey(MARK_PHL, wt, KIND_MARK), packVal(VK_MARK, rec.maxMtime));
    ix.commit();
  }
  return { blocks: blocks, codes: state.codes, direct: direct, rec: rec };
}

//  --- the answer -----------------------------------------------------------
//  A query is `{ Sta: "OPEN", Who: "gritzko" }` — every clause must hold (the
//  board intersects `Sta`+`Who`+`Due` on `path_hl` before it opens anything):
//    "OPEN" / a number   exact match on the packed value
//    null / true         PRESENCE — the key is there, any value
//    { lo, hi }          a half-open RANGE, literals only (a hash only equals)
function clauseOf(key, want) {
  const code = codeOf(key);
  if (code === null) throw "meta index: '" + key + "' is not a meta key " +
                           "(capital then two lowercase/digits, like Now or On1)";
  if (want === null || want === undefined || want === true)
    return { code: code, kind: "any" };
  if (typeof want === "object") {
    const lo = want.lo !== undefined ? valPayload(packValue(key, want.lo)) : null;
    const hi = want.hi !== undefined ? valPayload(packValue(key, want.hi)) : null;
    return { code: code, kind: "range", lo: lo, hi: hi };
  }
  return { code: code, kind: "eq", val: packValue(key, want) };
}
function holds(cl, rows) {
  const v = rows[cl.code];
  if (v === undefined) return false;
  if (cl.kind === "any") return true;
  if (cl.kind === "eq") return v === cl.val;
  if (valKind(v) !== VK_LIT) return false;                   // a hash only equals
  const p = valPayload(v);
  return (cl.lo === null || p >= cl.lo) && (cl.hi === null || p < cl.hi);
}
//  The row values a ticket carries, keyed by the 3-char name — TODO-004 sorts
//  on `v` (a literal is ordered; a hash is an identity only).
function metaOf(rows) {
  const out = {};
  for (const code in rows) {
    const nm = keyOf(BigInt(code));
    if (nm) out[nm] = { lit: valKind(rows[code]) === VK_LIT, v: valPayload(rows[code]) };
  }
  return out;
}

//  --- roots ----------------------------------------------------------------
//  Where the tickets, the worktree and the shard are.  `opts` overrides every
//  one of them (the golden plants its own tree); the defaults are the ONE
//  ticket tree `be.todoRoot()` and the shard `store.shardDir` resolves from the
//  anchor at it — a shard is NEVER composed by hand, and `<wt>/.be` is a FILE
//  whenever the store is not colocated.
function roots(opts) {
  const o = opts || {};
  let todo = o.todo;
  if (!todo) {
    if (typeof be === "undefined" || !be.todoRoot)
      throw "meta index: no ticket tree — this is not a beagle project";
    todo = be.todoRoot();
    if (!todo) throw "meta index: no ticket tree — this is not a beagle project";
  }
  let wt = o.wt, shard = o.shard;
  if (!wt || !shard) {
    const t = require("../core/resolve_hash.js").treeAt(todo);
    if (!wt) wt = t.wt;
    if (!shard) shard = store.shardDir(t.storePath, t.project);
  }
  return { todo: todo, wt: wt, shard: shard };
}

//  --- THE entry point ------------------------------------------------------
//  find(query[, opts]) — the fs is scanned, unindexed files are indexed, the
//  max mtime becomes the mark, and the filtered ticket list comes out of the
//  index.  ONE call; there is nothing to build first.
//
//    { tickets: [ { id, rel, code, file, mtime, meta } ], sweep: { … } }
//
//  `id` is the ticket's identity (the DIR for a fat ticket), `rel` the indexed
//  path, `file` its absolute path, `meta` its packed pair values.  The index
//  decides WHICH tickets to open, never what to show — titles and bodies come
//  off the file.
function find(query, opts) {
  const r = roots(opts);
  const tickets = scan(r.todo, opts && opts._hash);
  const wt = wtCode(r.wt);
  const ix = openIndex(r.shard);
  let s;
  try { s = sweep(ix, tickets, wt, opts); }
  finally { try { ix.close(); } catch (e) {} }

  const clauses = [];
  for (const key in (query || {})) clauses.push(clauseOf(key, query[key]));
  //  absent-key early-out: a code no row in the lane carries (and no directly
  //  read ticket has) can match nothing — answer without touching a ticket.
  const directCodes = Object.create(null);
  for (const t of s.direct) for (const c in t.pairs) directCodes[c] = true;
  for (const cl of clauses)
    if (!s.codes[cl.code] && !directCodes[cl.code]) {
      s.rec.earlyOut = keyOf(cl.code);
      return { tickets: [], sweep: s.rec };
    }

  const out = [];
  for (const t of tickets) {
    const rows = t.pairs || (s.blocks[t.phl] ? s.blocks[t.phl].rows : null);
    if (!rows) continue;
    let all = true;
    for (const cl of clauses) if (!holds(cl, rows)) { all = false; break; }
    if (!all) continue;
    out.push({ id: t.id, rel: t.rel, code: t.code, file: t.file,
               mtime: t.mtime, meta: metaOf(rows) });
  }
  return { tickets: out, sweep: s.rec };
}

module.exports = {
  find: find,
  //  the packing surface, exposed for the golden and for TODO-004's own rows
  IDX_EXT: IDX_EXT, IDX_BATCH: IDX_BATCH,
  MARK_PHL: MARK_PHL, CODE_HEAD: CODE_HEAD,
  KIND_BLOCK: KIND_BLOCK, KIND_MARK: KIND_MARK,
  VK_LIT: VK_LIT, VK_HASH: VK_HASH, VK_MARK: VK_MARK, VK_HEAD: VK_HEAD,
  VK_TOMB: VK_TOMB,
  packKey: packKey, packVal: packVal, valKind: valKind, valPayload: valPayload,
  keyPhl: keyPhl, keyCode: keyCode, keyKind: keyKind,
  pathHl: pathHl, wtCode: wtCode, codeOf: codeOf, keyOf: keyOf,
  normalize: normalize, packValue: packValue,
  classify: classify, scan: scan, lex: lex, openIndex: openIndex,
  //  TODO-004: the raw `Key: value` read for RENDER (the row payload is a match
  //  token, never a render source), and the ONE token-level matcher underneath
  //  it — views/todo/todo.js hangs its click spells off THIS, so the rendered
  //  pair and the indexed pair are the same pair by construction.
  pairs: pairs, metaBlock: metaBlock
};
