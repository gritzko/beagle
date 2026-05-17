#include "KEYW.h"

#include <string.h>

#include "abc/PRO.h"

//  8-bit hash: first two chars of the token + length, mixed with
//  shifts chosen so length and second char occupy disjoint upper
//  bits.  See KEYW.h for rationale.
fun u8 keyw_hash(u8c *head, u64 len) {
    return (u8)((u64)head[0] + ((u64)head[1] << 2) + (len << 5));
}

ok64 KEYWOpen(keyw *k, u8cs *kws, u32 nkw) {
    sane(k && kws);
    test(nkw <= KEYW_MAX, KEYWFULL);

    zero(k->table);
    k->kws = kws;
    k->nkw = nkw;

    for (u32 i = 0; i < nkw; i++) {
        u64 len = u8csLen(kws[i]);
        test(len >= 2, KEYWSHORT);
        u8 h = keyw_hash(kws[i][0], len);
        while (k->table[h] != 0) h++;   // linear probe; u8 wraps at 256
        k->table[h] = (u8)(i + 1);      // 1-based (0 reserved for empty)
    }
    done;
}

b8 KEYWHas(keyw const *k, u8csc tok) {
    if (!k || !k->kws) return NO;
    u64 len = u8csLen(tok);
    if (len < 2) return NO;

    u8 h = keyw_hash(tok[0], len);
    while (k->table[h] != 0) {
        u8csc kw = {k->kws[k->table[h] - 1][0], k->kws[k->table[h] - 1][1]};
        if (u8csEq(tok, kw)) return YES;
        h++;
    }
    return NO;
}
