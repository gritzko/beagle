#ifndef TOK_JAT_H
#define TOK_JAT_H

#include "TOK.h"

con ok64 JATBAD = 0x4ca74b28d;
con ok64 JATFAIL = 0x1329d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} JATstate;

ok64 JATLexer(JATstate *state);

#endif
