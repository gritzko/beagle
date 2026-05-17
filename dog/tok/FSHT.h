#ifndef TOK_FSHT_H
#define TOK_FSHT_H

#include "TOK.h"

con ok64 FSHTBAD = 0xf71174b28d;
con ok64 FSHTFAIL = 0x3dc45d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} FSHTstate;

ok64 FSHTLexer(FSHTstate *state);

#endif
