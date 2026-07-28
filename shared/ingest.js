//  ingest.js — land a received wire packfile into a fresh local keeper store
//  (JS-040).  Pure JS over io fs leaves + the ULOG writer.  A keeper store is
//  just `NNNNN.keeper` pack-logs + a `refs` ULOG (no prebuilt index needed —
//  native scans on open; verified empirically), so a full-clone pack lands by
//  writing it (minus its 20-byte git trailer) as `0000000001.keeper` and
//  recording the tip in `refs`.  Mirrors keeper/UNPK + KEEPIngestFile, minus
//  the OFS re-encode (a verbatim full-clone pack already IS an OFS-only log).
//  No keeper dog linked.
//
//  clone(packBytes, beDir, proj, tip, remoteUri):
//    beDir     <wt>/.be  (created as a DIR — a PRIMARY, own-store worktree)
//    proj      project shard name
//    tip       40-hex tip sha (from the wire advert)
//    remoteUri the origin, recorded as a remote-tracking refs row
//
//  Thin packs (REF_DELTA, incremental fetch) need the OFS re-encode +
//  REF-base resolve — a follow-up; a full clone ships OFS-only verbatim.

"use strict";

const join = require("./util/path.js").join;   // JSQUE-016: path.js -> shared/util/
const ulog = require("./ulog.js");
const idxmaint = require("./idxmaint.js");     // JS-116: run-lifecycle upkeep
const shalib = require("./util/sha.js");       // JS-117: tail-walk git-sha

//  JS-117: tail-append cap ([/wiki/PackLog] many packs per log) — a new log
//  opens only past this; 64 MiB bounds the per-log mmap, batches ~10^4 posts.
const KEEP_LOG_MAX = 64 * 1024 * 1024;

const NAME_TYPE = { commit: 1, tree: 2, blob: 3, tag: 4 };
const TYPE_NAME = { 1: "commit", 2: "tree", 3: "blob", 4: "tag" };
//  Best-effort git type from resolved bytes (the store.js twin) — a delta
//  record's own type is the base's, so classify by the object header shape.
function inferType(bytes) {
  const n = Math.min(64, bytes.length);
  let head = "";
  for (let i = 0; i < n; i++) head += String.fromCharCode(bytes[i]);
  if (head.startsWith("tree ") && head.indexOf("\n") > 0) return 1;
  if (head.startsWith("object ")) return 4;
  if (/^[0-7]{5,6} /.test(head)) return 2;
  return 3;
}

//  PACK-003: census EVERY parseable record offset from the log's start.
//  pk.next() extent-walks records but STALLS at anything that is not a
//  record: a verbatim embedded PACK header (GET-046 keeper-served logs land
//  whole store logs) is skipped and the walk resumes behind it; anything
//  else — a torn append's zero tail (the JAB-008 crash window), a mid-log
//  corrupt region — ends the census.  Offsets, not entries: resolving is
//  resolveEntries()'s job.
function walkOffsets(pk) {
  const offs = [];
  pk.rewind();
  for (;;) {
    while (pk.next()) offs.push(pk.offset);
    const at = pk._read;              // stall: the last record's end offset
    if (at + 12 > pk.byteLength) break;
    if (pk[at] !== 0x50 || pk[at + 1] !== 0x41 ||       // "PACK" v2 magic —
        pk[at + 2] !== 0x43 || pk[at + 3] !== 0x4b ||   // anything else is
        pk[at + 4] !== 0 || pk[at + 5] !== 0 ||         // torn/corrupt: stop
        pk[at + 6] !== 0 || pk[at + 7] !== 2) break;
    if (!pk.seek(at + 12)) break;     // a header with no record behind: stop
    offs.push(at + 12);
  }
  return offs;
}

//  Resolve each record at `offs` → wh128 { key, off } pairs per object
//  (unresolvable/ref-delta records are skipped — pure-JS OFS-only limits).
//  PACK-003: an ofs-delta record's pk.size is the DELTA's own size, not the
//  resolved object's — a fixed out buf made resolve NOROOM and silently DROP
//  the record (11269 of beagle's 28986 salvageable records); grow and retry
//  instead (the loop.js/_grow idiom), give up only on a non-NOROOM error.
const RESOLVE_CAP = 1 << 28;
function resolveEntries(pk, offs) {
  const out = [];
  for (const off of offs) {
    pk.seek(off);
    if (pk.type === "ref-delta") continue;      // unresolvable in pure JS
    let bytes = null, tname;
    for (let cap = (pk.size || 0) * 4 + 256; cap <= RESOLVE_CAP; cap *= 4) {
      try {
        const b = io.buf(cap);
        pk.seek(off); pk.resolve(b); bytes = b.data(); tname = pk.type;
      } catch (e) { if (("" + e).includes("NOROOM")) continue; }
      break;
    }
    if (bytes === null) continue;
    const type = NAME_TYPE[tname] || inferType(bytes);
    const h = shalib.hashlet60FromBytes(
        hex.decode(shalib.frameSha(TYPE_NAME[type], bytes)));
    out.push({ key: (h << 4n) | BigInt(type), off: off });
  }
  return out;
}

