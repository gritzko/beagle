#!/bin/sh
#  get/29-file-worktree — `be get file:<beagle-store>` wires a local
#  SIBLING WORKTREE (VERBS.md §"Worktree management" Example 2).
#
#  The target's `.be/` is a beagle store, so be takes the worktree
#  path (NOT the git-clone transport path): the new cwd's `.be`
#  becomes a regular wtlog FILE whose row-0 `repo` URI points back at
#  the shared store, and the store's tip tree is checked out.
#
#  Companion to get/30-file-git-clone, which drives the SAME
#  single-slash `file:<abs>` form at a git repo and must instead
#  clone.  Together they pin be_file_get_route's on-disk-type routing:
#  beagle store → worktree, git repo → clone.

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh shielded $SCRATCH/.be; drop it so the subdirs below
#  bootstrap as their own fresh stores / worktrees.
rm -rf "$SCRATCH/.be"

# ---------------------------------------------------------------------
# 1. primary beagle store + a seed commit (a file and a subdir file).
# ---------------------------------------------------------------------
mkdir prim
(
    cd prim
    printf 'v1\n' > x.txt
    mkdir d && printf 'v2\n' > d/y.txt
    "$BE" put x.txt d/y.txt >/dev/null 2>&1
    "$BE" post '#seed'      >/dev/null 2>&1
) || { echo "FAIL(setup): primary seed failed" >&2; exit 1; }
[ -d prim/.be ] || { echo "FAIL(setup): prim/.be should be a store dir" >&2; exit 1; }

# ---------------------------------------------------------------------
# 2. sibling worktree wired via the single-slash `file:<abs>` form.
# ---------------------------------------------------------------------
mkdir wt
(
    cd wt
    "$BE" get "file:$SCRATCH/prim" > get.out 2> get.err
) || { cat wt/get.err >&2; echo "FAIL: be get file:<store> (worktree)" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. assertions — secondary wt shape + checked-out tree.
#    `.be` is a regular FILE (the local wtlog), not a dir or symlink.
# ---------------------------------------------------------------------
[ -f wt/.be ] && [ ! -d wt/.be ] \
    || { echo "FAIL: wt/.be must be a regular file (wtlog)" >&2; ls -la wt >&2; exit 1; }
[ ! -L wt/.be ] \
    || { echo "FAIL: wt/.be must not be a symlink" >&2; exit 1; }
grep -q 'repo' wt/.be \
    || { echo "FAIL: wt/.be row-0 should carry a repo anchor" >&2; cat -v wt/.be >&2; exit 1; }

match prim/x.txt   wt/x.txt
match prim/d/y.txt wt/d/y.txt

echo "get/29-file-worktree: OK"
