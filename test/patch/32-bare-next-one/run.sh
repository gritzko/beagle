#!/bin/sh
#  32-bare-next-one — DIS-030 default flip: bare `be patch ?feat` is
#  NEXT-ONE scope (absorb the single oldest un-absorbed commit), NOT a
#  squash of the whole stack.  `be patch ?feat!` is the WHOLE-branch
#  scope.  Assert the absorbed-commit banner count: bare = 1, `!` = all.
#
#  Topology:
#       T0 ── T1 ── T2     ← cur (trunk)
#         \
#          F1 ── F2        ← ?feat  (two commits diverged)

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0 baseline' >/dev/null

"$BE" put '?./feat' >/dev/null

sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't1 greet=hello' >/dev/null

sleep 0.02; cp "$CASE/03.lib.t2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't2 bye=farewell' >/dev/null
T2=$(head_hex)

"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/04.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f1 add parens' >/dev/null

sleep 0.02; cp "$CASE/05.lib.f2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f2 add mul' >/dev/null

"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T2" ] || fail "wt should be at T2"

# THE ACTION: bare `?feat` = NEXT-ONE.  Banner must list exactly ONE
# absorbed commit (F1, the oldest un-absorbed), NOT the whole F1+F2 stack.
"$BE" patch '?feat' >"$ETMP/one.out" 2>"$ETMP/one.err" \
    || fail "be patch '?feat' failed: $(cat $ETMP/one.err)"
none=$(grep -Ec 'post[[:space:]]+\?' "$ETMP/one.out")
[ "$none" -eq 1 ] \
    || fail "bare ?feat must absorb 1 commit, banner shows $none: $(cat $ETMP/one.out)"
grep -Eq 'post[[:space:]]+\?[0-9a-f]+#f1 add parens' "$ETMP/one.out" \
    || fail "bare ?feat should absorb F1 (the oldest), banner: $(cat $ETMP/one.out)"
grep -Eq 'post[[:space:]]+\?[0-9a-f]+#f2 add mul' "$ETMP/one.out" \
    && fail "bare ?feat must NOT absorb F2 (whole-stack = ?feat!): $(cat $ETMP/one.out)"

note "bare-next-one OK: ?feat absorbed 1 (F1); whole-stack is ?feat!"
echo "=== patch/32-bare-next-one: OK ==="
