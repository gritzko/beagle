#!/bin/sh
#  17-conflict-refactor — two branches refactor the SAME variable in
#  incompatible ways: trunk renames `i` → `j` (still an int); feat
#  retypes `i` from int to `char *` and switches the printf format
#  from "%i" to "%s".  Same identifier, same lines, divergent intent.
#
#  Topology:
#       T0 ── T1   ← cur (trunk)    int j; printf("%i", j)
#         \
#          F1     ← ?feat           char *i; printf("%s!", i)
#
#  main() reads argv[1], mutates it, writes the result back to stdout
#  — the rename on trunk vs the retype on feat collide on every line
#  that mentions the variable.
#
#  Captures WEAVE's current behaviour as a golden fixture
#  (`04.lib.merged.c`) so a future change in the merger is forced to
#  acknowledge what this case produces.  As of writing, WEAVE token-
#  merges the two refactors without emitting `<<<<` markers and the
#  output is non-compilable (`j` referenced but undeclared, `jargv[1]`
#  fused token).  Whether that should be a conflict instead is up to
#  the merger; this test pins observed behaviour.

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

"$BE" put '?./feat' >/dev/null

# Trunk T1: rename i → j.
sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't1 rename i to j' >/dev/null
T1=$(head_hex)

# Feat F1: retype i from int to char *.
"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/03.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'f1 retype i to char*' >/dev/null
F1=$(head_hex)

"$BE" get '?..' >/dev/null

# THE ACTION: merge feat into trunk — must conflict.
if "$BE" patch '?feat' '#merge feat' >"$ETMP/c.out" 2>"$ETMP/c.err"; then
    fail "be patch ?feat should have failed (conflict expected)"
fi

# Per-file status row: lib.c → conflict.
grep -E '[[:space:]]+conflict[[:space:]]+(\./)?lib\.c$' "$ETMP/c.out" \
    || fail "expected 'patch conflict lib.c' status row; got: $(cat $ETMP/c.err)"

# WEAVE conflict markers — 4-char `<<<<` / `||||` / `>>>>` framing
# the divergent refactors.  Must NOT use git's 7-char markers.
grep -F '<<<<' lib.c >/dev/null \
    || fail "expected '<<<<' marker in lib.c; got: $(cat lib.c)"
grep -F '||||' lib.c >/dev/null \
    || fail "expected '||||' separator in lib.c"
grep -F '>>>>' lib.c >/dev/null \
    || fail "expected '>>>>' marker in lib.c"
grep -F '<<<<<<<' lib.c >/dev/null \
    && fail "found 7-char git-style marker; spec says 4-char"

# Pin the merged tree byte-for-byte — captures WEAVE's current
# output shape (token-interleaved inside markers).  Update alongside
# any intentional change to the merger.
match "$CASE/04.lib.merged.c" lib.c

# POST must be refused — conflict markers in tracked file.
mustnt "$BE" post '#try anyway'

note "conflict-refactor OK: WEAVE markers present, patch exited non-zero"
echo "=== patch/17-conflict-refactor: OK ==="
