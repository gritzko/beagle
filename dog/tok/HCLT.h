#ifndef TOK_HCLT_H
#define TOK_HCLT_H

#include "TOK.h"

con ok64 HCLTBAD = 0x1131574b28d;
con ok64 HCLTFAIL = 0x44c55d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} HCLTstate;

ok64 HCLTLexer(HCLTstate *state);

#endif
