#!/bin/sh
#  34-restore-deleted-tracked — DIS-017.  A bareword `be get <file>`
#  classifies path-vs-branch by TRACKED-in-baseline-tree status, not
#  by on-disk existence.  Three assertions:
#
#    A. A tracked file deleted from disk: `be get greet.txt` must
#       RESURRECT it from cur's baseline (it is a tracked tree entry
#       even though stat misses it).  Pre-fix this promoted to
#       `?greet.txt` and failed as a missing branch (SNIFFFAIL).
#    B. A tracked file present on disk (locally edited): `be get
#       greet.txt` still restores the baseline bytes normally.
#    C. A bareword that is NOT a tracked entry routes to `?branch`:
#       a real branch name switches; a bogus one refuses as a missing
#       ref (never silently treated as a file restore).

. "$(dirname "$0")/../../lib/case.sh"

# T0: baseline with two tracked files.
sleep 0.02
printf 'hello\n' > greet.txt
printf 'doc\n'   > README
"$BE" put greet.txt > /dev/null
"$BE" put README    > /dev/null
"$BE" post '#v1'    > /dev/null

# --- B: tracked + present (edited) → restore baseline bytes ----------
printf 'EDITED\n' > greet.txt
"$BE" get greet.txt > b.got.out 2> b.got.err
match_re "$CASE/b.err.txt" b.got.err
printf 'hello\n' > b.want.txt
match b.want.txt greet.txt

# --- A: tracked + DELETED → resurrect from baseline (the bug) --------
rm greet.txt
"$BE" get greet.txt > a.got.out 2> a.got.err
match_re "$CASE/a.err.txt" a.got.err
[ -f greet.txt ] || { echo "A: greet.txt was not resurrected" >&2; exit 1; }
printf 'hello\n' > a.want.txt
match a.want.txt greet.txt

# --- C1: real branch name (not a tracked entry) → branch switch ------
"$BE" put '?./feat' > /dev/null
"$BE" get feat > c1.got.out 2> c1.got.err
grep -E '^sniff: checkout done$' c1.got.err > /dev/null || {
    echo "C1: bareword 'feat' did not route to a branch switch" >&2
    cat c1.got.err >&2
    exit 1
}

# --- C2: bogus bareword (no tracked entry, no branch) → refuse -------
#   Must NOT be treated as a file restore; routes to ?nosuchbranch and
#   fails as a missing ref.
"$BE" get nosuchbranch > c2.got.out 2> c2.got.err && {
    echo "C2: bareword 'nosuchbranch' unexpectedly succeeded" >&2
    exit 1
}
[ -e nosuchbranch ] && {
    echo "C2: bareword 'nosuchbranch' created a file (treated as path)" >&2
    exit 1
}
exit 0
