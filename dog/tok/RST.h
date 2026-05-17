#ifndef TOK_RST_H
#define TOK_RST_H

#include "TOK.h"

con ok64 RSTBAD = 0x6dc74b28d;
con ok64 RSTFAIL = 0x1b71d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} RSTstate;

ok64 RSTLexer(RSTstate *state);

#endif
