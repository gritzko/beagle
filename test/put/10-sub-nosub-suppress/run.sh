#!/bin/sh
#  put/10-sub-nosub-suppress — `be put --nosub` stages only the parent side
#  and does NOT recurse into the mounted sub, even when the sub is dirty.
#  Confirms the --nosub opt-out (BEActSubsRelay early-return).  SUBS audit
#  probe 9.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing (sub not mounted)"

printf '\n// parent edit put/10\n' >> main.c
printf '\n// sub edit put/10\n'    >> vendor/sub/core.c

"$BE" put --nosub >02.put.out 2>02.put.err
rc=$?
[ "$rc" = 0 ] || fail "be put --nosub exited $rc (want 0); stderr:
$(cat 02.put.err)"

#  Parent staged.
grep -qE 'put[[:space:]]+main\.c' .be/wtlog \
    || fail "parent main.c not staged under --nosub:
$(cat .be/wtlog)"
#  Sub NOT staged — recursion suppressed.
if grep -qE 'put[[:space:]]+core\.c' vendor/sub/.be; then
    fail "--nosub did not suppress sub recursion (sub core.c staged):
$(cat vendor/sub/.be)"
fi

note "put/10 ok: --nosub stages parent only, sub untouched"
