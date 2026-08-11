//  store.js — the pure-JS object + ref store (JS-030, JS-048).  Pure JS over
//  the JABC bindings: `git.pack` (offset-addressed pack-log read + OFS/REF
//  delta chase), `abc.index` (wh128 lane: the sha->offset object index), the
//  ULOG family (the project `refs` reflog), `git.tree`/`git.parseCommit`
//  (object parsers) and `codec` (`sha1`/`hex`).  No C, no dog — shares
//  zero code with the keeper dog; the on-disk formats ARE libdog/abc so
//  read is reimplementable.  Mirrors keeper/KEEP.c (KEEPGet / KEEPLookup
//  / KEEPResolveRef) + keeper/REFS.c.  Object read + ref READ are here;
//  the ref WRITERS (createShard/set/tombstone) folded in from refs.js
//  (JS-048) sit next to the resolveRef/eachTip readers below.
//
//  open(storePath, project) → reader where
//    storePath = the store root (`<wt>` for a colocated primary, or the
//                redirected store dir for a secondary wt; be.treeAt().storePath)
//    project   = the shard name (`be.treeAt().project`); when empty, shardDir
//                resolves one — the DEFAULT-titled shard, else the single
//                shard dir under <store>/.be.  GET-060 RULING 2: the answer is
//                always `.be/<shard>/`, never `.be` itself (no flat store).
//  The reader exposes:
//    getObject(sha)        → { type, bytes } | undefined   (inflate + delta chase)
//    resolveRef(refOrBranch) → "<40hex>" | undefined        (refs ULOG)
//    eachTip(cb)           local branch tips   cb({ key, branch, sha, ts })
//    eachRemote(cb)        remote-tracking tips cb({ key, host, query, sha, ts })
//    readTree(sha)         → [{ mode, name, sha }]          (git.tree)
//    commitTree(sha)       → "<40hex>"                       (git.parseCommit)
//    commitParents(sha)    → ["<40hex>", …]
//    readTreeRecursive(sha, cb)  per-leaf cb({ path, mode, sha, kind })
//
//  Object location (JS-056): `locate()` mmaps the on-disk `keeper.idx`
//  LSM runs the keeper already wrote (`abc.index("wh128",{dir,ext})`) and
//  ranges `[hashlet60(sha)<<4, …|0xf]` (type-agnostic) — newest-wins, NO
//  startup scan.  The ON-DISK val is `offset40 | file_id20 | flags4`
//  (`wh64Pack`, keeper/KEEP.h), so `offset = val>>24`, `file_id =
//  (val>>4)&0xfffff`; `file_id` is the numeric `NNNNNNNNNN.keeper` id,
//  mapped to a `packs()` index.  The pack at that index is seeked +
//  resolved on demand (`getObject`/`readRecord` unchanged).
//  FALLBACK (no `.keeper.idx` run in the shard): build the memidx index
//  ONCE — `git.pack.mmap` each `<shard>/NNNNN.keeper` and `pack.scan` it
//  into one wh128 memidx keyed by the same `hashlet60<<4|type`
//  WHIFFKeyPack; that path's `val` is `fileIdx<<40 | offset` (NOT the
//  on-disk layout).  Both decode in `locate()` under one `onDisk` flag.

"use strict";

const pathlib = require("./util/path.js");   // JSQUE-016: util libs -> shared/util/
const safeRel = pathlib.safeRel;             // JS-065: worktree-confinement guard
const shalib = require("./util/sha.js");
const ulog = require("./ulog.js");
const branchlib = require("./branch.js");    // SUBS-050: the ONE branch codec
const ingest = require("./ingest.js");       // DOG-027: the keeper.idx family
const memidx = require("./memidx.js");       // DOG-027: the in-RAM fallback index
const stats = require("./util/stats.js");    // CFOLD-001: env-gated read counters
const shape = require("./util/shape.js");    // BE-066: hidden-class pinning
const join = pathlib.join;

//  BE-066: the object-read row literals, one pin per key SEQUENCE — every tree
//  entry and every record read builds one, and an unpinned shape is re-cloned.
const PIN_ENTRY = shape.pin(["mode", "name", "sha"]);          // readTree
//  ...including the one git.tree hands the callback (JS-028's documented
//  {mode, nameStart, nameEnd, sha, nextOff}) — a C-built object, pinned here.
const PIN_GITENT = shape.pin(["mode", "nameStart", "nameEnd", "sha", "nextOff"]);
const PIN_LEAF = shape.pin(["path", "mode", "sha", "kind"]);   // readTreeRecursive
const PIN_LOC = shape.pin(["fileIdx", "offset", "type"]);      // locate
const PIN_REC = shape.pin(["bytes", "type"]);                  // readRecord
const PIN_OBJ = shape.pin(["type", "bytes"]);                  // getObject
//  GET-060: the shard-path arithmetic (beDirOf/defaultTitle) reads the store
//  path by NAME — through the ONE path lib, never a hand-rolled slice.
const basename = pathlib.basename, dirname = pathlib.dirname;
const isFullSha = shalib.isFullSha;
const isZeroSha = shalib.isZeroSha;
const hashlet60FromBytes = shalib.hashlet60FromBytes;
const frameSha = shalib.frameSha;
const hexDecode = hex.decode;   // the JABC hex.decode binding (js/codec.cpp)

