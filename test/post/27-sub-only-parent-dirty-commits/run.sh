#!/bin/sh
#  post/27-sub-only-parent-dirty-commits — REPRO for SUBS-001 (manifestation
#  B).  With a clean sub and a dirty parent, `be post` must make a normal
#  commit-all commit recording main.c.  Today the recursion stages a
#  `be put vendor/sub` row for the UNCHANGED pin, flipping POST to selective
#  mode -> POSTNONE/206, the parent commit is dropped, and a sticky put-row
#  is left behind.
#  Registered WILL_FAIL until SUBS-001 lands.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing (sub not mounted)"

#  Only the parent is dirty; the sub stays clean.
printf '\nint mark27_parent(void){return 27;}\n' >> main.c

"$BE" post 'only parent post/27' >02.post.out 2>02.post.err
rc=$?

#  SUBS-001/B: must commit cleanly, not POSTNONE.
if grep -qiE 'POSTNONE' 02.post.err; then
    fail "SUBS-001: be post (only-parent-dirty) returned POSTNONE — clean sub \
poisoned the parent commit; stderr:
$(cat 02.post.err)"
fi
[ "$rc" = 0 ] || fail "SUBS-001: be post exited $rc, want 0; stderr:
$(cat 02.post.err)"
"$BE" blob:main.c?master >03.blob.out 2>03.blob.err \
    || fail "be blob:main.c?master failed; stderr:
$(cat 03.blob.err)"
grep -q 'mark27_parent' 03.blob.out \
    || fail "SUBS-001: parent main.c edit not committed:
$(cat 03.blob.out)"

note "post/27 ok: only-parent-dirty post commits the parent edit"
