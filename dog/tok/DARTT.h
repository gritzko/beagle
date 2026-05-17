#ifndef TOK_DARTT_H
#define TOK_DARTT_H

#include "TOK.h"

con ok64 DARTTBAD = 0x34a6dd74b28d;
con ok64 DARTTFAIL = 0xd29b75d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} DARTTstate;

ok64 DARTTLexer(DARTTstate *state);

#endif
