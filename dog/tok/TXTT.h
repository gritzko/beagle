#ifndef TOK_TXTT_H
#define TOK_TXTT_H

#include "TOK.h"

con ok64 TXTTBAD = 0x1d85d74b28d;
con ok64 TXTTFAIL = 0x76175d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} TXTTstate;

ok64 TXTTLexer(TXTTstate *state);

#endif
