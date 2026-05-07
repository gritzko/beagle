#!/bin/sh
#  04-nonexistent — `be delete <typo>` where the path is absent on
#  disk AND was never in the baseline tree: silent no-op (no row, no
#  error).  The user named a path that doesn't exist anywhere; sniff
#  has nothing to record.

. "$(dirname "$0")/../../lib/case.sh"

echo "tracked" > a.txt
"$BE" put a.txt        > /dev/null
"$BE" post 'baseline'  > /dev/null

ROWS_BEFORE=$(wc -l < .sniff)

"$BE" delete typo.txt > /dev/null

ROWS_AFTER=$(wc -l < .sniff)
[ "$ROWS_BEFORE" = "$ROWS_AFTER" ] || {
    echo "FAIL: .sniff grew by $((ROWS_AFTER - ROWS_BEFORE)) row(s) — expected 0" >&2
    tail .sniff >&2
    exit 1
}
