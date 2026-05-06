#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage two files of different exts so the ext filter has work.
sleep 0.02; cp "$CASE/a.c"  a.c
sleep 0.02; cp "$CASE/a.py" a.py
"$BE" put a.c a.py > /dev/null 2>&1
"$BE" post 'init msg'  > /dev/null 2>&1

# 02: ext-filtered grep — only the .py file should appear.
"$SPOT" -g foo .py > 03.grep.got.out 2> 03.grep.got.err
match "$CASE/03.grep.want.out" 03.grep.got.out
empty 03.grep.got.err
