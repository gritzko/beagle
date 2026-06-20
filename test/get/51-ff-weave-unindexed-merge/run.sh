#!/bin/sh
#  51-ff-weave-unindexed-merge — DIS-041.  A behind-worktree FF
#  (`be get '?'`) on a file BOTH sides edited (disjoint regions) MUST
#  weave the union of both edits — even when the trunk commits live in
#  keeper packs but are NOT in graf's persisted DAG index (the state a
#  wire push / `be patch` / fresh clone leaves behind).
#
#  The bug: with no graf parent-edge index, DAGTopoSortTunable saw every
#  commit as a parent-less root, hashlet-sorted them, and the FF replay
#  ran out of causal order — silently DROPPING intermediate commits'
#  edits (non-deterministically, keyed on the commit shas).  The fix
#  (graf/GET.c::GRAFMergeWtFileTunable) indexes both merge endpoints'
#  ancestry into graf BEFORE the weave, guaranteeing a coherent DAG and
#  a correct, reproducible replay; an index failure is a hard error,
#  never a silent ours-only / leave-untouched.
#
#  Topology (F starts L1..L30; behind-wt forks at BASE):
#
#      BASE ── C1(L1) ── C2(L2) ── C3(L3) ── C4(L4) ── C5(L5)  [trunk]
#        \
#         (behind-wt: dirty edit to L30, never committed)
#
#  `be get '?'` from the behind-wt must FF to C5 AND keep the wt's L30
#  edit: result top = L1-C1..L5-C5 (all five trunk edits, NONE dropped),
#  bottom = L30-BEHIND.  Five commits make a scrambled topo order drop
#  at least one edit deterministically.  The store's graf.idx is wiped
#  before the FF so the FF path is forced to (re)index.

. "$(dirname "$0")/../../lib/case.sh"
rm -rf "$SCRATCH/.be"

OUT="$SCRATCH/out"; mkdir -p "$OUT"

#  --- seed a bare central store with project `seed` at BASE ----------
STORE="$SCRATCH/store/.be"; mkdir -p "$STORE"
S="$SCRATCH/seed"; mkdir -p "$S/.be"
( cd "$S"
  i=1; while [ $i -le 30 ]; do echo "L$i"; i=$((i+1)); done > F.txt
  sleep 0.02; "$BE" put F.txt          > /dev/null 2>&1
  sleep 0.02; "$BE" post 'base'        > /dev/null 2>&1 )
[ -d "$S/.be/seed" ] || {
    echo "FAIL(setup): seed shard not named 'seed': $(ls "$S/.be")" >&2; exit 1; }
cp -r "$S/.be/seed" "$STORE/seed"
cp    "$S/.be/wtlog" "$STORE/wtlog"

#  --- behind worktree: clone at BASE, dirty-edit L30 (do NOT commit) -
mkdir -p "$SCRATCH/behind"
( cd "$SCRATCH/behind"
  "$BE" get "file://$STORE?/seed/" > "$OUT/behind.get.out" 2>&1 ) || {
    echo "FAIL: behind clone failed" >&2; cat "$OUT/behind.get.out" >&2; exit 1; }
sleep 0.02
sed 's/^L30$/L30-BEHIND/' "$SCRATCH/behind/F.txt" > "$SCRATCH/behind/F.new"
mv "$SCRATCH/behind/F.new" "$SCRATCH/behind/F.txt"

#  --- trunk worktree: 5 commits, each editing a different TOP line ---
mkdir -p "$SCRATCH/trunk"
( cd "$SCRATCH/trunk"
  "$BE" get "file://$STORE?/seed/" > /dev/null 2>&1
  n=1; while [ $n -le 5 ]; do
    sleep 0.02; sed "s/^L$n\$/L$n-C$n/" F.txt > F.tmp && mv F.tmp F.txt; "$BE" post "c$n" > /dev/null 2>&1
    n=$((n+1))
  done )

#  --- simulate "commits in packs, NOT in graf's DAG index": wipe the
#      store's persisted graf index so the FF must (re)index ----------
rm -f "$STORE/seed/"*graf*

#  --- THE TEST: FF the behind-wt; both sides touched F (disjoint) ----
( cd "$SCRATCH/behind"
  "$BE" get '?' > "$OUT/ff.out" 2> "$OUT/ff.err" ) || {
    echo "FAIL: be get '?' returned nonzero" >&2; cat "$OUT/ff.err" >&2; exit 1; }

#  No silent bail / ours-only.
if grep -q 'graf err' "$OUT/ff.err"; then
    echo "FAIL: FF emitted 'graf err' — weave bailed instead of index+retry" >&2
    cat "$OUT/ff.err" >&2; exit 1
fi
if grep -q 'leaving wt content untouched' "$OUT/ff.err"; then
    echo "FAIL: FF left the file UNMERGED" >&2
    cat "$OUT/ff.err" >&2; exit 1
fi
#  BE-005 verb-output sweep: the weave-merge count rides the ULOG status
#  hunk on stdout now (not stderr).
grep -q 'weave-merged [1-9]' "$OUT/ff.out" || {
    echo "FAIL: F was not weave-merged" >&2
    cat "$OUT/ff.out" "$OUT/ff.err" >&2; exit 1; }

#  The merged file MUST carry ALL FIVE trunk edits AND the wt's edit.
F="$SCRATCH/behind/F.txt"
for want in 1:L1-C1 2:L2-C2 3:L3-C3 4:L4-C4 5:L5-C5 30:L30-BEHIND; do
    ln=${want%%:*}; exp=${want#*:}
    got=$(sed -n "${ln}p" "$F")
    [ "$got" = "$exp" ] || {
        echo "FAIL: line $ln = '$got', want '$exp' — a divergent edit was DROPPED" >&2
        echo "--- merged F.txt ---" >&2; cat "$F" >&2
        exit 1
    }
done

echo "=== get/51-ff-weave-unindexed-merge: OK ==="
