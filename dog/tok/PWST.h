#ifndef TOK_PWST_H
#define TOK_PWST_H

#include "TOK.h"

con ok64 PWSTBAD = 0x1981c74b28d;
con ok64 PWSTFAIL = 0x66071d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} PWSTstate;

ok64 PWSTLexer(PWSTstate *state);

#endif
