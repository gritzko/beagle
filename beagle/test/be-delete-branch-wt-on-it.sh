#!/bin/sh
#  be-delete-branch-wt-on-it.sh — `be delete ?feat` is refused when
#  the wt is currently on feat (would orphan the wt's branch
#  pointer).  All observables intact.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + ?feat; wt switched onto feat"
vc_fresh_wt
sp_seed_trunk
sp_label_feat
sp_switch_feat            # baseline (feat, T1)

vc_snapshot before

vc_step "be delete ?feat from wt-on-feat — refused"
vc_run del "$BE" delete "?feat"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr del "wt is on"
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-delete-branch-wt-on-it: OK ==="
