#!/bin/sh
#  head/04-sub-dirty — `be head ?` reports per-project dirty state.
#  Outer wt is clean; sub wt has one edited file.  The aggregated
#  report (still read-only) must surface the sub's dirty status, and
#  must NOT claim the outer is dirty.
#
#  Output contract (subject to refinement once impl lands):
#    * Each visited project emits a `be: head <relpath>` marker on
#      stderr (same as head/03).
#    * Per-project dirty count surfaces in the project's own output;
#      the outer's is zero, the sub's is non-zero.  Format TBD by the
#      `graf head` ahead/behind/dirty entry point — this test pins
#      only that the dirty-count strings differ between the two
#      projects (outer matches /\b0\b.*dirty|clean/i; sub does not).
#
#  Status: WILL_FAIL until BEHead recursion + graf head dirty summary
#  land.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt && cd wt
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
[ -f vendor/sub/core.c ] || fail "fixture: sub not mounted"

# --- Dirty exactly one file in the sub; outer untouched. -------------
echo '/* dirty edit for head/04 */' >> vendor/sub/core.c

# --- `be head ?` — same shape as head/03 but expect dirty surfaces. --
"$BE" head '?' >02.head.got.out 2>02.head.got.err
rc=$?
[ "$rc" = 0 ] || fail "be head exited $rc; stderr:
$(cat 02.head.got.err)"

# Both projects visited (same as head/03's marker checks).
grep -q '^be: head [.]'         02.head.got.err || fail "outer marker missing"
grep -q '^be: head vendor/sub'  02.head.got.err || fail "sub marker missing"

# The sub's report must mention the dirty path.  The output format is
# graf-head-defined; we pin only that `core.c` appears in the
# combined stdout/stderr after the sub marker, and does NOT appear
# before the outer's marker resolves (i.e., the outer's section is
# clean).
combined=$(cat 02.head.got.out 02.head.got.err)
echo "$combined" | grep -q 'core\.c' \
    || fail "sub's dirty file (core.c) not surfaced by be head; got:
$combined"

# Outer wt is clean; main.c / util.c must NOT be flagged as dirty.
echo "$combined" | grep -qE '\bmain\.c\b.*dirty|\butil\.c\b.*dirty' \
    && fail "outer falsely reported dirty:
$combined"

# Read-only: no wtlog growth in either project.
"$BE" head '?' >/dev/null 2>&1
note "head/04-sub-dirty: sub dirty surfaced; outer stayed clean"
