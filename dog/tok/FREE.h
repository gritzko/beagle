#ifndef TOK_FREE_H
#define TOK_FREE_H

#include "TOK.h"

// FREE — reusable "free text" scanner for natural-language slices
// (comment bodies, docstrings, markdown paragraphs).  Splits a slice
// into space/punctuation-separated UTF-8 words, recognises issue
// keys (ABC-123) as one token, treats newlines as standalone
// whitespace tokens (never fused into a word).  No strict UTF-8
// validation — any 0x80..0xff byte runs as a word char.
//
// Native tags emitted by FREELexer:
//   S — word (ASCII alnum runs and/or hi-byte runs)
//   L — number (decimal, fractional, hex)
//   P — punctuation (one byte at a time)
//   W — whitespace (horizontal runs OR a single newline)
//
// FREEu8sFeed wraps FREELexer with an overlay tag — pass 'D' from a
// comment callback to retag every chunk as D, etc.  Pass 0 to keep
// native tags.

con ok64 FREEBAD  = 0x5e6e63b28d;
con ok64 FREEFAIL = 0x16f5e6e6ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} FREEstate;

ok64 FREELexer(FREEstate *state);

ok64 FREEu8sFeed(u8 overlay, u8cs slice, TOKcb cb, void *ctx);

#endif
