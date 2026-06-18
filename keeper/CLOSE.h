#ifndef KEEPER_CLOSE_H
#define KEEPER_CLOSE_H

//  CLOSE: object-closure walk + sorted sha membership set.
//
//  The reachable-object closure of a commit — the commit plus every
//  parent commit, every tree, and every blob it points at — is the
//  unit every pack builder ships.  Both wire ends need it:
//    * the push client (WIRECLI) walks the local tip's closure minus
//      the peer's have-set to build the pack it uploads;
//    * the upload-pack server (WIRE) walks a want's closure minus the
//      client's have-set to build the thin pack it serves (GIT-005).
//  This module is the one implementation both consume — no per-caller
//  re-roll of the tree/commit drain (CLAUDE §13).
//
//  `close_set` is a sorted sha1 array with binary-search membership;
//  insertion keeps it sorted and deduped.  Backing storage is caller-
//  owned (a_carve / a_pad), so the walk never allocates.
//
//  The walk is have-aware: a sha already in the `have` set is treated
//  as closed — not emitted, its sub-graph not enumerated (the peer is
//  assumed to hold it).  Submodule gitlinks (160000) are never
//  recursed into — they live in a different shard.  Missing objects in
//  haveset-build mode (out == NULL) are tolerated so a partial local
//  history doesn't abort the walk; in pack-build mode (out != NULL) a
//  missing object is a hard error.

#include "abc/INT.h"
#include "abc/OK.h"
#include "dog/git/SHA1.h"
#include "keeper/KEEP.h"

con ok64 CLOSEFULL = 0x0c3f5d5d86d8616;   //  out array hit its cap

//  Sorted sha1 set with binary-search membership.  `items` is caller-
//  owned backing of capacity `cap`; `n` is the live count.
typedef struct {
    sha1 *items;
    u32   n;
    u32   cap;
} close_set;

//  YES iff `q` is present.  Binary search; O(log n).
b8   CLOSESetHas(close_set const *s, sha1cp q);

//  Insert `v` keeping the array sorted and deduped.  No-op on a full
//  set or a duplicate.
void CLOSESetAdd(close_set *s, sha1cp v);

//  Walk the closure of the commit at `commit_sha` (parents included),
//  appending each reachable commit/tree/blob sha into `out[0..*n)`
//  (capacity `cap`).  See the file banner for have-pruning,
//  submodule, and missing-object semantics.
//
//    out          — sha1 array to append into, or NULL for haveset
//                   build mode (only `add_to_have` is populated).
//    n / cap      — live count + capacity of `out`.  CLOSEFULL when
//                   `out` overflows in pack-build mode.
//    have         — shas to treat as already-present (skip + don't
//                   recurse), or NULL.
//    add_to_have  — set to also record every visited sha into, or
//                   NULL.  Used to build a have-set from a tip; also
//                   the within-closure dedup (pass the same set you
//                   pass as the local-walk `seen`).
ok64 CLOSEWalkCommit(sha1cp commit_sha, sha1 *out, u32 *n, u32 cap,
                     close_set const *have, close_set *add_to_have);

//  Tree-only variant of CLOSEWalkCommit: walks the tree at `tree_sha`
//  and its blobs/subtrees.  Same have-pruning + dedup contract.
ok64 CLOSEWalkTree(sha1cp tree_sha, sha1 *out, u32 *n, u32 cap,
                   close_set const *have, close_set *add_to_have);

#endif
