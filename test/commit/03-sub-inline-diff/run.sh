#!/bin/sh
#  commit/03-sub-inline-diff — COMMIT-002, sub composition.  A commit
#  that bumps a submodule pin must, under `commit:?<sha>`, show the
#  commit metadata AND the full inline diff — and that inline diff
#  composes under sub recursion EXACTLY like a bare `diff:?<sha>`: the
#  sub's pin-range content diff relays path-prefixed under the subpath.
#
#  Built on the same local-`file:` git fixture as diff/10-sub-pin-range-add
#  (route #3, structural pins from the two endpoint trees) so the sub
#  content diff is deterministic and offline (no ssh).

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

#  sub `ch` (bumped between the parent's two commits); parent `par`.
mkg ch  || { echo "FAIL(setup): git init ch"; exit 1; }
printf 'alpha\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm c1
mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/ch" chsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add chsub"; exit 1; }
git -C par add -A; git -C par commit -qm p1

#  P2: bump chsub alpha->beta and re-commit the parent pin.
printf 'beta\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm c2
C2=$(git -C ch rev-parse HEAD)
git -C par/chsub fetch -q origin >/dev/null 2>&1
git -C par/chsub checkout -q "$C2"
git -C par add -A; git -C par commit -qm 'bump chsub alpha->beta'

#  clone into a beagle store.
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >"$SCRATCH/01.out" 2>"$SCRATCH/01.err" ) \
    || { cat "$SCRATCH/01.err" >&2; echo "FAIL(setup): clone par into B1" >&2; exit 1; }

P2=$( cd B1 && "$BE" sha1:'?master' )
[ -n "$P2" ] || { echo "FAIL: B1 has no tip" >&2; exit 1; }

#  --- commit:?<P2>: metadata header + INLINE diff, sub diff relayed ---
( cd B1 && "$BE" commit:"?$P2" >"$SCRATCH/02.out" 2>"$SCRATCH/02.err" )
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: be commit:?$P2 exited $rc" >&2
    echo "--- stderr ---"; cat "$SCRATCH/02.err" >&2; exit 1; }

#  (1) commit metadata header for the resolved sha.
grep -q "^commit $P2" "$SCRATCH/02.out" || {
    echo "commit/03: missing commit header for $P2" >&2
    cat "$SCRATCH/02.out" >&2; exit 1; }

#  (2) parent gitlink range line — the inline diff (graf, relayed).
grep -Eq 'chsub +[0-9a-f]{40}\.\.[0-9a-f]{40}' "$SCRATCH/02.out" || {
    echo "commit/03: inline diff missing chsub gitlink range line" >&2
    cat "$SCRATCH/02.out" >&2; exit 1; }

#  (3) the sub's pin-range CONTENT diff, path-prefixed under chsub/.
grep -q '+beta' "$SCRATCH/02.out" || {
    echo "commit/03: sub content diff (c.txt alpha->beta) not relayed inline" >&2
    cat "$SCRATCH/02.out" >&2; exit 1; }
grep -q 'chsub/c.txt' "$SCRATCH/02.out" || {
    echo "commit/03: sub content diff not path-prefixed under chsub/" >&2
    cat "$SCRATCH/02.out" >&2; exit 1; }

echo "commit/03-sub-inline-diff: OK"
