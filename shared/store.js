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
//    project   = the shard name (`be.treeAt().project`); when empty the
//                single shard dir under <store>/.be is auto-detected.
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
//  FALLBACK (no `.keeper.idx` run in the shard): build the in-RAM index
//  ONCE — `git.pack.mmap` each `<shard>/NNNNN.keeper` and `pack.scan` it
//  into one `abc.index` wh128 lane keyed by the same `hashlet60<<4|type`
//  WHIFFKeyPack; that path's `val` is `fileIdx<<40 | offset` (NOT the
//  on-disk layout).  Both decode in `locate()` under one `onDisk` flag.

"use strict";

const pathlib = require("./util/path.js");   // JSQUE-016: util libs -> shared/util/
const safeRel = pathlib.safeRel;             // JS-065: worktree-confinement guard
const shalib = require("./util/sha.js");
const ulog = require("./ulog.js");
const branchlib = require("./branch.js");    // SUBS-050: the ONE branch codec
const idxmaint = require("./idxmaint.js");   // JS-116: keeper.idx run lifecycle
const join = pathlib.join;
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

//  Locate the shard dir `<store>/.be/<project>/`.  When `project` is
//  empty, auto-detect the single non-dotted subdir under `<store>/.be`.
//  A colocated primary store IS `<wt>/.be`; the project shard sits inside.
function shardDir(storePath, project) {
  //  URI-016: a trailing slash is legitimate — [/wiki/URI]'s record spells `store`
  //  as `/home/gritzko/.be/` — but io.readdir(dir + "/") returns every name with
  //  its FIRST CHARACTER EATEN (array form, L165), so a pack lists as
  //  `000000001.keeper` and mmap ENOENTs.  Normalise here, the one choke point.
  storePath = String(storePath).replace(/\/+$/, "") || "/";
  let beDir = join(storePath, BE);
  if (!isDir(beDir)) {
    //  storePath might already point at the .be dir (or be the shard).
    if (isDir(storePath)) beDir = storePath;
  }
  if (project) {
    const cand = join(beDir, project);
    if (isDir(cand)) return cand;
  }
  //  Auto-detect: the single project subdir (skip dotted, like .lock).
  let found;
  try {
    io.readdir(beDir, function (name) {
      if (name[name.length - 1] !== "/") return "more";
      const base = name.slice(0, -1);
      if (!base || base[0] === ".") return "more";
      found = join(beDir, base);   // last non-dotted subdir wins
      return "more";
    });
  } catch (e) { /* */ }
  //  If a project was named but the dir was not found, last resort: the
  //  beDir itself (single-shard flat store).
  return found || (project ? join(beDir, project) : beDir);
}

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
  //  `.keeper.idx` run, else null so locate() falls back to the in-RAM build.
  //  Memoized; `false` is the "no run, don't retry" sentinel.
  let diskIx = undefined;
  function diskIndex() {
    if (diskIx !== undefined) return diskIx || null;
    //  JS-116: run-lifecycle maintenance before the open — batch an overfull
    //  stack, persist stale tail runs, compact >32; RO stores degrade as-is.
    let n;
    try { n = idxmaint.maintain(shard); }
    catch (e) { n = idxmaint.listRuns(shard).length; }
    //  JS-116: still past the native 64-run query cap (RO + overfull) — the
    //  in-RAM fallback keeps the read verb alive where range() would throw.
    diskIx = (n > 0 && n <= idxmaint.RUN_CAP)
           ? abc.index("wh128", { dir: shard, ext: "keeper.idx" }) : false;
    return diskIx || null;
  }

  //  Build the in-memory wh128 object index once: scan every pack,
  //  remapping `val` to carry the FILE INDEX in the high bits so a hit
  //  knows which pack to seek.  `pack.scan` emits val = bare in-pack
  //  offset (low 40 bits); we OR in (fileIdx << 40).
  let idx = null;
  function index() {
    if (idx) return idx;
    const ix = abc.index("wh128", { mem: 1 << 16 });
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
    ix.flush();
    idx = ix;
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
  function readRecord(fileIdx, offset) {
    const pk = packAt(fileIdx);
    if (!pk) return undefined;
    pk.seek(offset);
    const sz = pk.size || 0;
    //  resolve writes the fully-reconstructed object bytes into the Buf.
    let cap = sz + 64;
    for (let tries = 0; tries < 24; tries++) {
      const out = io.buf(cap);
      try {
        pk.seek(offset);
        pk.resolve(out);
        return { bytes: out.data().slice(), type: pk.type };
      } catch (e) {
        cap *= 2;       // NOROOM → grow and retry
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

module.exports = { open: open, shardDir: shardDir, frameSha: frameSha,
                   hashlet60FromBytes: hashlet60FromBytes,
                   TYPE_NAME: TYPE_NAME, NAME_TYPE: NAME_TYPE,
                   createShard: createShard, set: set, tombstone: tombstone,
                   keyURI: keyURI, refKey: refKey,
                   ZERO_SHA: ZERO_SHA, modeKind: modeKind };
