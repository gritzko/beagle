#!/bin/sh
#  diff/07-hex-named-branch — URI-001 §"The one rule" (Stage 4c):
#  branch-vs-hash disambiguation is BRANCH-FIRST.  A branch whose NAME
#  is all hex (`c0ffee`) must resolve as a BRANCH in a diff ref, never
#  be misrouted into the hashlet (`#<sha>`) path.  This is the exact
#  same scenario as 02-divergent-children with `fix2` renamed to the
#  hex-spelled `c0ffee`, so the byte-exact diff output is identical —
#  the want files are copies of 02's.  Before Stage 4c the syntactic
#  `DOGIsHashlet("c0ffee")` guess sent the ref to keeper's hashlet
#  lookup (6-hex prefix, no such object) and the diff failed.

. "$(dirname "$0")/../../lib/case.sh"

#  --- trunk baseline -----------------------------------------------
sleep 0.02; cp "$CASE/01.foo.c" foo.c
touch -d "2026-04-20 12:01:00" foo.c
"$BE" put foo.c >/dev/null
"$BE" post 'baseline msg' >/dev/null

#  --- create + switch to ?fix1, edit, post --------------------------
"$BE" put '?./fix1' >/dev/null
"$BE" get  '?fix1'   >/dev/null
sleep 0.02; cp "$CASE/02.foo.fix1.c" foo.c
touch -d "2026-04-20 12:02:00" foo.c
"$BE" put foo.c >/dev/null
"$BE" post 'c1 msg' >/dev/null

#  --- back to trunk, create + switch to the HEX-NAMED ?c0ffee -------
"$BE" get  '?..'       >/dev/null
"$BE" put '?./c0ffee' >/dev/null
"$BE" get  '?c0ffee'   >/dev/null
sleep 0.02; cp "$CASE/03.foo.c0ffee.c" foo.c
touch -d "2026-04-20 12:03:00" foo.c
"$BE" put foo.c >/dev/null
"$BE" post 'c1 msg' >/dev/null

#  --- diff fix1..c0ffee (hex name in the fragment / to-side) --------
"$BE" get 'diff:foo.c?fix1#c0ffee' \
    >04.diff_fix1_c0ffee.got.out 2>04.diff_fix1_c0ffee.got.err
match "$CASE/04.diff_fix1_c0ffee.want.txt" 04.diff_fix1_c0ffee.got.out
empty 04.diff_fix1_c0ffee.got.err

#  --- diff c0ffee..fix1 (hex name in the query / from-side) ---------
"$BE" get 'diff:foo.c?c0ffee#fix1' \
    >05.diff_c0ffee_fix1.got.out 2>05.diff_c0ffee_fix1.got.err
match "$CASE/05.diff_c0ffee_fix1.want.txt" 05.diff_c0ffee_fix1.got.out
empty 05.diff_c0ffee_fix1.got.err
