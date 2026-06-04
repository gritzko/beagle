# test/lib/submodules.sh — bare-upstream submodule fixture for `be`
# submodule tests.  Sourced from a case's run.sh; layers on top of
# branches.sh (which provides BE, KEEPER, ETMP, fail/note, head_hex).
#
# After sourcing, the calling test has two bare git upstream repos and
# the per-commit SHAs to drive scenarios against:
#
#   $SUB_BARE / $PARENT_BARE          bare upstream paths (under $SCRATCH)
#   $SUB_URL  / $PARENT_URL           ssh://localhost/<rel> URLs
#   $SUB_C1   $SUB_C2  $SUB_C3        sub commit SHAs
#   $PARENT_C1 $PARENT_C2 $PARENT_C3  parent commit SHAs
#   $SUB_TIP=$SUB_C3, $PARENT_TIP=$PARENT_C3
#   $PARENT_PINNED=$SUB_C2            gitlink target recorded in $PARENT_C3
#   $FIX                              path to test/lib (fixture .c files)
#
# Layout produced under $SCRATCH:
#
#   sub.git/      bare sub upstream    (3 commits, files: core.c helper.c)
#   parent.git/   bare parent upstream (3 commits, files: main.c util.c
#                                       .gitmodules; gitlink vendor/sub
#                                       pinned to $PARENT_PINNED)
#
# The fixture deliberately pins the parent gitlink at SUB_C2, not the
# sub tip — leaves room for tests that exercise "advance sub from C2
# to C3, re-commit parent with the new pin" without further setup.
#
# Submodule tests then `mkdir wt && cd wt && "$BE" get "$PARENT_URL?master"`
# and drive `be` from there.
#
# Requires passwordless ssh to localhost.  Cases that source this MUST
# register themselves in test/CMakeLists.txt's `_BE_NEEDS_SSH_CASES`
# list — keeper resolves `ssh://localhost/<rel>` as $HOME-relative on
# the peer side, which is why $SCRATCH must live under $HOME.

. "$(dirname "$0")/../../lib/branches.sh"

# Silence the git 2.30+ "initial branch name" hint; we pin `master`.
export GIT_CONFIG_GLOBAL=/dev/null

# POST-001 phase 2: an ssh:// (git) parent does NOT recurse into subs
# by default — keeper/beagle sources do.  This fixture builds an ssh
# (git) parent, so every recursing verb here must opt in with `--sub`.
# Wrap $BE in a generated shim that appends `--sub` to each call so the
# whole fixture (and the suites built on it) keep exercising the
# recurse path unchanged.  `--nosub` still wins inside `be`, so cases
# that pass it explicitly are unaffected.  The shim lives under $ETMP
# (process-scoped); $REAL_BE keeps the unwrapped binary for any case
# that needs it.
REAL_BE="$BE"
_BE_SHIM="$ETMP/be-sub-shim"
mkdir -p "$(dirname "$_BE_SHIM")"
cat > "$_BE_SHIM" <<SHIM
#!/bin/sh
exec "$REAL_BE" "\$@" --sub
SHIM
chmod +x "$_BE_SHIM"
BE="$_BE_SHIM"
export BE REAL_BE

# Keeper's ssh-side path resolution requires $SCRATCH under $HOME.
[ -n "${HOME:-}" ] || fail "submodules.sh: \$HOME unset"
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) fail "submodules.sh: SCRATCH=$SCRATCH not under \$HOME=$HOME" ;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

# case.sh creates `$SCRATCH/.be/` as part of its standard scratch
# setup, which makes any inner wt look like a sub-mount of $SCRATCH
# and triggers `be_sub_shard_setup` on the first `be get`.  For
# submodule tests we want a clean primary-wt layout under $SCRATCH/wt
# (parent project at the cwd root); drop the case.sh `.be/` here.
rmdir "$SCRATCH/.be" 2>/dev/null || true

SUB_BARE="$SCRATCH/sub.git"
PARENT_BARE="$SCRATCH/parent.git"
SUB_URL="ssh://localhost/$REL_SCRATCH/sub.git"
PARENT_URL="ssh://localhost/$REL_SCRATCH/parent.git"

FIX=$(cd "$(dirname "$0")/../../lib" && pwd)

# Seed dirs live under $ETMP (not $SCRATCH) so they don't pollute the
# wt the calling test will create inside $SCRATCH.
SUB_SEED="$ETMP/sub-seed"
PARENT_SEED="$ETMP/parent-seed"
rm -rf "$SUB_SEED" "$PARENT_SEED"

# --- sub upstream -----------------------------------------------------
git init --bare "$SUB_BARE" >/dev/null
git init "$SUB_SEED" >/dev/null
git -C "$SUB_SEED" config user.email t@t
git -C "$SUB_SEED" config user.name  T
git -C "$SUB_SEED" checkout -b master >/dev/null 2>&1 || true