const BE = ".be";

//  Pack/git object type numbers (git pack format).
const T_COMMIT = 1, T_TREE = 2, T_BLOB = 3, T_TAG = 4;
const TYPE_NAME = { 1: "commit", 2: "tree", 3: "blob", 4: "tag" };
const NAME_TYPE = { commit: 1, tree: 2, blob: 3, tag: 4 };

function statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }
function isDir(p) { return statKind(p) === "dir"; }

//  --- thin-pack (REF_DELTA) fallback index walk ------------------------
//  pack.scan (PIDXScan) emits one (key,val) per object for an OFS-only log,
//  but THROWS on a thin pack carrying a REF_DELTA — e.g. a wire-cloned
//  store whose verbatim full-clone pack mixes REF_DELTA bodies in.  This
//  fallback walks such a pack record-by-record, resolving every raw /
//  OFS-delta object to bytes, git-sha-ing it, and putting its wh128 entry;
//  the REF_DELTA records themselves stay unresolvable in pure JS and are
//  log-and-skipped (a candidate JS-034 REF-base resolve leaf), but every
//  resolvable object IS indexed — so the baseline tree still populates.
//  Shares frameSha/hashlet60FromBytes (sha.js) and hex.decode; no inline
//  hex/sha math.

//  WHIFFKeyPack(type, hashlet60): type in the low 4 bits, hashlet60 high.
function keyFor(type, hashlet60) {
  return (hashlet60 << 4n) | (BigInt(type) & 0xfn);
}

//  Best-effort git type from resolved object bytes (the record type is a
//  delta).  Trees are "<mode> <name>\0<20b>"* (mode digits then space);
//  commits start "tree <40hex>\n"; tags "object <40hex>\n"; else blob.
//  ASCII-only header peek — NOT utf8.Decode (a blob's head is often binary
//  and utf8.Decode THROWS on malformed UTF-8, aborting the index build).
function inferType(bytes) {
  const n = Math.min(64, bytes.length);
  let head = "";
  for (let i = 0; i < n; i++) head += String.fromCharCode(bytes[i]);
  if (head.startsWith("tree ") && head.indexOf("\n") > 0) return T_COMMIT;
  if (head.startsWith("object ")) return T_TAG;
  if (/^[0-7]{5,6} /.test(head)) return T_TREE;
  return T_BLOB;
}

//  JS-117: `afterOff` (default -1) skips records at/below that offset — used to
//  index ONLY a tail-appended pack that pk.scan (count-driven) couldn't see.
function indexPackByWalk(pk, fhi, ix, afterOff) {
  const after = (afterOff == null) ? -1 : afterOff;
  pk.rewind();
  const offsets = [];
  while (pk.next()) if (pk.offset > after) offsets.push(pk.offset);
  for (const off of offsets) {
    pk.seek(off);
    if (pk.type === "ref-delta") {
      io.log("store.js: skipping ref-delta at " + pk.shard +
             " off=" + off + " (unresolvable in pure JS)\n");
      continue;
    }
    let bytes, tname;
    try {
      const out = io.buf((pk.size || 0) * 4 + 256);
      pk.seek(off);
      pk.resolve(out);
      bytes = out.data();
      tname = pk.type;   // raw record type (the OFS-delta base type)
    } catch (e) { continue; }
    const type = NAME_TYPE[tname] || inferType(bytes);
    const h = hashlet60FromBytes(hexDecode(frameSha(TYPE_NAME[type], bytes)));
    ix.put(keyFor(type, h), BigInt(off) | fhi);
  }
}

//  GET-060: the store `.be` dir of a `store` path, which is legitimately spelled
//  EITHER way — the tree root (`<wt>`), or the `.be` dir itself ([/wiki/URI]'s
//  record is `/home/gritzko/.be/`, and serve resolves a `be:<path>/.be` selector
//  to exactly that).  Told apart by NAME.  The old `isDir(storePath)` probe
//  answered for ANY dir that happened to exist, so a SHARD (or a bare wt) could
//  pose as the store root — half of how a clone's pack landed in `.be/` itself.
function beDirOf(storePath) {
  return basename(storePath) === BE ? storePath : join(storePath, BE);
}

//  GET-060 RULING 2 (gritzko 2026-08-06): there is no flat store — a store
//  `.be/` holds SHARDS and nothing else.  So a project-less resolution still
//  answers a SHARD, and the name it defaults to is the WORKTREE's own dir name.
//  That default is not invented here: submount.js:290 already titles a url-less
//  sub `basename(subpath)`, and test/lib/repo-setup.sh documents the same rule
//  for an in-place bootstrap ("the project (Title) defaults to the wt basename,
//  so its refs/packs live in `.be/<name>/`").  "repo" is the last resort, the
//  same one GET-042 hands a title-less clone.
function defaultTitle(storePath) {
  const b = basename(storePath);
  const t = (b === BE) ? basename(dirname(storePath)) : b;
  return (t && t !== "/" && t !== "." && t !== "..") ? t : "repo";
}

