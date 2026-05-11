#!/bin/sh
#  branches/03-secondary-wt — extracted from workflow-branches.sh stages 16-19.
#  Two worktrees sharing one keeper; ff/rebase races on trunk.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# --- setup: trunk with one commit (T_pre_rebase) ---
echo "x v1" > x.txt
"$BE" post 'v1 msg' >/dev/null
T_pre_rebase=$(head_hex)
[ -n "$T_pre_rebase" ] || fail "no trunk tip after seed post"

echo "=== 16. setup secondary wt (WT2) sharing one keeper ==="
WT2="$ETMP/wt2"
mkdir -p "$WT2"
cp "$WT/x.txt" "$WT2/x.txt"
# Secondary worktree: `<wt>/.be` is a regular file = its own wtlog.
# Row 0's `repo` URI (inherited from primary) names the shared store.
cp "$WT/.be/wtlog" "$WT2/.be"
note "WT2 forked at trunk tip T_pre_rebase=$T_pre_rebase"

echo "=== 17. WT advances trunk: a new commit lands first ==="
sleep 0.01
echo "racing-1" > racing.txt
"$BE" put racing.txt >/dev/null || fail "WT: be put racing.txt failed"
"$BE" post 'racing-first msg' >/dev/null || fail "WT: be post racing-first failed"
T_advance=$(head_hex)
[ "$T_advance" != "$T_pre_rebase" ] || fail "WT advance didn't change tip"
note "WT advanced trunk to T_advance=$T_advance"

echo "=== 18. WT2 posts on top of stale tip → rebase ==="
cd "$WT2"
sleep 0.01
echo "wt2-only" > wt2.txt
"$BE" put wt2.txt >/dev/null || fail "WT2: be put wt2.txt failed"
"$BE" post 'wt2-rebase msg' 2>"$ETMP/wt2-rebase.err" >/dev/null \
    || { cat "$ETMP/wt2-rebase.err"; fail "WT2: be post should rebase"; }
T_rebased=$(head_hex)
[ -n "$T_rebased" ] && [ "$T_rebased" != "$T_advance" ] \
    && [ "$T_rebased" != "$T_pre_rebase" ] \
    || fail "WT2: rebased tip $T_rebased not distinct from T_advance/T_pre_rebase"
note "WT2 rebased onto T_advance; new trunk tip T_rebased=$T_rebased"

TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T_rebased" ] \
    || fail "trunk REFS at $TRUNK_REFS; want T_rebased=$T_rebased"
PARENT_REBASED=$("$KEEPER" get ".#$T_rebased" 2>/dev/null \
                    | awk '/^parent / { print $2; exit }')
[ "$PARENT_REBASED" = "$T_advance" ] \
    || fail "T_rebased's parent is $PARENT_REBASED; want T_advance=$T_advance"
note "T_rebased.parent = T_advance (rebase landed on top)"

echo "=== 19. WT3 conflict abort: deferred ==="
skip "explicit conflict-abort scenario deferred — needs scripted .be/wtlog rewind"

echo "=== branches/03-secondary-wt: OK ==="
