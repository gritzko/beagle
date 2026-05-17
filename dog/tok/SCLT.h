#ifndef TOK_SCLT_H
#define TOK_SCLT_H

#include "TOK.h"

con ok64 SCLTBAD = 0x1c31574b28d;
con ok64 SCLTFAIL = 0x70c55d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} SCLTstate;

ok64 SCLTLexer(SCLTstate *state);

#endif
