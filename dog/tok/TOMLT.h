#ifndef TOK_TOMLT_H
#define TOK_TOMLT_H

#include "TOK.h"

con ok64 TOMLTBAD = 0x75859574b28d;
con ok64 TOMLTFAIL = 0x1d61655d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} TOMLTstate;

ok64 TOMLTLexer(TOMLTstate *state);

#endif
