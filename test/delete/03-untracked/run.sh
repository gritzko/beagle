#!/bin/sh
#  03-untracked — `be delete <path>` on a file that exists on disk but
#  was never put-staged refuses with DELDIRTY.  Sniff treats the file
#  as a user edit (mtime ∉ stamp-set) and refuses to clobber it; the
#  user is expected to `rm` it directly or stage with `be put` first.

. "$(dirname "$0")/../../lib/case.sh"

echo "tracked" > a.txt
"$BE" put a.txt        > /dev/null
"$BE" post 'baseline'  > /dev/null

#  Place an untracked file alongside.  Mtime is fresh — not in the
#  ULOG stamp-set.
echo "untracked" > stray.txt

set +e
"$BE" delete stray.txt > out 2> err
RC=$?
set -e

[ "$RC" != "0" ] || {
    echo "FAIL: be delete stray.txt should refuse, got rc=0" >&2
    cat err >&2
    exit 1
}

[ -f stray.txt ] || {
    echo "FAIL: stray.txt was unlinked despite refusal" >&2
    cat err >&2
    exit 1
}

#  No delete row was appended (the refusal happened before the row
#  was written).
if grep -q 'delete	stray\.txt' .sniff; then
    echo "FAIL: stray.txt got a delete row anyway" >&2
    tail .sniff >&2
    exit 1
fi
