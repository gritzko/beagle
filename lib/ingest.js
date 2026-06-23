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

const join = require("./path.js").join;
const ulog = require("./ulog.js");

function writeBytes(path, u8) {
  const fd = io.open(path, "c");
  try {
    try { io.resize(fd, 0); } catch (e) {}
    const b = io.buf(u8.length + 8);
    b.feed(u8);
    io.writeAll(fd, b);
  } finally { io.close(fd); }
}

//  Build a fresh ULOG in RAM, feed rows, write its DATA region to `path`.
function writeUlog(path, rows) {
  const log = abc.ram("ULOG", Math.max(1 << 16, rows.length * 256));
  for (const r of rows) log.feed(r.verb, r.uri);
  const n = Number(log.buffer.watermark);
  writeBytes(path, log.subarray(0, n));
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
  const path = join(shard, ron.encode(ron.now()) + ".keeper.idx");
  const out = abc.book("HEAPwh128", path, mem.size);
  abc.merge([mem], out);
  abc.close(out);
}

//  file_id = the keeper-log's 10-digit sequence prefix (0000000001 → 1).
function fileIdOf(logName) { return parseInt(logName, 10) || 1; }

function clone(packBytes, beDir, proj, tip, remoteUri) {
  try { io.mkdir(beDir); } catch (e) {}
  const shard = join(beDir, proj);
  try { io.mkdir(shard); } catch (e) {}
  writeBytes(join(shard, "0000000001.keeper"), packLogBytes(packBytes));
  buildIndex(shard, "0000000001.keeper", 1);
  //  refs: the origin remote-tracking row + the local trunk tip (`post ?#`),
  //  the row keeper.resolveRef('') matches.  Remote URI query stripped to `?`.
  const origin = remoteUri.replace(/\?.*/, "?");
  writeUlog(join(shard, "refs"), [
    { verb: "get",  uri: origin + "#" + tip },
    { verb: "post", uri: "?#" + tip }
  ]);
}

//  Pad a positive integer to the 10-digit `NNNNNNNNNN.keeper` log name.
function logName(n) {
  let s = "" + n;
  while (s.length < 10) s = "0" + s;
  return s + ".keeper";
}

//  add(): land another full pack into an EXISTING shard as the next-numbered
//  pack-log, and append the new tip to the shard's refs (remote-track + the
//  local `post ?#` trunk row).  Used by the remote re-get (update) path.
function add(packBytes, shard, remoteUri, tip) {
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
  //  Append (not rewrite) the refs ULOG with the new tip rows.
  const old = [];
  ulog.each(join(shard, "refs"),
            function (log) { old.push({ verb: log.verb, uri: log.uri }); });
  const origin = remoteUri.replace(/\?.*/, "?");
  writeUlog(join(shard, "refs"), old.concat([
    { verb: "get",  uri: origin + "#" + tip },
    { verb: "post", uri: "?#" + tip }
  ]));
}

module.exports = { clone, add, buildIndex, writeUlog, writeBytes,
                   packLogBytes, logName, fileIdOf };
