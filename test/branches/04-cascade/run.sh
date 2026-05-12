#!/bin/sh
#  branches/04-cascade — extracted from workflow-branches.sh stages 20-24.
#  Two-level cascade rebase: ?L1 + ?L1/L2.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# --- setup: trunk with one commit ---
echo "x v1" > x.txt
"$BE" post 'v1 msg' >/dev/null
note "trunk seeded"

echo "=== 20. setup ?L1 with a commit ==="
"$BE" put "?./L1" >/dev/null || fail "be put ?./L1 failed"
"$BE" get "?L1" >/dev/null || fail "be get ?L1 failed"
sleep 0.01
echo "L1 v1" > l1.txt
"$BE" put l1.txt >/dev/null
"$BE" post 'L1 c1' >/dev/null || fail "be post on ?L1 failed"
C_L1=$(head_hex)
[ -n "$C_L1" ] || fail "no L1 tip"
note "?L1 at C_L1=$C_L1"

echo "=== 21. setup ?L1/L2 with a commit ==="
"$BE" put "?./L2" >/dev/null || fail "be put ?./L2 (under L1) failed"
"$BE" get "?L1/L2" >/dev/null || fail "be get ?L1/L2 failed"
[ "$(cur_branch)" = "L1/L2" ] || fail "wt should be on L1/L2; got '$(cur_branch)'"
sleep 0.01
echo "L2 v1" > l2.txt
"$BE" put l2.txt >/dev/null
"$BE" post 'L2 c1' >/dev/null || fail "be post on ?L1/L2 failed"
C_L2=$(head_hex)
[ -n "$C_L2" ] && [ "$C_L2" != "$C_L1" ] || fail "C_L2 didn't advance"
note "?L1/L2 at C_L2=$C_L2"

echo "=== 22. fork WTL1 on ?L1 (shares keeper, baseline ?L1@C_L1) ==="
"$BE" get "?L1" >/dev/null || fail "be get ?L1 (back to L1) failed"
WTL1="$ETMP/wtl1"
mkdir -p "$WTL1"
cp "$WT/x.txt"  "$WTL1/x.txt"  2>/dev/null || true
cp "$WT/l1.txt" "$WTL1/l1.txt" 2>/dev/null || true
# Secondary wt: `.be` is a regular file = its own wtlog.  Row 0's
# `repo` URI (inherited from primary) names the shared store.
cp "$WT/.be/wtlog" "$WTL1/.be"
note "WTL1 forked off ?L1 at C_L1"

echo "=== 23. WT advances ?L1 to C_L1b ==="
sleep 0.01
echo "L1 v2 (advance)" > l1b.txt
"$BE" put l1b.txt >/dev/null || fail "be put l1b.txt on ?L1 failed"
"$BE" post 'L1 advance' >/dev/null || fail "be post advance on ?L1 failed"
C_L1b=$(head_hex)
[ -n "$C_L1b" ] && [ "$C_L1b" != "$C_L1" ] || fail "C_L1b didn't advance"
note "?L1 advanced to C_L1b=$C_L1b"

echo "=== 24. WTL1 posts on stale ?L1 — rebase + cascade ?L1/L2 ==="
cd "$WTL1"
sleep 0.01
echo "L1 v3 wtl1" > l1c.txt
"$BE" put l1c.txt >/dev/null || fail "WTL1: be put l1c.txt failed"
"$BE" post 'L1 wtl1' 2>"$ETMP/wtl1.err" >/dev/null \
    || { cat "$ETMP/wtl1.err"; fail "WTL1: be post should rebase + cascade"; }
C_L1c=$(head_hex)
[ -n "$C_L1c" ] && [ "$C_L1c" != "$C_L1b" ] && [ "$C_L1c" != "$C_L1" ] \
    || fail "WTL1 rebased tip $C_L1c not distinct from C_L1b/C_L1"
note "?L1 rebased onto C_L1b → C_L1c=$C_L1c"

L1_REFS=$(ref_tip "?L1")
[ "$L1_REFS" = "$C_L1c" ] \
    || fail "?L1 REFS at $L1_REFS; want C_L1c=$C_L1c"
L2_REFS=$(ref_tip "?L1/L2")
[ -n "$L2_REFS" ] || fail "?L1/L2 REFS missing after cascade"
[ "$L2_REFS" != "$C_L2" ] \
    || fail "cascade did not advance ?L1/L2 (still at $C_L2)"
L2_PARENT=$("$KEEPER" get "?L1/L2#$L2_REFS" 2>/dev/null \
              | awk '/^parent / { print $2; exit }')
[ "$L2_PARENT" = "$C_L1c" ] \
    || fail "?L1/L2 rebased tip's parent is $L2_PARENT; want C_L1c=$C_L1c"
note "cascade landed: ?L1/L2 rebased; parent = C_L1c"

echo "=== branches/04-cascade: OK ==="
