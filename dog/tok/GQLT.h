#ifndef TOK_GQLT_H
#define TOK_GQLT_H

#include "TOK.h"

con ok64 GQLTBAD = 0x1069574b28d;
con ok64 GQLTFAIL = 0x41a55d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} GQLTstate;

ok64 GQLTLexer(GQLTstate *state);

#endif
