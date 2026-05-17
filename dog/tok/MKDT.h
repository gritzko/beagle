#ifndef TOK_MKDT_H
#define TOK_MKDT_H

#include "TOK.h"

con ok64 MKDTBAD = 0x1650d74b28d;
con ok64 MKDTFAIL = 0x59435d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} MKDTstate;

ok64 MKDTLexer(MKDTstate *state);

#endif
