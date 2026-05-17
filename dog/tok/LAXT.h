#ifndef TOK_LAXT_H
#define TOK_LAXT_H

#include "TOK.h"

con ok64 LAXTBAD = 0x152a174b28d;
con ok64 LAXTFAIL = 0x54a85d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} LAXTstate;

ok64 LAXTLexer(LAXTstate *state);

#endif
