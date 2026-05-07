#!/bin/sh
#  05-missing-tracked — `be delete <path>` on a tracked file that has
#  already been removed from disk (e.g. `rm a.txt` outside `be`):
#  emits the delete row anyway, no error.  The next POST drops the
#  path from the new commit's tree.

. "$(dirname "$0")/../../lib/case.sh"

echo "alpha" > a.txt
echo "beta"  > b.txt
"$BE" put a.txt b.txt > /dev/null
"$BE" post 'baseline'  > /dev/null

#  Manual rm of a tracked file.
rm a.txt

"$BE" delete a.txt > /dev/null

[ ! -e a.txt ] || { echo "FAIL: a.txt re-appeared after delete" >&2; exit 1; }

grep -q 'delete	a\.txt' .sniff || {
    echo "FAIL: no delete row for missing-but-tracked a.txt" >&2
    tail .sniff >&2
    exit 1
}
