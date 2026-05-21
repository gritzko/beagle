#!/bin/sh
#  10-squash-foster — `be patch ?feat` (no fragment) absorbs feat's
#  whole stack as a squash; next POST records ONE foster header
#  pointing at feat.tip, no parent-from-feat in the DAG.
#
#  Topology:
#       T0 ── T1 ── T2     ← cur (trunk)
#         \
#          F1 ── F2        ← ?feat
#
#  Trunk T1 edits the `string` block (token-level, greet=hello);
#  T2 edits another token (bye=farewell) + block-inserts say_bye().
#  Feat F1 wraps add() in parens (token-level); F2 inserts mul()
#  (block-level).  Sides edit disjoint regions — WEAVE composes
#  without conflict.

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0 baseline' >/dev/null
T0=$(head_hex)

# Fork ?feat off T0 (cur stays on trunk).
"$BE" put '?./feat' >/dev/null

sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't1 greet=hello' >/dev/null
T1=$(head_hex)

sleep 0.02; cp "$CASE/03.lib.t2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't2 bye=farewell + say_bye' >/dev/null
T2=$(head_hex)

# Switch to feat at T0; build F1, F2.
"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/04.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f1 add parens' >/dev/null
F1=$(head_hex)

sleep 0.02; cp "$CASE/05.lib.f2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f2 add mul' >/dev/null
F2=$(head_hex)

# Back to trunk at T2.
"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T2" ] || fail "wt should be at T2 after switch back"

# THE ACTION: squash feat into wt.
"$BE" patch '?feat' >"$ETMP/patch.out" 2>"$ETMP/patch.err" \
    || fail "be patch ?feat failed: $(cat $ETMP/patch.err)"

# Per-file status: lib.c was edited on both sides → merged.
grep -E '[[:space:]]+merged[[:space:]]+(\./)?lib\.c$' "$ETMP/patch.out" \
    || fail "expected 'patch merged lib.c'; got: $(cat $ETMP/patch.err)"

# Wt must carry every edit from both sides.
match "$CASE/06.lib.want.c" lib.c

# POST the squash.
"$BE" post '#squash feat' >/dev/null || fail "be post failed"
SQ=$(head_hex)
[ -n "$SQ" ] && [ "$SQ" != "$T2" ] || fail "post did not advance cur"

# Inspect commit body: must have parent T2 AND foster F2.
BODY=$("$KEEPER" get ".#$SQ" 2>/dev/null) || fail "keeper get .#$SQ failed"
echo "$BODY" | grep -q "^parent $T2$" || fail "first parent != T2 in commit $SQ"
echo "$BODY" | grep -q "^foster $F2$" || fail "foster $F2 missing in commit $SQ"
echo "$BODY" | grep -q "^parent $F2$" && fail "feat tip recorded as parent (should be foster)"
echo "$BODY" | grep -q '^picked' && fail "picked trailer leaked into squash"

note "squash OK: cur=$SQ has parent=$T2 + foster=$F2"
echo "=== patch/10-squash-foster: OK ==="
