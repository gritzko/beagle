#ifndef TOK_LLT_H
#define TOK_LLT_H

#include "TOK.h"

con ok64 LLTBAD = 0x55574b28d;
con ok64 LLTFAIL = 0x1555d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} LLTstate;

ok64 LLTLexer(LLTstate *state);

#endif
