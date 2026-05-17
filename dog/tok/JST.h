#ifndef TOK_JST_H
#define TOK_JST_H

#include "TOK.h"

con ok64 JSTBAD = 0x4dc74b28d;
con ok64 JSTFAIL = 0x1371d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} JSTstate;

ok64 JSTLexer(JSTstate *state);

#endif
