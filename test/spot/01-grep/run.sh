#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage a single .c file and commit so the worktree is canonical.
sleep 0.02; cp "$CASE/foo.c" foo.c
"$BE" put foo.c   > /dev/null 2> 01.put.got.err
"$BE" post 'init msg' > /dev/null 2> 02.post.got.err

# 02: substring grep — expect both matching lines (one hunk).
"$SPOT" -g foo > 03.grep.got.out 2> 03.grep.got.err
match    "$CASE/03.grep.want.out" 03.grep.got.out
empty    03.grep.got.err
