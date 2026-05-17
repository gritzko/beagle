#ifndef TOK_DKFT_H
#define TOK_DKFT_H

#include "TOK.h"

con ok64 DKFTBAD = 0xd50f74b28d;
con ok64 DKFTFAIL = 0x3543dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} DKFTstate;

ok64 DKFTLexer(DKFTstate *state);

#endif
