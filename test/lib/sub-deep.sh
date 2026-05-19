# test/lib/sub-deep.sh — 3-level forest fixture (parent → sub → leaf).
# Each level pins its child as a 160000 gitlink in its tree.  Used by
# depth-3 head/get cases where the recursion mechanism is exercised
# at every level.
#
# After sourcing, the calling test has three bare git upstream repos
# and per-commit SHAs:
#
#   $LEAF_BARE    $LEAF_URL    $LEAF_TIP       leaf bare + ssh URL + tip
#   $SUB_BARE     $SUB_URL     $SUB_TIP        sub  bare + ssh URL + tip
#   $PARENT_BARE  $PARENT_URL  $PARENT_TIP     parent bare + ssh URL + tip
#
# Pinning:
#   $SUB_TIP    has .gitmodules pinning vendor/leaf → $LEAF_TIP
#   $PARENT_TIP has .gitmodules pinning vendor/sub  → $SUB_TIP
#
# Layout produced under $SCRATCH:
#
#   leaf.git/    bare leaf upstream    (1 commit, leaf.txt)
#   sub.git/    bare sub  upstream    (1 commit, sub.txt + .gitmodules
#                                       + gitlink vendor/leaf)
#   parent.git/  bare parent upstream  (1 commit, main.c + .gitmodules
#                                       + gitlink vendor/sub)
#
# Single commit per project — depth-3 cases test the recursion shape,
# not commit-history navigation.
#
# Requires passwordless ssh to localhost; same $HOME-relative
# constraint as submodules.sh.  Register sourcing cases in
# test/CMakeLists.txt's `_BE_NEEDS_SSH_CASES`.

. "$(dirname "$0")/../../lib/branches.sh"

export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || fail "sub-deep.sh: \$HOME unset"
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) fail "sub-deep.sh: SCRATCH=$SCRATCH not under \$HOME=$HOME" ;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

# case.sh's `$SCRATCH/.be/` would trip `be_sub_shard_setup`; drop it.
rmdir "$SCRATCH/.be" 2>/dev/null || true

LEAF_BARE="$SCRATCH/leaf.git"
SUB_BARE="$SCRATCH/sub.git"
PARENT_BARE="$SCRATCH/parent.git"
LEAF_URL="ssh://localhost/$REL_SCRATCH/leaf.git"
SUB_URL="ssh://localhost/$REL_SCRATCH/sub.git"
PARENT_URL="ssh://localhost/$REL_SCRATCH/parent.git"

LEAF_SEED="$ETMP/leaf-seed"
SUB_SEED="$ETMP/sub-seed"
PARENT_SEED="$ETMP/parent-seed"
rm -rf "$LEAF_SEED" "$SUB_SEED" "$PARENT_SEED"

git_init_seed() {
    _seed=$1
    git init "$_seed" >/dev/null
    git -C "$_seed" config user.email t@t
    git -C "$_seed" config user.name  T
    git -C "$_seed" checkout -b master >/dev/null 2>&1 || true
}

# --- leaf upstream ----------------------------------------------------
git init --bare "$LEAF_BARE" >/dev/null
git_init_seed "$LEAF_SEED"
echo 'leaf payload v1' > "$LEAF_SEED/leaf.txt"
git -C "$LEAF_SEED" add leaf.txt >/dev/null
git -C "$LEAF_SEED" commit -qm 'leaf: initial'
LEAF_TIP=$(git -C "$LEAF_SEED" rev-parse HEAD)
git -C "$LEAF_SEED" push -q "$LEAF_BARE" master:master

# --- sub upstream (pins leaf) -----------------------------------------
git init --bare "$SUB_BARE" >/dev/null
git_init_seed "$SUB_SEED"
echo '/* sub payload v1 */' > "$SUB_SEED/sub.txt"
cat > "$SUB_SEED/.gitmodules" <<EOF
[submodule "vendor/leaf"]
	path = vendor/leaf
	url = $LEAF_URL
EOF
git -C "$SUB_SEED" add sub.txt .gitmodules >/dev/null
git -C "$SUB_SEED" update-index --add --cacheinfo \
    160000,"$LEAF_TIP",vendor/leaf
git -C "$SUB_SEED" commit -qm 'sub: with vendor/leaf'
SUB_TIP=$(git -C "$SUB_SEED" rev-parse HEAD)
git -C "$SUB_SEED" push -q "$SUB_BARE" master:master

# --- parent upstream (pins sub) ---------------------------------------
git init --bare "$PARENT_BARE" >/dev/null
git_init_seed "$PARENT_SEED"
echo 'int main(void) { return 0; }' > "$PARENT_SEED/main.c"
cat > "$PARENT_SEED/.gitmodules" <<EOF
[submodule "vendor/sub"]
	path = vendor/sub
	url = $SUB_URL
EOF
git -C "$PARENT_SEED" add main.c .gitmodules >/dev/null
git -C "$PARENT_SEED" update-index --add --cacheinfo \
    160000,"$SUB_TIP",vendor/sub
git -C "$PARENT_SEED" commit -qm 'parent: with vendor/sub'
PARENT_TIP=$(git -C "$PARENT_SEED" rev-parse HEAD)
git -C "$PARENT_SEED" push -q "$PARENT_BARE" master:master

export LEAF_BARE SUB_BARE PARENT_BARE
export LEAF_URL  SUB_URL  PARENT_URL
export LEAF_TIP  SUB_TIP  PARENT_TIP

note "sub-deep.sh: parent=$PARENT_BARE tip=$PARENT_TIP"
note "sub-deep.sh: sub   =$SUB_BARE tip=$SUB_TIP"
note "sub-deep.sh: leaf  =$LEAF_BARE tip=$LEAF_TIP"
