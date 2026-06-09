#!/bin/sh
#  delete/07-sub-delete-all-missing — bare `be delete` (delete-all of files
#  missing on disk) recurses into a mounted submodule: a sub file removed on
#  disk is staged for deletion in the sub and listed path-prefixed, exit 0.
#  SUBS audit probe 7.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/helper.c ] || fail "vendor/sub/helper.c missing (sub not mounted)"

#  Remove a tracked file inside the sub (parent stays clean).
rm vendor/sub/helper.c

"$BE" delete >02.del.out 2>02.del.err
rc=$?
[ "$rc" = 0 ] || fail "bare be delete (sub file missing) exited $rc (want 0); stderr:
$(cat 02.del.err)"

#  Deletion staged in the sub's mount wtlog.
grep -qE 'delete[[:space:]]+helper\.c' vendor/sub/.be \
    || fail "sub helper.c deletion not staged in vendor/sub/.be:
$(cat vendor/sub/.be)"
#  Report lists the sub path, prefix-rebased.
grep -qE 'vendor/sub/helper\.c' 02.del.out \
    || fail "stdout report missing vendor/sub/helper.c:
$(cat 02.del.out)"

note "delete/07 ok: bare delete recurses, sub helper.c staged for removal"
