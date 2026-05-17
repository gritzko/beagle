#ifndef TOK_CSST_H
#define TOK_CSST_H

#include "TOK.h"

con ok64 CSSTBAD = 0xc71c74b28d;
con ok64 CSSTFAIL = 0x31c71d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} CSSTstate;

ok64 CSSTLexer(CSSTstate *state);

#endif
