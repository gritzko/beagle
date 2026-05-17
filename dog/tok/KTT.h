#ifndef TOK_KTT_H
#define TOK_KTT_H

#include "TOK.h"

con ok64 KTTBAD = 0x51d74b28d;
con ok64 KTTFAIL = 0x1475d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} KTTstate;

ok64 KTTLexer(KTTstate *state);

#endif
