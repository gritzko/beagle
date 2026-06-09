#!/bin/sh
#  branches/07-patch-id-dedup — cross-branch dedup via diff-noop.
#  ?fix1 has [T1, C1, C2]; ?fix2 was forked at T1 with cherry C1' (same
#  patch-id as C1, distinct sha).  `be patch ?fix2#` from cur=?fix1
#  picks C1' as the next unabsorbed commit, but its diff (T1 → C1') is
#  already on disk (cur's C1 made the same edit).  Every file
#  classifies as `noop`, no bytes change, and `be post` refuses with
#  POSTNONE.  Net effect: ?fix1 is unchanged, ?fix2 is unchanged — the
#  same "skip a same-patch-id commit" outcome as patch-id dedup.
#
#  TODO: when explicit patch-id dedup lands in resolve_rebase_one
#  (sniff/PATCH.c line 1180), `be patch ?fix2#` should refuse up-front
#  with PATCHFAIL ("nothing to replay") instead of going through the
#  diff and finding it noop.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

echo "=== 29. cross-branch rebase via patch+post ==="
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

# Switch to ?fix1; rebase via `be patch ?fix2` (next-one) + `be post`.
"$BE" get "?fix1" >/dev/null || fail "§29: switch back to ?fix1 failed"
F2_TIP_PRE=$(ref_tip "?fix2")
F1_TIP_PRE=$(ref_tip "?fix1")
[ "$F2_TIP_PRE" = "$F29_C1P" ] || fail "§29: ?fix2 unexpectedly moved"

"$BE" patch "?fix2" 2>"$ETMP/p29.err" >/dev/null \
    || { cat "$ETMP/p29.err"; fail "§29: be patch ?fix2 failed"; }

#  C1''s diff matches C1's diff, both already on disk in cur's wt → all
#  files noop; `be post` should refuse for lack of content changes.
grep -q "noop=" "$ETMP/p29.err" \
    || fail "§29: expected 'noop' from patch; got: $(cat $ETMP/p29.err)"

if "$BE" post 'should not commit' 2>"$ETMP/p29.post.err" >/dev/null; then
    fail "§29: be post should refuse (same patch-id absorbed = noop)"
fi
grep -q "POSTNONE\|no changes" "$ETMP/p29.post.err" \
    || fail "§29: expected POSTNONE; got: $(cat $ETMP/p29.post.err)"

F2_TIP_POST=$(ref_tip "?fix2")
F1_TIP_POST=$(ref_tip "?fix1")
[ "$F2_TIP_POST" = "$F2_TIP_PRE" ] \
    || fail "§29: ?fix2 moved (POST may not write a non-cur ref)"
[ "$F1_TIP_POST" = "$F1_TIP_PRE" ] \
    || fail "§29: ?fix1 moved despite dedup-noop ($F1_TIP_PRE -> $F1_TIP_POST)"

note "§29 OK: same-patch-id absorbed as noop; both refs stay put"

echo "=== branches/07-patch-id-dedup: OK ==="
