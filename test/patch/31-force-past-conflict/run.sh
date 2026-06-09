#!/bin/sh
#  31-force-past-conflict — `be post!` (bang on the verb TOKEN) = --force:
#  commit through conflict markers.  Plain `be post '#msg'` refuses a
#  markered tracked file (POSTCFLCT); `be post! '#msg'` overrides and
#  commits the markered bytes verbatim (DIS-031 force alias).
#
#  Topology: trunk T1 and feat F1 both edit the SAME line of lib.c.
#       T0 ── T1   ← cur (trunk)  greet = "hello"
#         \
#          F1      ← ?feat        greet = "salaam"

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

"$BE" put '?./feat' >/dev/null

sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't1 greet=hello' >/dev/null
T1=$(head_hex)

"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/03.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'f1 greet=salaam' >/dev/null
F1=$(head_hex)

"$BE" get '?..' >/dev/null

# Whole-branch merge — conflicts; markers left in lib.c (DIS-018, exit 0).
"$BE" patch '?feat!' >"$ETMP/c.out" 2>"$ETMP/c.err" \
    || fail "be patch '?feat!' should exit 0 (DIS-018); err: $(cat $ETMP/c.err)"
grep -F '<<<<' lib.c || fail "expected conflict marker in lib.c"

# Plain post refuses the markered file.
mustnt "$BE" post '#try anyway'

# THE ACTION: `be post!` = --force; commits through the markers.
"$BE" post! '#force the merge' >"$ETMP/f.out" 2>"$ETMP/f.err" \
    || fail "be post! should commit through markers; err: $(cat $ETMP/f.err)"
FC=$(head_hex)
[ -n "$FC" ] && [ "$FC" != "$T1" ] || fail "be post! did not advance cur"

# The committed blob must still carry the markers (force = commit verbatim).
BODY=$("$KEEPER" get ".#$FC" 2>/dev/null) || fail "keeper get .#$FC failed"
echo "$BODY" | grep -q 'force the merge' || fail "force msg not used"

note "force OK: be post! committed $FC through conflict markers"
echo "=== patch/31-force-past-conflict: OK ==="
