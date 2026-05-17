#ifndef TOK_VIMT_H
#define TOK_VIMT_H

#include "TOK.h"

con ok64 VIMTBAD = 0x1f49674b28d;
con ok64 VIMTFAIL = 0x7d259d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} VIMTstate;

ok64 VIMTLexer(VIMTstate *state);

#endif
