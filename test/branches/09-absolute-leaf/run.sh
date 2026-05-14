#!/bin/sh
#  branches/09-absolute-leaf — extracted from workflow-branches.sh stages 32-34.
#  Absolute-path leaf creation, trailing-slash basename reuse, absolute
#  tree-parent auto-syncs cur.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# === 32. ?<absolute>/<newleaf> create-on-miss ===
echo "=== 32. ?<absolute>/<newleaf> create-on-miss ==="
cd "$WT"

sleep 0.01
echo "x32" > x32.txt
"$BE" put x32.txt >/dev/null
"$BE" post '32-base msg' >/dev/null || fail "§32: base post failed"
T32_TRUNK=$(ref_tip "?")

"$BE" put "?./feat" >/dev/null || fail "§32: create ?feat failed"
"$BE" put "?./fix1" >/dev/null || fail "§32: create ?fix1 failed"
"$BE" get "?fix1" >/dev/null || fail "§32: switch ?fix1 failed"
sleep 0.01
echo "fix1 work 32" > fwork32.txt
"$BE" put fwork32.txt >/dev/null
"$BE" post 'fix1-32 msg' >/dev/null || fail "§32: post fix1-32 failed"
F1_TIP_32=$(ref_tip "?fix1")
FEAT_TIP_32=$(ref_tip "?feat")

# Run the create-on-miss arm: be put ?feat/new from cur=?fix1.
"$BE" put "?feat/new" 2>"$ETMP/p32.err" >/dev/null \
    || { cat "$ETMP/p32.err"; fail "§32: be put ?feat/new failed"; }

NEW32=$(ref_tip "?feat/new")
[ -n "$NEW32" ] || fail "§32: ?feat/new not in REFS"
F1_TIP_32_AFTER=$(ref_tip "?fix1")
[ "$F1_TIP_32_AFTER" = "$F1_TIP_32" ] \
    || fail "§32: cur ?fix1 moved: $F1_TIP_32 -> $F1_TIP_32_AFTER"
[ -d ".be/feat/new" ] || fail "§32: .be/feat/new shard missing"

seen_feat=NO
cur="$NEW32"; n=0
#  Keeper has no wt context here — pass `?fix1` so its PAST (trunk +
#  fix1's pack) covers the whole chain we're about to walk.  Keeper
#  refuses to auto-discover branches per the explicit-form contract.
while [ -n "$cur" ] && [ $n -lt 20 ]; do
    if [ "$cur" = "$FEAT_TIP_32" ]; then seen_feat=YES; break; fi
    p=$("$KEEPER" get "?fix1#$cur" 2>/dev/null | awk '/^parent / { print $2; exit }')
    cur=$p; n=$((n+1))
done
[ "$seen_feat" = "YES" ] \
    || fail "§32: ?feat/new tip's parent chain doesn't reach FEAT_TIP_32"
note "§32 OK: ?feat/new created at $NEW32"

# cleanup leftovers from §32 so §33 starts clean
"$BE" get "?.." >/dev/null
"$BE" delete "?feat/new" >/dev/null 2>&1 || true
"$BE" delete "?feat"     >/dev/null 2>&1 || true
"$BE" delete "?fix1"     >/dev/null 2>&1 || true
rm -f x32.txt fwork32.txt
"$BE" delete x32.txt    >/dev/null 2>&1 || true
"$BE" delete fwork32.txt >/dev/null 2>&1 || true
"$BE" post '32-cleanup msg'   >/dev/null 2>&1 || true

# === 33. ?<absolute>/ trailing-slash reuse ===
echo "=== 33. ?<absolute>/ trailing-slash reuse ==="
cd "$WT"
sleep 0.01
echo "x33" > x33.txt
"$BE" put x33.txt >/dev/null
"$BE" post '33-base msg' >/dev/null || fail "§33: base post failed"

"$BE" put "?./feat" >/dev/null || fail "§33: create ?feat failed"
"$BE" put "?./fix1" >/dev/null || fail "§33: create ?fix1 failed"
"$BE" get "?fix1" >/dev/null || fail "§33: switch ?fix1 failed"
sleep 0.01
echo "fix1-33" > f33.txt
"$BE" put f33.txt >/dev/null
"$BE" post 'fix1-33 msg' >/dev/null || fail "§33: post fix1-33 failed"
F1_PRE_33=$(ref_tip "?fix1")
FEAT_PRE_33=$(ref_tip "?feat")

