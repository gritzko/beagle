#ifndef TOK_SHT_H
#define TOK_SHT_H

#include "TOK.h"

con ok64 SHTBAD = 0x71174b28d;
con ok64 SHTFAIL = 0x1c45d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} SHTstate;

ok64 SHTLexer(SHTstate *state);

#endif
