#ifndef GRAF_WEAVE_H
#define GRAF_WEAVE_H

//  WEAVE: token-level history of one file as a single interleaved-delta
//  TLV stream (SCCS-weave style).  One `u8b tlv` holds, in weave order:
//
//    T | ZINT128(seq, pos) | token-bytes
//        one token: its literal text, plus a stable BIRTH-ID (seq, pos)
//    R | ZINTblocked(delta(sorted remover-seqs))
//        run-length: sets the current REMOVER set for the following T's
//
//  `seq` is a deterministic topo index of the inserting revision (the
//  caller assigns it — a total, canonical topo-sort so the SAME commit
//  gets the SAME seq on both sides of a merge).  `pos` is the engine-
//  assigned ordinal of the token within that revision's insert.  Together
//  (seq, pos) is the token's IDENTITY: two tokens are the same logical
//  token iff their birth-ids are equal, keeping genuinely distinct
//  same-byte tokens (e.g. the two `a`s of "aa") apart.
//
//  A token is ALIVE iff its R-set is empty.  Recovery / blame / `diff:`
//  membership read only `seq` (the inserter) and the R-set; `pos` is
//  purely build-internal.  No lexer/syntax tag is stored.
//
//  The whole commit DAG is replayed into ONE weave: WEAVEFromBlob (one
//  root version), WEAVEDiff (linear chain step), WEAVEApply (DAG step —
//  diff a commit against its parent-closure view, insert/delete in place
//  without reordering, so concurrent branches coexist; a merge is just an
//  Apply spanning its parents' closures, no separate merge op).

#include "abc/B.h"
#include "abc/INT.h"
#include "abc/S.h"
#include "dog/HUNK.h"

con ok64 WEAVEFAIL = 0x2038a7ce3ca495;

//  Sentinel `seq` for the worktree shadow version (uncommitted edits) and
//  for synthetic conflict-marker tokens.  Real topo seqs are small; the
//  top of the u32 range is reserved.
#define WEAVE_WT_SRC 0xFFFFFFFFu
#define WEAVE_CFLCT_SRC 0xFFFFFFFEu

//  Record-type letters (uppercase => TLV-long form, so values may exceed
//  255 bytes).
#define WEAVE_REC_T 'T'
#define WEAVE_REC_R 'R'

//  Max removers decoded from one R record by the public cursor.
#define WEAVE_SET_MAX 256

typedef struct {
    u8b tlv;
} weave;

ok64 WEAVEInit (weave *w);
void WEAVEReset(weave *w);
void WEAVEFree (weave *w);

//  A weave is empty (zero tokens) iff its TLV stream is empty.
fun b8 WEAVEEmpty(weave const *w) { return u8bDataLen(w->tlv) == 0; }

//  --- Builder ----------------------------------------------------------
//
//  `WEAVEBldInit` resets `w` and arms the builder.  Each `WEAVEBldPut`
//  appends one `T` (birth-id (seq, pos) + text), preceded by an `R` record
//  only when the remover set changes from the previous token's.  `rset` is
//  a sorted, deduplicated u32 slice of remover seqs (empty => alive).
typedef struct {
    weave *w;
    u32    prevr[WEAVE_SET_MAX];
    u32    npr;
    b8     started;
} weavebld;

void WEAVEBldInit(weavebld *b, weave *w);
ok64 WEAVEBldPut (weavebld *b, u8csc text, u32 seq, u32 pos, u32cs rset);

//  --- Cursor: sequential token reader ----------------------------------
//
//  Each `WEAVECurNext` advances to the next `T` token, exposing its bytes
//  in `text`, its birth-id in `seq` / `pos`, and its current remover set
//  in `rset[0..nr)`.  Returns NO at end of stream (or on a malformed
//  record, with `bad` set).
typedef struct {
    u8cs rest;
    u8cs text;
    u32  seq;
    u32  pos;
    u32  rset[WEAVE_SET_MAX];
    u32  nr;
    b8   bad;
} weavecur;

void WEAVECurInit(weavecur *c, weave const *w);
b8   WEAVECurNext(weavecur *c);

//  Emit `w`'s alive byte stream (tokens with empty R-set) into `out`
//  (reset on entry), in weave order.
ok64 WEAVEAliveBytes(weave const *w, u8b out);

//  Membership predicate over inserter/remover seqs: is `seq` reachable in
//  a given scope (e.g. a commit's ancestor closure)?  `seq == 0` is the
//  spine in emission predicates (member of every set); WEAVEApply's
//  baseline uses it as a pure predicate instead.
typedef b8 (*WEAVEsetfn)(u32 seq, void *ctx);

//  Membership helper: is seq `v` in the (sorted) remover set `set`?
fun b8 WEAVESetHas(u32 const *set, u32 n, u32 v) {
    for (u32 i = 0; i < n; i++) if (set[i] == v) return YES;
    return NO;
}

//  Build a one-version weave from raw blob bytes.  Tokenizes `data` with
//  the lexer for `ext`, splits multi-line tokens at '\n', stamps every
//  token (seq=src, pos=token-index), R={}.  src=0 marks the spine.
ok64 WEAVEFromBlob(weave *w, u8cs data, u8cs ext, u32 src);

//  dst = src diffed against nu.  INS tokens are copied from nu with a
//  fresh birth-id (seq=src_commit, pos=insert-ordinal); DEL tokens gain
//  src_commit in their R-set; surviving tokens keep their birth-id.
ok64 WEAVEDiff (weave *dst, weave const *src, weave const *nu, u32 src_commit);

//  Single-weave DAG replay step: dst = src with commit `seq`'s edit
//  applied IN PLACE.  Unlike WEAVEDiff (baseline = all alive), the
//  baseline is src's alive view restricted to `base`/`base_ctx` — the
//  commit's PARENT-CLOSURE membership predicate over inserter seqs (and
//  remover seqs).  The commit's content `nu` (pre-tokenized like the
//  weave) is diffed against that baseline; surviving baseline tokens it
//  dropped gain `seq` as a remover, its new tokens insert (birth-id
//  (seq, pos)) anchored before the next baseline token.  Tokens outside
//  the baseline (concurrent branches, already-dead) are kept verbatim and
//  never reordered — the SCCS-weave invariant that kills cyclic-concurrent
//  transposition.  `base == NULL` ⇒ empty baseline (root commit).  A merge
//  commit just passes a predicate covering the UNION of its parents'
//  closures; there is no separate merge op.
ok64 WEAVEApply(weave *dst, weave const *src, weave const *nu,
                u32 seq, WEAVEsetfn base, void *base_ctx);

// --- Diff emission ---

ok64 WEAVEEmitDiff(weave const *w, u8cs name,
                   WEAVEsetfn in_from, void *from_ctx,
                   WEAVEsetfn in_to,   void *to_ctx,
                   HUNKcb cb, void *cb_ctx);

ok64 WEAVEEmitFull(weave const *w, u8cs name,
                   WEAVEsetfn in_from, void *from_ctx,
                   WEAVEsetfn in_to,   void *to_ctx,
                   HUNKcb cb, void *cb_ctx);

// --- Conflict-aware merged-weave render ---
ok64 WEAVEEmitMerged(weave const *w,
                     WEAVEsetfn const *preds, void *const *ctxs,
                     u32 npreds, u8b out);

#endif
