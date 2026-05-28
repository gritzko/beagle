#!/bin/sh
#  patch/18-noop-already-absorbed — `be patch ssh://host?<branch>` when
#  theirs.tip is already in cur's first-parent ancestry must NOT fail.
#  It should be a clean no-op ("already up to date") since there are
#  zero commits between cur and theirs.tip.
#
#  Originating trace (2026-05-28 session):
#    $ be patch ssh://spot/src/dogs?recover
#    Enumerating objects: 8, done.
#    ...
#    Total 8 (delta 0), reused 0 (delta 0), pack-reused 0 (from 0)
#    Error: GRAFFAIL
#    Error: BEDOGEXIT
#  Local cur was strictly ahead of spot's recover (recover.tip in
#  cur's ancestry).  Same failure with `#merge recover` msg appended.
#
#  Site (suspected): the graf merge-base / PATCH path in
#  `graf/MERGE.c` or `graf/GET.c` doesn't handle the
#  `merge_base == theirs.tip` (i.e. theirs ⊆ cur) case — it
#  returns GRAFFAIL rather than a no-op OK.
#
#  Repro shape:
#    1. Seed origin (master @ A) via plain git.
#    2. Clone into wt via ssh.
#    3. Make one local commit B on top of A (cur=B, recover server-
#       side still at A).
#    4. `be patch ssh://origin?master` — theirs.tip (A) is now an
#       ancestor of cur (B).  Must succeed with zero applied
#       commits.
#    5. (Repeat with `'#merge master'` msg appended — must also
#       succeed.)
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "patch/14: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "patch/14: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2; exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"
LOGS="$SCRATCH/logs"; mkdir -p "$LOGS"

# ----------------------------------------------------------------
# 1. seed origin (master @ A).
# ----------------------------------------------------------------
git init --bare -b master "$ORIGIN" >/dev/null
git init -b master "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
echo "version A" > "$SEED/hello.c"
git -C "$SEED" add hello.c >/dev/null
git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# ----------------------------------------------------------------
# 2. clone into wt via ssh.
# ----------------------------------------------------------------
mkdir wt wt/.be
cd wt
"$BE" get "ssh://localhost/$REL_ORIGIN?master" \
    > "$LOGS/01.clone.out" 2> "$LOGS/01.clone.err" || {
    echo "FAIL: clone failed" >&2; cat "$LOGS/01.clone.err" >&2; exit 1; }

# ----------------------------------------------------------------
# 3. local commit B on top of A.  cur now strictly ahead of master.
# ----------------------------------------------------------------
sleep 0.02
echo "version B (local on top)" > hello.c
"$BE" post 'B on top of A' \
    > "$LOGS/02.bpost.out" 2> "$LOGS/02.bpost.err" || {
    echo "FAIL: local B post failed" >&2; cat "$LOGS/02.bpost.err" >&2; exit 1; }

# ----------------------------------------------------------------
# 4a. `be patch ssh://origin?master` — squash form.  theirs.tip (A)
#     is already in cur (B)'s ancestry.  Zero commits to absorb.
#     Must NOT exit non-zero with GRAFFAIL.
# ----------------------------------------------------------------
set +e
"$BE" patch "ssh://localhost/$REL_ORIGIN?master" \
    > "$LOGS/03.patch.out" 2> "$LOGS/03.patch.err"
RC_PATCH=$?
set -e
if [ "$RC_PATCH" -ne 0 ]; then
    echo "FAIL: be patch (squash) on already-absorbed remote returned $RC_PATCH" >&2
    echo "      stderr:" >&2
    sed 's/^/        /' "$LOGS/03.patch.err" >&2
    exit 1
fi

# ----------------------------------------------------------------
# 4b. `be patch ssh://origin?master '#merge master'` — merge form.
#     Same shape — must succeed cleanly.
# ----------------------------------------------------------------
set +e
"$BE" patch "ssh://localhost/$REL_ORIGIN?master" '#merge master' \
    > "$LOGS/04.patch-merge.out" 2> "$LOGS/04.patch-merge.err"
RC_MERGE=$?
set -e
if [ "$RC_MERGE" -ne 0 ]; then
    echo "FAIL: be patch (merge form) on already-absorbed remote returned $RC_MERGE" >&2
    echo "      stderr:" >&2
    sed 's/^/        /' "$LOGS/04.patch-merge.err" >&2
    exit 1
fi

# ----------------------------------------------------------------
# 5. hello.c must still hold B's bytes (no-op PATCH didn't reset wt).
# ----------------------------------------------------------------
[ "$(cat hello.c)" = "version B (local on top)" ] || {
    echo "FAIL: hello.c was modified by no-op PATCH; got: '$(cat hello.c)'" >&2
    exit 1
}

rm -rf "$LOGS"
