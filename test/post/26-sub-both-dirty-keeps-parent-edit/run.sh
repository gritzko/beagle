#!/bin/sh
#  post/26-sub-both-dirty-keeps-parent-edit — REPRO for SUBS-001.  When BOTH
#  the parent and the sub are dirty, `be post` must commit the parent's own
#  edit (commit-all) AND bump the gitlink.  Today the recursion's
#  `be put vendor/sub` row flips POST into selective mode scoped to the
#  gitlink, so the parent's main.c edit is SILENTLY dropped (exit 0).
#  Registered WILL_FAIL until SUBS-001 lands.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing (sub not mounted)"

#  Dirty both sides.
printf '\nint mark26_parent(void){return 26;}\n' >> main.c
printf '\n// sub edit post/26\n'                 >> vendor/sub/core.c

"$BE" post 'both dirty post/26' >02.post.out 2>02.post.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 02.post.err)"

#  SUBS-001: the committed main.c must contain the parent's own edit.
"$BE" blob:main.c?master >03.blob.out 2>03.blob.err \
    || fail "be blob:main.c?master failed; stderr:
$(cat 03.blob.err)"
grep -q 'mark26_parent' 03.blob.out \
    || fail "SUBS-001: parent main.c edit was dropped from the commit (silent \
data loss); committed main.c:
$(cat 03.blob.out)"

note "post/26 ok: parent own edit survived the sub gitlink bump"
