#!/bin/sh
. "$(dirname "$0")/../../lib/case.sh"

cp "$CASE/01.greet.txt" greet.txt
"$BE" put greet.txt > 01.put.got.out 2> 01.put.got.err
empty 01.put.got.out
match "$CASE/01.put.err.txt" 01.put.got.err

"$BE" post initial > 02.post.got.out 2> 02.post.got.err
match    "$CASE/02.post.want.txt" 02.post.got.out
match_re "$CASE/02.post.err.txt" 02.post.got.err
