#!/bin/sh
#  get/28-linear-ahead-preserved — `be get` against a remote that is
#  *behind* the local tip (local has unpushed commits) must NOT drop
#  the local commit.  Either refuse (FF rule) OR park the displaced
#  cur on a recoverable ref like `?back`.
#
#  Originating trace (Bug 6):
#    HEAD -> main, recover  f922149d  "more attention to URI canonization"
#                          aa9f3e20  "dubious fixes to weave disjoin-tail"
#    Server's spot tip = aa9f3e20.  `be get //spot` silently reset 5
#    files and f922149d was lost (no ref to it anymore).  Per the
#    user's clarification: "it was not divergent.  I had local commit
#    not posted to server yet … f922149d4c44e987… was dropped quietly
#    (locally)."
#
#  Repro shape (flat repo, no sub-mount — keeps the test simple):
#    1. Clone origin (master @ A) into wt.
#    2. Local commit B on top of A → cur at B (server still at A).
#    3. Run `be get` in three shapes that the dispatcher routes
#       through different code paths:
#         a. `be get ssh://origin?master`  — explicit transport + ref
#         b. `be get ssh://origin`         — transport, default branch
#         c. `be get //origin`             — cached form, no scheme
#    4. After each shape, assert B is still reachable: either cur
#       still points at B (refused) OR `be sha1:?back` (or REFS
#       inspection) resolves to B.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "get/28: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "get/28: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2; exit 1;;
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

# Look up server's tip A.
TIP_A=$(git -C "$ORIGIN" rev-parse master)

# ----------------------------------------------------------------
# 3. local commit B on top of A.
# ----------------------------------------------------------------
sleep 0.02
echo "version B (local only)" > hello.c
"$BE" post 'local ahead' \
    > "$LOGS/02.bpost.out" 2> "$LOGS/02.bpost.err" || {
    echo "FAIL: local B post failed" >&2; cat "$LOGS/02.bpost.err" >&2; exit 1; }

# Capture local tip B's sha from the wtlog.
TIP_B=$(awk -F'\t' '$2=="post" { last=$3 }
                    END { h=last; sub(/^[^#]*#/, "", h); print h }' .be/wtlog)
[ -n "$TIP_B" ] && [ "$TIP_B" != "$TIP_A" ] || {
    echo "FAIL: local TIP_B (= $TIP_B) didn't advance past TIP_A (= $TIP_A)" >&2
    exit 1
}

#  Helper: assert TIP_B is still reachable (cur points at it, OR
#  ?back resolves to it, OR REFS holds it under some name).
assert_b_preserved() {
    _shape=$1; _logbase=$2
    _cur=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                       END { h=last; sub(/^[^#]*#/, "", h); print h }' .be/wtlog)
    if [ "$_cur" = "$TIP_B" ]; then
        echo "  OK  $_shape: cur still pinned to TIP_B ($TIP_B)"
        return 0
    fi
    #  cur moved off B — look for B in the flat project REFS by grep.
    #  Flat layout: there is one refs file per project (no per-branch
    #  `.be/$P/<branch>/refs` glob).
    if grep -q "$TIP_B" .be/$P/refs 2>/dev/null; then
        echo "  OK  $_shape: cur moved to $_cur but TIP_B is parked in REFS"
        return 0
    fi
    echo "FAIL: $_shape lost TIP_B ($TIP_B); cur now at $_cur and no ref names it" >&2
    echo "      stderr:" >&2
    sed 's/^/        /' "$_logbase.err" >&2
    return 1
}

# ----------------------------------------------------------------
# 4a. `be get ssh://origin?master` — explicit transport + ref.
# ----------------------------------------------------------------
set +e
"$BE" get "ssh://localhost/$REL_ORIGIN?master" \
    > "$LOGS/04a.get.out" 2> "$LOGS/04a.get.err"
set -e
assert_b_preserved "ssh://?master" "$LOGS/04a.get" || exit 1

# ----------------------------------------------------------------
# 4b. `be get ssh://origin` — default branch shape.
# ----------------------------------------------------------------
set +e
"$BE" get "ssh://localhost/$REL_ORIGIN" \
    > "$LOGS/04b.get.out" 2> "$LOGS/04b.get.err"
set -e
assert_b_preserved "ssh://" "$LOGS/04b.get" || exit 1

# ----------------------------------------------------------------
# 4c. `be get //localhost` — bare cached form (the trace shape).
# ----------------------------------------------------------------
set +e
"$BE" get "//localhost" \
    > "$LOGS/04c.get.out" 2> "$LOGS/04c.get.err"
set -e
assert_b_preserved "//cached" "$LOGS/04c.get" || exit 1

rm -rf "$LOGS"
