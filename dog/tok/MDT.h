#ifndef TOK_MDT_H
#define TOK_MDT_H

#include "TOK.h"

con ok64 MDTBAD = 0x58d74b28d;
con ok64 MDTFAIL = 0x1635d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} MDTstate;

ok64 MDTLexer(MDTstate *state);

#endif
