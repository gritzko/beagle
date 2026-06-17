#!/bin/sh
. "$(dirname "$0")/../../lib/case.sh"

# POST-018/BE-005: put/post report through the shared ROWS banner on
# STDOUT (a `put:` / `post:` banner hunk: banner + rows + summary), NOT
# raw stderr lines.  stderr is empty.
sleep 0.02; cp "$CASE/01.greet.txt" greet.txt
"$BE" put greet.txt > 01.put.got.out 2> 01.put.got.err
match_re "$CASE/01.put.out.txt" 01.put.got.out
empty 01.put.got.err

"$BE" post '#initial' > 02.post.got.out 2> 02.post.got.err
match_re "$CASE/02.post.out.txt" 02.post.got.out
empty    02.post.got.err
