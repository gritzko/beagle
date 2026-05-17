#ifndef TOK_TOK_H
#define TOK_TOK_H

#include "abc/INT.h"

con ok64 TOKBAD = 0x75850b28d;
con ok64 TOKFAIL = 0x1d6143ca495;

// --- Packed u32 token ---
//   bits 31..27 (5): tag - 'A'
//   bit  26     (1): custom (display-time scratch, not on-wire)
//   bits 25..24 (2): diff side  0=eq  1=in  2=rm
//   bits 23..0 (24): end offset  (16 MiB cap per hunk text)
// Tags: D=comment, G=string, L=number, H=preproc, R=keyword, P=punct,
//       S=word/default, W=whitespace
typedef u32 tok32;
typedef tok32 const tok32c;
typedef tok32 *tok32s[2];
typedef tok32 const *tok32cs[2];
typedef tok32 const *const tok32csc[2];

#define TOK_OFF_BITS  24
#define TOK_OFF_MASK  ((1u << TOK_OFF_BITS) - 1)
#define TOK_SIDE_BITS 2
#define TOK_SIDE_MASK 0x3u
#define TOK_CUSTOM_BIT 26

#define TOK_SIDE_EQ 0u
#define TOK_SIDE_IN 1u
#define TOK_SIDE_RM 2u

fun u32  tok32Offset(tok32 t) { return t & TOK_OFF_MASK; }
fun u8   tok32Side(tok32 t)   {
    return (u8)((t >> TOK_OFF_BITS) & TOK_SIDE_MASK);
}
fun u8   tok32Custom(tok32 t) {
    return (u8)((t >> TOK_CUSTOM_BIT) & 1u);
}
fun u8   tok32Tag(tok32 t)    { return (u8)('A' + (t >> 27)); }

fun u32  tok32Pack(u8 tag, u32 off) {
    return ((u32)(tag - 'A') << 27) | (off & TOK_OFF_MASK);
}
fun u32  tok32PackSide(u8 tag, u8 side, u32 off) {
    return ((u32)(tag - 'A') << 27)
         | (((u32)side & TOK_SIDE_MASK) << TOK_OFF_BITS)
         | (off & TOK_OFF_MASK);
}
fun u32  tok32SetSide(tok32 t, u8 side) {
    return (t & ~(TOK_SIDE_MASK << TOK_OFF_BITS))
         | (((u32)side & TOK_SIDE_MASK) << TOK_OFF_BITS);
}
fun u32  tok32SetCustom(tok32 t, u8 custom) {
    return (t & ~(1u << TOK_CUSTOM_BIT))
         | (((u32)custom & 1u) << TOK_CUSTOM_BIT);
}

// Get source slice for token i (tokens are contiguous end offsets).
fun void tok32Val(u8cs out, tok32csc toks, u8cp base, int i) {
    u32 lo = (i > 0) ? tok32Offset(toks[0][i - 1]) : 0;
    u32 hi = tok32Offset(toks[0][i]);
    out[0] = base + lo;
    out[1] = base + hi;
}

typedef ok64 (*TOKcb)(u8 tag, u8cs tok, void *ctx);

// Split a text slice into word/space/punct sub-tokens,
// emitting each via cb with the given tag.  Reusable for
// comments, strings, or any blob that needs finer grain.
ok64 TOKSplitText(u8 tag, u8cs text, TOKcb cb, void *ctx);

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} TOKstate;

ok64 TOKLexer(TOKstate *state, u8csc ext);

// Check if ext matches a known language in the dispatch table
b8 TOKKnownExt(u8csc ext);

// Return the extension string at index i in TOK_TABLE, or NULL if out of range
const char *TOKExtAt(int i);

// Check if two extensions use the same lexer (e.g. .c and .h both use CT)
b8 TOKSameLexer(u8csc a, u8csc b);

#endif