//  GET-060: a FLAT store — packs sitting directly in `.be/` — is retired, and
//  silently reading it as an empty store would look like a wiped repo.  Say so
//  by name, in plain words, with the move that fixes it.
function flatStoreGuard(beDir, names) {
  for (const nm of names) if (nm.slice(-7) === ".keeper")
    throw "store: " + beDir + " keeps its packs directly in the store dir" +
          " — that flat layout is retired; move them into a shard: " +
          "mkdir " + beDir + "/<title> && mv " + beDir + "/*.keeper " +
          beDir + "/*.keeper.idx " + beDir + "/refs " + beDir + "/<title>/";
}

//  Locate the shard dir `<store>/.be/<project>/`.  The answer is ALWAYS a
//  `.be/<shard>/` path — never `.be` itself (RULING 2, above).  Resolution
//  order when no project is named: the DEFAULT title's shard if it exists, then
//  the single non-dotted subdir (a renamed worktree, a shard named otherwise),
//  then the default title as the path a first WRITE must mint.
function shardDir(storePath, project) {
  //  URI-016: a trailing slash is legitimate — [/wiki/URI]'s record spells `store`
  //  as `/home/gritzko/.be/` — but io.readdir(dir + "/") returns every name with
  //  its FIRST CHARACTER EATEN (array form, L165), so a pack lists as
  //  `000000001.keeper` and mmap ENOENTs.  Normalise here, the one choke point.
  storePath = String(storePath).replace(/\/+$/, "") || "/";
  const beDir = beDirOf(storePath);
  //  A NAMED project is the anchor's own word — take it, minted or not.
  if (project) return join(beDir, project);
  const def = join(beDir, defaultTitle(storePath));
  if (isDir(def)) return def;
  //  Auto-detect: the single project subdir (skip dotted, like .lock).  ONE
  //  readdir also carries the flat-store witness (a loose `.keeper`).
  let found, names = [];
  try {
    io.readdir(beDir, function (name) {
      names.push(name);
      if (name[name.length - 1] !== "/") return "more";
      const base = name.slice(0, -1);
      if (!base || base[0] === ".") return "more";
      found = join(beDir, base);   // last non-dotted subdir wins
      return "more";
    });
  } catch (e) { /* */ }
  if (found) return found;
  flatStoreGuard(beDir, names);
  return def;                      // the shard a first write mints
}

//  GET-060: open a reader ON a shard path (`.be/<title>/`) — the one honest
//  spelling for "this exact shard", split back into (store `.be`, title).  The
//  old `store.open(<shard>, "")` leaned on shardDir's isDir(storePath) guess to
//  let a shard pose as a store root, which is the flat store RULING 2 retires.
function openShard(shardPath) {
  const p = String(shardPath).replace(/\/+$/, "");
  return open(dirname(p), basename(p));
}

//  DOG-027: readers holding an OPEN index register here; the session loop
//  closes them at the dispatch boundary — an index is open only for the
//  duration of use (the render), never across verbs of a resident session.
const LIVE = [];
function closeAll() { while (LIVE.length) { try { LIVE.pop().close(); } catch (e) {} } }

