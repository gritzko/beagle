#!/bin/sh
#  11-cherry-pick — `be patch '#<F1-sha>'` applies one named commit;
#  POST records `picked: <F1>` trailer (no DAG edge), and reuses F1's
#  message because no own fragment was given and exactly one commit
#  was applied with a usable msg.
#
#  Topology:
#       T0 ── T1            ← cur (trunk)
#         \
#          F1 ── F2         ← ?feat (we cherry-pick F1, skip F2)

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

"$BE" put '?./feat' >/dev/null

# Trunk T1: edit greet (token-level).
sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't1 greet=hello' >/dev/null
T1=$(head_hex)

# Switch to feat; F1 inserts mul (block-level).
"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/03.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f1 add mul function' >/dev/null
F1=$(head_hex)

# F2: rewrite sub() body (line-level).
sleep 0.02; cp "$CASE/04.lib.f2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f2 sub identity' >/dev/null
F2=$(head_hex)

# Back to trunk at T1.
"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T1" ] || fail "wt should be at T1"

# THE ACTION: cherry-pick F1 via the bare `#<sha>` form (URI-001 Stage 4
# retired the located `?<branch>/<sha>` cherry — the flat per-project
# object pool makes the bare form resolve any commit without a locator).
"$BE" patch "#$F1" >"$ETMP/cp.out" 2>"$ETMP/cp.err" \
    || fail "be patch #F1 failed: $(cat $ETMP/cp.err)"

grep -E '[[:space:]]+merged[[:space:]]+(\./)?lib\.c$' "$ETMP/cp.out" \
    || fail "expected 'patch merged lib.c'; got: $(cat $ETMP/cp.err)"

match "$CASE/05.lib.want.c" lib.c

# POST with no fragment — msg must be reused from F1.
"$BE" post >/dev/null || fail "be post (no msg) failed; expected F1 msg reuse"
CP=$(head_hex)

BODY=$("$KEEPER" get ".#$CP" 2>/dev/null) || fail "keeper get .#$CP failed"
echo "$BODY" | grep -q "^parent $T1$"  || fail "first parent != T1"
echo "$BODY" | grep -q '^foster '       && fail "foster header leaked into cherry-pick"
echo "$BODY" | grep -q "^picked: $F1$" || fail "picked: $F1 trailer missing"
echo "$BODY" | grep -q 'f1 add mul function' || fail "F1 msg not reused"

note "cherry-pick OK: cur=$CP has picked: $F1, msg reused"
echo "=== patch/11-cherry-pick: OK ==="
