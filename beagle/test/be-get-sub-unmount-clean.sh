#!/bin/sh
#  be-get-sub-unmount-clean.sh — SUBS-008.  When `be get ?<branch>`
#  switches to a target tree where a gitlink has been removed (the sub
#  AND its `.gitmodules` entry are gone), GET must FULLY clean the
#  mount: unmount the `.be` anchor AND remove the sub's checked-out
#  worktree files + internal metadata under the parent wt.  The bug
#  (SUBS-008) was a *logical* unmount only — `vendor/sub/.be` removed
#  and the path pruned, but `core.c`, `helper.c`, `..be.idx` left
#  orphaned on disk.
#
#  Behavior pinned here (the cleanup choice): removing a gitlink in the
#  target tree removes the WHOLE sub mount directory `vendor/sub/` —
#  the sub's objects live in the parent shard, so the checked-out copy
#  under the parent wt is pure clutter once the gitlink is gone, exactly
#  like `git submodule deinit` + a tree-switch that drops the path.
#
#  Modelled on be-post-nosub.sh: seeds two git repos over an
#  ssh-reachable path (sub + parent-with-gitlink-on-master,
#  no-gitlink-on-norm), be-get clones the parent recursively (mounting
#  the sub) on master, then switches to norm and asserts the mount is
#  gone from disk.
#
#  Skips cleanly when sshd-to-localhost is not configured.
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
BE="$BIN/be"

TEST_ID=${TEST_ID:-be-get-sub-unmount-clean}
#  Shared isolated repo-setup.  ssh-clones a SOURCE repo whose basename
#  names the remote-side keeper shard ($HOME/.be/<basename>), so the
#  source dirs MUST be uniquely named (not generic) or they corrupt the
#  developer's REAL $HOME/.be store.  We name them under $TEST_ID-*.
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
mkdir -p "$TMP"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }
skip() { echo "SKIP: $*" >&2; exit 0; }

#  ssh-localhost required for the ssh:// clone path; skip when absent.
if ! ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new \
        -o ConnectTimeout=5 localhost true 2>/dev/null; then
    skip "ssh localhost not reachable in batch mode"
fi

export GIT_CONFIG_GLOBAL=/dev/null