function open(storePath, project) {
  const shard = shardDir(storePath, project);

  //  Lazily list the keeper pack-logs in the shard, sorted by name so
  //  file index 0,1,… is stable.  Each entry: { name, path, pack? }.
  let packsList = null;
  function packs() {
    if (packsList) return packsList;
    const out = [];
    let names = [];
    try { names = io.readdir(shard); } catch (e) { names = []; }
    for (const nm of names) {
      if (nm.endsWith(".keeper")) out.push({ name: nm, path: join(shard, nm),
                                             pack: null });
    }
    out.sort(function (a, b) { return a.name < b.name ? -1 : a.name > b.name ? 1 : 0; });
    packsList = out;
    return out;
  }
  function packAt(i) {
    const p = packs()[i];
    if (!p) return undefined;
    if (!p.pack) {
      const pk = git.pack.mmap(p.path, "r");
      //  An RO-mapped pack opens with watermark 0 (write head); the read
      //  cursor (next/seek/scan) uses watermark as DATA length, so expose
      //  the whole file by setting it to the mapped byte length — the
      //  same fix-up the ULOG reader does (JS-029 finding).
      pk.buffer.watermark = pk.byteLength;
      p.pack = pk;
    }
    return p.pack;
  }

  //  JS-056: map the on-disk `file_id` (the numeric `NNNNNNNNNN.keeper`
  //  log id) to a `packs()` index.  packs() sorts by name, so the array
  //  index is NOT the file_id in general; parse the leading digits.
  let fidMap = null;
  function fileIdToPackIdx(fid) {
    if (!fidMap) {
      fidMap = {};
      const list = packs();
      for (let i = 0; i < list.length; i++) {
        const n = parseInt(list[i].name, 10);   // NNNNNNNNNN.keeper -> N
        if (n === n) fidMap[n] = i;              // NaN guard
      }
    }
    const idx = fidMap[fid];
    return idx === undefined ? -1 : idx;
  }

  //  JS-056: open the on-disk keeper.idx LSM runs (mmap, no scan).  Returns
  //  the abc.index (newest-wins across runs) when the shard HAS at least one
  //  usable `.keeper.idx` run, else null so locate() falls back to the in-RAM
  //  build.  Memoized; `false` is the "no run, don't retry" sentinel.
  //  DOG-027: the marker audit, the tail rebuild and the whole run lifecycle
  //  are the keeper family's (ingest.openIndex) — the ladder is C's.  This
  //  sits under EVERY read verb, so openIndex NEVER throws: a read-only /
  //  EROFS / over-deep store degrades to null and the in-RAM fallback.
  let diskIx = undefined;
  function diskIndex() {
    if (diskIx !== undefined) return diskIx || null;
    let ix = null;
    try { ix = ingest.openIndex(shard); } catch (e) { ix = null; }
    diskIx = ix || false;
    //  register on ACQUIRE (again after a close self-heal), so the dispatch
    //  sweep always covers every reader actually holding a pup slot.
    if (ix && LIVE.indexOf(reader) < 0) LIVE.push(reader);
    return diskIx || null;
  }

  //  Build the in-memory wh128 object index once: scan every pack,
  //  remapping `val` to carry the FILE INDEX in the high bits so a hit
  //  knows which pack to seek.  `pack.scan` emits val = bare in-pack
  //  offset (low 40 bits); we OR in (fileIdx << 40).
  let idx = null;
  function index() {
    if (idx) return idx;
    const ix = memidx.open(1 << 16);   // DOG-027: no `{mem}` lane any more
    const list = packs();
    for (let fi = 0; fi < list.length; fi++) {
      const pk = packAt(fi);
      if (!pk) continue;
      const fhi = BigInt(fi) << 40n;
      //  Fast path: pack.scan (PIDXScan) emits one (key,val) per object for
      //  an OFS-only log — which every STORED keeper pack is (the wiki
      //  PackLog invariant: OFS_DELTA is pack-local, REF_DELTA only rides
      //  transient foreign imports).  On a thin pack (a stray REF_DELTA,
      //  e.g. a wire-cloned store) scan throws → fall back to the gated
      //  per-record walk, so one such pack can't blind the whole shard.
      const cnt = pk.count || 0;
      const buf = io.buf(cnt * 16 + 256);
      let ents;
      try { ents = pk.scan(buf); } catch (e) { ents = null; }
      if (ents) {
        let maxOff = -1;
        for (let i = 0; i < ents.length; i += 2) {
          ix.put(ents[i], ents[i + 1] | fhi);
          const o = Number(ents[i + 1] & 0xffffffffffn);
          if (o > maxOff) maxOff = o;
        }
        //  JS-117: scan is count-driven (first pack only) — walk any records
        //  past its coverage so a tail-appended pack (multi-pack log) indexes.
        indexPackByWalk(pk, fhi, ix, maxOff);
      } else {
        indexPackByWalk(pk, fhi, ix, -1);   // thin-pack fallback (see above)
      }
    }
    idx = ix;                          // DOG-027: memidx sorts on first range()
    return ix;
  }

  //  Locate a sha → { fileIdx, offset, type }.  Range the lane on the
  //  60-bit hashlet (type-agnostic): keys are hashlet<<4|type, so the
  //  object (whatever its type) lies in [h<<4, h<<4 | 0xf].  JS-056: prefer
  //  the mmap'd on-disk keeper.idx runs (val `offset40|file_id20|flags4`,
  //  newest-wins, no scan); fall back to the in-RAM scan-build only when the
  //  shard has no `.keeper.idx` run.  The two val layouts DIFFER, so decode
  //  per the source under `onDisk`.
  function locate(sha) {
    const bytes = (typeof sha === "string") ? hexDecode(sha) : sha;
    //  GET-060: an empty/short sha is a MISS in plain words — it used to blow
    //  up two frames deeper as `TypeError: Invalid argument type in ToBigInt
    //  operation` (hexDecode("") yields zero bytes, so sha20[0] is undefined).
    if (!bytes || bytes.length < 20) return undefined;
    const h = hashlet60FromBytes(bytes);
    const lo = h << 4n;
    const hi = lo | 0xfn;
    const disk = diskIndex();
    const ix = disk || index();
    const onDisk = !!disk;
    let hit;
    ix.range(lo, hi + 1n, function (kv) {
      const key = kv[0], val = kv[1];
      const type = Number(key & 0xfn);
      //  DOG-027: 0xF is the family MARKER (a PACK bookmark), never an object.
      if (type === 0xf) return true;
      let fileIdx, offset;
      if (onDisk) {
        //  on-disk wh64Pack: offset = val>>24, file_id = (val>>4)&0xfffff.
        offset = Number(val >> 24n);
        fileIdx = fileIdToPackIdx(Number((val >> 4n) & 0xfffffn));
        if (fileIdx < 0) return true;   // unknown file_id → keep scanning range
      } else {
        //  in-RAM build: val = fileIdx<<40 | offset.
        fileIdx = Number(val >> 40n);
        offset = Number(val & 0xffffffffffn);
      }
      hit = { fileIdx: fileIdx, offset: offset, type: type };
      return false;   // first match wins
    });
    return hit;
  }

  //  resolveHexAny(prefix) -> "<40hex>" | undefined.  The KEEPLookup twin for the
  //  `sha1:?<short-hex>` / `?#<short-hex>` form (JAB-006): resolve a 1..39-hex
  //  prefix to the unique full sha of ANY stored object (blob/tree/commit/tag),
  //  NOT only tips — so it finds a mid-history commit or a non-root subtree the
  //  tip-only core/resolve.js::resolveHex would miss.  Ranges the SAME wh128
  //  lane locate() uses over the hashlet60 window the prefix pins, reframes each
  //  candidate's bytes to its full sha (frameSha), and prefix-matches.  AMBIGUOUS
  //  (two distinct full shas share the prefix) → undefined, matching KEEPLookup
  //  (an under-specified prefix resolves to nothing).  hashlet60 = the MS 60 bits
  //  of the sha = the first 15 hex nibbles; the index key is hashlet60<<4|type,
  //  so a prefix of P hex chars pins the range [hLo<<4, (hHi<<4)|0xf] where hLo/
  //  hHi are the prefix zero-/f-filled to 15 nibbles (a prefix longer than 15 is
  //  one hashlet, then the full-sha reframe verifies the rest).
  function resolveHexAny(prefix) {
    if (!/^[0-9a-f]{1,39}$/.test(prefix)) return undefined;
    //  Derive the hashlet60 [lo,hi] window the prefix admits (15 nibbles = 60b).
    let hLo, hHi;
    if (prefix.length >= 15) {
      const h = BigInt("0x" + prefix.slice(0, 15));
      hLo = h; hHi = h;
    } else {
      const fill = 15 - prefix.length;
      const base = BigInt("0x" + prefix) << BigInt(fill * 4);
      hLo = base;
      hHi = base | ((1n << BigInt(fill * 4)) - 1n);
    }
    const lo = hLo << 4n;
    const hi = (hHi << 4n) | 0xfn;
    const disk = diskIndex();
    const ix = disk || index();
    const onDisk = !!disk;
    let hit;                                 // the unique full sha (or "" ambiguous)
    let ambiguous = false;
    ix.range(lo, hi + 1n, function (kv) {
      if (ambiguous) return false;
      const key = kv[0], val = kv[1];
      if ((key & 0xfn) === 0xfn) return true;   // DOG-027: a marker row, not an object
      let fileIdx, offset;
      if (onDisk) {
        offset = Number(val >> 24n);
        fileIdx = fileIdToPackIdx(Number((val >> 4n) & 0xfffffn));
        if (fileIdx < 0) return true;        // unknown file_id → keep scanning
      } else {
        fileIdx = Number(val >> 40n);
        offset = Number(val & 0xffffffffffn);
      }
      const type = Number(key & 0xfn);
      const rec = readRecord(fileIdx, offset);
      if (!rec) return true;
      const tname = TYPE_NAME[type] || rec.type;
      const full = frameSha(tname, rec.bytes);
      if (full.indexOf(prefix) !== 0) return true;   // hashlet collision, skip
      if (hit && hit !== full) { ambiguous = true; return false; }
      hit = full;
      return true;                           // keep scanning: detect ambiguity
    });
    return (hit && !ambiguous) ? hit : undefined;
  }

  //  Inflate + delta-chase one record at (fileIdx, offset) → Uint8Array.
  //  git.pack.resolve handles the full OFS/REF chase into a Buf; we size
  //  the out buffer to the record's declared size with slack and grow if
  //  resolve reports a larger result.
  //  GIT-021: ONE reader-wide resolve buffer, PACK-003 slack.  `pk.size` on an
  //  ofs-delta is the DELTA's own size, not the resolved object's, so a fresh
  //  `io.buf(sz+64)` per read NOROOM'd and re-ran the WHOLE chase up to 24x:
  //  a linux-v3.0 checkout burned 64 646 retries / 848 MB of throwaway buffers
  //  over 36 783 leaves (anon RSS 1.8 GB).  The buffer only ever grows, so a
  //  warmed reader resolves on the first try.
  let rbuf = null;
  function readRecord(fileIdx, offset) {
    const pk = packAt(fileIdx);
    if (!pk) return undefined;
    //  PTR-010: a rejected offset UNPOSITIONS the cursor — resolve would then
    //  fail 24 times over, growing the ladder for nothing.
    if (!pk.seek(offset)) return undefined;
    let cap = (pk.size || 0) * 4 + 256;             // PACK-003 delta slack
    if (rbuf && rbuf.cap > cap) cap = rbuf.cap;     // never shrink
    for (let tries = 0; tries < 12; tries++) {
      if (!rbuf || rbuf.cap < cap) rbuf = io.buf(cap);
      rbuf.reset();
      try {
        pk.seek(offset);
        pk.resolve(rbuf);
        return { bytes: rbuf.data().slice(), type: pk.type };
      } catch (e) {
        //  Only a sizing failure is worth a bigger buffer; a ref-delta or a
        //  corrupt record fails the same way at every cap.
        if (("" + e).indexOf("NOROOM") < 0) throw e;
        cap *= 4;
        if (cap > (1 << 30)) throw e;
      }
    }
    return undefined;
  }

  const reader = {
    storePath: storePath,
    project: project,
    shard: shard,

    //  getObject(sha) → { type:"blob"|"tree"|"commit"|"tag", bytes }.
    getObject: function (sha) {
      const loc = locate(sha);
      if (!loc) return undefined;
      const rec = readRecord(loc.fileIdx, loc.offset);
      if (!rec) return undefined;
      //  resolve gives the resolved record's own type string for a raw
      //  object; for a delta the pack reports the base's type via the
      //  index key (loc.type).  Prefer the index type (canonical).
      const tname = TYPE_NAME[loc.type] || rec.type;
      stats.bump("obj"); stats.bump(tname);   // CFOLD-001 repro counters
      return { type: tname, bytes: rec.bytes };
    },

    //  --- refs ULOG --------------------------------------------------
    //  Rows: `<ts>\t<verb>\t<from-uri>#<sha>`.  key = URI up to '#'
    //  (`?`, `?heads/x`, `//host?heads/x`); val = fragment (the sha).
    //  Local tip = host-less key; remote = key with an authority/host.

    //  Drain refs, returning latest-per-key rows newest-first ordering.
    _refs: null,
    refs: function () {
      if (this._refs) return this._refs;
      this._refs = ulog.drain(join(shard, "refs"));
      return this._refs;
    },

    //  resolveRef('?' | '' | 'heads/main' | '<branch>') → sha | undefined.
    //  Trunk = key `?` (empty query, host-less).  Reverse-scan; latest
    //  matching, non-tombstone row wins.
    resolveRef: function (refOrBranch) {
      const rows = this.refs();
      let want = refOrBranch == null ? "" : String(refOrBranch);
      if (want === "?" ) want = "";
      if (want.length && want[0] === "?") want = want.slice(1);
      //  strip a leading `/project/` canonical prefix if present.
      for (let i = rows.length - 1; i >= 0; i--) {
        const u = rows[i].uri;
        const local = (u.authority === "" || u.authority == null);
        if (!local) continue;            // local tips only here
        const q = u.query || "";
        //  trunk: want empty AND row query empty.
        let match;
        if (want === "") match = (q === "");
        else match = (q === want || q === ("heads/" + want) ||
                      stripProj(q) === want);
        if (!match) continue;
        const sha = shaOf(u);
        if (sha && isFullSha(sha) && !isZeroSha(sha)) return sha;
        return undefined;   // tombstone (empty/zero) → absent
      }
      return undefined;
    },

    //  eachTip(cb): local branch tips (host-less rows), latest per key.
    eachTip: function (cb) {
      const rows = this.refs();
      const seen = {};
      for (let i = rows.length - 1; i >= 0; i--) {
        const u = rows[i].uri;
        const local = (u.authority === "" || u.authority == null);
        if (!local) continue;
        const key = (u.query || "") + "#";
        if (seen[key]) continue;
        seen[key] = 1;
        const sha = shaOf(u);
        if (!sha || !isFullSha(sha) || isZeroSha(sha)) continue;  // skip tombstones
        cb({ key: u.query || "?", branch: stripProj(u.query || ""),
             sha: sha, ts: rows[i].ts });
      }
    },

    //  eachRemote(cb): remote-tracking tips (rows carrying an authority).
    eachRemote: function (cb) {
      const rows = this.refs();
      const seen = {};
      for (let i = rows.length - 1; i >= 0; i--) {
        const u = rows[i].uri;
        const local = (u.authority === "" || u.authority == null);
        if (local) continue;
        const key = (u.authority || "") + (u.query || "") + "#";
        if (seen[key]) continue;
        seen[key] = 1;
        const sha = shaOf(u);
        if (!sha || !isFullSha(sha) || isZeroSha(sha)) continue;
        cb({ key: key, host: u.host || u.authority, query: u.query || "",
             //  GET-047: the re-fetchable origin URI (scheme+authority+path).
             remote: String(URI.make(u.scheme, u.authority, u.path, undefined, undefined)),
             sha: sha, ts: rows[i].ts });
      }
    },

    //  --- git object parsers ----------------------------------------
    readTree: function (sha) {
      const obj = this.getObject(sha);
      if (!obj || obj.type !== "tree") return undefined;
      const out = [];
      git.tree(obj.bytes, function (e) {
        out.push({ mode: e.mode, name: e.str, sha: e.sha });
      });
      return out;
    },

    //  TEST-004: peel an annotated tag down to the commit it names.  A
    //  `?tags/X` ref advertises the TAG object — the peeled `X^{}` line is
    //  dropped from the ref list (wire.js §336) — while everything
    //  downstream (tree, ancestry, diff, post) wants the commit.  A tag may
    //  name another tag, so loop; a non-tag sha comes back untouched.
    peel: function (sha) {
      for (let i = 0; i < 8; i++) {
        const obj = this.getObject(sha);
        if (!obj || obj.type !== "tag") return sha;
        //  `object <40hex>\n` is the first header line of a tag object.
        let head = "";
        const n = obj.bytes.length < 64 ? obj.bytes.length : 64;
        for (let j = 0; j < n; j++) head += String.fromCharCode(obj.bytes[j]);
        const m = /^object ([0-9a-f]{40})\n/.exec(head);
        if (!m) return sha;
        sha = m[1];
      }
      return sha;
    },

    commitTree: function (sha) {
      const obj = this.getObject(sha);
      if (!obj || obj.type !== "commit") return undefined;
      return git.parseCommit(obj.bytes).tree;
    },

    commitParents: function (sha) {
      const obj = this.getObject(sha);
      if (!obj || obj.type !== "commit") return undefined;
      return git.parseCommit(obj.bytes).parents;
    },

    parseCommit: function (sha) {
      const obj = this.getObject(sha);
      if (!obj || obj.type !== "commit") return undefined;
      return git.parseCommit(obj.bytes);
    },

    //  readTreeRecursive(treeSha, cb): walk the tree depth-first,
    //  calling cb({ path, mode, sha, kind }) per LEAF (file/exe/symlink/
    //  gitlink — not dirs).  Mirrors keeper/WALK.c::KEEPTreeULog leaf set.
    //  kind: "f" regular, "x" exec, "l" symlink, "s" submodule (gitlink).
    readTreeRecursive: function (treeSha, cb) {
      const self = this;
      function walk(sha, prefix) {
        const entries = self.readTree(sha);
        if (!entries) return;
        for (const e of entries) {
          const path = prefix ? (prefix + "/" + e.name) : e.name;
          const m = e.mode;
          if (m === 0o40000) { walk(e.sha, path); continue; }      // dir
          if (!safeRel(path)) throw "store: unsafe tree path " + path;  // JS-065
          if (m === 0o160000) { cb({ path: path, mode: m, sha: e.sha, kind: "s" }); continue; }
          if (m === 0o120000) { cb({ path: path, mode: m, sha: e.sha, kind: "l" }); continue; }
          if (m === 0o100755) { cb({ path: path, mode: m, sha: e.sha, kind: "x" }); continue; }
          cb({ path: path, mode: m, sha: e.sha, kind: "f" });       // 100644 etc.
        }
      }
      walk(treeSha, "");
    },

    //  descendPath(rootTreeSha, segments) -> { sha, mode, kind } | undefined.
    //  The single-path descender (the KEEPTreeDescend / proj_descend twin) the
    //  object views (tree:/sha1:/blob:/commit:/size:/type:, JAB-006..011) share:
    //  walk `segments` from `rootTreeSha` one '/'-segment at a time, resolving
    //  each through readTree.  A "."/""/"./" segment (the collapse rule) and an
    //  EMPTY segment list both return the root tree itself.  Returns the LEAF
    //  entry's { sha, mode, kind } — kind one of "tree"|"blob"|"exe"|"link"|
    //  "commit" (the mode-class names; "blob" = 100644, "exe" = 100755, "link" =
    //  120000, "commit" = 160000 gitlink, "tree" = 040000 dir).  undefined when a
    //  segment is absent (PROJNONE) OR an intermediate segment is not a tree
    //  (can't descend through a blob/gitlink — PROJFAIL at the caller).  Does NOT
    //  itself require a DIR leaf — the caller (tree:) enforces that on the result.
    descendPath: function (rootTreeSha, segments) {
      let cur = { sha: rootTreeSha, mode: 0o40000, kind: "tree" };
      const segs = (segments || []).filter(function (s) {
        return s !== "" && s !== "." ;   // "."/""/"./"-tail collapse to no-op
      });
      for (let i = 0; i < segs.length; i++) {
        //  Can only descend INTO a tree; a blob/gitlink mid-path has no entries.
        if (cur.kind !== "tree") return undefined;
        const ents = this.readTree(cur.sha);
        if (!ents) return undefined;
        let hit;
        for (const e of ents) if (e.name === segs[i]) { hit = e; break; }
        if (!hit) return undefined;                 // missing segment (PROJNONE)
        cur = { sha: hit.sha, mode: hit.mode, kind: modeKind(hit.mode) };
      }
      return cur;
    },

    //  resolveHexAny(prefix): the KEEPLookup short-hex twin (JAB-006) — any-object
    //  prefix resolve over the wh128 lane; ambiguous/miss → undefined.
    resolveHexAny: resolveHexAny,

    //  DOG-027: release the keeper.idx pup slot (jab's handle table holds 32);
    //  fan-out verbs (work) close every store they open.  Reads re-probe after.
    close: function () {
      if (diskIx) { try { diskIx.close(); } catch (e) {} }
      diskIx = undefined;
      const i = LIVE.indexOf(reader);
      if (i >= 0) LIVE.splice(i, 1);
    },

    //  expose for tests / verification
    _locate: locate,
    _index: index,
    _diskIndex: diskIndex,   // JS-056: the mmap'd keeper.idx runs | null
    _packs: packs,
    frameSha: frameSha
  };

  return reader;
}

