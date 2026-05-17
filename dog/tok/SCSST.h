#ifndef TOK_SCSST_H
#define TOK_SCSST_H

#include "TOK.h"

con ok64 SCSSTBAD = 0x70c71c74b28d;
con ok64 SCSSTFAIL = 0x1c31c71d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} SCSSTstate;

ok64 SCSSTLexer(SCSSTstate *state);

#endif
