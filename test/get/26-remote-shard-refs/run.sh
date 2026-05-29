#!/bin/sh
#  get/26-remote-shard-refs — a fresh-clone wire fetch lands its
#  peer-URI ref row in the project trunk's reflog at
#  `.be/<project>/refs`.  Local branches and remote refs are not in
#  1:1 correspondence, so the row carries the peer URI verbatim and
#  is keyed by `(scheme,host,path,?ref)` — no per-leaf shard, no
#  per-branch remote sub-dir.
#
#  Negative: the row MUST NOT land in `<project>/<ref>/refs` (a leaf
#  shard named after the remote's ref — the stale 1:1 mapping).
#
#  Companion to get/25-remote-shard (which checks the empty-seed dir
#  layout BEEnsureProjectRepo lays down before the wire opens).
#
#  Requires passwordless ssh to localhost (gated by WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || { echo "get/26: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "get/26: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

#  case.sh seeds an empty `.be/` to anchor HOMEFindDogs at SCRATCH.
#  The clone target needs its own clean `.be/` — drop the seed so
#  `be get` lays down a fresh project shard.
rmdir "$SCRATCH/.be" 2>/dev/null || true

SRC_BARE="$SCRATCH/src.git"
WT="$SCRATCH/wt"
REL_SRC="$REL_SCRATCH/src.git"

# --- 1. tiny upstream: one commit on master ---------------------------
git init --bare -b master "$SRC_BARE" >/dev/null
SEED=$(mktemp -d)
git -c init.defaultBranch=master init -q "$SEED"
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  t
echo 'int main(void){return 0;}' > "$SEED/main.c"
git -C "$SEED" add main.c
git -C "$SEED" commit -qm 'seed'
git -C "$SEED" push -q "$SRC_BARE" master:master
rm -rf "$SEED"

# --- 2. clone via ssh -------------------------------------------------
mkdir -p "$WT/.be"
cd "$WT"
"$BE" get "ssh://localhost/$REL_SRC?master" \
    > 01.get.got.out 2> 01.get.got.err

#  Pick whichever project name BEEnsureProjectRepo derived (basename
#  of $SRC_BARE without `.git` → `src`).  Find it dynamically so the
#  test doesn't bake the convention.
PROJ=$(ls .be | grep -vE '^(\.lock|config|refs|wtlog|remotes|sniff\.pid|\.refs\.idx|\.wtlog\.idx)$' | head -1)
[ -n "$PROJ" ] || { echo "FAIL: no project shard under .be/" >&2; ls -la .be >&2; exit 1; }

# --- 3. assert the row landed in the project trunk's refs -------------
TRUNK_REFS=".be/$PROJ/refs"
[ -s "$TRUNK_REFS" ] || {
    echo "FAIL: $TRUNK_REFS missing or empty after wire fetch" >&2
    find ".be/$PROJ" -type f >&2
    exit 1
}
grep -q '//localhost' "$TRUNK_REFS" || {
    echo "FAIL: $TRUNK_REFS doesn't carry the //localhost peer URI" >&2
    cat "$TRUNK_REFS" >&2
    exit 1
}

# --- 4. negative: peer URI must NOT be in a leaf shard named after ---
#       the remote's ref (stale 1:1 local-branch=remote-ref mapping).
LEAF_REFS=".be/$PROJ/master/refs"
if [ -f "$LEAF_REFS" ] && grep -q '//localhost' "$LEAF_REFS"; then
    echo "FAIL: legacy leaf $LEAF_REFS carries a peer URI row" >&2
    echo "      (a wire fetch should not mint a local leaf branch)" >&2
    cat "$LEAF_REFS" >&2
    exit 1
fi
