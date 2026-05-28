#!/bin/sh
#  patch/20-divergent-merge — `be patch ssh://host?<branch>` against a
#  genuinely divergent remote (cur and theirs share an LCA but each has
#  unique commits on top) must run the 3-way weave-merge and either
#  produce clean merged bytes (zero conflicts) or surface conflict
#  markers + non-zero exit.  It MUST NOT exit GRAFFAIL/BEDOGEXIT for
#  the clean-merge case.
#
#  Originating trace (2026-05-28 session):
#    spot ~/beagle local cur = e76a669e  (parent f922149)
#    spot's remote recover   = 5c3bd26   (parent f922149)
#    LCA(cur, recover)        = f922149  — divergent, each side has
#                                          one unique commit on top
#    `be patch ssh://spot/src/dogs?recover` → GRAFFAIL after fetching
#    the 8 objects.  The two unique commits touch different files
#    (PUT.c on cur, KEEP.cli.c on recover) so the merge should be
#    text-clean.
#
#  Repro shape mirrors that:
#    1. Seed origin (master @ A); the seed has hello.c only.
#    2. Clone into wt.
#    3. Local edit cur-side file `local.c` + commit B (cur=B).
#    4. Server-side: add commit C touching only `server.c` (origin
#       master = C).  Local and server now diverge on top of A,
#       touching disjoint files.
#    5. `be patch ssh://origin?master` from local wt — clean
#       3-way merge expected.  Must NOT exit non-zero with
#       GRAFFAIL; the wt should carry both local.c and server.c.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "patch/20: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "patch/20: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2; exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"
LOGS="$SCRATCH/logs"; mkdir -p "$LOGS"

# ----------------------------------------------------------------
# 1. seed origin (master @ A); only hello.c present.
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
# 3. local commit B touching ONLY local.c (cur=B; parent=A).
# ----------------------------------------------------------------
sleep 0.02
echo "// added on local side" > local.c
"$BE" put local.c \
    > "$LOGS/02a.localput.out" 2> "$LOGS/02a.localput.err"
"$BE" post 'B: add local.c on local side' \
    > "$LOGS/02.bpost.out" 2> "$LOGS/02.bpost.err" || {
    echo "FAIL: local B post failed" >&2; cat "$LOGS/02.bpost.err" >&2; exit 1; }

# ----------------------------------------------------------------
# 4. server-side commit C touching ONLY server.c (origin master = C;
#    parent = A; sibling of B).
# ----------------------------------------------------------------
sleep 0.02
echo "// added on server side" > "$SEED/server.c"
git -C "$SEED" add server.c >/dev/null
git -C "$SEED" commit -qm "C: add server.c on server side"
git -C "$SEED" push -q "$ORIGIN" master:master

# ----------------------------------------------------------------
# 5. `be patch ssh://origin?master` — divergent, disjoint files.
#    Must NOT exit GRAFFAIL.  Either clean merge (zero exit) or
#    conflict markers + non-zero exit; both are valid outcomes.
#    For non-overlapping file changes the spec-correct outcome is
#    clean merge, so we also assert local.c and server.c BOTH
#    exist in the wt after the patch.
# ----------------------------------------------------------------
set +e
"$BE" patch "ssh://localhost/$REL_ORIGIN?master" \
    > "$LOGS/03.patch.out" 2> "$LOGS/03.patch.err"
RC=$?
set -e

#  GRAFFAIL on a clean-mergeable disjoint-file case is the bug.
if grep -q 'GRAFFAIL' "$LOGS/03.patch.err"; then
    echo "FAIL: be patch exited GRAFFAIL on a divergent disjoint-files merge" >&2
    echo "      stderr:" >&2
    sed 's/^/        /' "$LOGS/03.patch.err" >&2
    exit 1
fi

if [ "$RC" -ne 0 ]; then
    echo "FAIL: be patch on disjoint-files divergence returned $RC" >&2
    echo "      stderr:" >&2
    sed 's/^/        /' "$LOGS/03.patch.err" >&2
    exit 1
fi

# ----------------------------------------------------------------
# 6. wt should now carry BOTH local.c (from B) and server.c (from
#    C).  hello.c (from A) is untouched on both sides — still there.
# ----------------------------------------------------------------
[ -f local.c ]  || { echo "FAIL: local.c missing after merge" >&2; exit 1; }
[ -f server.c ] || { echo "FAIL: server.c missing after merge (wt didn't pick up theirs)" >&2; exit 1; }
[ -f hello.c ]  || { echo "FAIL: hello.c missing after merge" >&2; exit 1; }

rm -rf "$LOGS"
