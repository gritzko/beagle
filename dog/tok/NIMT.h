#ifndef TOK_NIMT_H
#define TOK_NIMT_H

#include "TOK.h"

con ok64 NIMTBAD = 0x1749674b28d;
con ok64 NIMTFAIL = 0x5d259d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} NIMTstate;

ok64 NIMTLexer(NIMTstate *state);

#endif
