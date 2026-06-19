#ifndef KEEPER_PACK_H
#define KEEPER_PACK_H

//  PACK: git packfile parser
//
//  Packfile format:
//    PACK <version:u32be> <count:u32be>
//    (<object-entry>)*
//    <20-byte-sha1>
//
//  Each object entry:
//    <varint: type(3 bits) + size>
//    <zlib-compressed data>
//
//  Delta types (OFS_DELTA, REF_DELTA) reference a base object.

#include "abc/B.h"
#include "abc/INT.h"
#include "abc/S.h"

con ok64 PACKFAIL   = 0x64a3143ca495;
con ok64 PACKBADFMT = 0x64a3142ca34f59d;
con ok64 PACKBADOBJ = 0x64a3142ca3582d3;
con ok64 PACKBADCHK = 0x64a3142ca34c454;
//  PACKResolveOfs hit a REF_DELTA record (GIT-004): the OFS-only native
//  resolver never chases sha-addressed bases; a stray REF in a native
//  log is corruption, surfaced as a bounded fail, never silent absence.
con ok64 PACKREF    = 0x64a3142ca35a4;

//  In-pack OFS delta-chain cap shared by every OFS resolver (GIT-004).
#define PACK_DELTA_CHAIN_MAX 256

// Git object types (3-bit field in packfile varint)
#define PACK_OBJ_COMMIT    1
#define PACK_OBJ_TREE      2
#define PACK_OBJ_BLOB      3
#define PACK_OBJ_TAG       4
#define PACK_OBJ_OFS_DELTA 6
#define PACK_OBJ_REF_DELTA 7

// Packfile header
typedef struct {
    u32 version;
    u32 count;
} pack_hdr;

// Single object entry header (parsed, before decompression)
typedef struct {
    u8 type;        // PACK_OBJ_*
    u64 size;       // uncompressed size
    u64 ofs_delta;  // offset delta (OFS_DELTA only)
    u8cs ref_delta; // 20-byte base SHA1 (REF_DELTA only)
} pack_obj;

//  Write the 12-byte git packfile header into `into`:
//    "PACK" magic (4) + version=2 (4) + count (4)
//  Advances `into` head by 12.  Caller pre-reserves room.
//  Used when starting a new pack log.  Both keeper and sniff staging
//  call this — no raw header bytes should appear in any caller.
ok64 PACKu8sFeedHdr(u8s into, u32 count);

//  Parse packfile header. Advances `from`.
ok64 PACKDrainHdr(u8cs from, pack_hdr *hdr);

//  Parse one object entry header (type + size + delta ref).
//  Advances `from` past the header to the start of zlib data.
//  Does NOT decompress — caller inflates `obj->size` bytes from `from`.
ok64 PACKDrainObjHdr(u8cs from, pack_obj *obj);

//  Encode the counterpart of PACKDrainObjHdr's varint: the first byte
//  carries `type` (PACK_OBJ_*) in bits 4-6 and the low 4 bits of
//  `size`; the remaining size bits follow as 7-bit little-endian groups
//  with a continuation MSB.  Appends to `buf`; round-trips through
//  PACKDrainObjHdr.  Shared by every pack writer (keeper's pack log,
//  the push pack builder) — no caller open-codes the header varint.
//  Returns SNOROOM if `buf` lacks room for the whole varint, so a full
//  buffer never emits a truncated header into the pack stream.
ok64 PACKu8sFeedObjHdr(u8bp buf, u8 type, u64 size);

//  Encode an OFS_DELTA negative-offset varint: the exact inverse of
//  the decoder inside PACKDrainObjHdr.  7-bit groups are emitted
//  MSB-first with a continuation bit on all but the last byte, the
//  higher groups carrying the decoder's `+1` bias.  Appends to `buf`;
//  round-trips through PACKDrainObjHdr's OFS branch.  Shared by every
//  pack writer that emits OFS_DELTA (keeper's pack log, the JABC
//  binding) — no caller open-codes the offset varint.  Returns
//  SNOROOM if `buf` lacks room for the whole varint, so a full buffer
//  never emits a truncated offset into the pack stream.
ok64 PACKu8sFeedOfs(u8bp buf, u64 val);

