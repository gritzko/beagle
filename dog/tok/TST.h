#ifndef TOK_TST_H
#define TOK_TST_H

#include "TOK.h"

con ok64 TSTBAD = 0x75c74b28d;
con ok64 TSTFAIL = 0x1d71d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} TSTstate;

ok64 TSTLexer(TSTstate *state);

#endif
