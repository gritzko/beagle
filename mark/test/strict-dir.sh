#!/bin/sh
#  mark --strict directory CI gate (DIS-013 regression).
#
#  The docs promise `mark --strict wiki/` "fails on any budget breach"
#  with a non-zero exit for CI.  The directory walk used to swallow every
#  per-file breach and exit 0; single-file mode was always correct (exit
#  157 / MARKLIMIT).  This asserts both modes agree under --strict, that
#  the walk reports every breaching page, and that a clean tree / lax
#  mode still exit 0.
#
#  Usage: strict-dir.sh <path-to-mark-binary>

set -u

MARK="$1"
[ -x "$MARK" ] || { echo "strict-dir: no mark binary at $MARK" >&2; exit 1; }

WORK=$(mktemp -d) || exit 1
trap 'rm -rf "$WORK"' EXIT

fail=0
note() { echo "strict-dir: FAIL: $1" >&2; fail=1; }

#  A page with two H1 openers breaks the one-opener-per-page rule.
two_h1() { printf '#   %s one\n\n#   %s two\n' "$1" "$1"; }

#  Case 1: single file --strict exits non-zero (the known-good baseline).
printf '#   T one\n\n#   T two\n' > "$WORK/solo.mkd"
"$MARK" --strict "$WORK/solo.mkd" >/dev/null 2>&1
[ $? -ne 0 ] || note "single-file --strict breach exited 0"

#  Case 2: directory --strict with a breaching page exits non-zero.
mkdir "$WORK/wiki"
printf '#   Good\n\nhello world\n' > "$WORK/wiki/good.mkd"
two_h1 Bad > "$WORK/wiki/bad.mkd"
"$MARK" --strict "$WORK/wiki" >/dev/null 2>&1
[ $? -ne 0 ] || note "directory --strict breach exited 0 (the DIS-013 bug)"

#  Case 3: the walk reports every breaching page, not just the first.
two_h1 Bad1 > "$WORK/wiki/bad1.mkd"
two_h1 Bad2 > "$WORK/wiki/bad2.mkd"
out=$("$MARK" --strict "$WORK/wiki" 2>&1)
n=$(printf '%s\n' "$out" | grep -c 'failed: MARKLIMIT')
[ "$n" -ge 2 ] || note "walk stopped early: only $n breaches reported"

#  Case 4: a clean directory under --strict exits 0.
mkdir "$WORK/clean"
printf '#   T\n\nhello world\n' > "$WORK/clean/ok.mkd"
"$MARK" --strict "$WORK/clean" >/dev/null 2>&1
[ $? -eq 0 ] || note "clean directory --strict exited non-zero"

#  Case 5: directory breach WITHOUT --strict still exits 0 (warn only).
mkdir "$WORK/lax"
two_h1 Lax > "$WORK/lax/bad.mkd"
"$MARK" "$WORK/lax" >/dev/null 2>&1
[ $? -eq 0 ] || note "lax directory breach exited non-zero"

[ $fail -eq 0 ] || exit 1
echo "strict-dir: all cases passed"
