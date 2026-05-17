#ifndef TOK_ERLT_H
#define TOK_ERLT_H

#include "TOK.h"

con ok64 ERLTBAD = 0xe6d574b28d;
con ok64 ERLTFAIL = 0x39b55d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} ERLTstate;

ok64 ERLTLexer(ERLTstate *state);

#endif
