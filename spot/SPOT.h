#ifndef LIBRDX_SPOT_H
#define LIBRDX_SPOT_H

#include "dog/tok/TOK.h"

con ok64 SPOTEND  = 0x1c65874e5cd;
con ok64 SPOTBAD  = 0x1c65874b28d;
con ok64 SPOTNONE = 0x71961d5d85ce;   // no-match-at-this-position
con ok64 SPOTBIG  = 0x1c65874b490;    // source too large for 24-bit tok offsets

#define SPOT_MAX_BINDS 52
#define SPOT_MAX_SUBS 32
#define SPOT_MAX_NTOKS 64

// Tokenize source into packed u32 buffer.
// ext: file extension with dot (e.g. ".c") — dot is stripped for tok/.
ok64 SPOTTokenize(u32bp toks, u8csc source, u8csc ext);

// Flattened needle token
typedef struct {
    u8cs val;
    u8   type;
    b8   skip;    // YES if 2+ space gap before this token
} SPOTntok;

typedef struct {
    u32cs ntoks;    // needle packed tokens (slice into caller buffer)
    u32cs htoks;    // haystack packed tokens
    u8cs  nsrc;     // needle source text
    u8cs  source;   // haystack source text
    int   hpos;     // current scan position in htoks
    u64   bound;
    match32 bind_matches[SPOT_MAX_BINDS];
    match32p ranges[4];
    range32 src_rng;
    range32 subs[SPOT_MAX_SUBS];
    u8    nsubs;
    b8    exhausted;
    // Needle analysis (from flatten step)
    SPOTntok flat[SPOT_MAX_NTOKS];
    int      nflat;
} SPOTstate;

// Initialize. Tokenizes needle_src with tok/.
// ndl_toks: caller-provided buffer for needle tokens.
// hay_toks: pre-tokenized haystack tokens (slice).
// source: original haystack source text.
ok64 SPOTInit(SPOTstate *st, u32bp ndl_toks,
              u8csc needle_src, u8csc ext,
              u32cs hay_toks, u8csc source);

// Find next match. Returns OK on match, SPOTEND when exhausted.
ok64 SPOTNext(SPOTstate *st);

// Apply SPOT replacement to all matches in one file.
// If nmatches != NULL, writes the number of replacements made.
ok64 SPOTReplace(u8s out, u8csc source, u32cs hay_toks,
                 u8csc needle_src, u8csc replace_src, u8csc ext,
                 int *nmatches);

#endif
