#ifndef TOK_ELXT_H
#define TOK_ELXT_H

#include "TOK.h"

con ok64 ELXTBAD = 0xe56174b28d;
con ok64 ELXTFAIL = 0x39585d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} ELXTstate;

ok64 ELXTLexer(ELXTstate *state);

#endif
