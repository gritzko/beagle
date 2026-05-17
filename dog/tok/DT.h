#ifndef TOK_DT_H
#define TOK_DT_H

#include "TOK.h"

con ok64 DTBAD = 0xd74b28d;
con ok64 DTFAIL = 0x35d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} DTstate;

ok64 DTLexer(DTstate *state);

#endif
