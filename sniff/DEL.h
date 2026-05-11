#ifndef SNIFF_DEL_H
#define SNIFF_DEL_H

//  DELETE: record explicit removal intent in the ULOG.
//
//  Append-only mirror of PUT: one `delete <path>` row per input URI.
//  No tree work, no pack writes — POST resolves the effective tree
//  at commit time.
//
//  nuris==0 is a no-op (bare `delete` → POST includes every tracked
//  file that's missing from disk, same sweep rule as PUT).
//
//  Each `uri` in `uris` is used for its path component only.

#include "SNIFF.h"
#include "abc/URI.h"

ok64 DELStage(u32 nuris, uri const *uris);

//  DELBranch: drop a label by writing a tombstone REFS row
//  (`post ?<branch>#0000…0`).  Default refuses if the branch has
//  any active descendant labels in REFS (use `recursive=YES` to
//  drop the whole subtree depth-first), or if the wt's `.be/wtlog`
//  baseline is currently on the branch being deleted (would
//  orphan the wt).  Cross-wt safety is the user's responsibility.
//
//  Returns OK on success, SNIFFFAIL on safety refusal.
//  Idempotent: deleting an already-tombstoned branch is a no-op.
ok64 DELBranch(uri const *u, b8 recursive);

#endif
