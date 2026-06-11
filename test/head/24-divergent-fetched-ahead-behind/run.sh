#!/bin/sh
#  24-divergent-fetched-ahead-behind — DIS-038.  `be head` against a
#  divergent peer whose tip was just fetched (but NOT graf-indexed) must
#  report the TRUE ahead/behind, not a root-deep bogus count.  Pre-fix,
#  DAGAncestors walked only the DAG index with no keeper-commit-body
#  fallback, so the fetched tip's ancestry bottomed out at itself: the
#  whole shared base counted as "ahead".  With a 3-commit shared base,
#  pre-fix reported "4 ahead"; the fix reports "1 ahead, 1 behind".

. "$(dirname "$0")/../../lib/case.sh"
rm -rf "$SCRATCH/.be"
HOME="$SCRATCH/home"; export HOME; mkdir -p "$HOME/.be"

#  parent store with a 3-commit shared base.
mkdir parent
( cd parent
  for n in 1 2 3; do
    printf 'base%s\n' "$n" >> f.c
    sleep 0.02; "$BE" put f.c   >/dev/null 2>&1
    sleep 0.02; "$BE" post "b$n" >/dev/null 2>&1
  done )
PROJ=$(ls parent/.be | grep -v wtlog | head -1)

#  independent clone at the shared base.
mkdir -p ticket/.be
( cd ticket && "$BE" get "file://$SCRATCH/parent/.be?/$PROJ/" >/dev/null 2>&1 )

#  parent advances 1 (T); ticket diverges 1 (C) off the same base.
( cd parent
  printf 'parent\n' >> f.c
  sleep 0.02; "$BE" put f.c >/dev/null 2>&1; sleep 0.02; "$BE" post 'T' >/dev/null 2>&1 )
( cd ticket
  printf 'ticket\n' >> f.c
  sleep 0.02; "$BE" put f.c >/dev/null 2>&1; sleep 0.02; "$BE" post 'C' >/dev/null 2>&1 )

#  fresh worktree at T, then head against the divergent fetched tip.
mkdir -p pw
( cd pw && "$BE" get "file://$SCRATCH/parent/.be?/$PROJ/" >/dev/null 2>&1 )
( cd pw && "$BE" head "file://$SCRATCH/ticket/.be?/$PROJ/" >"$SCRATCH/h.out" 2>"$SCRATCH/h.err" )

#  True divergence is 1 ahead, 1 behind — NOT a root-deep ahead-count.
grep -Eq '1 ahead, 1 behind' "$SCRATCH/h.out" || {
    echo "DIS-038: head did not report the true 1-ahead/1-behind divergence" >&2
    echo "--- stdout ---"; cat "$SCRATCH/h.out" >&2
    echo "--- stderr ---"; cat "$SCRATCH/h.err" >&2
    exit 1; }
echo "head/24-divergent-fetched-ahead-behind: OK"
