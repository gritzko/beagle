#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage two .c files; grep with explicit file list of just one.
cp "$CASE/a.c" a.c
cp "$CASE/b.c" b.c
"$BE" put a.c b.c > /dev/null 2>&1
"$BE" post 'init msg' > /dev/null 2>&1

# 02: explicit file list — only a.c is searched, b.c skipped.
"$SPOT" -g foo a.c > 03.grep.got.out 2> 03.grep.got.err
match "$CASE/03.grep.want.out" 03.grep.got.out
empty 03.grep.got.err
