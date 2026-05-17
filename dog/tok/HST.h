#ifndef TOK_HST_H
#define TOK_HST_H

#include "TOK.h"

con ok64 HSTBAD = 0x45c74b28d;
con ok64 HSTFAIL = 0x1171d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} HSTstate;

ok64 HSTLexer(HSTstate *state);

#endif
