#!/bin/sh
#  branches/02-patch — extracted from workflow-branches.sh stages 10-15.
#  Re-create child + 2-commit stack, switch back to trunk, PATCH absorbs
#  the stack into wt, POST squashes as single-parent commit on trunk.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# --- setup: trunk baseline (T1) ---
echo "x v1" > x.txt
"$BE" post v1 >/dev/null
T1=$(head_hex)
[ -n "$T1" ] || fail "no trunk tip after seed post"
note "trunk T1=$T1"

# 10. rebuild ?fix1 with a 2-commit stack (C1, C2)
echo "=== 10. rebuild ?fix1 with a 2-commit stack ==="
"$BE" post "?./fix1" >/dev/null || fail "be post ?./fix1 (re-create) failed"
"$BE" get "?fix1" >/dev/null || fail "be get ?fix1 (re-switch) failed"
[ "$(cur_branch)" = "fix1" ] || fail "wt should be on ?fix1; got '$(cur_branch)'"

sleep 0.01
echo "a v1 (fix1)" > a.txt
"$BE" put a.txt >/dev/null || fail "be put a.txt on ?fix1 failed"
"$BE" post fix1 c1 >/dev/null || fail "be post fix1 c1 failed"
C1=$(head_hex)
[ -n "$C1" ] && [ "$C1" != "$T1" ] || fail "C1 not advanced past T1 (got '$C1')"

sleep 0.01
echo "b v1 (fix1)" > b.txt
"$BE" put b.txt >/dev/null || fail "be put b.txt on ?fix1 failed"
"$BE" post fix1 c2 >/dev/null || fail "be post fix1 c2 failed"
C2=$(head_hex)
[ -n "$C2" ] && [ "$C2" != "$C1" ] || fail "C2 not advanced past C1"
note "?fix1 stack: T1 -> C1=$C1 -> C2=$C2"

# 11. switch back to trunk
echo "=== 11. switch back to trunk; T_pre ==="
"$BE" get "?.." >/dev/null || fail "be get ?.. (back to trunk) failed"
[ "$(cur_branch)" = "" ] || fail "wt should be on trunk; got '$(cur_branch)'"
T_pre=$(head_hex)
[ "$T_pre" = "$T1" ] || fail "trunk tip pre-patch should be T1=$T1"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T1" ] || fail "trunk REFS pre-patch should be T1=$T1"
[ ! -e a.txt ] || fail "a.txt should not be on trunk before patch"
[ ! -e b.txt ] || fail "b.txt should not be on trunk before patch"
note "trunk tip T_pre=$T_pre; wt has only x.txt"

# 12. be patch ?./fix1
echo "=== 12. be patch ?./fix1 — absorb stack into wt ==="
PATCH_OUT="$ETMP/patch.err"
"$BE" patch "?./fix1" 2>"$PATCH_OUT" >/dev/null \
    || fail "be patch ?./fix1 failed (stderr: $(cat "$PATCH_OUT"))"
grep -q '^sniff: patch:' "$PATCH_OUT" \
    || fail "no 'sniff: patch:' summary in stderr"
[ -f a.txt ] || fail "a.txt missing in trunk wt after patch"
[ -f b.txt ] || fail "b.txt missing in trunk wt after patch"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T_pre" ] \
    || fail "trunk REFS moved by PATCH (must wait for POST)"
note "patch landed a.txt + b.txt in wt; trunk REFS still T_pre"

# 13. squash via put + post
echo "=== 13. be put a.txt + be put b.txt + be post squash ==="
"$BE" put a.txt >/dev/null || fail "be put a.txt (post-patch) failed"
"$BE" put b.txt >/dev/null || fail "be put b.txt (post-patch) failed"
"$BE" post squash >/dev/null || fail "be post squash failed"
T_squash=$(head_hex)
[ -n "$T_squash" ] || fail "no trunk tip after post squash"
[ "$T_squash" != "$T_pre" ] || fail "post squash did not advance trunk tip"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T_squash" ] || fail "trunk REFS should be T_squash"
note "trunk tip advanced to T_squash=$T_squash"

# 14. verify squash invariants
echo "=== 14. verify squash invariants ==="
PARENTS=$("$KEEPER" get ".#$T_squash" 2>/dev/null \
            | grep -c '^parent ' || true)
[ "$PARENTS" = "1" ] \
    || fail "T_squash has $PARENTS parent line(s); want exactly 1"
PARENT_SHA=$("$KEEPER" get ".#$T_squash" 2>/dev/null \
                | awk '/^parent / { print $2; exit }')
[ "$PARENT_SHA" = "$T_pre" ] \
    || fail "T_squash parent is $PARENT_SHA; want T_pre=$T_pre"
note "T_squash has exactly 1 parent, == T_pre (no merge commit)"

FIX1_REFS=$(ref_tip "?fix1")
[ "$FIX1_REFS" = "$C2" ] \
    || fail "?fix1 tip moved across PATCH/POST: $C2 -> $FIX1_REFS"
note "?fix1 tip still at C2=$C2 (child untouched by PATCH)"

# 15. cleanup
echo "=== 15. cleanup: be delete ?fix1 ==="
"$BE" delete "?fix1" >/dev/null || fail "be delete ?fix1 (cleanup) failed"
[ -z "$(ref_tip "?fix1")" ] || fail "?fix1 still in REFS after cleanup"
[ ! -e .dogs/fix1 ] || fail ".dogs/fix1 left behind after cleanup"
note "?fix1 cleaned up"

echo "=== branches/02-patch: OK ==="
