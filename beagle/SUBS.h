#ifndef BEAGLE_SUBS_H
#define BEAGLE_SUBS_H

//  Beagle-side submodule recursion plumbing.  Two pieces:
//
//    * BESubsHere — one-level enumeration of submodules declared in
//      `<wt_root>/.gitmodules`, with the on-disk mount state of each.
//      Read-only; no `be` invocation.  Used by the per-verb wrappers
//      to decide what to recurse into.
//
//    * BERecurseInto — fork + chdir(<wt>/<sub>) + execvp(self, argv)
//      to re-enter the dispatcher inside a child project.  Each
//      child runs its own becli_inner against its own HOME and may
//      recurse further.
//
//  See SUBS.plan.md §"Shared mechanism".

#include "abc/INT.h"
#include "abc/S.h"

//  One declared sub at this level.
typedef struct {
    u8cs path;       // mount path, tree-relative (no leading '/')
    u8cs url;        // upstream URL from .gitmodules
    b8   mounted;    // YES iff <wt_root>/<path>/.be is a regular file
} besub;

//  Per-entry callback.  Slices in `*s` point into caller-internal
//  scratch valid only for the duration of the callback.  A non-OK
//  return aborts enumeration and is propagated.
typedef ok64 (*besub_cb)(besub const *s, void *ctx);

//  Enumerate the submodules declared in `<wt_root>/.gitmodules`.
//  Absence of the file is OK (zero callbacks).  Malformed
//  `.gitmodules` returns SUBSPARSE.
ok64 BESubsHere(u8cs wt_root, besub_cb cb, void *ctx);

//  fork + chdir(<wt_root>/<subpath>) + execvp(self, argv).  The
//  self-path is resolved from `/proc/self/exe` so the child reuses
//  the same `be` binary the parent was invoked as.  `argv` is the
//  child's argv (caller-owned, must include argv[0] convention but
//  the child's argv[0] is overwritten by the resolved self-path so
//  HOMEResolveSibling finds the same bin dir).
//
//  Returns:
//    OK         — child exited zero.
//    BEDOGEXIT  — child exited non-zero.  Code surface mirrors
//                 BERun's so the caller can aggregate uniformly.
//    BEDOGSIG   — child killed by signal.
ok64 BERecurseInto(u8cs wt_root, u8cs subpath, u8css argv);

#endif
