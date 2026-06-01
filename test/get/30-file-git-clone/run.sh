#!/bin/sh
#  get/30-file-git-clone — `be get file:<git-repo>` clones a LOCAL git
#  repo and checks out its tree.
#
#  The target is a plain git repo (not a beagle store), so be routes
#  the single-slash `file:<abs>` form through the transport/clone plan
#  (BEActKeeperGet → keeper get → git-upload-pack spawned locally,
#  exactly like ssh://), NOT the sibling-worktree plan.  Mirrors the
#  user-facing `be get file:/home/gritzko/src/dogs`.
#
#  Companion to get/29-file-worktree (same URI form, beagle-store
#  target → worktree).  --nosub: the fixture declares no submodules,
#  so the clone stays fully offline.

. "$(dirname "$0")/../../lib/case.sh"

#  Silence git 2.30+ "initial branch name" hint; keep init quiet and
#  stable across git versions with `-b master`.
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so `dest` below anchors at
#  its own fresh store.
rm -rf "$SCRATCH/.be"

# ---------------------------------------------------------------------
# 1. seed a local git source repo: one commit, a file + a subdir file.
# ---------------------------------------------------------------------
git init -b master src >/dev/null 2>&1 || { echo "FAIL(setup): git init" >&2; exit 1; }
git -C src config user.email t@t
git -C src config user.name  T
printf 'hello\n' > src/a.txt
mkdir src/sub && printf 'world\n' > src/sub/b.txt
git -C src add -A >/dev/null
git -C src commit -qm "init" >/dev/null 2>&1 || { echo "FAIL(setup): git commit" >&2; exit 1; }
SRC_TIP=$(git -C src rev-parse master | cut -c1-12)
[ -n "$SRC_TIP" ] || { echo "FAIL(setup): src tip unreadable" >&2; exit 1; }

# ---------------------------------------------------------------------
# 2. clone via the single-slash `file:<abs>` form into a fresh dest.
#    Shield dest with its own empty `.be/` so HOMEFindDogs anchors
#    here instead of walking up into an ancestor store.
# ---------------------------------------------------------------------
mkdir dest dest/.be
(
    cd dest
    "$BE" get --nosub "file:$SCRATCH/src" > get.out 2> get.err
) || { cat dest/get.err >&2; echo "FAIL: be get file:<git-repo> (clone)" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. the tree materialised, byte-exact with the source blobs.
# ---------------------------------------------------------------------
[ -f dest/a.txt ] \
    || { echo "FAIL: a.txt not checked out" >&2; find dest ! -path '*/.be/*' >&2; exit 1; }
[ -f dest/sub/b.txt ] \
    || { echo "FAIL: sub/b.txt not checked out" >&2; exit 1; }
match src/a.txt     dest/a.txt
match src/sub/b.txt dest/sub/b.txt

# ---------------------------------------------------------------------
# 4. proof it went through the wire fetch (keeper wrote a refs row
#    referencing the source tip), not some local copy.
# ---------------------------------------------------------------------
shard=$(ls -d dest/.be/*/ 2>/dev/null | head -1)
[ -n "$shard" ] && [ -f "${shard}refs" ] \
    || { echo "FAIL: no project shard refs after clone" >&2; ls -la dest/.be >&2; exit 1; }
grep -q "$SRC_TIP" "${shard}refs" \
    || { echo "FAIL: ${shard}refs has no row for src tip $SRC_TIP" >&2
         cat -v "${shard}refs" >&2; exit 1; }

echo "get/30-file-git-clone: OK"
