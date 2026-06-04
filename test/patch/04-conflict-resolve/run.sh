#!/bin/sh
#  04-conflict-resolve — exercise PATCH against a richly-evolved
#  one-page C file with two real conflicts the merger cannot pick:
#
#    * block-shape conflict: both sides insert a different multi-line
#      `if`-block at the same location (just before `return 0;`).
#      The merger frames the divergent inserts with `<<<<` / `||||`
#      / `>>>>` markers spanning multiple lines.
#    * token-shape conflict: both sides rewrite the same string
#      literal inside greet's printf, using a different first word.
#      The marker run sits inline within one source line.
#
#  Disjoint edits land cleanly:
#    * trunk T1 inserts mul() after add()
#    * trunk T2 changes x = add(1, 2)  to x = add(10, 20)
#    * fix   F1 inserts neg() after sub()
#    * fix   F2 changes y = sub(5, 3) to y = sub(50, 30)
#
#  Branch shape (cur=trunk after step 7):
#
#      trunk ─ T0 ─┬─ T1 ─ T2 ─ T3 ─ T4   ←── cur
#                  └─ fix1: F1 ─ F2 ─ F3 ─ F4
#
#  After `be patch ?./fix1` the wt's lib.c carries `<<<<` / `||||` /
#  `>>>>` markers around both conflict regions plus both clean
#  insertions.  We then hand-resolve, `be put`, and `be post` —
#  the merged tree must commit as a single-parent commit on cur.

. "$(dirname "$0")/../../lib/case.sh"

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

# T0 baseline on trunk
sleep 0.02; cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0 baseline' >/dev/null

# Fork fix1 off T0 (cur stays on trunk)
"$BE" put '?./fix1' >/dev/null

# --- trunk: T1..T4 -------------------------------------------------
sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't1 add mul' >/dev/null

sleep 0.02; cp "$CASE/03.lib.t2.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't2 widen x' >/dev/null

sleep 0.02; cp "$CASE/04.lib.t3.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't3 trunk block' >/dev/null

sleep 0.02; cp "$CASE/05.lib.t4.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't4 trunk greet' >/dev/null

# --- fix1: F1..F4 --------------------------------------------------
"$BE" get '?fix1' >/dev/null

sleep 0.02; cp "$CASE/06.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'f1 add neg' >/dev/null

sleep 0.02; cp "$CASE/07.lib.f2.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'f2 widen y' >/dev/null

sleep 0.02; cp "$CASE/08.lib.f3.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'f3 fix block' >/dev/null

sleep 0.02; cp "$CASE/09.lib.f4.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'f4 fix greet' >/dev/null

# --- back to trunk, weave fix1 in ----------------------------------
"$BE" get '?..' >/dev/null

# DIS-018: `be patch` now returns OK (exit 0) on conflict and reports
# `conf`; the markers stay in the file (POST is the safety net).
set +e
"$BE" patch '?./fix1' >"$OUT/patch.out" 2>"$OUT/patch.err"
PATCH_RC=$?
set -e
[ "$PATCH_RC" = "0" ] || {
    echo "FAIL: patch exited $PATCH_RC — expected 0 on conflict (DIS-018)" >&2
    cat "$OUT/patch.err" >&2
    exit 1
}

# Conflicted wt must match the captured reference byte-for-byte —
# pins the marker shapes (token-shape on greet's literal, block-shape
# spanning fix's while-loop vs trunk's diff/fputs tail) and the clean
# splice points (mul/neg fns, add(10,20), sub(50,30)).
match "$CASE/11.lib.conflicted.c" lib.c

# Hand-resolve: overwrite with the merged tree.
sleep 0.02; cp "$CASE/10.lib.resolved.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'resolve trunk + fix1' >"$OUT/post.out" 2>"$OUT/post.err"

# wt now holds the resolved tree, byte-for-byte.
match "$CASE/10.lib.resolved.c" lib.c
