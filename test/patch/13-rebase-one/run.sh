#!/bin/sh
#  13-rebase-one — `be patch ?feat#` applies the next not-yet-replayed
#  commit on feat (i.e. F1, the oldest unreachable) and records it as
#  a foster on next POST.  Msg reused from F1.
#
#  Topology:
#       T0 ── T1            ← cur (trunk)
#         \
#          F1 ── F2 ── F3   ← ?feat

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

"$BE" put '?./feat' >/dev/null

sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't1 greet=hello' >/dev/null
T1=$(head_hex)

"$BE" get '?feat' >/dev/null

# F1 (block-insert sub).
sleep 0.02; cp "$CASE/03.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f1 add sub function' >/dev/null
F1=$(head_hex)

# F2 (token-edit add).
sleep 0.02; cp "$CASE/04.lib.f2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f2 add parens' >/dev/null
F2=$(head_hex)

# F3 (line-append mul).
sleep 0.02; cp "$CASE/05.lib.f3.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f3 add mul' >/dev/null
F3=$(head_hex)

"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T1" ] || fail "wt should be at T1"

# THE ACTION: rebase one — applies F1 only.
"$BE" patch '?feat#' >"$ETMP/r.out" 2>"$ETMP/r.err" \
    || fail "be patch '?feat#' failed: $(cat $ETMP/r.err)"

# T1 edited lib.c (greet=hello), F1 edited lib.c (sub block) → merged.
grep -E '[[:space:]]+merged[[:space:]]+(\./)?lib\.c$' "$ETMP/r.out" \
    || fail "expected 'patch merged lib.c'; got: $(cat $ETMP/r.err)"

match "$CASE/06.lib.want.c" lib.c

# POST with no msg — must reuse F1's msg.
"$BE" post >/dev/null || fail "be post failed (expected F1 msg reuse)"
R=$(head_hex)

BODY=$("$KEEPER" get ".#$R" 2>/dev/null) || fail "keeper get failed"
echo "$BODY" | grep -q "^parent $T1$" || fail "first parent != T1"
echo "$BODY" | grep -q "^foster $F1$" || fail "foster $F1 missing"
echo "$BODY" | grep -q "^parent $F1$" && fail "F1 recorded as parent (should be foster)"
echo "$BODY" | grep -q '^picked' && fail "picked trailer leaked"
echo "$BODY" | grep -q 'f1 add sub function' || fail "F1 msg not reused"

note "rebase-one OK: cur=$R has parent=$T1 + foster=$F1, msg reused"
echo "=== patch/13-rebase-one: OK ==="
