#ifndef SNIFF_POST_H
#define SNIFF_POST_H

//  POST: two-phase commit-and-promote (see VERBS.md §POST).
//
//  Phase 1 — Commit-if-staged: any wt edits or PATCH-staged content
//  produces one new single-parent commit on cur (trailing words become
//  the message).  Selective vs implicit (commit-all) mode is determined
//  by presence of put/delete rows in scope.
//
//  Phase 2 — Promote: when the URI names another branch, ff-or-rebase
//  cur's stack onto that branch's tip via REFSCompareAndAppend on the
//  expected base.  Concurrent posters that move the target see REFSCAS.
//  Bare `post msg` runs only phase 1.
//
//  Single-parent invariant on the write path: every commit POST emits
//  has exactly one parent.  PATCH no longer chains `&<theirs>` onto
//  baseline; the multi-parent commit body builder has been removed
//  along with `POST_MAX_PARENTS` and `post_add_patch_parents`.

#include "SNIFF.h"
#include "keeper/KEEP.h"

//  Commit the wt to a branch.  `target_branch` overrides the branch
//  the commit lands on:
//    * empty (path[0] == NULL or len 0) — use the wt's recorded
//      baseline branch (same-branch POST).
//    * non-empty — cross-branch POST: the new commit goes on
//      `?<target_branch>`, the wt's recorded baseline branch is
//      left untouched in REFS, and `.sniff` is reset to
//      `(target_branch, new_tip)`.  Refused when the target
//      branch's REFS tip exists and is not an ancestor of the wt's
//      recorded base (non-ff).
ok64 POSTCommit(u8cs reporoot, u8cs target_branch,
                u8cs message, u8cs author, sha1 *sha_out);

//  Compose default message and author for a bare `be post` whose
//  ULOG carries one or more `patch` rows since the latest get/post.
//  Walks `SNIFFAtPatchChain`, fetches each absorbed commit's body via
//  keeper, and writes:
//    * msg_buf   : last entry's subject + " (+N)" when N=patches-1>0.
//    * auth_buf  : last entry's author identity ("Name <email>"); if
//                  any other entry's identity differs, " (et al)" is
//                  injected before the `<email>`.
//  Slices `*msg_out` / `*auth_out` are repointed into the freshly
//  written buffer regions and remain valid for as long as the caller
//  keeps the buffers alive.  Returns OK with `*n_out>0` when defaults
//  were composed, ULOGNONE when no patch rows are present (caller
//  falls back to the dry-run / refuse-empty arm).
ok64 POSTPatchDefaults(u8cs reporoot,
                       Bu8 msg_buf,  u8cs *msg_out,
                       Bu8 auth_buf, u8cs *auth_out,
                       u32 *n_out);

//  Dry run: walk the same change-set the next POSTCommit would
//  build, print one line per changed path to stdout (`M/A/D path`),
//  and a `sniff: <n> change(s)` summary to stderr.  No commit, no
//  REFS, no ULOG mutation.  Wired to bare `sniff post` (no -m,
//  no `?label`) so the user can sanity-check before committing.
ok64 POSTPrintStatus(u8cs reporoot);

//  Record `ref_uri → ?<sha_hex>` in keeper/refs via REFSAppend.
//  `ref_uri` is the URI the user typed on the CLI — e.g. `?heads/main`
//  or `?tags/v0.0.1` — passed straight through (`c->uris[i].data`).
//  `sha_hex` must be exactly 40 hex chars.
ok64 POSTSetLabel(u8cs ref_uri, u8cs sha_hex);

//  Cross-branch promote (no-msg `be post ?<X>` shapes per VERBS.md
//  §POST).  `target_branch` is the absolute branch path the user
//  named (already absolutised from `?./X`/`?../X`/`?..` by the
//  caller); empty means trunk.  Decides between four shapes:
//
//    target == dirname(cur)           ?..              upstream-promote
//    target startswith cur+'/'        ?./X (existing)  promote-into-child
//    target startswith cur+'/' (gap)  ?./newleaf       create-on-miss
//    other absolute existing branch   ?<absolute>      peer-promote
//
//  Operands per shape and cur auto-sync rules: see post_promote
//  body and the table in VERBS.md.  Returns:
//    OK          — promote landed (REFS advanced).
//    POSTNONE    — nothing to do (already in sync).
//    GRAFCNFL    — three-way merge conflict mid-rebase.
//    REFSCAS     — concurrent advance of target REFS row.
//    SNIFFFAIL   — generic dispatcher / resource error.
ok64 POSTPromote(u8cs reporoot, u8cs target_branch, b8 allow_create);

//  PUT-side branch creation (VERBS.md §PUT, `?branch` aspect).
//  Creates the named branch as a label at cur.tip without producing
//  a commit.  Refuses with `PUTDUP` if the branch already exists.
//  Reuses POSTPromote's create-on-miss arm; returns OK on success.
ok64 POSTCreateBranch(u8cs reporoot, u8cs target_branch);

//  Rebase cur's stack onto an arbitrary sha (no branch lookup).
//  Used by `be post //remote` (VERBS.md §"POST"): the target tip
//  is resolved via REFSResolve from the cached tracking ref log
//  in the dispatcher; this function just runs the replay +
//  cur-REFS advance + wt reset.
//
//    base_old = LCA(cur_tip, *target_tip)
//    base_new = *target_tip
//    child_tip = cur_tip
//
//  Cur's REFS row is CAS-advanced to the rebased tip; wt is reset
//  via GETCheckout.  Returns OK on success, POSTNONE when cur is
//  already at *target_tip, GRAFCNFL on conflict, REFSCAS on race.
ok64 POSTRebaseOntoSha(u8cs reporoot, sha1 const *target_tip);

#endif
