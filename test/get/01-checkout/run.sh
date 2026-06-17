#!/bin/sh
. "$(dirname "$0")/../../lib/case.sh"

# 01: stage greet.txt in a fresh wt.  POST-018/BE-005: `be put` reports
# through the shared `put:` banner hunk on STDOUT (banner + staged row +
# count summary), NOT a bare stderr line.  stderr is empty.
sleep 0.02; cp "$CASE/01.greet.txt" greet.txt
"$BE" put greet.txt > 01.put.got.out 2> 01.put.got.err
match_re "$CASE/01.put.out.txt" 01.put.got.out
empty 01.put.got.err

# 02: commit it.  POST-018/BE-005: `be post` reports through the shared
# `post:` banner hunk on STDOUT (banner + clickable commit row + the
# per-file change rows), NOT raw stderr lines.  stderr is empty.
"$BE" post 'v1 msg' > 02.post.got.out 2> 02.post.got.err
match_re "$CASE/02.post.out.txt" 02.post.got.out
empty    02.post.got.err

# 03: switch the wt to the branch tip (no-op on a clean wt — exercises
# the GET checkout path).  Per GET-026 the DEFAULT GET reports its
# resulting state through the shared ROWS banner, so stdout carries one
# `get ?#<hashlet>` state-banner line (no file rows on a clean no-op).
"$BE" get '?' > 03.tree.got.out 2> 03.tree.got.err.raw
grep -qE 'get \?#[0-9a-f]{8}' 03.tree.got.out || {
    echo "step 03: stdout missing the 'get ?#<hashlet>' state banner" >&2
    cat 03.tree.got.out >&2
    exit 1
}
