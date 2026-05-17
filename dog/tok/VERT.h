#ifndef TOK_VERT_H
#define TOK_VERT_H

#include "TOK.h"

con ok64 VERTBAD = 0x1f39b74b28d;
con ok64 VERTFAIL = 0x7ce6dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} VERTstate;

ok64 VERTLexer(VERTstate *state);

#endif
