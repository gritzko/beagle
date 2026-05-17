#ifndef KEEPER_ZINF_H
#define KEEPER_ZINF_H

//  ZINF: zlib inflate/deflate wrappers.
//  Isolated from abc/BUF.h to avoid voidp/voidpc typedef clash with zlib.
//  Takes slice pointers (u8**, u8c**) directly — pass u8bIdle(), etc.

#include "abc/01.h"

con ok64 ZINFFAIL   = 0x8d25cf3ca495;
con ok64 ZINFINIT   = 0x8d25cf49749d;
con ok64 ZINFTOOBIG = 0x8d25cf75860b490;

//  Inflate: into[0] advances by produced, zipped[0] by consumed.
ok64 ZINFInflate(u8p *into, u8cp *zipped);

//  Deflate: into[0] advances by produced, plain[0] by consumed.
ok64 ZINFDeflate(u8p *into, u8cp *plain);

#endif
