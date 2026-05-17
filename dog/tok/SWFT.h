#ifndef TOK_SWFT_H
#define TOK_SWFT_H

#include "TOK.h"

con ok64 SWFTBAD = 0x1c80f74b28d;
con ok64 SWFTFAIL = 0x7203dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} SWFTstate;

ok64 SWFTLexer(SWFTstate *state);

#endif
