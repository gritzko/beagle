#ifndef GRAF_WEAVE_H
#define GRAF_WEAVE_H

//  WEAVE: token-level history of one file as a single interleaved-delta
//  TLV stream (SCCS-weave style).  One `u8b tlv` holds, in weave order:
//
//    T <token bytes>      one token's literal text
//    I <sorted u32 set>   the set of commit hashlets that INSERTED the
//                         following run of tokens (LE-packed u32s)
//    R <sorted u32 set>   the set of commit hashlets that REMOVED the
//                         following run of tokens
//
//  `I` / `R` records are run-length: one is emitted only when the set
//  changes from the previous token ("per change, not per token").  A
//  decoder keeps a running `(curI, curR)`; each `I`/`R` record REPLACES
//  the corresponding running set, and every `T` inherits the current
//  pair.  A token is ALIVE iff its R-set is empty; a token is SPINE
//  (member of every revision — the NCA bootstrap) iff `0` is in its
//  I-set.  Concurrent inserts of the same token list every inserter in
//  one I-set; concurrent removes list every remover in one R-set.
//
//  No lexer/syntax tag is stored — every token is a bare `T`.
//
//  A weave is rebuilt fresh by every operation: WEAVEFromBlob (one
//  version from raw bytes), WEAVEDiff (linear chain step), WEAVEMerge
//  (concurrent branches).  Each writes into a destination weave the
//  caller reset.  Three weave instances (src, nu, dst) is the typical
//  caller pattern for incremental builds along a blob chain.
//
//  Splice canonicalization (compose-time invariant, enforced by
//  `NEILCanon`): within any maximal run of non-EQ EDL ops between two
//  EQs, all INS tokens precede all DEL tokens — no rm-then-in, no
//  in-rm-in alternation.

#include "abc/B.h"
#include "abc/INT.h"
#include "abc/S.h"
#include "dog/HUNK.h"

con ok64 WEAVEFAIL = 0x2038a7ce3ca495;

//  Sentinel `src` for the worktree shadow version (uncommitted edits).
//  Real `WHIFFHashlet40` truncations land in the bottom 32 bits — full
//  0xFFFFFFFF is overwhelmingly likely to be free.  Shared between BLAME
//  (worktree blame row) and the DIFF projector (wt as next-version-after
//  -base in the wt-vs-base shape per VERBS.md `diff:`).
#define WEAVE_WT_SRC 0xFFFFFFFFu

//  Sentinel `src` for synthetic conflict-marker tokens that frame
//  divergent regions in a merged weave's alive byte stream.  Picked
//  one shy of WEAVE_WT_SRC; only ever observed at render time, never
//  stored in committed history.
#define WEAVE_CFLCT_SRC 0xFFFFFFFEu

//  Record-type letters in the TLV stream (uppercase => TLV-long form,
//  so tokens / sets may exceed 255 bytes).
#define WEAVE_REC_T 'T'
#define WEAVE_REC_I 'I'
#define WEAVE_REC_R 'R'

//  Max elements decoded from one I/R set by the public cursor.  A token
//  set is the number of commits that concurrently inserted (or removed)
//  the exact same bytes at the same slot — tiny in practice.
#define WEAVE_SET_MAX 256

typedef struct {
    u8b tlv;
} weave;

ok64 WEAVEInit (weave *w);
void WEAVEReset(weave *w);
void WEAVEFree (weave *w);

//  A weave is empty (zero tokens) iff its TLV stream is empty.
fun b8 WEAVEEmpty(weave const *w) { return u8bDataLen(w->tlv) == 0; }

//  --- Builder: append tokens, RLE-encoding the I/R records --------------
//
//  `WEAVEBldInit` resets `w` and arms the builder over it.  Each
//  `WEAVEBldPut` appends one `T` record, preceded by an `I` and/or `R`
//  record only when the sorted set differs from the previous token's.
//  `iset` / `rset` are sorted, deduplicated u32 slices (rset empty =>
//  alive token).  The builder borrows `w`; it owns no memory.
typedef struct {
    weave *w;
    u32    previ[WEAVE_SET_MAX];
    u32    npi;
    u32    prevr[WEAVE_SET_MAX];
    u32    npr;
    b8     started;
} weavebld;

void WEAVEBldInit(weavebld *b, weave *w);
ok64 WEAVEBldPut (weavebld *b, u8csc text, u32cs iset, u32cs rset);

//  --- Cursor: sequential token reader ----------------------------------
//
//  `WEAVECurInit` arms a cursor over a built weave.  Each `WEAVECurNext`
//  advances to the next `T` token, exposing its bytes in `text` and its
//  decoded provenance in `iset[0..ni)` / `rset[0..nr)`.  Returns NO at
//  end of stream (or on a malformed record, with `bad` set).
typedef struct {
    u8cs rest;
    u8cs text;
    u32  iset[WEAVE_SET_MAX];
    u32  ni;
    u32  rset[WEAVE_SET_MAX];
    u32  nr;
    b8   bad;
} weavecur;