#  Helper: $HOME-relative ssh URL ($HOME-rooted; git strips leading /).
home_uri() {
    case "$1" in
        "$HOME"/*) printf 'ssh://localhost%s\n' "${1#$HOME}" ;;
        *) skip "TMP=$TMP not under \$HOME — adjust TMP to use ssh path" ;;
    esac
}

# --- 1. seed the sub upstream ----------------------------------------
echo "=== 1. seed sub git repo (core.c + helper.c) ==="
SUB="$TMP/$TEST_ID-sub"; mkdir -p "$SUB"
git init --quiet -b master "$SUB"
git -C "$SUB" config user.email t@t
git -C "$SUB" config user.name  t
echo "int core(void){return 1;}"   > "$SUB/core.c"
echo "int helper(void){return 2;}" > "$SUB/helper.c"
git -C "$SUB" add core.c helper.c
git -C "$SUB" commit --quiet -m "sub: core + helper"
SUB_C1=$(git -C "$SUB" rev-parse HEAD)
SUB_URI=$(home_uri "$SUB")
note "sub sha=$SUB_C1  uri=$SUB_URI"

# --- 2. seed the parent: master has gitlink, norm drops it -----------
echo "=== 2. seed parent: master = vendor/sub gitlink; norm drops it ==="
PARENT="$TMP/$TEST_ID-parent"; mkdir -p "$PARENT"
git init --quiet -b master "$PARENT"
git -C "$PARENT" config user.email t@t
git -C "$PARENT" config user.name  t
echo "int main(void){return 0;}" > "$PARENT/main.c"
cat > "$PARENT/.gitmodules" <<EOF
[submodule "vendor/sub"]
	path = vendor/sub
	url = $SUB_URI
EOF
git -C "$PARENT" add main.c .gitmodules
#  cacheinfo gitlink — pins vendor/sub at SUB_C1 without an eager clone.
git -C "$PARENT" update-index --add --cacheinfo \
    160000,"$SUB_C1",vendor/sub
git -C "$PARENT" commit --quiet -m "parent: vendor sub"
PARENT_C1=$(git -C "$PARENT" rev-parse HEAD)

#  Branch `norm`: same parent minus the gitlink AND the .gitmodules
#  entry — a tree where vendor/sub no longer exists at all.
git -C "$PARENT" checkout --quiet -b norm
git -C "$PARENT" rm --quiet --cached vendor/sub
rm -f "$PARENT/.gitmodules"
git -C "$PARENT" add -A
git -C "$PARENT" commit --quiet -m "norm: drop vendor/sub gitlink"
git -C "$PARENT" checkout --quiet master
PARENT_URI=$(home_uri "$PARENT")
note "parent master=$PARENT_C1  uri=$PARENT_URI (norm drops vendor/sub)"

# --- 3. be get --sub parent?master: mounts vendor/sub ----------------
echo "=== 3. be get --sub parent?master (mounts vendor/sub) ==="
WT="$TMP/wt"; rs_wt_at "$WT"
GETLOG="$TMP/be-get-master.log"
if ! timeout 30 "$BE" get --sub "$PARENT_URI?master" >"$GETLOG" 2>&1; then
    cat "$GETLOG" >&2
    fail "be get --sub master failed"
fi
[ -f main.c ]              || { cat "$GETLOG" >&2; fail "main.c missing after clone"; }
[ -f vendor/sub/.be ]      || { cat "$GETLOG" >&2; fail "vendor/sub not mounted"; }
[ -f vendor/sub/core.c ]   || { cat "$GETLOG" >&2; fail "vendor/sub/core.c missing"; }
[ -f vendor/sub/helper.c ] || { cat "$GETLOG" >&2; fail "vendor/sub/helper.c missing"; }
note "parent + vendor/sub mounted (core.c, helper.c present)"

# --- 4. fetch norm + switch to it (gitlink removed in target) --------
echo "=== 4. be get ssh://parent?norm then be get ?norm ==="
#  Fetch norm's pack into the store first (cached switch needs it).
FETCHLOG="$TMP/be-fetch-norm.log"
if ! timeout 30 "$BE" get "$PARENT_URI?norm" >"$FETCHLOG" 2>&1; then
    cat "$FETCHLOG" >&2
    fail "be get ssh parent?norm (fetch+switch) failed"
fi

#  After switching to norm, vendor/sub's gitlink + .gitmodules entry are
#  gone from the target tree.  GET must unmount AND remove the wt files.
SWLOG="$TMP/be-get-norm.log"
#  (the fetch above already left us on norm; do an explicit local switch
#  too, idempotent, to exercise the pure branch-switch unmount path.)
if ! timeout 30 "$BE" get ?norm >"$SWLOG" 2>&1; then
    cat "$SWLOG" >&2
    fail "be get ?norm (local switch) failed"
fi

# --- 5. assert: the mount is FULLY cleaned ---------------------------
echo "=== 5. assert vendor/sub fully cleaned ==="
[ ! -f .gitmodules ] || note ".gitmodules still present (target had none)"

#  The logical unmount must have happened.
if [ -f vendor/sub/.be ]; then
    cat "$SWLOG" >&2
    fail "vendor/sub/.be still present — sub not unmounted"
fi
#  The orphaned worktree files are the SUBS-008 bug.
for f in core.c helper.c; do
    if [ -e "vendor/sub/$f" ]; then
        ls -la vendor/sub/ >&2 || true
        cat "$SWLOG" >&2
        fail "vendor/sub/$f orphaned on disk after unmount (SUBS-008)"
    fi
done
#  Internal metadata too (`..be.idx` and friends under the mount).
if [ -e "vendor/sub/..be.idx" ]; then
    ls -la vendor/sub/ >&2 || true
    fail "vendor/sub/..be.idx orphaned on disk after unmount (SUBS-008)"
fi
#  Pinned behavior: the whole mount dir is gone (an empty leftover dir
#  would be tolerable, but we pin full removal to match git deinit +
#  tree switch).  Allow an absent dir; refuse a non-empty one.
if [ -d vendor/sub ] && [ -n "$(ls -A vendor/sub 2>/dev/null)" ]; then
    ls -la vendor/sub/ >&2 || true
    fail "vendor/sub still has leftover entries after unmount (SUBS-008)"
fi
note "vendor/sub fully cleaned (unmounted + wt files removed)"

echo "=== be-get-sub-unmount-clean: OK ==="