//  JS-117: walk a log's records PAST `afterOff` (pk.scan is header-count-
//  driven, blind to appended packs) → wh128 { key, off } pairs per object.
//  PACK-003: rides the walkOffsets census, so a verbatim embedded pack's
//  records (behind its mid-log PACK header) are indexed too, not silently
//  dropped at the header stall.
function walkTail(pk, afterOff) {
  const offs = [];
  for (const off of walkOffsets(pk)) if (off > afterOff) offs.push(off);
  return resolveEntries(pk, offs);
}

function writeBytes(path, u8) {
  const fd = io.open(path, "c");
  try {
    try { io.resize(fd, 0); } catch (e) {}
    const b = io.buf(u8.length + 8);
    b.feed(u8);
    io.writeAll(fd, b);
  } finally { io.close(fd); }
}

//  Strip a git packfile's trailing 20-byte SHA-1 → the keeper pack-log bytes
//  (PACK header + records; the log's extent is its byte length, no trailer).
function packLogBytes(packBytes) {
  if (packBytes.length < 32 || utf8.Decode(packBytes.subarray(0, 4)) !== "PACK")
    throw "ingest: not a PACK stream (" + packBytes.length + " bytes)";
  return packBytes.subarray(0, packBytes.length - 20);
}

//  GET-044: is this pack source a STREAMED tmp file ({packFile,packLen}) rather
//  than an in-memory Uint8Array?  Callers pass either shape through clone/add/land.
function isFileSrc(s) { return !!(s && s.packFile); }

//  KEEP-006/JAB-020: the fetch was already repacked INTO the shard by
//  `git.pack` — the keeper logs and their `.keeper.idx` runs are on disk and
//  nothing is left to land.  clone/add/land then only write the refs rows.
function isRepacked(s) { return !!(s && s.repacked); }

//  KEEP-006: the `NNNNNNNNNN.keeper` id the repack should START at — the
//  highest log still under the 2 GiB cap, which it APPENDS behind (a log holds
//  many packs, exactly as JS-117's tail-append did), else the next fresh id.
//  The threshold is the REPACK cap, not the 64 MiB legacy tail-append one:
//  the repack rotates onward by itself, and 64 MiB logs would need ~100 of
//  them for a kernel-scale fetch (REPACK_MAX_LOGS is 64).
function repackLogId(shard) {
  let maxN = 0, maxNm = null, maxSz = 0;
  try {
    for (const nm of io.readdir(shard)) {
      const m = /^(\d{10})\.keeper$/.exec(nm);
      if (m) { const v = parseInt(m[1], 10); if (v > maxN) { maxN = v; maxNm = nm; } }
    }
  } catch (e) {}
  if (maxNm) { try { maxSz = io.stat(join(shard, maxNm)).size; } catch (e) {} }
  return (maxNm && maxSz < MMAP_CAP) ? fileIdOf(maxNm) : maxN + 1;
}

//  GET-044: jab's mmap bindings are 31-bit — io.mmap returns a WRONG length and
//  git.pack.mmap ABORTS the process past 2^31-1 bytes (probed 2026-07-14).
const MMAP_CAP = 2147483647;

//  GET-044: mmap a streamed tmp pack file and verify its git 20-byte sha1
//  trailer == sha1(body) — zero-copy (no heap alloc).  Only for sources the
//  wire did NOT already stream-verify; refuses past MMAP_CAP (the map would
//  silently truncate).  Returns the mmap Buf; throws on bad magic / trailer.
function mapAndVerify(packFile, packLen) {
  if (packLen < 32) throw "ingest: not a PACK stream (" + packLen + " bytes)";
  if (packLen > MMAP_CAP)
    throw "ingest: pack " + packLen + " bytes exceeds the jab mmap cap (" +
          MMAP_CAP + ") — cannot map-verify (stream-verify it instead)";
  const buf = io.mmap(packFile, "r");
  const u = buf.data();
  if (utf8.Decode(u.subarray(0, 4)) !== "PACK")
    throw "ingest: not a PACK stream (bad magic)";
  const got = hex.encode(sha1(u.subarray(0, packLen - 20)));
  const want = hex.encode(u.subarray(packLen - 20, packLen));
  if (got !== want)
    throw "ingest: pack sha1 trailer mismatch (got " + got + " want " + want + ")";
  return buf;
}

