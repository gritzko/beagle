#ifndef TOK_GOT_H
#define TOK_GOT_H

#include "TOK.h"

con ok64 GOTBAD = 0x41874b28d;
con ok64 GOTFAIL = 0x1061d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} GOTstate;

ok64 GOTLexer(GOTstate *state);

#endif
