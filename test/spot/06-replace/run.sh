#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage one .c file with `foo` in two places.
sleep 0.02; cp "$CASE/x.c" x.c
"$BE" put x.c     > /dev/null 2>&1
"$BE" post 'init msg' > /dev/null 2>&1

# 02: structural search-and-replace — rewrites the file in place.
#     stdout is empty; stderr has per-file lines + a summary.
"$SPOT" -s 'foo' -r 'baz' .c > 03.replace.got.out 2> 03.replace.got.err
empty    03.replace.got.out
match_re "$CASE/03.replace.err.txt" 03.replace.got.err

# 03: file content reflects the replacement.
match "$CASE/x.after.c" x.c