//  GET-044: verify a streamed tmp pack (skip when the wire stream-verified it
//  already), then atomically RENAME it into the keeper-log path and drop the
//  20-byte trailer (io.resize).  tmp + dest share the shard's FS, so the
//  rename is atomic.  On any failure the tmp file is unlinked (store untouched).
function verifyAndPlace(src, logPath) {
  if (!src.verified) {
    try { mapAndVerify(src.packFile, src.packLen); }
    catch (e) { try { io.unlink(src.packFile); } catch (e2) {} throw e; }
  }
  io.rename(src.packFile, logPath);
  const fd = io.open(logPath, "rw");
  try { io.resize(fd, src.packLen - 20); } finally { io.close(fd); }
}

//  Build the native `<ron64>.keeper.idx` for one keeper-log: a sorted wh128
//  run of a PACK-summary entry + one entry per object.  Native keeper reads
//  this prebuilt index (it does NOT scan a bare `.keeper`), so a clone is
//  invisible (`unk`) without it.  Entry formats (keeper/KEEP.h):
//    object: key = WHIFFKeyPack(type, hashlet60)         (from pack.scan)
//            val = (offset[40] << 24) | (file_id[20] << 4) | flags[4]=1
//    PACK:   key = ((first_off<<20 | file_id) << 4) | 0xF
//            val = (count << 32) | (logBytes - 12)
function buildIndex(shard, logName, fileId) {
  //  GET-044: past MMAP_CAP git.pack.mmap ABORTS the process (31-bit binding);
  //  refuse cleanly — the landed log is durable, indexing needs a jab-side fix.
  const lsz = io.stat(join(shard, logName)).size;
  if (lsz > MMAP_CAP)
    throw "ingest: " + logName + " (" + lsz + " bytes) landed OK but exceeds " +
          "the jab 2^31-1 mmap cap — index/checkout need a jab-side windowed " +
          "mmap (GET-044 follow-up); the pack is preserved";
  const pk = git.pack.mmap(join(shard, logName), "r");
  pk.buffer.watermark = pk.byteLength;
  const cnt = pk.count || 0;
  //  PACK-003: the native scan is single-pack and header-count-driven; a log
  //  whose header count exceeds its parseable records — a torn append's zero
  //  tail (JAB-008 class: resize survived, record bytes lost), a mid-log
  //  corrupt region, an embedded PACK header in the count's way — makes it
  //  throw its generic "scan (out full? corrupt?)".  The log is durable
  //  data: fall back to the extent-walk census and index every record that
  //  still resolves.  The run bookmarks the FULL byte extent either way, so
  //  idxmaint stops re-attempting (and re-warning) on every open; the lost
  //  records stay miss until a re-fetch lands them again.
  let ents = null, tail;
  try { ents = pk.scan(io.buf(cnt * 16 + 256)); }   // key,val,... (val = offset)
  catch (e) {
    tail = resolveEntries(pk, walkOffsets(pk));
    io.log("ingest: " + logName + ": native scan failed (" + e + "); salvaged " +
           tail.length + " of " + cnt + " header-counted records\n");
  }
  const n = ents ? ents.length / 2 : 0;
  if (ents) {
    //  JS-117: a rebuilt multi-pack log must not lose its appended tail —
    //  walk the records past scan's (header-count-driven) coverage and
    //  index them.
    let maxOff = -1;
    for (let i = 1; i < ents.length; i += 2) {
      const o = Number(ents[i] & 0xffffffffffn);
      if (o > maxOff) maxOff = o;
    }
    tail = walkTail(pk, maxOff);
  }
  const mem = abc.ram("HEAPwh128", n + tail.length + 8);
  const fid = BigInt(fileId), FIRST = 12n, PACK = 0xfn;
  mem.push((((FIRST << 20n) | fid) << 4n) | PACK,
           (BigInt(n + tail.length) << 32n) | (BigInt(pk.byteLength) - 12n));
  for (let i = 0; i < n; i++) {
    const off = ents[i * 2 + 1] & 0xffffffffffn;
    mem.push(ents[i * 2], (off << 24n) | (fid << 4n) | 1n);
  }
  for (const t of tail)
    mem.push(t.key, (BigInt(t.off) << 24n) | (fid << 4n) | 1n);
  mem.sort();
  //  JS-116: collision-safe ron60 name — a pinned clock repeats ron.now(),
  //  and overwriting an existing run silently drops its coverage.
  const path = join(shard, idxmaint.freshRunName(shard));
  const out = abc.book("HEAPwh128", path, mem.size);
  abc.merge([mem], out);
  abc.close(out);
}

