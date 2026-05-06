#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage two .c files — only one defines a single-arg int function.
sleep 0.02; cp "$CASE/single.c" single.c
sleep 0.02; cp "$CASE/multi.c"  multi.c
"$BE" put single.c multi.c > /dev/null 2>&1
"$BE" post 'init msg'           > /dev/null 2>&1

# 02: structural pattern — `int a(int b)` matches single-arg int defs.
#     The lowercase placeholders match exactly one token each.
"$SPOT" -s 'int a(int b)' .c > 03.snippet.got.out 2> 03.snippet.got.err
match "$CASE/03.snippet.want.out" 03.snippet.got.out
empty 03.snippet.got.err
