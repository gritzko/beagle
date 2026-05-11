#!/bin/sh
#  06-all-missing — bare `be delete` (no paths) sweeps the baseline
#  tree, emitting a `delete <path>` row for every tracked file that
#  is missing from disk.  Files still on disk are left alone.  POST
#  then drops only the swept paths from the next commit.

. "$(dirname "$0")/../../lib/case.sh"

echo "alpha" > a.txt
echo "beta"  > b.txt
echo "gamma" > c.txt
"$BE" put a.txt b.txt c.txt > /dev/null
"$BE" post 'baseline'        > /dev/null

#  rm two of three tracked files outside `be` — simulates the user
#  cleaning up via shell, then wanting `be` to record the removals.
rm a.txt c.txt

"$BE" delete > /dev/null

#  Both rm'd paths must have a delete row.
grep -q 'delete	a\.txt' .be/wtlog || {
    echo "FAIL: missing a.txt has no delete row" >&2
    tail .be/wtlog >&2
    exit 1
}
grep -q 'delete	c\.txt' .be/wtlog || {
    echo "FAIL: missing c.txt has no delete row" >&2
    tail .be/wtlog >&2
    exit 1
}

#  b.txt was never missing — must NOT get a delete row.
if grep -q 'delete	b\.txt' .be/wtlog; then
    echo "FAIL: b.txt got a delete row but is still on disk" >&2
    tail .be/wtlog >&2
    exit 1
fi

#  b.txt still on disk.
[ -f b.txt ] || { echo "FAIL: b.txt vanished" >&2; exit 1; }