//  JS-117: append pack RECORDS at the log tail — grow the file, copy the bytes
//  into the mapped tail, msync.  Existing bytes are never touched; returns the
//  pre-append byte length (the new pack's first_off).
function appendRecords(path, records) {
  const fd = io.open(path, "rw");
  let base;
  try { base = io.size(fd); io.resize(fd, base + records.length); }
  finally { io.close(fd); }
  const map = io._mmap(path, "rw");
  map.set(records, base);
  io._msync(map);
  return base;
}

//  JS-117: index ONE tail-appended pack as a fresh ron60 run — the 0xF bookmark
//  (first_off, count<<32|recLen) + object rows with ABSOLUTE offsets rebased
//  from the standalone pack view `pk` (offsets from 12) to firstOff.  pk.scan
//  can't see a multi-pack log, so we scan the standalone pack the writer holds.
function indexAppended(shard, fileId, firstOff, pk, recLen) {
  const cnt = pk.count || 0;
  const buf = io.buf(cnt * 16 + 256);
  const ents = pk.scan(buf);
  const n = ents.length / 2;
  //  GET-046: the buildIndex JS-117 twin — pk.scan is header-count-driven,
  //  and a keeper-served store log arrives VERBATIM (the first embedded
  //  pack's header + EVERY appended record behind it), so the header count
  //  undercounts and scan misses the tail objects (the update-fetch repro:
  //  the new tip lands in the log but stays unindexed → "tip has no tree").
  //  Walk the records past scan's coverage and index them too.
  let maxOff = -1;
  for (let i = 1; i < ents.length; i += 2) {
    const o = Number(ents[i] & 0xffffffffffn);
    if (o > maxOff) maxOff = o;
  }
  const tail = walkTail(pk, maxOff);
  const mem = abc.ram("HEAPwh128", n + tail.length + 8);
  const fid = BigInt(fileId), PACK = 0xfn, delta = BigInt(firstOff) - 12n;
  mem.push((((BigInt(firstOff) << 20n) | fid) << 4n) | PACK,
           (BigInt(n + tail.length) << 32n) | BigInt(recLen));
  for (let i = 0; i < n; i++) {
    const off = (ents[i * 2 + 1] & 0xffffffffffn) + delta;
    mem.push(ents[i * 2], (off << 24n) | (fid << 4n) | 1n);
  }
  for (const t of tail)
    mem.push(t.key, ((BigInt(t.off) + delta) << 24n) | (fid << 4n) | 1n);
  mem.sort();
  const path = join(shard, idxmaint.freshRunName(shard));   // JS-116: no clobber
  const out = abc.book("HEAPwh128", path, mem.size);
  abc.merge([mem], out);
  abc.close(out);
}

//  JS-117: pick the log to write.  The highest-numbered .keeper under the
//  threshold is appended to (append=true, its own file_id); else the next seq
//  opens a fresh file (append=false) — also the empty-shard and over-cap cases.
function appendTarget(shard) {
  let maxN = 0, maxNm = null, maxSz = 0;
  try {
    for (const nm of io.readdir(shard)) {
      const m = /^(\d{10})\.keeper$/.exec(nm);
      if (m) { const v = parseInt(m[1], 10); if (v > maxN) { maxN = v; maxNm = nm; } }
    }
  } catch (e) {}
  if (maxNm) { try { maxSz = io.stat(join(shard, maxNm)).size; } catch (e) {} }
  if (maxNm && maxSz < KEEP_LOG_MAX)
    return { logName: maxNm, fileId: fileIdOf(maxNm), append: true };
  return { logName: logName(maxN + 1), fileId: maxN + 1, append: false };
}

//  file_id = the keeper-log's 10-digit sequence prefix (0000000001 → 1).
function fileIdOf(logName) { return parseInt(logName, 10) || 1; }

