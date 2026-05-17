#ifndef DOG_KEYW_H
#define DOG_KEYW_H

//  KEYW: tiny open-addressing keyword-set hash.
//
//  Purpose.  Every dogenizer (CT, PYT, SHT, …) needs a fast "is this
//  token one of the language's reserved keywords?" check on its hot
//  per-token path.  A naive scan of `N = ~60` keywords per token shows
//  up as a real hotspot on large ingests (src/git indexing was
//  dominated by `strlen`-inside-`CTIsKeyword`).  KEYW provides an
//  O(1) amortised lookup via a fixed 256-slot open-addressing table.
//
//  Layout.  `keyw.table[256]` is a byte per slot:
//      0         empty
//      1..N      1-based index into the caller's `kws[]` array
//  Linear probe on collision; the u8 index wraps at 256.  Each slot
//  costs 1 byte, so the whole table is exactly 256 bytes.
//
//  Contract.
//    * KEYWOpen copies no bytes: `kws` is borrowed and must outlive
//      the `keyw` instance.  Typical usage is a `static const u8csc
//      *_KWS[]` array in the dogenizer's .c file, built once at
//      process start (guarded by a `b8 inited` flag on first use).
//    * All keywords must be >= 2 bytes.  Real languages (C, Python,
//      Go, Rust, shell, JS, …) have no 1-char keywords, and the hash
//      reads tok[0][0] + tok[0][1] unconditionally.  `KEYWHas`
//      short-circuits len<2 at entry.
//    * At most KEYW_MAX = 64 keywords per table (load factor ~25 %
//      at the 256-slot size).  `KEYWOpen` fails with KEYWFULL above.
//
//  Hash.  `h = tok[0] + (tok[1] << 2) + (len << 5)`, u8 truncation.
//  Cheap (two shifts, two adds) and spreads well across 58 C
//  keywords — adds carry bits where XOR would collide, and `<< 5`
//  puts length in the high bits where lit2's shifted bits don't
//  reach.

#include "abc/INT.h"
#include "abc/OK.h"

#define KEYW_SLOTS 256
#define KEYW_MAX   64

con ok64 KEYWFAIL = 0x50e8a03ca495;   // general failure
con ok64 KEYWFULL = 0x50e8a03de555;  // > KEYW_MAX keywords
con ok64 KEYWSHORT = 0x143a281c4586dd;   // keyword < 2 bytes

typedef struct {
    u8          table[KEYW_SLOTS];  // 0 = empty; else 1-based idx into kws
    u8cs       *kws;                // borrowed; must outlive this keyw
    u32         nkw;
} keyw;

//  Build the table from `kws[0..nkw)`.  Each entry must be >= 2 bytes
//  and the set must contain no duplicates (duplicates waste slots but
//  aren't rejected).  `kws` is borrowed.
ok64 KEYWOpen(keyw *k, u8cs *kws, u32 nkw);

//  Is `tok` one of the registered keywords?  Returns NO for len<2
//  without probing the table.
b8 KEYWHas(keyw const *k, u8csc tok);

#endif
