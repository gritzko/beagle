#ifndef DOG_WEAVE_H
#define DOG_WEAVE_H

//  WEAVE (DOG-003): one file's whole DAG history as a COLUMNAR,
//  HUNK-compatible structure-of-arrays.  Supersedes the interleaved-TLV
//  graf/WEAVE: the old per-call 7-buffer decode (`wdec`) IS the canonical
//  form now, so `weave` is a zero-copy parsed view over a serialized blob
//  and the builders write a fresh blob into a caller-owned `u8s`.
//
//    text     all token bytes, concatenated in weave order        ('X')
//    toks     one tok32 per token: syntax tag + {in,rm} bits +     ('K')
//             custom bit + end-offset into `text`
//    ins      blocked-ZINT: one inserter index per IN-bit token    ('I')
//    rms      blocked-ZINT: remover index(es) per RM-bit token     ('M')
//    commits  index -> commit id (hi64 of the commit sha1);        ('C')
//             commits[0] is the SPINE/root (the in-bit-off inserter)
//
//  A token's IDENTITY is (commits[inserter], per-commit ordinal): the
//  ordinal is its position among that commit's tokens in weave order, so
//  it is recomputed during merge and NOT stored (no `pos` column).  Real
//  commit ids (not a positional topo seq) make WEAVEMerge a deterministic
//  shared-sequence interleave — the DIS-003 fix.
//
//  A token is ALIVE iff no remover is reachable in the scope of interest.
//  The whole DAG folds in: WEAVENext extends a linear chain by one commit;
//  WEAVEMerge unions two parents' weaves; there is no separate apply op.

#include "abc/B.h"
#include "abc/BIT.h"
#include "abc/INT.h"
#include "abc/S.h"
#include "dog/HUNK.h"

con ok64 WEAVEFAIL = 0x2038a7ce3ca495;

//  'W' outer TLV container.  'X'/'K' are byte-identical to HUNK's text/tok
//  sub-records (so a weave projects to a `hunk` for free); 'I'/'M'/'C' are
//  weave-only.
#define WEAVE_TLV      'W'
#define WEAVE_TLV_TXT  HUNK_TLV_TXT   // 'X'  token bytes
#define WEAVE_TLV_TOK  HUNK_TLV_TOK   // 'K'  tok32[] (LE)
#define WEAVE_TLV_INS  'I'            // blocked-ZINT inserter indices
#define WEAVE_TLV_RMS  'M'            // blocked-ZINT remover indices (+counts)
#define WEAVE_TLV_CMT  'C'            // blocked-ZINT commit-id table

//  commits[0] = spine/root (ancestor of every rev; the IN-bit-off
//  inserter).  Reserved commit ids for non-commit token sources:
#define WEAVE_SPINE     0u                    // commits[] index of the spine
#define WEAVE_WT_ID     ((u64)~(u64)0)        // worktree shadow (uncommitted)
#define WEAVE_CFLCT_ID  ((u64)(~(u64)0 - 1))  // synthetic conflict markers

typedef struct {
    u8cs    text;     // 'X'
    tok32cs toks;     // 'K'
    u8cs    ins;      // 'I'
    u8cs    rms;      // 'M'
    u64cs   commits;  // 'C'  commits[0] = spine
} weave;

//  tok32 side bits in a STORED weave (rewritten to display eq/in/rm only
//  at emit time, per scope).  `in`/`rm` are independent; `custom` with
//  `rm` set means MULTIPLE removers (a count + N indices in `rms`).
#define WEAVE_IN  TOK_SIDE_IN   // 1: explicit inserter -> 1 index in `ins`
#define WEAVE_RM  TOK_SIDE_RM   // 2: dead -> remover index(es) in `rms`

fun b8 WEAVEEmpty(weave const *w) { return (u32)$len(w->toks) == 0; }

// --- codec -----------------------------------------------------------
ok64 WEAVEParse    (weave *w, u8csc blob);      // zero-copy view over a 'W' blob
ok64 WEAVESerialize(u8s into, weave const *w);  // (builders write 'W' directly)

// --- builders: write a fresh 'W' blob into `into` --------------------
//  Next: diff `w`'s alive view against `new_blob` (tokenized by `ext`);
//  survivors keep identity, dropped survivors gain `commit` as a remover,
//  new tokens insert with inserter=`commit`.  `w` NULL/empty => from-blob.
ok64 WEAVENext (u8s into, weave const *w, u8csc new_blob, u8csc ext, u64 commit);
//  Merge: union of `a` and `b` keyed on token identity; a token dead in
//  either side is dead in the result; concurrent runs order by commit id.
//  `merge_commit` stamps any content the merge itself introduces (0 = none).
ok64 WEAVEMerge(u8s into, weave const *a, weave const *b, u64 merge_commit);

// --- sequential read: consume the weave's slices in lockstep ----------
//  No stateful cursor — copy `*w` into a local `weave c` and step it.
//  Each WEAVEStep consumes one token: it advances c.toks/c.text (and, per
//  the IN/RM/custom bits, c.ins/c.rms) and `*off` (the running token-end;
//  tok32 stores cumulative ends), then fills `out`.  The slices self-track
//  (CLAUDE §1) — no indices, no `bad` flag.  Loop while c.toks is non-empty;
//  WEAVEStep returns OK per token or WEAVEFAIL on a malformed stream, so it
//  composes with call()/try().  Re-scan: re-copy `*w` into `c`.
typedef struct {
    u8cs  text;      // this token's bytes
    u8    tag;       // syntax tag (for render + faithful carry-through)
    b8    has_in;    // IN bit: inserter is explicit (else the spine)
    u32   inserter;  // inserter commit index (WEAVE_SPINE if in-off)
    u32cs rms;       // remover commit indices, a view (empty => alive at tip)
} weavetok;

ok64 WEAVEStep(weave *c, u32 *off, weavetok *out);

// --- scope: active-commit bitmap over commits[] (abc/BIT u1) ----------
//  bit i set <=> commits[i] is reachable in the rev/closure of interest;
//  bit 0 (spine) is always set.  Classification is a single u1At on a
//  token's inserter/remover index — no per-token lookup.  A merge's
//  combined scope is `u1sOr` of the parents'.
typedef u1cs weavescope;
//  Fill `into` (caller-acquired, >= ncommits bits) so bit i = (i==0 ||
//  commits[i] in `active`).  `active` is a rev's reachable commit-id set,
//  supplied by graf's DAG (the weave stores no parent edges).
ok64 WEAVEScope(u1b *into, weave const *w, u64cs active);

// --- produce / emit over scopes --------------------------------------
ok64 WEAVEAlive  (weave const *w, u8b out);                   // tip (rm-clear)
ok64 WEAVEProduce(weave const *w, weavescope scope, u8b out); // bytes at any rev
//  Windowed and whole-file diff from `from`-scope to `to`-scope, emitted
//  as HUNK records (DIFF-003/004: `name`, `scheme`, `navver` URIs).
ok64 WEAVEEmitDiff(weave const *w, u8cs name, u8cs navver,
                   weavescope from, weavescope to, HUNKcb cb, void *ctx);
ok64 WEAVEEmitFull(weave const *w, u8cs name, u8cs scheme, u8cs navver,
                   weavescope from, weavescope to, HUNKcb cb, void *ctx);
//  Conflict-aware render: per-group membership; divergent regions framed
//  with render-time-only `<<<<`/`||||`/`>>>>` markers (never stored).
ok64 WEAVEEmitMerged(weave const *w, weavescope const *groups, u32 ngroups,
                     u8b out);

#endif