//  PATCH-011: ONE combined `.keeper.idx` run covering EVERY pack-log in the
//  shard.  The rolling per-log runs may miss older logs (the TEST-003 quirk),
//  blinding the reader to the wt's OWN history right when patch needs the
//  ours/fork trees — a fetch that lands objects must leave the WHOLE shard
//  readable.  Same entry formats as buildIndex (keeper/KEEP.h).
function reindexShard(shard) {
  const logs = [];
  try {
    for (const nm of io.readdir(shard))
      if (/^\d{10}\.keeper$/.test(nm)) logs.push(nm);
  } catch (e) {}
  logs.sort();
  const scans = [];
  let total = 0;
  for (const nm of logs) {
    //  GET-044: skip a log past the 31-bit mmap cap (native abort otherwise).
    let lsz = 0; try { lsz = io.stat(join(shard, nm)).size; } catch (e) {}
    if (lsz > MMAP_CAP) continue;
    const pk = git.pack.mmap(join(shard, nm), "r");
    pk.buffer.watermark = pk.byteLength;
    const buf = io.buf((pk.count || 0) * 16 + 256);
    let ents; try { ents = pk.scan(buf); } catch (e) { ents = null; }
    if (!ents) continue;                 // thin/odd log — reader walk-fallback
    scans.push({ nm: nm, pk: pk, ents: ents });
    total += ents.length / 2 + 1;
  }
  if (!scans.length) return;
  const mem = abc.ram("HEAPwh128", total + 8);
  const FIRST = 12n, PACK = 0xfn;
  for (const s of scans) {
    const fid = BigInt(fileIdOf(s.nm));
    mem.push((((FIRST << 20n) | fid) << 4n) | PACK,
             (BigInt(s.ents.length / 2) << 32n) | (BigInt(s.pk.byteLength) - 12n));
    for (let i = 0; i * 2 < s.ents.length; i++) {
      const off = s.ents[i * 2 + 1] & 0xffffffffffn;
      mem.push(s.ents[i * 2], (off << 24n) | (fid << 4n) | 1n);
    }
  }
  mem.sort();
  const path = join(shard, idxmaint.freshRunName(shard));   // JS-116: no clobber
  const out = abc.book("HEAPwh128", path, mem.size);
  abc.merge([mem], out);
  abc.close(out);
}

function clone(pack, beDir, proj, tip, remoteUri) {
  try { io.mkdir(beDir); } catch (e) {}
  const shard = join(beDir, proj);
  try { io.mkdir(shard); } catch (e) {}
  const logPath = join(shard, "0000000001.keeper");
  //  KEEP-006: a repacked fetch is already logged + indexed in the shard; a
  //  GET-044 streamed tmp file is verified + renamed into place (bounded RSS);
  //  an in-memory pack keeps the legacy write.
  if (!isRepacked(pack)) {
    if (isFileSrc(pack)) verifyAndPlace(pack, logPath);
    else writeBytes(logPath, packLogBytes(pack));
    buildIndex(shard, "0000000001.keeper", 1);
  }
  //  refs: the origin remote-tracking row + the local trunk tip (`post ?#`),
  //  the row keeper.resolveRef('') matches.  Remote URI query stripped to `?`.
  //  JS-073: the crash-safe native ULOG writer (temp+rename), not in-place.
  //  URI-013: the `origin` row is LEFT a hand-compose — the `.replace(/\?.*/,"?")`
  //  keeps the `?`-slot PRESENT-BUT-EMPTY ([URI-009] slot-presence, un-routable
  //  until the binding exposes presence), and a `uri._parse(remoteUri)` would
  //  THROW on an scp-style git remote (`git@host:owner/repo.git`) where the old
  //  concat never throws.  The local trunk `?#<tip>` row is the clean refKey shape.
  const origin = remoteUri.replace(/\?.*/, "?");
  ulog.write(join(shard, "refs"), [
    { verb: "get",  uri: origin + "#" + tip },
    { verb: "post", uri: URI.make(undefined, undefined, undefined, "", tip) }
  ]);
}

//  Pad a positive integer to the 10-digit `NNNNNNNNNN.keeper` log name.
function logName(n) {
  let s = "" + n;
  while (s.length < 10) s = "0" + s;
  return s + ".keeper";
}

