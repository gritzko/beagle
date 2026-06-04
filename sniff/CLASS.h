#ifndef SNIFF_CLASS_H
#define SNIFF_CLASS_H

//  CLASS — baseline ⊕ worktree path classifier.
//
//  Single chokepoint for "is this path tracked / untracked / on-disk
//  but absent / both?".  Builds two parallel ULOG streams (baseline
//  tree via `KEEPTreeULog`, wt via `SNIFFWtULog`), heap-merges them
//  by URI path through `SNIFFMergeWalk`, and dispatches one step per
//  distinct path to the caller's callback.
//
//  Replaces the per-caller "build a path-set in memory + linear
//  scan" pattern that PUT, DEL, and bare `sniff` were duplicating.
//  Same primitives POST already uses for its commit-time merge —
//  this file is just the read-only flavour with no decision rows.
//
//  Usage: caller provides a step callback; one call per distinct
//  path with the path slice, the kind, and pointers to the matching
//  baseline / wt ULOG records.  Slices and record pointers stay
//  valid only for the duration of the callback.

#include "abc/INT.h"
#include "abc/URI.h"
#include "dog/ULOG.h"

con ok64 CLASSFAIL = 0xc54a71c3ca495;

typedef enum {
    CLASS_BASE_ONLY = 1,   // path in baseline tree, not on disk
    CLASS_WT_ONLY   = 2,   // path on disk, not in baseline tree
    CLASS_BOTH      = 3,   // path in both
} class_kind;

typedef struct {
    u8cs        path;       // borrowed slice into ULOG buffers
    class_kind  kind;       // base/wt presence
    ulogreccp   base_rec;   // baseline tree row (NULL if absent)
    ulogreccp   wt_rec;     // wt scan row       (NULL if absent)
    ulogreccp   put_rec;    // .be/wtlog `put`  row since last post (NULL if none)
    ulogreccp   del_rec;    // .be/wtlog `del`  row since last post (NULL if none)
} class_step;

typedef ok64 (*class_cb)(class_step const *step, void *ctx);

//  Build baseline + wt + staged-put + staged-delete ULOG streams,
//  heap-merge by URI key, fan to `cb` per distinct path.  Skips
//  submodule entries (`gitlinks`).  Empty / no-baseline → all wt
//  paths surface as `CLASS_WT_ONLY`.  `cb` may return any non-OK to
//  abort the walk; OK to continue.
//
//  Reads the keeper + sniff singletons (caller has both open).
ok64 SNIFFClassify(class_cb cb, void *ctx);

//  YES iff the wt bytes at `<reporoot>/<rel>` hash equal to the
//  baseline blob sha encoded in `base_rec->uri.fragment` (40-hex).
//  Used by CLASS_BOTH consumers (`be`, `be ls:`, …) to distinguish
//  "touched-unchanged" (mtime drift, content matches baseline → `ok`)
//  from a real `mod`.  NO on any I/O or hash error, on a missing
//  baseline sha, or on a kind the helper can't hash (only regular
//  files and symlinks are supported — same set CLASS classifies).
b8 CLASSWtEqBase(u8cs reporoot, ulogreccp base_rec, u8cs rel);

//  Verdict for a CLASS_BOTH step (path present in both baseline and
//  wt, no in-scope put/del intent).
typedef enum {
    CLASS_WT_CLEAN    = 0,   // bytes == baseline (`ok` / `eq`)
    CLASS_WT_PATCHED  = 1,   // mtime stamps a `patch` row (`pat`)
    CLASS_WT_MODIFIED = 2,   // bytes differ from baseline (`mod`)
} class_wt_state;

//  Classify a CLASS_BOTH `step` into CLEAN / PATCHED / MODIFIED, the
//  one body of truth bare `be` status and `be ls:` share (DIS-023).
//
//  A `patch`-stamped file is PATCHED (merged-but-uncommitted bytes the
//  user must see).  Otherwise the file is CLEAN iff its on-disk bytes
//  still hash to the baseline blob sha — CONFIRMED by content, never by
//  mtime alone.  The mtime stamp-set hit is only a fast hint that the
//  file *should* be baseline; an editor that restores a stamped mtime
//  onto changed bytes (build tool, `touch -r`, rsync) would otherwise
//  read as clean and silently hide a real modification.  Read-only:
//  hashes on-disk bytes, never writes a stamp.
class_wt_state CLASSWtState(u8cs reporoot, class_step const *step);

#endif
