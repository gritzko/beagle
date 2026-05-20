#ifndef DOG_HOME_H
#define DOG_HOME_H

#include "abc/B.h"
#include "abc/BUF.h"
#include "abc/PATH.h"
#include "abc/URI.h"

con ok64 NOHOME    = 0x5d845858e;
con ok64 NOCONF    = 0x5d83185cf;
con ok64 HOMEOPEN  = 0x45858e619397;       // branch already open
con ok64 HOMEROBR  = 0x45858e6d82db;       // rw asked after a ro open,
                                           // or aux already pinned elsewhere
con ok64 HOMENOBR  = 0x45858e5d82db;       // no writable branch opened

#define HOME_ARENA_SIZE   (1ULL << 32)     // 4 GB VA, pages on demand
#define HOME_CONFIG_MAX   (1UL  << 16)     // 64 KB is plenty for .be/config
#define HOME_BRANCH_MAX   256              // canonical branch path cap

// Per-invocation ambient state for every dog.  Owned by the top of
// the call chain; each dog embeds a `home *base` in its state struct
// and propagates it through DOGOpen.  All HOME functions take `home
// *` as their first argument.
//
// Branch slots: at most two branches are ever loaded into the
// process at once — the writable one (the wt's current branch) and
// at most one read-only auxiliary for cross-branch reads
// (PATCH source, KEEPMoveCommits source, etc.).  No interning, no
// stack — just two `path8b` fields.
typedef struct {
    path8b root;     // repo root (where `.be/` lives), NUL-termed.
                     // Colocated default: equals `wt`.  Secondary
                     // worktrees override this from their `.be`
                     // file's `repo` URI so keeper/graf/spot open the
                     // shared store.
    path8b wt;       // worktree root (where `.be` lives — either the
                     // store dir or the secondary-wt wtlog file).
                     // May differ from `root` for secondary worktrees
                     // sharing a primary store.
    u8b    config;   // mmap of <root>/.be/config (empty if none)
    u8b    arena;    // scratch: 4 GB VA, stack-like consumption.  Each
                     // dog function must rewind the arena to its entry
                     // state before returning.  Use Bu8mark + Bu8rewind
                     // around any cross-dog call as a safety net.
    b8     rw;       // initial open mode for the home itself.

    //  Project segment — the first path component under `.be/` (see
    //  DOG.h §"Canonical on-disk layout").  Populated from the anchor
    //  URI by `home_anchor_resolve` via `DOGProjectFromBe`, or from
    //  the clone-URL basename on a fresh `be get`.  Empty during the
    //  layout migration window means "implicit single-project (legacy
    //  layout)" — readers may treat it as the project name being
    //  elided.  Distinct from `cur_branch`: `project` is wt-wide
    //  identity (set once at open), `cur_branch` is wt state (moves
    //  with each `get`/`post` row).
    u8b    project;

    //  Worktree current branch — the be-side branch path within the
    //  project, canonical form (empty = trunk, else trailing '/' as
    //  produced by `DPATHBranchNormFeed`).  Set by `HOMEOpenBranch`
    //  on the first open; re-targeted by `HOMESetCurBranch` on
    //  branch switches (KEEPSwitchBranch / GRAFSwitchBranch).  This
    //  is the single source of truth for "which branch is loaded";
    //  keeper / graf / spot consult it rather than carrying their
    //  own `leaf_branch` field.
    path8b cur_branch;  // Canonical form (DPATHBranchNormFeed):
                        // trunk = "", non-trunk = "path/" (no
                        // leading '/').  See `cur_held` to
                        // distinguish "trunk claimed" from
                        // "nothing claimed yet".
    b8     cur_held;    // YES once cur_branch has been claimed.
    b8     cur_rw;      // YES iff `cur_branch` was opened rw.

    //  Auxiliary branch — the optional second branch loaded for
    //  cross-branch reads (PATCH source, KEEPMoveCommits source).
    //  Empty when unused; never writable.  Pinned for the life of
    //  the home (a second non-matching ro-open returns HOMEROBR).
    path8b aux_branch;

    //  Worktree tip sha — the 40-hex sha learned from the wtlog at
    //  the top of the call chain (`be`) and forwarded to every dog
    //  via the `--at <root>?<branch>#<sha>` flag.  Empty when no tip
    //  is known (fresh clone, direct sub-dog invocation without
    //  `--at`).
    u8b    cur_sha;
} home;

