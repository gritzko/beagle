#ifndef KEEPER_DELT_H
#define KEEPER_DELT_H

//  DELT: git delta instruction parser/applier
//
//  Git delta format (after zlib decompression):
//    <base-size-varint> <result-size-varint>
//    (<copy-instruction> | <insert-instruction>)*
//
//  Copy:  high bit set, lower 7 bits select which of the next
//         1-4 bytes encode offset and 1-3 bytes encode size.
//  Insert: high bit clear, value 1..127 = number of literal bytes.

#include "abc/INT.h"

con ok64 DELTFAIL = 0x34e55d3ca495;
con ok64 DELTBADFMT = 0x34e55d2ca34f59d;

//  Apply delta instructions to a base object.
//  `delta`  — decompressed delta instruction stream
//  `base`   — base object content
//  `out`    — output gauge; written data in left portion after return
//  Returns OK, advances `delta` to end.
ok64 DELTApply(u8cs delta, u8cs base, u8g out);

//  Encode delta: produce git delta instruction stream.
//  `base`   — base object content
//  `target` — target object content
//  `out`    — output buffer; delta instructions appended to DATA
//  Returns OK.  If delta is larger than target, returns DELTFAIL
//  (caller should store raw instead).
ok64 DELTEncode(u8csc base, u8csc target, u8bp out);

#endif
