#ifndef TOK_PRTT_H
#define TOK_PRTT_H

#include "TOK.h"

con ok64 PRTTBAD = 0x196dd74b28d;
con ok64 PRTTFAIL = 0x65b75d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} PRTTstate;

ok64 PRTTLexer(PRTTstate *state);

#endif
