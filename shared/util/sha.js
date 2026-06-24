//  sha.js — git-sha helpers shared by bin/*.js (JS-043).  Pure JS over the
//  JABC `sha1`/`hex` globals (js/codec.cpp); no C, no dog.  Consolidates
//  the `isFullSha` predicate (was copied verbatim into wtlog.js, keeper.js,
//  classify.js, dag.js, subs.js and wire.js) plus the keeper hashlet60 and
//  loose-object framing helpers.
//
//    isFullSha(s)             → true iff s is exactly 40 lowercase-hex chars
//    hashlet60FromBytes(sha)  → the MS 60 bits of a 20-byte sha (BigInt)
//    frameSha(typeName, body) → the git loose-object sha-1 of body (40-hex)

"use strict";

//  A full git object id: 40 lowercase-hex characters.
function isFullSha(s) {
  if (!s || s.length !== 40) return false;
  for (let i = 0; i < 40; i++) {
    const c = s[i];
    if (!((c >= "0" && c <= "9") || (c >= "a" && c <= "f"))) return false;
  }
  return true;
}

//  The 40-zero tombstone sha (a deleted ref).  Native REFS collapses a
//  zero-sha row to "branch absent" (keeper/REFS.c); resolveRef/eachTip
//  must too, else a `delete` row would resolve to all-zeros.
function isZeroSha(s) {
  if (!s || s.length !== 40) return false;
  for (let i = 0; i < 40; i++) if (s[i] !== "0") return false;
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

module.exports = { isFullSha: isFullSha, isZeroSha: isZeroSha,
                   hashlet60FromBytes: hashlet60FromBytes,
                   frameSha: frameSha };