//  PATCH-011: land a pack into an EXISTING shard as the next-numbered pack-log,
//  OBJECTS ONLY — no refs append (patch's fetch leg must not move local tips).
function land(pack, shard) {
  //  KEEP-006: git.pack already wrote the logs AND re-laddered the index runs.
  if (isRepacked(pack)) return;
  let tgt = appendTarget(shard);
  const fromFile = isFileSrc(pack);
  //  GET-044: a streamed pack past MMAP_CAP cannot ride the append path (the
  //  tmp mmap would truncate) — land it as its own fresh log via rename.
  if (fromFile && tgt.append && pack.packLen > MMAP_CAP)
    tgt = { logName: logName(fileIdOf(tgt.logName) + 1),
            fileId: fileIdOf(tgt.logName) + 1, append: false };
  if (!tgt.append) {                       // JS-117: fresh file (empty/over-cap)
    const logPath = join(shard, tgt.logName);
    //  GET-044: streamed file verified + renamed; in-memory pack written.
    if (fromFile) verifyAndPlace(pack, logPath);
    else writeBytes(logPath, packLogBytes(pack));
    buildIndex(shard, tgt.logName, tgt.fileId);
  } else {
    //  JS-117: append this pack's records (strip PACK header + trailer) to the
    //  tail; crash order: records+sync THEN idx run — a torn tail is dead.
    //  GET-044: a streamed source mmaps the tmp file (records are a zero-copy
    //  subarray — no heap pack); an in-memory source keeps the .slice() copy.
    let recs, records, tmpFile = null;
    if (fromFile) {
      //  GET-044: abort (bad trailer) must unlink the tmp — store untouched.
      //  A wire-verified source skips the re-hash but still maps for the copy.
      let map;
      try {
        map = pack.verified ? io.mmap(pack.packFile, "r")
                            : mapAndVerify(pack.packFile, pack.packLen);
      } catch (e) { try { io.unlink(pack.packFile); } catch (e2) {} throw e; }
      recs = map.data().subarray(0, pack.packLen - 20);   // [PACK hdr | records]
      records = recs.subarray(12);                        // zero-copy
      tmpFile = pack.packFile;
    } else {
      recs = packLogBytes(pack);           // [PACK hdr | records]
      records = recs.subarray(12).slice();
    }
    const firstOff = appendRecords(join(shard, tgt.logName), records);
    const view = git.pack.over(recs);
    view.buffer.watermark = recs.length;
    indexAppended(shard, tgt.fileId, firstOff, view, records.length);
    if (tmpFile) try { io.unlink(tmpFile); } catch (e) {}
  }
  idxmaint.compactAfterAdd(shard);   // JS-116: restore the 1/8 run ladder
}

//  add(): land another full pack into an EXISTING shard as the next-numbered
//  pack-log, and append the new tip to the shard's refs (remote-track + the
//  local `post ?#` trunk row).  Used by the remote re-get (update) path.
function add(pack, shard, remoteUri, tip) {
  land(pack, shard);   // PATCH-011: the shared objects-only landing core (GET-044: file|mem)
  //  JS-073: append the new tip rows via ulog.append (native in-place booked
  //  append) — survivors keep their ORIGINAL ts; only the new rows get a stamp.
  //  URI-013: `origin` row LEFT hand-composed ([URI-009] present-empty `?` +
  //  scp-remote parse-throw risk — see clone()); the `?#<tip>` trunk row routed.
  const origin = remoteUri.replace(/\?.*/, "?");
  ulog.append(join(shard, "refs"), [
    { verb: "get",  uri: origin + "#" + tip },
    { verb: "post", uri: URI.make(undefined, undefined, undefined, "", tip) }
  ]);
}

//  GIT-016: after a successful push, record the pushed ref at its new tip as a
//  remote-tracking refs row (the SAME `{verb:"get", uri: <authority>?#tip}`
//  shape clone/add write, so store.eachRemote picks it up).  `shard` = the
//  project shard dir; `remoteUri` the raw push target; `tip` the new 40-hex sha.
function saveRemoteRef(shard, remoteUri, tip) {
  //  JS-073: in-place native append preserves every survivor's ts; no re-drain,
  //  no restamp (the old writeUlog re-fed rows with no ts, bumping them to now).
  //  URI-013: `origin` row LEFT hand-composed ([URI-009] present-empty `?` +
  //  scp-remote parse-throw risk — see clone()).
  const origin = remoteUri.replace(/\?.*/, "?");
  ulog.append(join(shard, "refs"), [{ verb: "get", uri: origin + "#" + tip }]);
}

module.exports = { clone, add, land, reindexShard, buildIndex, writeBytes,
                   packLogBytes, logName, fileIdOf, saveRemoteRef, repackLogId,
                   KEEP_LOG_MAX, MMAP_CAP, appendRecords, indexAppended,
                   appendTarget };
