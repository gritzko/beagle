#ifndef TOK_CST_H
#define TOK_CST_H

#include "TOK.h"

con ok64 CSTBAD = 0x31c74b28d;
con ok64 CSTFAIL = 0xc71d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} CSTstate;

ok64 CSTLexer(CSTstate *state);

#endif
