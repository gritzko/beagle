#!/bin/sh
#  06-submodule-skip — `be patch` must not flag files inside a nested
#  git repository (submodule).  Detection: any subdir that hosts its
#  own `.git` (file or directory) is treated as opaque to sniff.
#
#  Repro of the in-the-wild failure: `dogs/abc/` is a real git
#  submodule; `be patch ?patch` refused with "233 dirty file(s)" all
#  listed under `abc/...`.  The fix: SNIFFAtScanDirty's wt walk
#  prunes any directory whose `<dir>/.git` exists on disk.

. "$(dirname "$0")/../../lib/case.sh"

LOGS=$(cd .. && pwd)/logs-$NAME
rm -rf "$LOGS"; mkdir -p "$LOGS"

# ------------------------------------------------------------------
# 1. Trunk baseline + ?feat divergent edit.  Plain sniff workflow,
#    no git plumbing — the `.git`-on-disk heuristic doesn't need a
#    baseline-side 160000 entry.
# ------------------------------------------------------------------
echo "hello world" > greet.txt
"$BE" put greet.txt   > /dev/null
"$BE" post 'baseline' > /dev/null

"$BE" put '?./feat'   > /dev/null
"$BE" get '?feat'     > /dev/null
sleep 0.02
echo "hello, feat" > greet.txt
"$BE" put greet.txt   > /dev/null
"$BE" post 'feat msg' > /dev/null

"$BE" get '?..'       > /dev/null

# ------------------------------------------------------------------
# 2. Place a nested git repo inside the wt — the simplest shape that
#    triggers the submodule heuristic: a `submod/.git` directory plus
#    arbitrary content underneath.  These files are not tracked by
#    sniff and their mtimes are not in any get/post stamp set.
# ------------------------------------------------------------------
mkdir -p submod/.git
echo "ref: refs/heads/main" > submod/.git/HEAD
echo "submodule readme"     > submod/README.md
echo "more"                 > submod/extra.txt

# ------------------------------------------------------------------
# 3. THE TEST: `be patch ?./feat` must succeed and the dirty list
#    must be empty.  `submod/*` files must NEVER appear.
# ------------------------------------------------------------------
"$BE" patch '?./feat' > "$LOGS/patch.out" 2> "$LOGS/patch.err"
PATCH_RC=$?

if grep -q 'submod/' "$LOGS/patch.err"; then
    echo "FAIL: patch flagged submod/* as dirty:" >&2
    grep 'submod/' "$LOGS/patch.err" >&2
    exit 1
fi

[ "$PATCH_RC" = "0" ] || {
    echo "FAIL: patch exited $PATCH_RC; stderr:" >&2
    cat "$LOGS/patch.err" >&2
    exit 1
}

# ------------------------------------------------------------------
# 4. wt has feat's greet.txt; submod/* untracked files survived.
# ------------------------------------------------------------------
match() {
    diff -u "$1" "$2" >&2 && return 0
    return 1
}
[ "$(cat greet.txt)" = "hello, feat" ] || {
    echo "FAIL: greet.txt should hold feat's content; got:" >&2
    cat greet.txt >&2
    exit 1
}
[ -f submod/README.md ] || { echo "submod/README.md missing" >&2; exit 1; }
[ -f submod/extra.txt ] || { echo "submod/extra.txt missing" >&2; exit 1; }

rm -rf "$LOGS"
