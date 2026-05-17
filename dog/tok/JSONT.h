#ifndef TOK_JSONT_H
#define TOK_JSONT_H

#include "TOK.h"

con ok64 JSONTBAD = 0x4dc61774b28d;
con ok64 JSONTFAIL = 0x137185dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} JSONTstate;

ok64 JSONTLexer(JSONTstate *state);

#endif
