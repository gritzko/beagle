#!/bin/sh
#  be-delete-branch-descendants.sh — `be delete ?feat` is refused
#  when ?feat has any active descendant label (e.g. ?feat/sub).

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + ?feat + ?feat/sub; wt back on trunk"
vc_fresh_wt
sp_seed_trunk
sp_label_feat
sp_switch_feat
"$BE" post "?./sub" >/dev/null      # creates ?feat/sub at feat tip
"$BE" get "?" >/dev/null            # back on trunk

vc_snapshot before

vc_step "be delete ?feat — refused (has descendant ?feat/sub)"
vc_run del "$BE" delete "?feat"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr del "active descendant"
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-delete-branch-descendants: OK ==="
