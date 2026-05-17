#ifndef TOK_YMLT_H
#define TOK_YMLT_H

#include "TOK.h"

con ok64 YMLTBAD = 0x2259574b28d;
con ok64 YMLTFAIL = 0x89655d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} YMLTstate;

ok64 YMLTLexer(YMLTstate *state);

#endif
