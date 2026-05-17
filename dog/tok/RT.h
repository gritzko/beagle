#ifndef TOK_RT_H
#define TOK_RT_H

#include "TOK.h"

con ok64 RTBAD = 0x1b74b28d;
con ok64 RTFAIL = 0x6dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} RTstate;

ok64 RTLexer(RTstate *state);

#endif
