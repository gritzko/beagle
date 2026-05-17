#ifndef TOK_LUAT_H
#define TOK_LUAT_H

#include "TOK.h"

con ok64 LUATBAD = 0x1578a74b28d;
con ok64 LUATFAIL = 0x55e29d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} LUATstate;

ok64 LUATLexer(LUATstate *state);

#endif
