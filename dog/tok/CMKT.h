#ifndef TOK_CMKT_H
#define TOK_CMKT_H

#include "TOK.h"

con ok64 CMKTBAD = 0xc59474b28d;
con ok64 CMKTFAIL = 0x31651d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} CMKTstate;

ok64 CMKTLexer(CMKTstate *state);

#endif
