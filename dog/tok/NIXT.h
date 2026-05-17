#ifndef TOK_NIXT_H
#define TOK_NIXT_H

#include "TOK.h"

con ok64 NIXTBAD = 0x174a174b28d;
con ok64 NIXTFAIL = 0x5d285d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} NIXTstate;

ok64 NIXTLexer(NIXTstate *state);

#endif