//  git tree-entry mode -> the mode-class name (WALKu8sModeKind twin): the
//  octal mode bits classify a tree entry as a dir / blob / exec / symlink /
//  gitlink.  Shared by descendPath (above) and the tree: row mode/type map.
function modeKind(mode) {
  if (mode === 0o40000)  return "tree";
  if (mode === 0o160000) return "commit";
  if (mode === 0o120000) return "link";
  if (mode === 0o100755) return "exe";
  return "blob";   // 0o100644 and any other regular-file mode
}

//  SUBS-050: strip a leading `/<project>/` from a ref query → the branch KEY,
//  routed through the ONE branch codec (was a byte-identical hand-rolled twin
//  of wtlog.stripProject).
function stripProj(q) {
  return branchlib.key(branchlib.parse(q || "", ""));
}

//  Extract the sha from a refs row URI: fragment (`#<sha>` or `#?<sha>`),
//  else the query tail.
function shaOf(u) {
  let f = u.fragment || "";
  if (f && f[0] === "?") f = f.slice(1);
  if (isFullSha(f)) return f;
  //  Some rows pin the sha in the query as `/proj/branch/<sha>`.
  const q = u.query || "";
  const segs = q.split("/");
  const last = segs[segs.length - 1];
  if (isFullSha(last)) return last;
  return f || "";
}

