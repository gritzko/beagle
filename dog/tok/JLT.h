#ifndef TOK_JLT_H
#define TOK_JLT_H

#include "TOK.h"

con ok64 JLTBAD = 0x4d574b28d;
con ok64 JLTFAIL = 0x1355d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} JLTstate;

ok64 JLTLexer(JLTstate *state);

#endif
