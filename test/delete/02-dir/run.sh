#!/bin/sh
#  02-dir — `be delete <prefix>/` (trailing slash) recursively unlinks
#  every tracked file under the prefix and appends a single
#  `delete <prefix>/` row.  Files outside the prefix are untouched.

. "$(dirname "$0")/../../lib/case.sh"

mkdir -p src tools
echo "main"   > src/a.c
echo "lib"    > src/b.c
echo "build"  > tools/run.sh
echo "readme" > README.md
"$BE" put src/a.c src/b.c tools/run.sh README.md > /dev/null
"$BE" post 'baseline'                            > /dev/null

#  Drop everything under src/.
"$BE" delete 'src/' > /dev/null

[ ! -e src/a.c ] || { echo "FAIL: src/a.c still present"    >&2; exit 1; }
[ ! -e src/b.c ] || { echo "FAIL: src/b.c still present"    >&2; exit 1; }
[ -f tools/run.sh ] || { echo "FAIL: tools/run.sh removed"  >&2; exit 1; }
[ -f README.md ]    || { echo "FAIL: README.md removed"     >&2; exit 1; }

grep -q 'delete	src/$' .sniff || {
    echo "FAIL: no 'delete src/' row in .sniff" >&2
    tail .sniff >&2
    exit 1
}
