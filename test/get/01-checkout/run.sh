#!/bin/sh
. "$(dirname "$0")/../../lib/case.sh"

# 01: stage greet.txt in a fresh wt
sleep 0.02; cp "$CASE/01.greet.txt" greet.txt
"$BE" put greet.txt > 01.put.got.out 2> 01.put.got.err
empty 01.put.got.out
match "$CASE/01.put.err.txt" 01.put.got.err

# 02: commit it.  stdout is empty (byte-exact); stderr is "A <path>"
# + "sniff: commit <sha>" — sha varies with author/committer time, so
# match it as a regex.
"$BE" post 'v1 msg' > 02.post.got.out 2> 02.post.got.err
empty    02.post.got.out
match_re "$CASE/02.post.err.txt" 02.post.got.err

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
