#ifndef TOK_TYST_H
#define TOK_TYST_H

#include "TOK.h"

con ok64 TYSTBAD = 0x1d89c74b28d;
con ok64 TYSTFAIL = 0x76271d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} TYSTstate;

ok64 TYSTLexer(TYSTstate *state);

#endif