# C1: core.c initial
cp "$FIX/subfix_sub_core_v1.c" "$SUB_SEED/core.c"
git -C "$SUB_SEED" add core.c >/dev/null
git -C "$SUB_SEED" commit -qm 'sub: core skeleton'
SUB_C1=$(git -C "$SUB_SEED" rev-parse HEAD)

# C2: core.c extended + helper.c added
cp "$FIX/subfix_sub_core_v2.c"   "$SUB_SEED/core.c"
cp "$FIX/subfix_sub_helper_v1.c" "$SUB_SEED/helper.c"
git -C "$SUB_SEED" add core.c helper.c >/dev/null
git -C "$SUB_SEED" commit -qm 'sub: helper + inc'
SUB_C2=$(git -C "$SUB_SEED" rev-parse HEAD)

# C3: core.c refactored (adds sub_add)
cp "$FIX/subfix_sub_core_v3.c" "$SUB_SEED/core.c"
git -C "$SUB_SEED" add core.c >/dev/null
git -C "$SUB_SEED" commit -qm 'sub: add-by-n'
SUB_C3=$(git -C "$SUB_SEED" rev-parse HEAD)

git -C "$SUB_SEED" push -q "$SUB_BARE" master:master

# --- parent upstream --------------------------------------------------
git init --bare "$PARENT_BARE" >/dev/null
git init "$PARENT_SEED" >/dev/null
git -C "$PARENT_SEED" config user.email t@t
git -C "$PARENT_SEED" config user.name  T
git -C "$PARENT_SEED" checkout -b master >/dev/null 2>&1 || true

# C1: main.c only
cp "$FIX/subfix_parent_main_v1.c" "$PARENT_SEED/main.c"
git -C "$PARENT_SEED" add main.c >/dev/null
git -C "$PARENT_SEED" commit -qm 'parent: main'
PARENT_C1=$(git -C "$PARENT_SEED" rev-parse HEAD)

# C2: main.c calls util_double + util.c added
cp "$FIX/subfix_parent_main_v2.c" "$PARENT_SEED/main.c"
cp "$FIX/subfix_parent_util_v1.c" "$PARENT_SEED/util.c"
git -C "$PARENT_SEED" add main.c util.c >/dev/null
git -C "$PARENT_SEED" commit -qm 'parent: util_double'
PARENT_C2=$(git -C "$PARENT_SEED" rev-parse HEAD)

# C3: main.c calls sub, util.c gets util_square, submodule added at
# vendor/sub pinned to SUB_C2.  `git submodule add` would clone the
# url eagerly; bypass via plumbing (cacheinfo 160000) to keep the seed
# offline.
PARENT_PINNED="$SUB_C2"
cp "$FIX/subfix_parent_main_v3.c" "$PARENT_SEED/main.c"
cp "$FIX/subfix_parent_util_v2.c" "$PARENT_SEED/util.c"
cat > "$PARENT_SEED/.gitmodules" <<EOF
[submodule "vendor/sub"]
	path = vendor/sub
	url = $SUB_URL
EOF
git -C "$PARENT_SEED" add main.c util.c .gitmodules >/dev/null
git -C "$PARENT_SEED" update-index --add --cacheinfo \
    160000,"$PARENT_PINNED",vendor/sub
git -C "$PARENT_SEED" commit -qm 'parent: vendor sub at C2'
PARENT_C3=$(git -C "$PARENT_SEED" rev-parse HEAD)

git -C "$PARENT_SEED" push -q "$PARENT_BARE" master:master

# Auxiliary branches so tests can fetch specific commits by name
# (keeper's wire side doesn't support fetching by raw sha — it picks
# from the peer's advertised refs).  `prev` = C2 (last commit before
# the sub was added); `mid` = C1 (initial main.c only).
git -C "$PARENT_SEED" push -q "$PARENT_BARE" "$PARENT_C2:refs/heads/prev"
git -C "$PARENT_SEED" push -q "$PARENT_BARE" "$PARENT_C1:refs/heads/mid"

PARENT_TIP="$PARENT_C3"
SUB_TIP="$SUB_C3"

export SUB_BARE PARENT_BARE SUB_URL PARENT_URL FIX
export SUB_C1 SUB_C2 SUB_C3 SUB_TIP
export PARENT_C1 PARENT_C2 PARENT_C3 PARENT_TIP PARENT_PINNED

note "submodules.sh: parent=$PARENT_BARE tip=$PARENT_TIP"
note "submodules.sh: sub   =$SUB_BARE tip=$SUB_TIP (parent pin=$PARENT_PINNED)"