//  Append ONE object record into the pack-log buffer `log`, advancing
//  its DATA boundary, then return so the caller can index the record
//  at the offset it captured before the call.  The record is OFS_DELTA
//  when a usable in-log base is supplied, else raw:
//
//    `type`     — PACK_OBJ_* of the object (1..4); recorded in the raw
//                 header and used by the caller's index.
//    `content`  — the object's full uncompressed bytes.
//    `base`     — the resolved base object's full uncompressed bytes,
//                 or an empty slice for "store raw" (no delta attempt).
//    `cur_off`  — byte offset of THIS record's start in `log`
//                 (= u8bDataLen(log) at the call site, captured before
//                 any FILEBook growth so it is the stable record offset).
//    `base_off` — byte offset of the base record's start in `log`.
//    `delta`    — caller-provided scratch buffer for the encoded delta
//                 instructions; reset internally, never grown here.
//
//  When `base` is non-empty the record is OFS_DELTA only if DELTEncode
//  succeeds AND the delta is smaller than `content`; otherwise it falls
//  back to a raw record (DELTFAIL or not-smaller → raw, same as a missing
//  base).  OFS_DELTA emits PACKu8sFeedObjHdr + PACKu8sFeedOfs(cur_off -
//  base_off) + the deflated delta; raw emits PACKu8sFeedObjHdr + the
//  deflated content.  NEVER emits REF_DELTA — sha-addressed bases are an
//  ingest-boundary concern, not a stored-log one.  The byte output is
//  identical to keeper's prior inline writer for both the raw and OFS
//  cases.  `log` must already have IDLE room for the record (caller
//  reserves via its growable-log machinery); no malloc, no refs/index.
//  Checks every emit return so a full buffer never leaves a truncated
//  record in the stream.  `out_delta` (optional) is set YES when an
//  OFS_DELTA was emitted, NO for raw — lets the caller assert/trace.
ok64 PACKu8sFeedObj(u8bp log, u8 type, u8csc content,
                    u8csc base, u64 cur_off, u64 base_off,
                    u8bp delta, b8 *out_delta);

//  Inflate zlib-compressed data from `from` into `into`.
//  `into` must have room for `size` bytes.
//  Advances `from` past the consumed compressed data.
ok64 PACKInflate(u8cs from, u8s into, u64 size);

//  GIT-007: byte offset just past the record that starts at `offset` in
//  `pack` (header + the whole zlib stream).  A zlib stream is not
//  length-delimited, so the extent is learned by draining the header and
//  inflating into BASS scratch to measure the consumed compressed bytes —
//  the same scan keeper/js need to walk records sequentially, owned here
//  (not open-coded in the binding).  *end_out is the next record's start.
ok64 PACKRecordEnd(u8cs pack, u64 offset, u64 *end_out);

//  GIT-004: OFS-only delta-chase resolver, shared by keeper's native
//  store resolver AND the js/JABC binding.  Resolve the object at byte
//  `offset` in the OFS-only pack `pack` to its full inflated bytes:
//    - read the object header at `offset`;
//    - if a base type (1..4), inflate it directly;
//    - if OFS_DELTA, subtract `ofs_delta` and loop, recording the hop;
//    - apply the recorded delta chain bottom-up onto the base.
//  `pack` is the whole pack-byte slice (header through the last record);
//  `offset` is the start of the target object's record.  `base` and
//  `delta` are caller-owned scratch slices (no allocation here, per
//  CLAUDE.md §5): `base` must hold the largest inflated object, `delta`
//  the largest single inflated delta-instruction stream PLUS one apply
//  result (so it is split internally).  On OK, `out` aliases bytes in
//  `base` or `delta` (valid until the scratch is reused) and `*out_type`
//  carries the resolved git object type (1..4).
//  Corruption bounds (GIT-004 preserved): every chased `offset` is
//  re-checked against the pack length before use; OFS deltas with
//  `ofs_delta == 0` (self-ref) or `ofs_delta > cur` (underflow) are
//  rejected; the chain length is capped at PACK_DELTA_CHAIN_MAX.
//  A REF_DELTA record returns PACKREF (assert-guarded backstop in the
//  OFS-only native store — never a silent absence).
ok64 PACKResolveOfs(u8cs pack, u64 offset, u8s base, u8s delta,
                    u8csp out, u8p out_type);

#endif
