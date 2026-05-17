#ifndef TOK_ZIGT_H
#define TOK_ZIGT_H

#include "TOK.h"

con ok64 ZIGTBAD = 0x2349074b28d;
con ok64 ZIGTFAIL = 0x8d241d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} ZIGTstate;

ok64 ZIGTLexer(ZIGTstate *state);

#endif
