#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage one .c file.
sleep 0.02; cp "$CASE/u.c" u.c
"$BE" put u.c     > /dev/null 2>&1
"$BE" post 'init msg' > /dev/null 2>&1

# 02: `be grep:#bar.c` — projector scheme + fragment body dispatches
#     to spot's grep backend (VERBS.md §"View projectors").
"$BE" 'grep:#bar.c' > 03.uri.got.out 2> 03.uri.got.err
match "$CASE/03.uri.want.out" 03.uri.got.out
empty 03.uri.got.err
