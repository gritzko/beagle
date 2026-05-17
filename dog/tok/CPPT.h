#ifndef TOK_CPPT_H
#define TOK_CPPT_H

#include "TOK.h"

con ok64 CPPTBAD = 0xc65974b28d;
con ok64 CPPTFAIL = 0x31965d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} CPPTstate;

ok64 CPPTLexer(CPPTstate *state);

#endif
