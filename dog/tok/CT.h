#ifndef TOK_CT_H
#define TOK_CT_H

#include "TOK.h"

con ok64 CTBAD = 0xc74b28d;
con ok64 CTFAIL = 0x31d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} CTstate;

ok64 CTLexer(CTstate *state);

#endif
