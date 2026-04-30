#!/bin/sh
. "$(dirname "$0")/../../lib/case.sh"

# 01: stage greet.txt in a fresh wt
cp "$CASE/01.greet.txt" greet.txt
"$BE" put greet.txt > 01.put.got.out 2> 01.put.got.err
empty 01.put.got.out
match "$CASE/01.put.err.txt" 01.put.got.err

# 02: commit it.  stdout is empty (byte-exact); stderr is "A <path>"
# + "sniff: commit <sha>" — sha varies with author/committer time, so
# match it as a regex.
"$BE" post v1 > 02.post.got.out 2> 02.post.got.err
empty    02.post.got.out
match_re "$CASE/02.post.err.txt" 02.post.got.err

# 03: switch the wt to the branch tip (no-op on a clean wt — exercises
# the GET checkout path).  GET prints debug "GETDBG ..." lines plus
# "sniff: checkout done" to stderr; only the trailing summary line is
# stable across builds, so we filter.  stdout must be empty.
"$BE" get '?' > 03.tree.got.out 2> 03.tree.got.err.raw
grep -E '^sniff: checkout done$' 03.tree.got.err.raw > 03.tree.got.err || true
empty 03.tree.got.out
[ -s 03.tree.got.err ] || {
    echo "step 03: did not see 'sniff: checkout done' in stderr" >&2
    cat 03.tree.got.err.raw >&2
    exit 1
}
