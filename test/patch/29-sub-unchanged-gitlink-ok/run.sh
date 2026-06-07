#!/bin/sh
#  patch/29-sub-unchanged-gitlink-ok — POSITIVE CONTROL for SUBS-002.
#  A be-internal regular-file patch over a parent that carries a
#  `160000` gitlink must succeed (exit 0) and leave the gitlink — and
#  the mounted sub — untouched.  This is the no-op-gitlink half of the
#  spec ("an unchanged gitlink must be a no-op"): the parent's WEAVE
#  merge only touches main.c; the sub stays at its pin.
#
#  Drives the same submodule fixture as patch/28 but never bumps the
#  gitlink, so it exercises the gitlink-skip path WITHOUT the sub
#  re-get.  Should pass once SUBS-002's gitlink-skip lands (before, the
#  shared gitlink in the absorbed tree tripped `bad ref ''`).

. "$(dirname "$0")/../../lib/submodules.sh"

#  --- Clone parent at master; sub mounts at C2.
mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing (sub not mounted)"
sub_before=$(cat vendor/sub/core.c)

#  --- be-internal child branch `feat`: edit a regular file only.
"$BE" put '?./feat'  >/dev/null 2>&1 || fail "be put ?./feat failed"
"$BE" get '?./feat'  >/dev/null 2>&1 || fail "be get ?./feat failed"
sleep 0.02
printf '\nint mark29(void){return 29;}\n' >> main.c
"$BE" put main.c     >/dev/null 2>&1 || fail "be put main.c failed"
"$BE" post 'feat: mark29' >/dev/null 2>&1 || fail "be post on feat failed"

#  --- Back on master, patch the child's regular-file change.  The
#  gitlink is shared (unchanged) — must absorb cleanly, no sub re-get.
"$BE" get '?master' >/dev/null 2>&1 || fail "be get ?master failed"
"$BE" patch '?./feat' >02.patch.out 2>02.patch.err
rc=$?
[ "$rc" = 0 ] || fail "be patch exited $rc (expected 0); stderr:
$(cat 02.patch.err)"

#  The regular-file edit landed.
grep -q 'mark29' main.c \
    || fail "patch did not absorb the feat main.c edit; main.c:
$(cat main.c)"

#  The gitlink is unchanged → the sub stayed at its pin.
[ "$(cat vendor/sub/core.c)" = "$sub_before" ] \
    || fail "unchanged gitlink should leave the sub untouched; core.c drifted:
$(cat vendor/sub/core.c)"

#  No gitlink resolved as a ref.
grep -qi 'bad ref' 02.patch.err \
    && fail "SUBS-002: gitlink resolved as a ref:
$(cat 02.patch.err)"

note "patch/29 ok: regular-file patch over a shared gitlink, sub untouched"
