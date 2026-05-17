#ifndef TOK_PHPT_H
#define TOK_PHPT_H

#include "TOK.h"

con ok64 PHPTBAD = 0x1945974b28d;
con ok64 PHPTFAIL = 0x65165d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} PHPTstate;

ok64 PHPTLexer(PHPTstate *state);

#endif
