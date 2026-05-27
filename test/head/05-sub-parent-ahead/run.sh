#!/bin/sh
#  head/05-sub-parent-ahead — commit on parent only; `be head ?master`
#  reports parent ahead=1 vs origin/master, sub clean.  Exercises the
#  per-project ahead/behind summary across the forest.
#
#  Setup:
#    1. Clone fixture (parent has 3 commits, sub mounted).
#    2. Add a file on the parent's cur, `be post 'local-only'`.
#       Parent is now 1 ahead of its origin/master; sub untouched.
#    3. `be head ?master` — outer reports ahead=1, sub reports
#       ahead=0 vs its own origin/master.
#
#  Status: WILL_FAIL until BEHead recursion + per-project ahead/behind
#  reporting land.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
[ -f vendor/sub/core.c ] || fail "fixture: sub not mounted"

# --- Advance parent's cur by one local commit. -----------------------
echo 'int extra = 42;' > extra.c
"$BE" put extra.c           >/dev/null 2>02.put.got.err
"$BE" post 'local-only'     >03.post.got.out 2>03.post.got.err \
    || fail "parent post failed: $(cat 03.post.got.err)"

# --- `be head ?master` — recurse, report ahead/behind per project. ---
"$BE" head '?master' >04.head.got.out 2>04.head.got.err
rc=$?
[ "$rc" = 0 ] || fail "be head exited $rc; stderr:
$(cat 04.head.got.err)"

# Pre-order markers (same canary as head/03).
grep -q '^be: head [.]'        04.head.got.err || fail "outer marker missing"
grep -q '^be: head vendor/sub' 04.head.got.err || fail "sub marker missing"

# Outer must surface the local-only commit message in its ahead list.
combined=$(cat 04.head.got.out 04.head.got.err)
echo "$combined" | grep -q 'local-only' \
    || fail "outer ahead list missing 'local-only' commit; got:
$combined"

# Sub is at its origin tip; its section must NOT mention the
# parent's local-only commit.  Cheap check: ensure the marker for
# the sub is followed by output that doesn't repeat 'local-only'.
sub_section=$(awk '
    /^be: head vendor\/sub/ { in_sub=1; next }
    /^be: head / { in_sub=0 }
    in_sub
' 04.head.got.err)
echo "$sub_section" | grep -q 'local-only' \
    && fail "sub's section leaked outer's local-only commit:
$sub_section"

note "head/05-sub-parent-ahead: parent ahead=1, sub clean"
