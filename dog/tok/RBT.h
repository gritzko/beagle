#ifndef TOK_RBT_H
#define TOK_RBT_H

#include "TOK.h"

con ok64 RBTBAD = 0x6cb74b28d;
con ok64 RBTFAIL = 0x1b2dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} RBTstate;

ok64 RBTLexer(RBTstate *state);

#endif
