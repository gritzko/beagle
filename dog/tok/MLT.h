#ifndef TOK_MLT_H
#define TOK_MLT_H

#include "TOK.h"

con ok64 MLTBAD = 0x59574b28d;
con ok64 MLTFAIL = 0x1655d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} MLTstate;

ok64 MLTLexer(MLTstate *state);

#endif
