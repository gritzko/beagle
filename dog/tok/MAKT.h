#ifndef TOK_MAKT_H
#define TOK_MAKT_H

#include "TOK.h"

con ok64 MAKTBAD = 0x1629474b28d;
con ok64 MAKTFAIL = 0x58a51d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} MAKTstate;

ok64 MAKTLexer(MAKTstate *state);

#endif
