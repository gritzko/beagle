#ifndef TOK_AGDT_H
#define TOK_AGDT_H

#include "TOK.h"

con ok64 AGDTBAD = 0xa40d74b28d;
con ok64 AGDTFAIL = 0x29035d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} AGDTstate;

ok64 AGDTLexer(AGDTstate *state);

#endif
