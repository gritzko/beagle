#!/bin/sh
#  01-existing-file — `be delete <path>` on a tracked file present on
#  disk: file is unlinked and a `delete <path>` row is appended.

. "$(dirname "$0")/../../lib/case.sh"

echo "alpha" > a.txt
echo "beta"  > b.txt
"$BE" put a.txt b.txt > /dev/null
"$BE" post 'baseline'  > /dev/null

#  Pre-conditions.
[ -f a.txt ] || { echo "FAIL: a.txt missing pre-delete" >&2; exit 1; }

"$BE" delete a.txt > /dev/null

#  File gone from disk.
[ ! -e a.txt ] || { echo "FAIL: a.txt still on disk after delete" >&2; exit 1; }

#  ULOG row appended.
grep -q 'delete	a\.txt' .sniff || {
    echo "FAIL: no delete row for a.txt in .sniff" >&2
    tail .sniff >&2
    exit 1
}

#  b.txt untouched.
[ -f b.txt ] || { echo "FAIL: b.txt erroneously removed" >&2; exit 1; }
