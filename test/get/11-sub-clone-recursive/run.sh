#!/bin/sh
#  get/11-sub-clone-recursive — `be get $PARENT_URL?master` mounts the
#  parent project plus its declared submodule end-to-end.
#
#  Setup (from submodules.sh):
#      parent.git/  — bare git upstream, 3 commits, .gitmodules pins
#                     vendor/sub to sub's C2.
#      sub.git/     — bare git upstream, 3 commits.
#
#  Test:
#      1. `be get ssh://localhost/<rel>/parent.git?master` into wt/.
#      2. Assert parent files materialised (main.c, util.c, .gitmodules).
#      3. Assert sub mount: vendor/sub/.be is a regular-file anchor
#         and vendor/sub/core.c materialised at SUB_C2's content.
#      4. Assert pre-order recursion markers (`be: get .` outer +
#         `be: get vendor/sub`) on stderr.
#      5. Assert sub's recorded tip matches $PARENT_PINNED (= $SUB_C2).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

# --- parent files materialised ---------------------------------------
[ -f main.c ]        || fail "wt/main.c missing"
[ -f util.c ]        || fail "wt/util.c missing"
[ -f .gitmodules ]   || fail "wt/.gitmodules missing"
grep -q 'vendor/sub' .gitmodules \
    || fail ".gitmodules content unexpected: $(cat .gitmodules)"

# --- sub mounted as secondary wt -------------------------------------
[ -d vendor/sub ]            || fail "vendor/sub dir missing"
[ -f vendor/sub/.be ]        || fail "vendor/sub/.be anchor missing (not a file)"
[ ! -d vendor/sub/.be ]      || fail "vendor/sub/.be should be a regular file"
[ -f vendor/sub/core.c ]     || fail "vendor/sub/core.c missing"

#  Sub is pinned at SUB_C2 per .gitmodules; that commit's core.c lacks
#  sub_add but has sub_inc.
grep -q 'sub_inc' vendor/sub/core.c \
    || fail "sub/core.c missing sub_inc (expected at SUB_C2):
$(cat vendor/sub/core.c)"

# --- recursion markers, pre-order ------------------------------------
grep -q '^be: get [.]'        01.get.got.err \
    || fail "outer get marker missing; stderr:
$(cat 01.get.got.err)"
grep -q '^be: get vendor/sub' 01.get.got.err \
    || fail "sub get marker missing; stderr:
$(cat 01.get.got.err)"

outer_line=$(grep -n '^be: get [.]' 01.get.got.err | head -1 | cut -d: -f1)
sub_line=$(grep -n '^be: get vendor/sub' 01.get.got.err | head -1 | cut -d: -f1)
[ -n "$outer_line" ] && [ -n "$sub_line" ] && [ "$outer_line" -lt "$sub_line" ] \
    || fail "expected outer marker (line=$outer_line) before sub marker (line=$sub_line)"

# --- sub's recorded pin matches what .gitmodules pinned (SUB_C2) -----
sub_tip=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                      END { h=last; sub(/^[^#]*#/, "", h); print h }' \
          vendor/sub/.be)
[ "$sub_tip" = "$PARENT_PINNED" ] \
    || fail "sub tip mismatch: got '$sub_tip' want '$PARENT_PINNED'"

# --- sub-shard carries the sub's OWN keeper data ---------------------
#  Per SUBS.plan.md §"Storage layout": parent_root/.be/<basename>/ is
#  the sub's private keeper shard.  The recursive mount currently
#  short-circuits and lets sub objects piggy-back on parent's trunk,
#  leaving .be/sub/ empty — that's the bug.  This assertion pins the
#  intended invariant: the shard must hold the sub's refs reflog at
#  minimum (a real fetch also drops a keeper pack run in there).
[ -d .be/sub ] || fail ".be/sub shard dir missing"
[ -s .be/sub/refs ] \
    || fail ".be/sub/refs missing or empty — sub keeper data not landing in shard:
$(ls -la .be/sub 2>&1)"

note "get/11-sub-clone-recursive: parent + vendor/sub mounted at SUB_C2"
