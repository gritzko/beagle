#!/bin/sh
#  get/05-submodule-from-ssh — `be get` materialises a submodule from
#  an ssh:// (git) URL by DEFAULT.  Recursion is unconditional: every
#  source type recurses into declared subs unless `--nosub` is passed.
#  This test exercises the default recursive clone of an ssh parent.
#
#  Layout (under $SCRATCH, which lives under $HOME so keeper's
#  remote-HOME-relative path resolution works):
#
#      sub.git/        bare upstream of the sub
#      parent.git/     bare upstream of the parent (has .gitmodules
#                      pointing at ssh://localhost/<rel>/sub.git and
#                      a 160000 gitlink at vendor/sub)
#      wt/             the working tree we `be get` the parent into
#
#  After GET we expect:
#    1. wt/main.c                  — parent blob
#    2. wt/vendor/sub/lib.h        — sub blob
#    3. wt/vendor/sub/.be          — regular file (secondary-wt anchor),
#                                    row-0 `repo` URI naming the sub store
#    4. wt/.be/sub/                — sub store dir (basename of sub.git
#                                    minus the `.git` suffix)
#    5. wt/.be/wtlog               — parent's wtlog with one `get` row
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

# Silence the "initial branch name" hint git 2.30+ prints on every
# `git init`; we pin `master` explicitly with `checkout -b master`.
export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || { echo "get/05: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "get/05: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       echo "        keeper's ssh-side path resolution requires it" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

SUB_BARE="$SCRATCH/sub.git"
PARENT_BARE="$SCRATCH/parent.git"
SUB_SEED="$SCRATCH/sub-seed"
PARENT_SEED="$SCRATCH/parent-seed"
REL_SUB="$REL_SCRATCH/sub.git"
REL_PARENT="$REL_SCRATCH/parent.git"

#  case.sh pre-creates `$SCRATCH/.be/` as ambient state; with that
#  placeholder present, `be get` inside `wt/` would treat it as a
#  subdir-of-existing-repo and lay down a nested shard.  This test
#  wants the canonical primary-wt-clones-a-sub layout from
#  SUBS.plan.md (`wt/.be/` colocated, `wt/.be/sub/` for the submodule),
#  so drop the placeholder first.  Mirrors submodules.sh.
rmdir "$SCRATCH/.be" 2>/dev/null || true

cd "$SCRATCH"

# ====================================================================
# 1. seed sub.git with a single file (lib.h) on master
# ====================================================================
git init --bare "$SUB_BARE" >/dev/null
git init "$SUB_SEED" >/dev/null
git -C "$SUB_SEED" config user.email t@t
git -C "$SUB_SEED" config user.name  T
git -C "$SUB_SEED" checkout -b master >/dev/null 2>&1 || true
echo "#define SUB 1" > "$SUB_SEED/lib.h"
git -C "$SUB_SEED" add lib.h >/dev/null
git -C "$SUB_SEED" commit -qm 'sub: initial'
git -C "$SUB_SEED" push -q "$SUB_BARE" master:master

# ====================================================================
# 2. seed parent.git with main.c, a .gitmodules blob, and a 160000
#    gitlink at vendor/sub.  We bypass `git submodule add` (which
#    insists on cloning the remote URL eagerly through its own URL
#    rewriting machinery) and build the tree from plumbing: cacheinfo
#    100644 for main.c + .gitmodules, cacheinfo 160000 for vendor/sub.
#    The pinned sub commit sha is taken from the sub.git we just
#    pushed to.  This gives us exactly the on-disk shape Phase 3 of
#    MODULES.plan.md is meant to read.
# ====================================================================
SUB_TIP=$(git -C "$SUB_BARE" rev-parse master)
git init --bare "$PARENT_BARE" >/dev/null
git init "$PARENT_SEED" >/dev/null
git -C "$PARENT_SEED" config user.email t@t
git -C "$PARENT_SEED" config user.name  T
git -C "$PARENT_SEED" checkout -b master >/dev/null 2>&1 || true
echo 'int main(void){return 0;}' > "$PARENT_SEED/main.c"
cat > "$PARENT_SEED/.gitmodules" <<EOF
[submodule "vendor/sub"]
	path = vendor/sub
	url = ssh://localhost/$REL_SUB
EOF
git -C "$PARENT_SEED" add main.c .gitmodules >/dev/null
git -C "$PARENT_SEED" update-index --add --cacheinfo 160000,"$SUB_TIP",vendor/sub
git -C "$PARENT_SEED" commit -qm 'parent: initial w/ submodule'
git -C "$PARENT_SEED" push -q "$PARENT_BARE" master:master

# ====================================================================
# 3. THE TEST: `be get ssh://localhost/<rel>/parent.git?master` into wt/
# ====================================================================
#  Shield wt with its own empty `.be/` so HOMEFindDogs anchors here
#  instead of walking up into $HOME/.be (a normal user-store with
#  unrelated projects).  Tests that need a fresh wt must create
#  their own .be explicitly — see CLAUDE.md / test/lib/case.sh.
mkdir wt wt/.be && cd wt
"$BE" get "ssh://localhost/$REL_PARENT?master" \
    > 01.get.got.out 2> 01.get.got.err
#  `be get` now prints a status report (`<HH:MM>\tnew\t<path>` per
#  materialised file) to stdout — see commit 366259dd "reflect the
#  commit difference in be get status report".  Assert the expected
#  paths appear; don't pin the timestamp.
for f in .gitmodules main.c; do
    grep -qE "^[[:space:]]*[0-9:]+[[:space:]]+new[[:space:]]+$f$" \
            01.get.got.out || {
        echo "FAIL: missing 'new $f' line in stdout" >&2
        cat 01.get.got.out >&2
        exit 1
    }
done

# ====================================================================
# 4. parent files materialised
# ====================================================================
[ -f main.c ] || { echo "FAIL: wt/main.c missing" >&2; exit 1; }
grep -q 'int main' main.c || {
    echo "FAIL: wt/main.c content unexpected:" >&2
    cat main.c >&2; exit 1
}

# ====================================================================
# 5. sub mount materialised at vendor/sub/
# ====================================================================
[ -d vendor/sub ] || {
    echo "FAIL: wt/vendor/sub/ not created" >&2
    ls -la vendor 2>&1 >&2 || true
    exit 1
}
[ -f vendor/sub/lib.h ] || {
    echo "FAIL: wt/vendor/sub/lib.h missing" >&2
    ls -la vendor/sub 2>&1 >&2 || true
    exit 1
}
grep -q 'SUB 1' vendor/sub/lib.h || {
    echo "FAIL: wt/vendor/sub/lib.h content unexpected:" >&2
    cat vendor/sub/lib.h >&2; exit 1
}

# ====================================================================
# 6. secondary-wt anchor: vendor/sub/.be is a regular file, and its
#    row-0 `repo` URI points at the sub store dir.
# ====================================================================
[ -f vendor/sub/.be ] || {
    echo "FAIL: vendor/sub/.be should be a regular file" >&2
    ls -la vendor/sub/.be 2>&1 >&2 || true
    exit 1
}
[ ! -d vendor/sub/.be ] || {
    echo "FAIL: vendor/sub/.be must NOT be a directory" >&2
    exit 1
}

# ====================================================================
# 7. Sub store under the wt's keeper, sibling-keyed by URL basename
#    (sub.git → wt/.be/sub/).  Top-level subdir of `.be/`, same shape
#    as the trunk dir (NNNNN.keeper, refs, wtlog).
# ====================================================================
[ -d .be ] || { echo "FAIL: wt/.be should be a colocated store dir" >&2; exit 1; }
[ -d .be/sub ] || {
    echo "FAIL: parent keeper missing sub shard at wt/.be/sub/" >&2
    ls -la .be 2>&1 >&2 || true
    exit 1
}
#  Sub-shard must hold the sub's own keeper data (not piggy-back on
#  the parent's trunk).  Minimum invariant: a non-empty REFS reflog
#  for the sub (the wire fetch lands a peer-key `get` row at the
#  leaf).
[ -s .be/sub/refs ] || {
    echo "FAIL: wt/.be/sub/refs empty — sub keeper data" >&2
    echo "      not landing in the shard (regression on SUBS.plan.md)" >&2
    ls -la .be/sub 2>&1 >&2 || true
    exit 1
}

# ====================================================================
# 8. parent wtlog has the `get` row (one ULOG entry past row-0 repo)
# ====================================================================
[ -f .be/wtlog ] || { echo "FAIL: parent wtlog missing" >&2; exit 1; }
