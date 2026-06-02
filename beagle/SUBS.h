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

//  Relay one submodule's report.  Spawn `be <argv...>` (with `--tlv`
//  forced) in the sub mount `<wt_root>/<subpath>`, capture its TLV
//  hunk stream, then re-emit every hunk to stdout in the *parent's*
//  HUNKMode with each hunk URI's path rebased under `<subpath>`
//  (HUNKu8sRelay).  Hunks stay one sequential stream — never nested;
//  a sub-of-sub child already carries its own inner prefix, so the
//  prefixes accumulate.  This is the per-verb submodule-aggregation
//  step: the verb prints its own per-file report, then calls this
//  once per mounted sub so the affected files inside subs are listed
//  too.  Captured output is relayed even when the child exits
//  non-zero (e.g. a PATCH conflict); the child's status is then
//  returned (BEDOGEXIT / BEDOGSIG / OK).
ok64 BERelaySub(u8cs wt_root, u8cs subpath, u8css argv);

//  Resolve sibling `tool` (via HOMEResolveSibling), spawn it with
//  `argv`, wait, translate exit into BEDOGEXIT / BEDOGSIG / NONE
//  (low-byte *NONE residue) / OK.  `bg=YES` skips the wait — caller
//  reaps.  Defined in BE.cli.c.
ok64 BERun(u8csc tool, u8css argv, b8 bg);

//  --- `be get` sub-orchestration helpers (per-row spawn + recurse).
//  Live in beagle/SUBS.c — purely BE-side orchestration; the actual
//  mount/unmount syscalls land via `sniff sub-mount` (a separate
//  process) and inline `unlink`.

//  Spawn `keeper subs ?<query>` and capture its ULOG output into
//  `out`.  Empty `out` on the no-sub case (still OK).
ok64 BEGetKeeperSubs(u8cs query, u8bp out);

//  Spawn `sniff sub-mount ./<subpath>#<pin>` from the parent wt to
//  do a first-time mount (anchor + WIREFetchAll + checkout) in a
//  clean keeper state.  cwd inherits the parent process's cwd.
ok64 BEGetSubMount(u8cs subpath, u8cs pin);

//  Unmount a sub: unlink `<wt>/<subpath>/.be`.  Leaves the wt files
//  in place.  Idempotent.  Logs `be: get <subpath>: unmounted`.
ok64 BEGetSubUnmount(u8cs wt_root, u8cs subpath);

//  Iterate `keeper subs` ULOG rows.  Per row: spawn `sniff sub-mount`
//  for the pin, then BERecurseInto with `be get [flags] ?<pin>` cwd =
//  mount.  `flag_head`/`flag_term` carry the flags to forward.
ok64 BEGetDrainSubs(u8cs wt_root, u8cs subs_ulog,
                    u8cs *flag_head, u8cs *flag_term);

//  Walk `baseline_ulog`; unmount any sub not present in `target_ulog`.
//  Skips paths whose anchor is already gone.  Worst per-row code or OK.
ok64 BEGetDrainRemoved(u8cs wt_root, u8cs baseline_ulog,
                       u8cs target_ulog);

#endif
