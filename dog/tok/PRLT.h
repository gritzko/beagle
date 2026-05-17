#ifndef TOK_PRLT_H
#define TOK_PRLT_H

#include "TOK.h"

con ok64 PRLTBAD = 0x196d574b28d;
con ok64 PRLTFAIL = 0x65b55d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} PRLTstate;

ok64 PRLTLexer(PRLTstate *state);

#endif
