#ifndef TOK_ODNT_H
#define TOK_ODNT_H

#include "TOK.h"

con ok64 ODNTBAD = 0x1835774b28d;
con ok64 ODNTFAIL = 0x60d5dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} ODNTstate;

ok64 ODNTLexer(ODNTstate *state);

#endif
