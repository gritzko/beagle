#!/bin/sh
#  branches/07-patch-id-dedup — extracted from workflow-branches.sh stage 29.
#  Cross-branch patch-id dedup E2E: a sibling promote skips a commit
#  whose patch-id matches one already reachable from the target.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

echo "=== 29. patch-id dedup E2E ==="
cd "$WT"

# Seed shared baseline files (deterministic content).
sleep 0.01
echo "fix base" > fix29.txt
echo "other base" > other29.txt
"$BE" put fix29.txt other29.txt >/dev/null \
    || fail "§29: stage baseline failed"
"$BE" post '29-base msg' >/dev/null \
    || fail "§29: baseline commit failed"
T29_T1=$(head_hex)
note "§29: T1=$T29_T1"

# Build ?fix1 with C1 (fix.txt) + C2 (other.txt).
"$BE" put "?./fix1" >/dev/null || fail "§29: create ?fix1 failed"
"$BE" get "?fix1" >/dev/null || fail "§29: switch to ?fix1 failed"
sleep 0.01
echo "fix changed" > fix29.txt
"$BE" put fix29.txt >/dev/null || fail "§29: put fix29.txt on ?fix1 failed"
"$BE" post 'fix1c1 msg' >/dev/null || fail "§29: post fix1c1 failed"
F29_C1=$(head_hex)
sleep 0.01
echo "other changed" > other29.txt
"$BE" put other29.txt >/dev/null || fail "§29: put other29.txt on ?fix1 failed"
"$BE" post 'fix1c2 msg' >/dev/null || fail "§29: post fix1c2 failed"
F29_C2=$(head_hex)
note "§29: ?fix1 stack T1=$T29_T1 -> C1=$F29_C1 -> C2=$F29_C2"

# Build ?fix2 forked at T1 with cherry-picked C1' (same content as C1).
"$BE" get "?.." >/dev/null || fail "§29: back to trunk failed"
"$BE" put "?./fix2" >/dev/null || fail "§29: create ?fix2 failed"
"$BE" get "?fix2" >/dev/null || fail "§29: switch to ?fix2 failed"
sleep 0.01
echo "fix changed" > fix29.txt
"$BE" put fix29.txt >/dev/null || fail "§29: put fix29.txt on ?fix2 failed"
"$BE" post 'fix2c1prime msg' >/dev/null || fail "§29: post fix2c1prime failed"
F29_C1P=$(head_hex)
[ "$F29_C1P" != "$F29_C1" ] \
    || fail "§29: C1' should be a distinct commit object"
note "§29: ?fix2 has C1'=$F29_C1P"

# Switch to ?fix1, run be post ?fix2.
"$BE" get "?fix1" >/dev/null || fail "§29: switch back to ?fix1 failed"
F2_TIP_PRE=$(ref_tip "?fix2")
F1_TIP_PRE=$(ref_tip "?fix1")
[ "$F2_TIP_PRE" = "$F29_C1P" ] || fail "§29: ?fix2 unexpectedly moved"

"$BE" post "?fix2" 2>"$ETMP/p29.err" >/dev/null \
    || { cat "$ETMP/p29.err"; fail "§29: be post ?fix2 failed"; }

F2_TIP_POST=$(ref_tip "?fix2")
F1_TIP_POST=$(ref_tip "?fix1")
[ "$F1_TIP_POST" = "$F1_TIP_PRE" ] \
    || fail "§29: cur (?fix1) moved across sibling promote"
[ "$F2_TIP_POST" != "$F2_TIP_PRE" ] || fail "§29: ?fix2 didn't advance"

# Walk parents from F2_TIP_POST until we reach F2_TIP_PRE; expect 1 hop.
hops=0
cur="$F2_TIP_POST"
while [ -n "$cur" ] && [ "$cur" != "$F2_TIP_PRE" ]; do
    p=$("$KEEPER" get ".#$cur" 2>/dev/null | awk '/^parent / { print $2; exit }')
    hops=$((hops+1))
    cur=$p
    [ $hops -gt 10 ] && break
done
[ "$hops" = "1" ] \
    || fail "§29: expected 1 hop (C2 only) past C1' due to dedup; got $hops"
note "§29 OK: C1 deduped against C1'; ?fix2 advanced by exactly 1 commit"

echo "=== branches/07-patch-id-dedup: OK ==="
