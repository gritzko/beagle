#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage one .c file with multiple call patterns.
sleep 0.02; cp "$CASE/calls.c" calls.c
"$BE" put calls.c  > /dev/null 2>&1
"$BE" post 'init msg'  > /dev/null 2>&1

# 02: pcre — Thompson NFA on `foo\(.\)` matches single-arg calls.
"$SPOT" -p 'foo\(.\)' .c > 03.pcre.got.out 2> 03.pcre.got.err
match "$CASE/03.pcre.want.out" 03.pcre.got.out
empty 03.pcre.got.err