void WEAVECurInit(weavecur *c, weave const *w);
b8   WEAVECurNext(weavecur *c);

//  Emit `w`'s alive byte stream (tokens with empty R-set) into `out`
//  (reset on entry), in weave order.
ok64 WEAVEAliveBytes(weave const *w, u8b out);

//  Set helpers (used by external readers of the cursor).
fun b8 WEAVESetHas(u32 const *set, u32 n, u32 v) {
    for (u32 i = 0; i < n; i++) if (set[i] == v) return YES;
    return NO;
}
//  A token is spine (member of every revision) iff its I-set carries the
//  NCA bootstrap stamp 0 (or, defensively, is empty).
fun b8 WEAVEIsSpine(u32 const *iset, u32 ni) {
    return ni == 0 || WEAVESetHas(iset, ni, 0);
}

//  Build a one-version weave from raw blob bytes.  Tokenizes `data`
//  with the lexer for `ext`, splits multi-line tokens at '\n', stamps
//  every token I={src}, R={}.  Pass src=0 to mark all tokens as
//  pre-timeframe spine (NCA bootstrap).
ok64 WEAVEFromBlob(weave *w, u8cs data, u8cs ext, u32 src);

//  dst = src diffed against nu.  `nu` is a one-version weave produced
//  by WEAVEFromBlob; tokens the diff classifies as INS are copied from
//  nu into dst with I={src_commit}, R={}; tokens classified DEL gain
//  src_commit in their R-set; surviving tokens keep their provenance.
//  dst is reset before composition.  src and nu are read-only.
ok64 WEAVEDiff (weave *dst, weave const *src, weave const *nu, u32 src_commit);

//  dst = a merged with b (concurrent branches sharing an ancestor).
//  Both inputs must be weaves built incrementally from a common
//  ancestor.  EQ tokens reconcile by I=union, R=union; byte-equal
//  alive tokens that align across the two sides of a non-EQ run collapse
//  to ONE token whose I-set unions both inserters (concurrent identical
//  insert); divergent tokens emit on both sides with their own
//  provenance — the weave records every history.  No conflict-marker
//  bytes are ever stored.  dst is reset before composition.
ok64 WEAVEMerge(weave *dst, weave const *a, weave const *b);

//  WEAVEReplay: build a weave for a known merge commit.  Merge all
//  parents pairwise, then WEAVEDiff against the shipped result blob:
//  INS tokens (manual conflict-resolution bytes) get I={merge_in},
//  DEL tokens (dropped at the merge) gain merge_in in their R-set.  The
//  output weave's alive byte sequence equals `result_blob` exactly.
//  parents[] non-NULL, nparents >= 1; N==1 reduces to WEAVEDiff.
ok64 WEAVEReplay(weave *dst,
                 weave const *const *parents, u32 nparents,
                 u8cs result_blob, u8cs ext,
                 u32 merge_in);

// --- Diff emission ---
//
//  Walk a built weave and emit hunks classifying every relevant token by
//  its I/R-set membership in the `from` and `to` reachable sets.  Per
//  token (alive = some inserter reachable and no remover reachable):
//    alive_to && !alive_from -> 'I'  (inserted on the to-side)
//    alive_from && !alive_to -> 'D'  (deleted on the to-side)
//    alive_from && alive_to  -> context
//    else                    -> skipped
typedef b8 (*WEAVEsetfn)(u32 commit_h32, void *ctx);

ok64 WEAVEEmitDiff(weave const *w, u8cs name,
                   WEAVEsetfn in_from, void *from_ctx,
                   WEAVEsetfn in_to,   void *to_ctx,
                   HUNKcb cb, void *cb_ctx);

//  Like `WEAVEEmitDiff` but without context-windowing: walks every
//  classified token and ships them as hunks (split only past
//  `WEAVE_FULL_HUNK_MAX` bytes).  Backs the `cat:` projector.
ok64 WEAVEEmitFull(weave const *w, u8cs name,
                   WEAVEsetfn in_from, void *from_ctx,
                   WEAVEsetfn in_to,   void *to_ctx,
                   HUNKcb cb, void *cb_ctx);

// --- Conflict-aware merged-weave render ---
//
//  Emit alive bytes of a merged weave into `out`, framing divergent
//  regions with `<<<<` / `||||` / `>>>>` when the merge inputs disagree.
//  `preds[0..npreds)` carry one membership predicate per merge head;
//  tokens whose I-set is spine (`0` present) are member of every
//  predicate.  Conflict per non-EQ run: two alive tokens with disjoint
//  memberships.  `out` is reset on entry.  npreds <= 32.
ok64 WEAVEEmitMerged(weave const *w,
                     WEAVEsetfn const *preds, void *const *ctxs,
                     u32 npreds, u8b out);

#endif
