#ifndef TOK_SOLT_H
#define TOK_SOLT_H

#include "TOK.h"

con ok64 SOLTBAD = 0x1c61574b28d;
con ok64 SOLTFAIL = 0x71855d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} SOLTstate;

ok64 SOLTLexer(SOLTstate *state);

#endif
