#!/bin/sh
#  put/08-sub-stage-all-both-dirty — bare `be put` recurses into a mounted
#  submodule.  With BOTH the parent and the sub dirty, stage-all must stage
#  the parent blob AND relay into the sub to stage the sub blob, exit 0.
#  See Submodules.mkd §"TLV report aggregation"; SUBS audit probe 2.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing (sub not mounted)"

#  Dirty both sides.
printf '\n// parent edit put/08\n' >> main.c
printf '\n// sub edit put/08\n'    >> vendor/sub/core.c

"$BE" put >02.put.out 2>02.put.err
rc=$?
[ "$rc" = 0 ] || fail "bare be put with both dirty exited $rc (want 0); stderr:
$(cat 02.put.err)"

#  Parent blob staged.
grep -qE 'put[[:space:]]+main\.c' .be/wtlog \
    || fail "parent main.c not staged in .be/wtlog:
$(cat .be/wtlog)"
#  Sub blob staged via the relay (row in the sub's mount wtlog).
grep -qE 'put[[:space:]]+core\.c' vendor/sub/.be \
    || fail "sub core.c not staged in vendor/sub/.be:
$(cat vendor/sub/.be)"
#  Report lists the sub path, prefix-rebased.
grep -qE 'vendor/sub/core\.c' 02.put.out \
    || fail "stdout report missing vendor/sub/core.c:
$(cat 02.put.out)"

note "put/08 ok: both parent + sub staged, exit 0"