// Initialize a `home` in place.  `at` is a URI carrying everything
// the home needs to know about its anchor:
//   path     → repo root (where `.be/` lives).  Empty → auto-detect
//              via HOMEFindDogs from cwd.
//   query    → current be-side branch path.  Empty == trunk.  When
//              non-empty (or path is set), HOMEOpenBranch is called
//              internally so slot 0 is claimed in one step.
//   fragment → current commit sha as 40 hex.  Empty when no tip is
//              recorded (fresh clone).
// rw=YES allows downstream dogs to populate `.be/` (per-branch dirs,
// puppy files).  Reserves the arena, mmaps `.be/config` if present.
//
// Pass an empty `uri` for the historical "empty `at`" behaviour.
ok64 HOMEOpen(home *h, uricp at, b8 rw);

// Path-only shim for tests / fixtures that have a repo root but no
// branch / sha to forward.  Wraps `HOMEOpen` with a URI carrying just
// `path = root`.  Empty `root` triggers the same cwd-walk fallback.
fun ok64 HOMEOpenAt(home *h, u8cs root, b8 rw) {
    uri at = {};
    if ($ok(root) && !u8csEmpty(root)) {
        at.path[0] = root[0];
        at.path[1] = root[1];
    }
    return HOMEOpen(h, &at, rw);
}

// Release arena, config mmap, and path buffer.
ok64 HOMEClose(home *h);

// Walk up from cwd to the first ancestor containing a `.be` anchor
// (either the store directory or the secondary-wt wtlog file).  Feeds
// the found path into h->root.  Returns NOHOME if the walk reaches /
// without finding one.
ok64 HOMEFind(home *h);

// Alias of HOMEFind kept for callers that want intent-named lookup.
ok64 HOMEFindDogs(home *h);

// Resolve a peer binary into `out`: same directory as `argv0`
// (preserving symlinks), or, if `argv0` has no '/', the PATH entry
// that holds it.  Falls back to feeding just `name`.
ok64 HOMEResolveSibling(home *h, path8b out, u8csc name, u8csc argv0);

// Read one value from <root>/.be/config (TOML) addressed by a dotted
// path-style `needle` — e.g. for `[a.b] c = "v"` caller builds
// `a_path(n, "a", "b", "c")` and passes `$path(n)`.  Feeds the value
// bytes into `value`, advancing value[0] past them.  Returns NOCONF if
// the file is absent or the needle doesn't resolve.  Lexer errors
// propagate.
ok64 HOMEGetConfig(home *h, u8s value, path8s needle);

// Local host name used as prefix for local-origin ref keys (e.g.
// `//<host>?master`, `//<host>?HEAD`).  Resolution order:
//   1. config `user.host`   (explicit override)
//   2. config `user.email`  (default identity)
//   3. NOCONF (caller decides what to do)
// Feeds the raw bytes into `out` and advances `out[0]` past them.
ok64 HOMEHost(home *h, u8s out);

// --- Branch slots ---
//
// Claims `branch` in `h->cur_branch` (first call) or `h->aux_branch`
// (second ro call on a different branch).  Normalizes the input via
// `DPATHBranchNormFeed` — trunk aliases `""`, `main`, `master`,
// `trunk`, and their `heads/` forms → `""`; non-trunk branches gain
// a trailing '/'.
//
// Semantics:
//   * cur empty                    → cur := branch, cur_rw := rw.
//   * cur == branch                → HOMEOPEN (cur_rw upgraded to rw
//                                    if requested and currently NO;
//                                    downgrade NO → rw still HOMEOPEN).
//   * cur set, rw==YES, different  → HOMEROBR (cur pinned elsewhere).
//   * cur set, rw==NO,  different  → aux slot:
//                                      aux empty   → aux := branch
//                                      aux==branch → HOMEOPEN
//                                      aux pinned  → HOMEROBR
//
// To re-target the wt's current branch (POST.c branch switch path,
// KEEPSwitchBranch / GRAFSwitchBranch internals), use
// `HOMESetCurBranch` instead — it does not enforce the pinning rule.
ok64 HOMEOpenBranch(home *h, u8cs branch, b8 rw);

// Re-target `h->cur_branch` to `new_branch` (normalized).  Used by
// keeper / graf switch helpers and by sniff's POST cross-branch path.
// No pinning — overwrites whatever cur_branch held.  Does not touch
// `aux_branch` or `cur_rw`.
ok64 HOMESetCurBranch(home *h, u8cs new_branch);

// Feeds `h->cur_branch` into `out`.  Returns HOMENOBR if cur was
// not opened rw (no writable branch in this process).
ok64 HOMEWriteBranch(home const *h, u8cs out);

// YES iff `branch` is an ancestor (prefix in canonical form) of
// `h->cur_branch` or `h->aux_branch`, or equals either.  Used by
// resolvers to decide whether a flat-stack entry's home branch is
// in scope.  `branch` must already be canonical (trunk=`""`, else
// trailing `/`).
b8 HOMEBranchVisible(home const *h, u8cs branch);

#endif
