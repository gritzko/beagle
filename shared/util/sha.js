//  sha.js — git-sha helpers shared by bin/*.js (JS-043).  Pure JS over the
//  JABC `sha1`/`hex` globals (js/codec.cpp); no C, no dog.  Consolidates
//  the `isFullSha` predicate (was copied verbatim into wtlog.js, keeper.js,
//  classify.js, dag.js, subs.js and wire.js) plus the keeper hashlet60 and
//  loose-object framing helpers.
//
//    isFullSha(s)             → true iff s is exactly 40 lowercase-hex chars
//    hashlet60FromBytes(sha)  → the MS 60 bits of a 20-byte sha (BigInt)
//    frameSha(typeName, body) → the git loose-object sha-1 of body (40-hex)
//    blobShaOfFd(fd, size)    → the same, read straight into a shared scratch

"use strict";

//  A full git object id: 40 lowercase-hex characters.
//  BE-065: charCodeAt, not s[i] — indexing a string allocated a one-char JS
//  string PER CHARACTER (11.2% of all engine allocations on the pager frame).
function isFullSha(s) {
  if (typeof s !== "string" || s.length !== 40) return false;
  for (let i = 0; i < 40; i++) {
    const c = s.charCodeAt(i);
    if (!((c >= 48 && c <= 57) || (c >= 97 && c <= 102))) return false;   // 0-9 a-f
  }
  return true;
}

//  The 40-zero tombstone sha (a deleted ref).  Native REFS collapses a
//  zero-sha row to "branch absent" (keeper/REFS.c); resolveRef/eachTip
//  must too, else a `delete` row would resolve to all-zeros.
function isZeroSha(s) {
  if (typeof s !== "string" || s.length !== 40) return false;
  for (let i = 0; i < 40; i++) if (s.charCodeAt(i) !== 48) return false;   // BE-065
  return true;
}

//  hashlet60: the MS 60 bits of the sha, big-endian.  Mirrors
//  dog/WHIFF.h::whiff_hashlet(s,15): big-endian u64 of the first 8 sha
//  bytes, then drop the low 4 bits → 60.
function hashlet60FromBytes(sha20) {
  let h = 0n;
  for (let i = 0; i < 8; i++) h = (h << 8n) | BigInt(sha20[i]);
  return h >> 4n;  // 64 - 60 = 4
}

//  Re-frame "<type> <size>\0" + content and sha1 it — git's loose-object
//  identity, the JS twin of dog/git PIDXObjSha / keeper KEEPObjSha.
//  `sha1` + `hex` are JABC globals (js/codec.cpp): sha1 → Uint8Array(20),
//  hex.encode → lowercase 40-hex string.
function frameSha(typeName, content) {
  const hdr = utf8.Encode(typeName + " " + content.length + "\0");
  const buf = io.buf(hdr.length + content.length);
  buf.feed(hdr);
  buf.feed(content);
  return hex.encode(sha1(buf.data()));
}

//  STATUS-021: the content sweep's blob sha — ONE module-level io.ram
//  scratch (anonymous mmap, pages fault in lazily), so a hashed file costs
//  no buffer alloc and ONE copy (the read).  256 MiB of virtual space.
const _SCRATCH_CAP = 1 << 28;
let _scratch = null;

//  blobShaOfFd(fd, size) → frameSha("blob", <the fd's `size` bytes>) without
//  materialising them: "blob <size>\0" is feedStr-ed into the scratch and the
//  file is read in right after it, sha1 over the no-copy data() view.  A short
//  read (the file shrank under us) re-frames the bytes we got — same answer as
//  the two-buffer path, which sized its header from what it actually read.
function blobShaOfFd(fd, size) {
  const hdr = "blob " + size + "\0";                 // ASCII: chars == bytes
  if (hdr.length + size > _SCRATCH_CAP) {           // pathological giant file
    const b1 = io.buf(size + 16);
    io.readAll(fd, b1);
    return frameSha("blob", b1.data());
  }
  const b = _scratch === null ? (_scratch = io.ram(_SCRATCH_CAP)) : _scratch;
  b.reset();
  b.feedStr(hdr);
  const n = size === 0 ? 0 : io.readAll(fd, b.idle().subarray(0, size));
  b.fed(n);
  if (n !== size) return frameSha("blob", b.data().subarray(hdr.length));
  return hex.encode(sha1(b.data()));
}

module.exports = { isFullSha: isFullSha, isZeroSha: isZeroSha,
                   hashlet60FromBytes: hashlet60FromBytes,
                   frameSha: frameSha, blobShaOfFd: blobShaOfFd };
