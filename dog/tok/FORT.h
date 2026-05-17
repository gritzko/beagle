#ifndef TOK_FORT_H
#define TOK_FORT_H

#include "TOK.h"

con ok64 FORTBAD = 0xf61b74b28d;
con ok64 FORTFAIL = 0x3d86dd3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} FORTstate;

ok64 FORTLexer(FORTstate *state);

#endif
