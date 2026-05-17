#ifndef TOK_GLST_H
#define TOK_GLST_H

#include "TOK.h"

con ok64 GLSTBAD = 0x1055c74b28d;
con ok64 GLSTFAIL = 0x41571d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} GLSTstate;

ok64 GLSTLexer(GLSTstate *state);

#endif
