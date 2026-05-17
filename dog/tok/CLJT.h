#ifndef TOK_CLJT_H
#define TOK_CLJT_H

#include "TOK.h"

con ok64 CLJTBAD = 0xc55374b28d;
con ok64 CLJTFAIL = 0x3154dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} CLJTstate;

ok64 CLJTLexer(CLJTstate *state);

#endif
