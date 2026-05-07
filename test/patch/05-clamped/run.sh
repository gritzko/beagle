#!/bin/sh
#  05-clamped — `be patch ?branch#hash` clamps the absorbed range's
#  upper bound to `hash` instead of `tip(branch)`.
#
#  Topology (one one-pager.c file, edits stack across commits):
#
#      trunk:  A ── B ── C ── D                                  ←── cur
#                    \    \
#                     \    feat2: F ── G
#                      feat1: E
#
#  Per-commit edits:
#    A — baseline             (initial 11-line file)
#    B — token edit           (// version: 0  →  // version: 1)
#    C — block insert         (add `int f3(void)` before main)
#    D — line edit            ("hi" → "hello" in main's printf)
#    E — block remove         (drop `int f2(void)` block)        [feat1]
#    F — line edit            (// version: 1  →  // version: 2)  [feat2]
#    G — block insert         (add `int f4(void)`)               [feat2]
#
#  On trunk@D run `be patch ?./feat2#F`.  The clamp limits theirs to
#  F (G's f4 must NOT appear).  3-way merge:
#    base   = LCA(trunk, feat2) = C
#    ours   = D    (has the "hello" edit)
#    theirs = F    (has the version: 2 edit)
#  Result: D's "hello" + F's "version: 2", f3 already shared at C,
#  no f4.  feat1 is built but not patched — it's there to make the
#  3-branch topology realistic.

. "$(dirname "$0")/../../lib/case.sh"

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

#  Latest sniff baseline row's URI sha (post|get|patch).  Patch rows
#  now carry `theirs`; for capturing post commits we want the post
#  row's fragment, which is the new tip's sha.
head_hex() {
    awk -F'\t' '$2=="post"||$2=="get"||$2=="patch" { last=$3 }
                END {
                    h = last; sub(/^[^#]*#/, "", h)
                    if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
                }' .sniff
}

# A — baseline on trunk
sleep 0.02; cp "$CASE/01.A.c" one-pager.c
"$BE" put one-pager.c >/dev/null
"$BE" post 'A baseline' >/dev/null
A=$(head_hex)
[ -n "$A" ] || { echo "A sha missing" >&2; exit 1; }

# B — token edit on trunk
sleep 0.02; cp "$CASE/02.B.c" one-pager.c
"$BE" put one-pager.c >/dev/null
"$BE" post 'B version 1' >/dev/null
B=$(head_hex)

# Fork feat1 at B (cur stays on trunk, B is fork point)
"$BE" put '?./feat1' >/dev/null

# C — block insert on trunk
sleep 0.02; cp "$CASE/03.C.c" one-pager.c
"$BE" put one-pager.c >/dev/null
"$BE" post 'C add f3' >/dev/null
C=$(head_hex)

# Fork feat2 at C
"$BE" put '?./feat2' >/dev/null

# D — line edit on trunk
sleep 0.02; cp "$CASE/04.D.c" one-pager.c
"$BE" put one-pager.c >/dev/null
"$BE" post 'D hello' >/dev/null
D=$(head_hex)

# E — block remove on feat1
"$BE" get '?feat1' >/dev/null
sleep 0.02; cp "$CASE/05.E.c" one-pager.c
"$BE" put one-pager.c >/dev/null
"$BE" post 'E drop f2' >/dev/null
E=$(head_hex)

# F — line edit on feat2
"$BE" get '?feat2' >/dev/null
sleep 0.02; cp "$CASE/06.F.c" one-pager.c
"$BE" put one-pager.c >/dev/null
"$BE" post 'F version 2' >/dev/null
F=$(head_hex)
[ -n "$F" ] || { echo "F sha missing" >&2; exit 1; }

# G — block insert on feat2 (must NOT end up in the clamped patch)
sleep 0.02; cp "$CASE/07.G.c" one-pager.c
"$BE" put one-pager.c >/dev/null
"$BE" post 'G add f4' >/dev/null
G=$(head_hex)

# Back to trunk @ D
"$BE" get '?..' >/dev/null

# Clamped patch: feat2 stack absorbed up to F only.
"$BE" patch "?./feat2#$F" >"$OUT/patch.out" 2>"$OUT/patch.err"

# wt must equal D + F's delta, with NO f4 from G.
match "$CASE/08.D-plus-F.c" one-pager.c

# Negative checks: G's `f4` must not have leaked in.
if grep -q 'f4' one-pager.c; then
    echo "FAIL: clamp leaked G's f4 into wt" >&2
    cat one-pager.c >&2
    exit 1
fi

# Positive checks: F's bump (version: 2) and D's edit (hello) both present.
grep -q 'version: 2' one-pager.c || {
    echo "FAIL: F's version-2 edit missing" >&2; exit 1; }
grep -q 'hello' one-pager.c || {
    echo "FAIL: D's hello edit missing" >&2; exit 1; }

#  Patch row carries theirs = F (not G), confirming the clamp.
PATCH_ROW=$(awk -F'\t' '$2=="patch" { last=$3 } END { print last }' .sniff)
case "$PATCH_ROW" in
    "#$F") ;;  # ok
    *) echo "FAIL: patch row fragment '$PATCH_ROW' != #$F" >&2; exit 1;;
esac

rm -rf "$OUT"