"$BE" put "?feat/" 2>"$ETMP/p33.err" >/dev/null \
    || { cat "$ETMP/p33.err"; fail "§33: be put ?feat/ failed"; }

NEW33=$(ref_tip "?feat/fix1")
[ -n "$NEW33" ] || fail "§33: ?feat/fix1 not in REFS"
F1_AFTER_33=$(ref_tip "?fix1")
[ "$F1_AFTER_33" = "$F1_PRE_33" ] \
    || fail "§33: cur ?fix1 moved"
[ -d ".be/feat/fix1" ] || fail "§33: .be/feat/fix1 shard missing"
seen_feat=NO
cur="$NEW33"; n=0
#  Same rationale as §32 — explicit branch hint required for keeper.
while [ -n "$cur" ] && [ $n -lt 20 ]; do
    if [ "$cur" = "$FEAT_PRE_33" ]; then seen_feat=YES; break; fi
    p=$("$KEEPER" get "?fix1#$cur" 2>/dev/null | awk '/^parent / { print $2; exit }')
    cur=$p; n=$((n+1))
done
[ "$seen_feat" = "YES" ] \
    || fail "§33: ?feat/fix1 tip's parent chain doesn't reach FEAT_PRE_33"
note "§33 OK: trailing-slash rewrote to ?feat/fix1 ($NEW33)"

# cleanup
"$BE" get "?.." >/dev/null
"$BE" delete "?feat/fix1" >/dev/null 2>&1 || true
"$BE" delete "?feat"      >/dev/null 2>&1 || true
"$BE" delete "?fix1"      >/dev/null 2>&1 || true
rm -f x33.txt f33.txt
"$BE" delete x33.txt >/dev/null 2>&1 || true
"$BE" delete f33.txt >/dev/null 2>&1 || true
"$BE" post '33-cleanup msg' >/dev/null 2>&1 || true

# === 34. `be patch ?feat#` from cur=?feat/fix is a no-op when ?feat
#         is an ancestor of cur — patch refuses with "nothing to
#         replay"; both refs stay put. ===
echo "=== 34. be patch ?feat# from ?feat/fix refuses (parent is ancestor) ==="
cd "$WT"
sleep 0.01
echo "x34" > x34.txt
"$BE" put x34.txt >/dev/null
"$BE" post '34-base msg' >/dev/null || fail "§34: base post failed"

"$BE" put "?./feat" >/dev/null || fail "§34: create ?feat failed"
"$BE" get "?feat" >/dev/null || fail "§34: switch ?feat failed"
"$BE" put "?./fix" >/dev/null || fail "§34: create ?feat/fix failed"
"$BE" get "?feat/fix" >/dev/null || fail "§34: switch ?feat/fix failed"
sleep 0.01
echo "feat/fix work" > ff34.txt
"$BE" put ff34.txt >/dev/null
"$BE" post 'feat-fix-c1 msg' >/dev/null || fail "§34: post feat-fix-c1 failed"
FF_PRE_34=$(ref_tip "?feat/fix")
FEAT_PRE_34=$(ref_tip "?feat")

set +e
"$BE" patch "?feat#" 2>"$ETMP/p34.err" >/dev/null
EC34=$?
set -e
[ "$EC34" != "0" ] \
    || fail "§34: be patch ?feat# should refuse (ancestor)"
grep -q "already reachable\|nothing to replay" "$ETMP/p34.err" \
    || fail "§34: stderr should mention 'already reachable'; got: $(cat $ETMP/p34.err)"

FEAT_AFTER_34=$(ref_tip "?feat")
FF_AFTER_34=$(ref_tip "?feat/fix")
[ "$FF_AFTER_34" = "$FF_PRE_34" ] \
    || fail "§34: cur ?feat/fix moved (patch refused — should be no-op)"
[ "$FEAT_AFTER_34" = "$FEAT_PRE_34" ] \
    || fail "§34: ?feat moved (patch refused — no commits should appear)"
note "§34 OK: patch onto ancestor refuses; refs unchanged"

echo "=== branches/09-absolute-leaf: OK ==="
