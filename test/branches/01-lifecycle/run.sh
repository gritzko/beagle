#!/bin/sh
#  branches/01-lifecycle — extracted from workflow-branches.sh stages 1-9.
#  Branch creation + edit + commit on child + switch back + delete.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# Anchor project shard at .be/$P/ so subsequent be invocations
# don't derive the project name from the first URI's basename.
"$BE" put "?/$P/" 2>/dev/null || true

# 1. seed trunk
echo "=== 1. trunk baseline ==="
echo "x v1" > x.txt
"$BE" post 'v1 msg' >/dev/null
T1=$(head_hex)
[ -n "$T1" ] || fail "no trunk tip after first post"
[ "$(cur_branch)" = "" ] || fail "expected trunk (empty branch), got '$(cur_branch)'"
TRUNK_REFS_T1=$(ref_tip "?")
[ "$TRUNK_REFS_T1" = "$T1" ] \
    || fail "trunk REFS tip $TRUNK_REFS_T1 != T1=$T1"
note "trunk T1=$T1"

# 2. create child branch via PUT
echo "=== 2. be put ?./fix1 — create child ==="
"$BE" put "?./fix1" >/dev/null \
    || fail "be put ?./fix1 failed"
FIX1_REFS=$(ref_tip "?fix1")
[ -n "$FIX1_REFS" ] || fail "?fix1 not in REFS after be put ?./fix1"
[ "$FIX1_REFS" = "$T1" ] || fail "?fix1 should fork at T1=$T1; got $FIX1_REFS"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T1" ] || fail "trunk REFS moved by branch create"
note "?fix1 forked at $FIX1_REFS; trunk unchanged"
# flat layout: create makes the branch a REFS row, NOT a directory
[ ! -d .be/$P/fix1 ] || fail "per-branch shard .be/$P/fix1 must not exist (flat layout)"
"$BE" get "?fix1" >/dev/null || fail "?fix1 does not resolve after create"
note "?fix1 resolves after create (REFS row, no dir)"

# 3. switch wt to the child
echo "=== 3. be get ?fix1 — switch wt ==="
"$BE" get "?fix1" >/dev/null || fail "be get ?fix1 failed"
[ "$(cur_branch)" = "fix1" ] || fail "wt should be 'fix1', got '$(cur_branch)'"
[ "$(head_hex)" = "$T1" ] || fail "wt tip should still be T1 right after switch"
note "wt now on ?fix1 at T1"

# 4. edit on child
echo "=== 4. edit on child branch ==="
sleep 0.01
echo "x v2 (fix1)" > x.txt
note "x.txt modified on ?fix1"

# 5. stage + commit on the child
echo "=== 5. be put + be post on ?fix1 ==="
"$BE" put x.txt >/dev/null || fail "be put x.txt on ?fix1 failed"
"$BE" post 'fix1 v2' >/dev/null || fail "be post on ?fix1 failed"
T2=$(head_hex)
[ -n "$T2" ] || fail "no tip after post on ?fix1"
[ "$T2" != "$T1" ] || fail "post on ?fix1 did not advance tip"
note "?fix1 tip advanced to $T2"

# 6. verify branch tips
echo "=== 6. verify branch tips ==="
FIX1_REFS=$(ref_tip "?fix1")
TRUNK_REFS=$(ref_tip "?")
[ "$FIX1_REFS" = "$T2" ] || fail "REFS ?fix1 should be T2=$T2; got $FIX1_REFS"
[ "$TRUNK_REFS" = "$T1" ] || fail "REFS ? should be T1=$T1; got $TRUNK_REFS"
note "REFS: ?=$T1 ?fix1=$T2"

# 7. switch back via ?..
echo "=== 7. be get ?.. — switch back to trunk ==="
"$BE" get "?.." >/dev/null || fail "be get ?.. failed"
[ "$(cur_branch)" = "" ] || fail "wt should be on trunk; got '$(cur_branch)'"
[ "$(head_hex)" = "$T1" ] || fail "wt tip on trunk should be T1=$T1"
note "wt back on trunk at T1"

# 8. delete child
echo "=== 8. be delete ?fix1 ==="
"$BE" delete "?fix1" >/dev/null || fail "be delete ?fix1 failed"

# 9. verify deletion
echo "=== 9. verify deletion ==="
FIX1_REFS=$(ref_tip "?fix1")
[ -z "$FIX1_REFS" ] || fail "?fix1 still visible in REFS after delete: $FIX1_REFS"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T1" ] || fail "trunk REFS moved across delete"
# flat layout: delete writes a REFS tombstone; objects linger, no dir removed.
# ?fix1 must no longer resolve (ref unresolvable).
if "$BE" get "?fix1" >/dev/null 2>&1; then
    fail "?fix1 still resolves after delete (should be unresolvable)"
fi
note "?fix1 deleted; ref unresolvable; trunk unchanged at T1=$T1"

echo "=== branches/01-lifecycle: OK ==="
