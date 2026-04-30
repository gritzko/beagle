#!/bin/sh
. "$(dirname "$0")/../../lib/case.sh"

# Stage 1: seed two files and commit a baseline.
cp "$CASE/01.unchanged.txt" unchanged.txt
cp "$CASE/02.to-modify.txt" to-modify.txt
"$BE" put unchanged.txt to-modify.txt >/dev/null 2>&1
"$BE" post baseline >/dev/null 2>&1

# Stage 2: mutate one file, drop in a new one, leave one alone.
cp "$CASE/03.modified.txt" to-modify.txt
cp "$CASE/04.new.txt"      new.txt

# Stage 3: re-`put` all three via a glob.  Expansion is sorted:
#   new.txt  to-modify.txt  unchanged.txt
#
# `be put` now warn-and-skips unchanged paths in a multi-arg list,
# staging the modified + new entries on the same invocation.  Exit
# is success because at least one file was staged.
"$BE" put *.txt >05.put.got.out 2>05.put.got.err

match    "$CASE/05.put.want.txt" 05.put.got.out
match_re "$CASE/05.put.err.txt" 05.put.got.err
