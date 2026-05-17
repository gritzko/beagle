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

#include "abc/INT.h"

con ok64 PACKFAIL   = 0x64a3143ca495;
con ok64 PACKBADFMT = 0x64a3142ca34f59d;
con ok64 PACKBADOBJ = 0x64a3142ca3582d3;
con ok64 PACKBADCHK = 0x64a3142ca34c454;

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

//  Inflate zlib-compressed data from `from` into `into`.
//  `into` must have room for `size` bytes.
//  Advances `from` past the consumed compressed data.
ok64 PACKInflate(u8cs from, u8s into, u64 size);

#endif
