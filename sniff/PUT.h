#ifndef SNIFF_PUT_H
#define SNIFF_PUT_H

//  PUT: ref-writer for the worktree (VERBS.md §PUT).  Two surfaces:
//
//    1. Path-form (`be put <path>`, `be put <dir>/`, `be put <old>#<new>`)
//       — append one `put` row per URI to the ULOG; no tree work, no
//       pack writes, no hashing.  POST does the real work at commit
//       time by walking the baseline tree against the wt.
//
//    2. Branch-form (`be put ?<branch>`, `be put ?<branch>#<sha>`,
//       legacy explicit-sha label form) — write the branch's REFS
//       row; the `#<sha>` shape additionally migrates the sha's
//       reachable closure into the target shard via KEEPMoveCommits.
//
//  None of these creates a commit — POST is the only commit-maker.

#include "SNIFF.h"
#include "abc/URI.h"

ok64 PUTStage(u32 nuris, uri const *uris);

//  `be put ?<branch>` — create the named branch as a label at cur.tip
//  with no commit.  Refuses with `PUTDUP` when the branch already
//  exists; delegates to POSTPromote's create-on-miss arm.
ok64 PUTCreateBranch(u8cs reporoot, u8cs target_branch);

//  `be put ?<branch>#<sha>` — write `?<target_branch> → ?<sha-hex>`
//  to keeper REFS.  Creates the branch when it doesn't yet exist and
//  migrates `<sha>`'s FP-reachable closure into the target's shard
//  via `KEEPMoveCommits` so a reader opening only that shard finds
//  every commit / tree / blob the new ref covers.  Refuses with
//  `SNIFFFAIL "no shared ancestry"` when the FP chain from `<sha>`
//  doesn't reach the existing `?<target_branch>.tip`.  Caller must
//  have validated `sha_hex` (40 hex chars, resolvable in keeper).
ok64 PUTSetBranch(u8cs reporoot, u8cs target_branch, u8cs sha_hex);

//  Append `<ref_uri> → ?<sha_hex>` to keeper REFS, canonicalising the
//  ref URI first.  `ref_uri` is the URI the user typed on the CLI
//  (e.g. `?heads/main`, `?tags/v0.0.1`); `sha_hex` must be 40 hex
//  chars.  Materialises the per-branch keeper shard for non-trunk
//  labels.  Used by ref-set flows that bypass the branch-form
//  dispatcher (sniff/SNIFF.exe.c's tag-set / explicit-sha-set paths).
ok64 PUTSetLabel(u8cs ref_uri, u8cs sha_hex);

#endif
