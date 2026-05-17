#ifndef TOK_GLMT_H
#define TOK_GLMT_H

#include "TOK.h"

con ok64 GLMTBAD = 0x1055674b28d;
con ok64 GLMTFAIL = 0x41559d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} GLMTstate;

ok64 GLMTLexer(GLMTstate *state);

#endif
