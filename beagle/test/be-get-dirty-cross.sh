#!/bin/sh
#  be-get-dirty-cross.sh — verbcheck-driven test:
#
#    Cross-branch `be get ?feat` from a wt with a dirty tracked file
#    is refused with SNIFFDRTY, and leaves *all* observables
#    unchanged (`.sniff`, `refs`, `wt`, `baseline`).

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + ?feat label, then dirty-edit on trunk"
vc_fresh_wt
sp_seed_trunk             # exports T1
sp_label_feat             # exports FEAT_TIP (= T1); wt still on trunk
sp_make_dirty x.txt       # tracked file → mtime ∉ stamp-set

vc_step "snapshot before"
vc_snapshot before

vc_step "be get ?feat (cross-branch with dirty wt → refused)"
vc_run get_attempt "$BE" get "?feat"

vc_step "snapshot after + assert"
vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr get_attempt "cross-branch GET refused"
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

vc_note "all observables intact after refused cross-branch GET"
echo "=== be-get-dirty-cross: OK ==="
