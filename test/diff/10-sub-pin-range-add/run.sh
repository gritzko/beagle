#!/bin/sh
#  diff/10-sub-pin-range-add — DIFF-001 part-b, route #3 (structural via
#  keeper).  Distinguishes route #3 (pins sourced from the two endpoint
#  TREES via KEEPSubsAt) from route #1 (scraping graf's rendered gitlink
#  text).  Two route-#3-only assertions:
#
#    (a) EXPLICIT RANGE form `diff:?<P1>#<P2>`: the sub content diff for
#        the pin range must render even though `be` never parses graf's
#        text — pins come straight from the P1 and P2 trees.  Exercises
#        the has_range endpoint arm (distinct from commit-show).
#    (b) ADDED sub: a second sub introduced in P2 renders its gitlink
#        line (graf, part-a) but NO in-sub content diff — the old side
#        is the null-oid (sub absent from the P1 tree), so route #3
#        skips the in-sub recursion (null-either-side).

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }
mkg() {
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

#  subs `ch` (bumped) and `nu` (added later) ; parent `par`.
mkg ch  || { echo "FAIL(setup): git init ch"; exit 1; }
printf 'alpha\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm c1
mkg nu  || { echo "FAIL(setup): git init nu"; exit 1; }
printf 'newsub\n' > nu/n.txt; git -C nu add -A; git -C nu commit -qm n1

#  P1: parent pins chsub→c1.
mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/ch" chsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add chsub"; exit 1; }
git -C par add -A; git -C par commit -qm p1

#  P2: bump chsub→c2 AND add a second sub nusub.
printf 'beta\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm c2
C2=$(git -C ch rev-parse HEAD)
git -C par/chsub fetch -q origin >/dev/null 2>&1
git -C par/chsub checkout -q "$C2"
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/nu" nusub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add nusub"; exit 1; }
git -C par add -A; git -C par commit -qm 'bump chsub + add nusub'

#  clone into a beagle store.
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >"$SCRATCH/01.out" 2>"$SCRATCH/01.err" ) \
    || { cat "$SCRATCH/01.err" >&2; echo "FAIL(setup): clone par into B1" >&2; exit 1; }

#  Resolve P1 (parent's first commit) and P2 (tip) shas via the store.
P2=$( cd B1 && "$BE" sha1:'?master' )
[ -n "$P2" ] || { echo "FAIL: B1 has no tip" >&2; exit 1; }
#  P1 = first parent of P2 (commit-show shape on P2).
P1=$( cd B1 && "$BE" commit:"?$P2" 2>/dev/null | awk '/^parent /{print $2; exit}' )
[ -n "$P1" ] || { echo "FAIL: cannot resolve P1 (first parent of $P2)" >&2; exit 1; }

#  --- (a) EXPLICIT RANGE: diff:?P1#P2 — sub content diff must render.
( cd B1 && "$BE" diff:"?$P1#$P2" >"$SCRATCH/02.out" 2>"$SCRATCH/02.err" )
grep -Eq 'chsub +[0-9a-f]{40}\.\.[0-9a-f]{40}' "$SCRATCH/02.out" || {
    echo "route#3(range): chsub gitlink line missing" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    echo "--- stderr ---"; cat "$SCRATCH/02.err" >&2
    exit 1; }
grep -q '+beta' "$SCRATCH/02.out" || {
    echo "route#3(range): sub content diff (c.txt alpha->beta) missing" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    exit 1; }
grep -q 'chsub/c.txt' "$SCRATCH/02.out" || {
    echo "route#3(range): sub content diff not path-prefixed under chsub/" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    exit 1; }

#  --- (b) ADDED sub: nusub renders its gitlink line (added: ..<new>),
#  but NO in-sub content diff (null old side → no two-endpoint range).
grep -q 'nusub' "$SCRATCH/02.out" || {
    echo "route#3(add): added sub nusub gitlink line missing" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    exit 1; }
if grep -q 'nusub/n.txt' "$SCRATCH/02.out"; then
    echo "route#3(add): added sub wrongly recursed into content diff" >&2
    echo "  (null old pin must skip the in-sub recursion)" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    exit 1
fi

#  --- (c) commit-show form diff:?P2 reaches the SAME endpoints — sub
#  content diff still renders (first-parent resolution == P1).
( cd B1 && "$BE" diff:"?$P2" >"$SCRATCH/03.out" 2>"$SCRATCH/03.err" )
grep -q '+beta' "$SCRATCH/03.out" || {
    echo "route#3(commit-show): sub content diff missing on diff:?$P2" >&2
    echo "--- stdout ---"; cat "$SCRATCH/03.out" >&2
    echo "--- stderr ---"; cat "$SCRATCH/03.err" >&2
    exit 1; }

echo "diff/10-sub-pin-range-add: OK"
