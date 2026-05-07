#!/bin/sh
#  12-merge — `be patch ?feat '#merge feat into trunk'` records a
#  `parent <feat.tip>` header on next POST (true merge, multi-parent).
#  Msg comes from the patch row's fragment.
#
#  Topology:
#       T0 ── T1 ── T2     ← cur (trunk)
#         \
#          F1 ── F2        ← ?feat

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

"$BE" put '?./feat' >/dev/null

sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't1 greet=hello' >/dev/null
T1=$(head_hex)

sleep 0.02; cp "$CASE/03.lib.t2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't2 bye=farewell' >/dev/null
T2=$(head_hex)

"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/04.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f1 add mul' >/dev/null
F1=$(head_hex)

sleep 0.02; cp "$CASE/05.lib.f2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f2 add parens' >/dev/null
F2=$(head_hex)

"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T2" ] || fail "wt should be at T2"

# THE ACTION: merge feat into trunk with explicit msg.
"$BE" patch '?feat' '#merge feat into trunk' \
    >"$ETMP/m.out" 2>"$ETMP/m.err" \
    || fail "be patch ?feat '#merge ...' failed: $(cat $ETMP/m.err)"

grep -E '^patch[[:space:]]+merged[[:space:]]+(\./)?lib\.c$' "$ETMP/m.err" \
    || fail "expected 'patch merged lib.c'; got: $(cat $ETMP/m.err)"

match "$CASE/06.lib.want.c" lib.c

# POST with no own fragment — msg reused from patch row's "merge feat into trunk".
"$BE" post >/dev/null || fail "be post failed"
M=$(head_hex)

BODY=$("$KEEPER" get ".#$M" 2>/dev/null) || fail "keeper get failed"
echo "$BODY" | grep -q "^parent $T2$" || fail "first parent != T2"
echo "$BODY" | grep -q "^parent $F2$" || fail "second parent (feat.tip $F2) missing"
echo "$BODY" | grep -q '^foster ' && fail "foster header leaked into merge"
echo "$BODY" | grep -q '^picked' && fail "picked trailer leaked into merge"
echo "$BODY" | grep -q 'merge feat into trunk' || fail "merge msg not used"

note "merge OK: cur=$M has parents [$T2, $F2], msg from patch row"
echo "=== patch/12-merge: OK ==="