//  --- refs ULOG WRITERS (folded in from refs.js, JS-048) ----------------
//  The write twin of the resolveRef/eachTip readers above.  A `refs` row is
//  a dog/ULOG row whose URI keys the ref (`?` trunk, `?<branch>`,
//  `//<host>?<branch>`) and pins a BARE 40-hex sha in the fragment
//  (`?<branch>#<sha>`).  Verbs: `post` (a local move), `delete` (tombstone,
//  sha = 40 zeros).  See keeper/REFS.h / REF.md.  CAS stays the caller's
//  job (POST does resolve-then-conditional-set); set/tombstone append
//  unconstrained (the reflog escape hatch).

const REFS = "refs";
const ZERO_SHA = "0".repeat(40);

//  ref-key (`""` trunk, `"feat"`, `"heads/main"`) → URI key prefix.  A
//  leading `?` is tolerated and shed so callers may pass either form.
//  URI-013: compose the `?<k>#<sha>` ref URI via URI.make (query=k, fragment=sha;
//  no scheme/authority/path).  A present-empty query ("") renders the bare `?`
//  (trunk key `?#<sha>`) — byte-identical to the old `"?"+k+"#"+sha` (abc/URI.c
//  URIutf8Feed / js/test/uri.js).  Fall back to the concat if make ever returns falsy.
function keyURI(key, sha) {
  let k = key == null ? "" : String(key);
  if (k && k[0] === "?") k = k.slice(1);
  return URI.make(undefined, undefined, undefined, k, String(sha)) ||
         ("?" + k + "#" + sha);
}

