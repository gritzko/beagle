#ifndef TOK_PYT_H
#define TOK_PYT_H

#include "TOK.h"

con ok64 PYTBAD = 0x66274b28d;
con ok64 PYTFAIL = 0x1989d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} PYTstate;

ok64 PYTLexer(PYTstate *state);

#endif
