#!/bin/sh
#  delete/09-sub-nosub-suppress — `be delete --nosub` does NOT recurse into a
#  mounted sub even when a sub file is missing on disk.  Confirms the --nosub
#  opt-out for DELETE (BEActSubsRelay early-return).  SUBS audit probe 9.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/helper.c ] || fail "vendor/sub/helper.c missing (sub not mounted)"

rm vendor/sub/helper.c

"$BE" delete --nosub >02.del.out 2>02.del.err
rc=$?
[ "$rc" = 0 ] || fail "be delete --nosub exited $rc (want 0); stderr:
$(cat 02.del.err)"

#  Sub deletion NOT staged — recursion suppressed.
if grep -qE 'delete[[:space:]]+helper\.c' vendor/sub/.be; then
    fail "--nosub did not suppress sub recursion (sub helper.c delete staged):
$(cat vendor/sub/.be)"
fi

note "delete/09 ok: --nosub leaves the sub untouched"
