#ifndef GRAF_WEAVE_H
#define GRAF_WEAVE_H

//  WEAVE: token-level history of one file as a single sequence.
//
//  Per token, four parallel arrays of equal length N:
//    text     — token bytes, indexed by tok32 cumulative end-offset.
//               Token i spans tok32Offset(toks[i-1]) .. tok32Offset(toks[i]).
//    toks     — tok32(tag, end_offset_in_text).
//    hashlets — RAPHash of the token's bytes; used for u64 token diffing.
//    inrm     — (in, rm) pair of 32-bit commit hashlets.
//                 in == 0 → token predates the timeframe (NCA bootstrap).
//                 rm == 0 → token still alive.
//
//  A weave is rebuilt fresh by every operation: WEAVEFromBlob (one-version
//  weave from raw bytes), WEAVEDiff (linear chain step), WEAVEMerge
//  (concurrent branches).  Each writes into a destination weave that
//  the caller has reset.  Three weave instances (src, nu, dst) is the
//  typical caller pattern for incremental builds along a blob chain.
//
//  Splice canonicalization (compose-time invariant):
//    Within any maximal run of non-EQ EDL ops between two EQs,
//    all INS tokens precede all DEL tokens in the output.  No
//    rm-then-in adjacency, no in-rm-in alternation.

#include "abc/INT.h"
#include "abc/RAP.h"
#include "dog/TOK.h"

con ok64 WEAVEFAIL = 0x2038a7ce3ca495;

typedef struct {
    u32 in;
    u32 rm;
} inrm;

typedef inrm const inrmc;
typedef inrm *inrmp;
typedef inrm const *inrmcp;

fun int inrmcmp(inrmcp a, inrmcp b) {
    if (a->in != b->in) return (a->in < b->in) ? -1 : 1;
    if (a->rm != b->rm) return (a->rm < b->rm) ? -1 : 1;
    return 0;
}

fun b8 inrmZ(inrmcp a, inrmcp b) {
    return a->in < b->in || (a->in == b->in && a->rm < b->rm);
}

#define X(M, name) M##inrm##name
#include "abc/Bx.h"
#undef X

typedef struct {
    Bu8    text;
    Bu32   toks;
    Bu64   hashlets;
    Binrm  inrm;
} weave;

ok64 WEAVEInit (weave *w);
void WEAVEReset(weave *w);
void WEAVEFree (weave *w);

//  Build a one-version weave from raw blob bytes.  Tokenizes `data`
//  with the lexer for `ext`, hashes each token, stamps every token
//  with inrm = {src, 0}.  Pass src=0 to mark all tokens as
//  pre-timeframe (NCA bootstrap).
ok64 WEAVEFromBlob(weave *w, u8cs data, u8cs ext, u32 src);

//  dst = src diffed against nu.  `nu` is a one-version weave produced
//  by WEAVEFromBlob; tokens that the diff classifies as INS are copied
//  from nu into dst with in=src_commit, rm=0.  dst is reset before
//  composition.  src and nu are read-only.
ok64 WEAVEDiff (weave *dst, weave const *src, weave const *nu, u32 src_commit);

//  dst = a merged with b (concurrent branches sharing an ancestor).
//  Stub for now.
ok64 WEAVEMerge(weave *dst, weave const *a, weave const *b);

#endif
