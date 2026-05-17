#ifndef TOK_HTMT_H
#define TOK_HTMT_H

#include "TOK.h"

con ok64 HTMTBAD = 0x1175674b28d;
con ok64 HTMTFAIL = 0x45d59d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} HTMTstate;

ok64 HTMTLexer(HTMTstate *state);

#endif
