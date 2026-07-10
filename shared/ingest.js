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

//  Build the native `<ron64>.keeper.idx` for one keeper-log: a sorted wh128
//  run of a PACK-summary entry + one entry per object.  Native keeper reads
//  this prebuilt index (it does NOT scan a bare `.keeper`), so a clone is
//  invisible (`unk`) without it.  Entry formats (keeper/KEEP.h):
//    object: key = WHIFFKeyPack(type, hashlet60)         (from pack.scan)
//            val = (offset[40] << 24) | (file_id[20] << 4) | flags[4]=1
//    PACK:   key = ((first_off<<20 | file_id) << 4) | 0xF
//            val = (count << 32) | (logBytes - 12)
function buildIndex(shard, logName, fileId) {
  const pk = git.pack.mmap(join(shard, logName), "r");
  pk.buffer.watermark = pk.byteLength;
  const cnt = pk.count || 0;
  const buf = io.buf(cnt * 16 + 256);
  const ents = pk.scan(buf);                  // key,val,... (val = bare offset)
  const n = ents.length / 2;
  const mem = abc.ram("HEAPwh128", n + 8);
  const fid = BigInt(fileId), FIRST = 12n, PACK = 0xfn;
  mem.push((((FIRST << 20n) | fid) << 4n) | PACK,
           (BigInt(n) << 32n) | (BigInt(pk.byteLength) - 12n));
  for (let i = 0; i < n; i++) {
    const off = ents[i * 2 + 1] & 0xffffffffffn;
    mem.push(ents[i * 2], (off << 24n) | (fid << 4n) | 1n);
  }
  mem.sort();
  //  JS-116: collision-safe ron60 name — a pinned clock repeats ron.now(),
  //  and overwriting an existing run silently drops its coverage.
  const path = join(shard, idxmaint.freshRunName(shard));
  const out = abc.book("HEAPwh128", path, mem.size);
  abc.merge([mem], out);
  abc.close(out);
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

function clone(packBytes, beDir, proj, tip, remoteUri) {
  try { io.mkdir(beDir); } catch (e) {}
  const shard = join(beDir, proj);
  try { io.mkdir(shard); } catch (e) {}
  writeBytes(join(shard, "0000000001.keeper"), packLogBytes(packBytes));
  buildIndex(shard, "0000000001.keeper", 1);
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
function land(packBytes, shard) {
  let max = 0;
  try {
    for (const nm of io.readdir(shard)) {
      const m = /^(\d{10})\.keeper$/.exec(nm);
      if (m) { const v = parseInt(m[1], 10); if (v > max) max = v; }
    }
  } catch (e) {}
  const nm = logName(max + 1);
  writeBytes(join(shard, nm), packLogBytes(packBytes));
  buildIndex(shard, nm, fileIdOf(nm));
  idxmaint.compactAfterAdd(shard);   // JS-116: restore the 1/8 run ladder
}

//  add(): land another full pack into an EXISTING shard as the next-numbered
//  pack-log, and append the new tip to the shard's refs (remote-track + the
//  local `post ?#` trunk row).  Used by the remote re-get (update) path.
function add(packBytes, shard, remoteUri, tip) {
  land(packBytes, shard);   // PATCH-011: the shared objects-only landing core
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
                   packLogBytes, logName, fileIdOf, saveRemoteRef };
