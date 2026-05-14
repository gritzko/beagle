#ifndef SNIFF_PATCH_H
#define SNIFF_PATCH_H

//  PATCH: 3-way worktree merge driven by graf.
//
//  `be patch ?<target>` resolves the current branch tip (ours),
//  the target ref / sha (theirs), and their LCA (base) via graf,
//  then walks the three trees in tandem and applies per-file
//  actions to the worktree: take-target, leave-ours, or emit the
//  3-way merged bytes produced by `GRAFGet <path>?<ours>&<theirs>`.
//
//  Every byte that ends up on disk comes through graf; sniff owns
//  only the filesystem writes, sniff-registry updates, and
//  classification against the LCA.  No commit is created — a
//  subsequent `sniff post` picks up the merged wt as staged
//  content.
//
//  See VERBS.md §PATCH for the user-facing semantics.

#include "abc/INT.h"
#include "abc/BUF.h"
#include "abc/URI.h"

//  PATCH URI shapes (see VERBS.md §PATCH four-shapes table and
//  sniff/AT.md "Patch row shapes").  Classified from URIPattern(u)
//  bits plus u8csEmpty(u->fragment):
//
//    PATCH_SQUASH       URI_QUERY only                — `?br`
//    PATCH_CHERRY       URI_FRAGMENT only             — `#hash`
//    PATCH_MERGE        QUERY + FRAGMENT (non-empty)  — `?br#msg`
//    PATCH_REBASE_ONE   QUERY + FRAGMENT (empty)      — `?br#`
#define PATCH_SHAPE_BAD     0
#define PATCH_SHAPE_SQUASH  1
#define PATCH_SHAPE_CHERRY  2
#define PATCH_SHAPE_MERGE   3
#define PATCH_SHAPE_REBASE1 4

//  Classify the user's PATCH URI into one of the four shapes above.
//  Returns PATCH_SHAPE_BAD if the URI is neither query- nor
//  fragment-bearing.  Path-scoped URIs are routed to PATCHApplyFile
//  by the caller and are not classified here.
u8 PATCHShape(uricp u);

//  Apply a 3-way merge from `u` (the user's PATCH URI) into the wt.
//
//  `reporoot`  absolute path of the wt root (already resolved by
//              the caller via HOMEOpen).
//  `u`         the user's parsed URI.  PATCH classifies via
//              PATCHShape(u) and writes a `patch` row whose URI
//              carries the resolved commit sha:
//                  squash      → `?<sha>`
//                  cherry-pick → `#<sha>`
//                  merge       → `?<sha>#<msg>`
//                  rebase-one  → `?<sha>#`
//
//  Returns OK on clean merge (exit 0).  Returns PATCHCFLCT when at
//  least one path ended up with conflict markers or a modify/delete
//  clash — conflict paths are logged to stderr; callers map this to
//  a non-zero CLI exit.
ok64 PATCHApply(u8cs reporoot, uricp u);

//  Single-file variant: merge one path only.  Everything else in
//  the wt is left alone.  `frag` follows the same contract as
//  `PATCHApply` (cherry-pick when `target_query` is empty).
ok64 PATCHApplyFile(u8cs reporoot, u8cs filepath,
                    u8cs target_query, u8cs frag);

// --- Error / sentinel codes ---

con ok64 PATCHFAIL     = 0x1929d3113ca495;   // general failure
con ok64 PATCHCFLCT    = 0x64a74c44c3d531d;  // conflicts recorded in wt
con ok64 PATCHDIRTY    = 0x64a74c44d49b762;  // wt has dirty files that
                                             // would be clobbered
con ok64 PATCHURELT    = 0x64a74c45e6ce55d;  // no shared ancestor
con ok64 PATCHBUSY     = 0x1929d3112de722;   // merge already in progress —
                                             // baseline is a `patch` row;
                                             // complete with `be post` or
                                             // abort by checking out the
                                             // pre-patch commit
con ok64 PATCHDET      = 0x64a74c44d39d;     // detached wt: PATCH refuses
                                             // (per VERBS.md Invariant 7);
                                             // re-attach to a branch first

#endif