//  refKey(branch, sha) → the `?<branch>#<sha>` refs-log key URI, the shared
//  builder the ~39 open-coded `?b#sha` concats across the verbs/core adopt.
//  Same shape/semantics as keyURI (a leading `?` on `branch` is shed; a `""`
//  branch is the trunk `?#<sha>`); single-sourced on URI.make.
function refKey(branch, sha) {
  return keyURI(branch, sha);
}

//  createShard(shard[, key]): mkdir the shard dir (with parents) and seed an
//  empty refs log if absent.  `key` is accepted for signature symmetry with
//  the verbs but unused — a shard seeds EMPTY (no tip) so resolveRef reports
//  the key absent until the first set (matches a fresh keeper store).
function createShard(shard, key) {
  io.mkdir(shard);
  const path = join(shard, REFS);
  if (!exists(path)) ulog.write(path, []);
}

//  set(shard, key, sha): append a `post` row pinning the bare-40hex `sha`.
function set(shard, key, sha) {
  ensureShard(shard);
  ulog.append(join(shard, REFS), [{ verb: "post", uri: keyURI(key, sha) }]);
}

//  tombstone(shard, key): append a `delete` row (zero sha) marking the key
//  absent.  resolveRef collapses the zero sha to undefined.
function tombstone(shard, key) {
  ensureShard(shard);
  ulog.append(join(shard, REFS), [{ verb: "delete", uri: keyURI(key, ZERO_SHA) }]);
}

function exists(p) { try { io.stat(p); return true; } catch (e) { return false; } }

//  A set/tombstone on a shard with no refs file yet seeds one first, so the
//  append has a tail to read (and the shard dir exists).
function ensureShard(shard) {
  const path = join(shard, REFS);
  if (!exists(path)) createShard(shard);
}

module.exports = { open: open, openShard: openShard, closeAll: closeAll,
                   shardDir: shardDir, defaultTitle: defaultTitle, frameSha: frameSha,
                   hashlet60FromBytes: hashlet60FromBytes,
                   TYPE_NAME: TYPE_NAME, NAME_TYPE: NAME_TYPE,
                   createShard: createShard, set: set, tombstone: tombstone,
                   keyURI: keyURI, refKey: refKey,
                   ZERO_SHA: ZERO_SHA, modeKind: modeKind };
