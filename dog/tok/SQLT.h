#ifndef TOK_SQLT_H
#define TOK_SQLT_H

#include "TOK.h"

con ok64 SQLTBAD = 0x1c69574b28d;
con ok64 SQLTFAIL = 0x71a55d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} SQLTstate;

ok64 SQLTLexer(SQLTstate *state);

#endif
