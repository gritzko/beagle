#!/bin/sh
#  patch/28-sub-gitlink-bump-reget — REPRO for SUBS-002.  `be patch ?<branch>`
#  on a parent whose tree carries a `160000` gitlink must NOT abort: the
#  parent's WEAVE merge has to SKIP the gitlink (it's the sub's concern),
#  and `be` must still recurse into the sub to re-get it at the patched pin.
#
#  Bug: sniff PATCH resolved the gitlink entry as a ref (`bad ref ''`) and
#  aborted PATCHCFLCT (157); because BE_PLAN_PATCH runs BEActSniffPatch
#  before BEActSubsPatch and BEExecute returns on the first non-NONE error,
#  BEActSubsPatch never ran and the sub stayed at the old pin.
#
#  Fixture: parent gitlink vendor/sub pinned at SUB_C2 (core.c lacks
#  `sub_add`).  Branch `adv` bumps the pin to SUB_C3 (core.c gains
#  `sub_add`).  Patching master with `?adv` must land SUB_C3 in the wt.
#  Registered WILL_FAIL until SUBS-002 lands.

. "$(dirname "$0")/../../lib/submodules.sh"

#  --- Build parent branch `adv`: same tree as master but gitlink bumped
#  to SUB_C3.  Work off a throwaway clone of the bare parent so the
#  fixture seed (under $ETMP, not exported) is untouched.
ADV_WORK="$ETMP/adv-work"
rm -rf "$ADV_WORK"
git clone -q "$PARENT_BARE" "$ADV_WORK"
git -C "$ADV_WORK" config user.email t@t
git -C "$ADV_WORK" config user.name  T
git -C "$ADV_WORK" checkout -q -b adv master
git -C "$ADV_WORK" update-index --cacheinfo 160000,"$SUB_C3",vendor/sub
git -C "$ADV_WORK" commit -qm 'parent: bump vendor/sub to C3'
PARENT_ADV=$(git -C "$ADV_WORK" rev-parse HEAD)
git -C "$ADV_WORK" push -q "$PARENT_BARE" adv:refs/heads/adv
note "patch/28: parent adv=$PARENT_ADV bumps vendor/sub $SUB_C2 -> $SUB_C3"

#  --- Clone the parent at master; sub mounts at C2 (no sub_add).
mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing (sub not mounted)"
grep -q 'sub_add' vendor/sub/core.c \
    && fail "precondition: vendor/sub/core.c already has sub_add (should be C2)"

#  --- Fetch + patch the gitlink-bumping branch.  Must NOT abort on the
#  gitlink; must re-get the sub to C3.
"$BE" patch "$PARENT_URL?adv" >02.patch.out 2>02.patch.err
rc=$?
[ "$rc" = 0 ] || fail "be patch exited $rc (expected 0); stderr:
$(cat 02.patch.err)"

#  SUBS-002 core assertion: the sub advanced to the new pin (sub_add present).
grep -q 'sub_add' vendor/sub/core.c \
    || fail "SUBS-002: sub was NOT re-got to the patched pin; vendor/sub/core.c:
$(cat vendor/sub/core.c)"

#  The gitlink entry must not have leaked into the parent's dirty/fail
#  report (no `bad ref` / `failed vendor/sub`).
grep -qi 'bad ref' 02.patch.err \
    && fail "SUBS-002: gitlink resolved as a ref:
$(cat 02.patch.err)"

note "patch/28 ok: gitlink-bump patch re-got the sub to SUB_C3"
