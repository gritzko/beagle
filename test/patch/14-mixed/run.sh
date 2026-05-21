#!/bin/sh
#  14-mixed — combine cherry-pick (`#<B1>`) AND merge (`?feat '#msg'`)
#  in a single POST.  Two commits applied → POST refuses without an
#  own fragment (msg ambiguity); rerun with explicit msg succeeds.
#  Resulting commit has parent <T1>, parent <feat.tip>, picked <B1>.
#
#  Topology:
#       T0 ── T1                ← cur (trunk)
#         \
#          F1 ── F2             ← ?feat
#         \
#          B1                   ← ?bug

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

"$BE" put '?./feat' >/dev/null
"$BE" put '?./bug'  >/dev/null

sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't1 greet=hello' >/dev/null
T1=$(head_hex)

# Build feat: F1, F2.
"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/03.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f1 mul' >/dev/null
F1=$(head_hex)

sleep 0.02; cp "$CASE/04.lib.f2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f2 parens' >/dev/null
F2=$(head_hex)

# Build bug: B1.
"$BE" get '?bug' >/dev/null
sleep 0.02; cp "$CASE/05.lib.b1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'b1 fix bye spelling' >/dev/null
B1=$(head_hex)

# Back to trunk at T1.
"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T1" ] || fail "wt should be at T1"

# Cherry-pick B1.  T1 edited lib.c, B1 edited lib.c → merged.
# Located form (`?bug/<sha>`) per dog/DOG.h §DOGRefSplitPin.
"$BE" patch "?bug/$B1" >"$ETMP/cp.out" 2>"$ETMP/cp.err" \
    || fail "be patch ?bug/B1 failed: $(cat $ETMP/cp.err)"
grep -E '[[:space:]]+merged[[:space:]]+(\./)?lib\.c$' "$ETMP/cp.out" \
    || fail "cherry-pick: expected 'patch merged lib.c'; got: $(cat $ETMP/cp.err)"

# Merge feat with explicit msg.
"$BE" patch '?feat' '#feat+bug' >"$ETMP/m.out" 2>"$ETMP/m.err" \
    || fail "be patch ?feat '#feat+bug' failed: $(cat $ETMP/m.err)"
grep -E '[[:space:]]+merged[[:space:]]+(\./)?lib\.c$' "$ETMP/m.out" \
    || fail "merge: expected 'patch merged lib.c'; got: $(cat $ETMP/m.err)"

# Two commits applied → POST without own msg must refuse (ambiguous).
mustnt "$BE" post

# POST with explicit msg succeeds.
"$BE" post '#mixed: feat+bug fix' >/dev/null || fail "be post '#mixed ...' failed"
MX=$(head_hex)

BODY=$("$KEEPER" get ".#$MX" 2>/dev/null) || fail "keeper get failed"
echo "$BODY" | grep -q "^parent $T1$" || fail "first parent != T1"
echo "$BODY" | grep -q "^parent $F2$" || fail "feat.tip parent missing"
echo "$BODY" | grep -q "^picked: $B1$" || fail "picked: B1 trailer missing"
echo "$BODY" | grep -q "^foster" && fail "no foster expected (merge form, not squash/rebase)"
echo "$BODY" | grep -q 'mixed: feat+bug fix' || fail "POST msg not used"

match "$CASE/06.lib.want.c" lib.c

note "mixed OK: cur=$MX has parents [$T1, $F2] + picked: $B1"
echo "=== patch/14-mixed: OK ==="
