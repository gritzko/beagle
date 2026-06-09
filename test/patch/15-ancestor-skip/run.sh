#!/bin/sh
#  15-ancestor-skip — overlapping ancestor sets are NOT re-applied.
#  Two PATCH calls share an unseen-commit prefix; the second skips
#  the commits the first already absorbed.  The "already-applied"
#  set = cur's first-parent ancestors ∪ all foster/parent reachable
#  from prior posts.
#
#  Topology:
#       T0 ── T1                              ← cur (trunk)
#         \
#          F1 ── F2 ── F3                     ← ?feat
#                  \
#                   G1                        ← ?feat/gizmo (shares F1, F2)
#
#  Step 1: from cur=trunk, `be patch ?feat/gizmo` (squash) → POST.  Cur
#          now has foster=G1.tip; ancestors include F1, F2, G1.
#  Step 2: `be patch ?feat` (squash again).  Must absorb only F3
#          (since F1 and F2 are already reachable via the foster
#          chain from step 1).  POST records foster=F3 (==feat.tip),
#          and msg reuse from F3 is allowed.
#
#  The wt also carries note.txt — never touched by any commit, but
#  goes dirty before step 2 to exercise the `dirty` per-file status.

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c"   lib.c
cp "$CASE/09.note.t0.txt" note.txt
"$BE" put lib.c note.txt >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

"$BE" put '?./feat' >/dev/null

sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 't1' >/dev/null
T1=$(head_hex)

# Build feat: F1, F2.
"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/03.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f1 add sub' >/dev/null
F1=$(head_hex)

sleep 0.02; cp "$CASE/04.lib.f2.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f2 add mul' >/dev/null
F2=$(head_hex)

# Fork ?feat/gizmo at F2 (cur is feat; child branch shares F1+F2).
"$BE" put '?./gizmo' >/dev/null

# F3 on feat.
sleep 0.02; cp "$CASE/05.lib.f3.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'f3 divmod' >/dev/null
F3=$(head_hex)

# Switch to gizmo at F2, add G1.
"$BE" get '?feat/gizmo' >/dev/null
[ "$(head_hex)" = "$F2" ] || fail "gizmo should fork at F2"
sleep 0.02; cp "$CASE/06.lib.g1.c" lib.c
"$BE" put lib.c >/dev/null; "$BE" post 'g1 farewell' >/dev/null
G1=$(head_hex)

# Back to trunk at T1.
"$BE" get '?'  >/dev/null
[ "$(head_hex)" = "$T1" ] || fail "wt should be at T1"

# Step 1: squash gizmo (whole-branch).  Absorbs F1+F2+G1.
"$BE" patch '?feat/gizmo!' >"$ETMP/p1.out" 2>"$ETMP/p1.err" \
    || fail "patch '?feat/gizmo!' failed: $(cat $ETMP/p1.err)"
match "$CASE/07.lib.want_step1.c" lib.c

# `#msg!` = new msg + forget (foster) = squash.
"$BE" post '#absorb gizmo!' >/dev/null || fail "post #1 failed"
P1=$(head_hex)

BODY=$("$KEEPER" get ".#$P1" 2>/dev/null) || fail "keeper get $P1 failed"
echo "$BODY" | grep -q "^foster $G1$" || fail "expected foster=$G1 on first POST"

# Make note.txt dirty.
sleep 0.02
echo "user scribble before step 2" >> note.txt

# Step 2: squash feat.  Already-applied = ancestors(P1) which
# includes G1, F2, F1, T1, T0.  Only F3 should weave in.
"$BE" patch '?feat!' >"$ETMP/p2.out" 2>"$ETMP/p2.err" \
    || fail "patch '?feat!' failed: $(cat $ETMP/p2.err)"

# Per-file status: lib.c → merged; note.txt → mod (ours-diverged,
# theirs untouched, preserved).
grep -E '[[:space:]]+merged[[:space:]]+(\./)?lib\.c$' "$ETMP/p2.out" \
    || fail "expected 'patch merged lib.c'; got: $(cat $ETMP/p2.err)"
grep -E '[[:space:]]+mod[[:space:]]+(\./)?note\.txt$' "$ETMP/p2.out" \
    || fail "expected 'patch mod note.txt'; got: $(cat $ETMP/p2.err)"

match "$CASE/08.lib.want_step2.c" lib.c
match "$CASE/10.note.want.txt"   note.txt

# Step 2 reporting: ancestor-skip means exactly ONE absorbed commit
# (F3).  The banner lists it as a `post\t?<hashlet>#<subject>` row on
# stdout (p2.out); F1/F2 are already reachable via the prior foster
# chain, so they must NOT appear (by subject or by sha).
nb=$(grep -Ec 'post[[:space:]]+\?' "$ETMP/p2.out")
[ "$nb" -eq 1 ] \
    || fail "expected exactly 1 absorbed-commit row (F3), got $nb: $(cat $ETMP/p2.out)"
grep -Eq 'post[[:space:]]+\?[0-9a-f]+#f3 divmod' "$ETMP/p2.out" \
    || fail "banner should list F3 (f3 divmod); got: $(cat $ETMP/p2.out)"
grep -Eq 'add sub|add mul' "$ETMP/p2.out" \
    && fail "F1/F2 subjects leaked into step-2 banner (should be skipped)"
grep -q "$F1" "$ETMP/p2.out" "$ETMP/p2.err" \
    && fail "F1 should have been skipped (already reachable via prior foster)"
grep -q "$F2" "$ETMP/p2.out" "$ETMP/p2.err" \
    && fail "F2 should have been skipped (already reachable via prior foster)"

# POST with explicit msg + forget (`#msg!` → foster): whole-branch
# scope never auto-reuses a msg (multiple commits collapsed in general).
# Even though only F3 was effectively new, the row is WHOLE so the user
# supplies the msg; the trailing `!` records feat.tip as a foster.
"$BE" post '#absorb feat (F3)!' >/dev/null || fail "be post failed"
P2=$(head_hex)

BODY=$("$KEEPER" get ".#$P2" 2>/dev/null) || fail "keeper get $P2 failed"
echo "$BODY" | grep -q "^parent $P1$" || fail "first parent != $P1 in second POST"
echo "$BODY" | grep -q "^foster $F3$" || fail "expected foster=$F3 (feat.tip)"
echo "$BODY" | grep -q 'absorb feat' || fail "POST msg not used"

note "ancestor-skip OK: P1 absorbed gizmo; P2 absorbed only F3 (F1/F2 skipped)"
echo "=== patch/15-ancestor-skip: OK ==="
