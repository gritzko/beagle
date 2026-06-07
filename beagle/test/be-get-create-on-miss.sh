#!/bin/sh
#  be-get-create-on-miss.sh — `be get ?./sub` from wt on `?feat`,
#  where `?feat/sub` doesn't exist, must error.  GET never creates a
#  branch (per https://replicated.wiki/html/wiki/Verbs.html); the spec-aligned create path is `be post
#  ?./sub` followed by `be get ?./sub`.  All observables intact on the
#  miss.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + ?feat label, wt switched onto feat"
vc_fresh_wt
sp_seed_trunk
sp_label_feat
sp_switch_feat            # baseline (feat, T1)

vc_snapshot before

vc_step "be get ?./sub  (relative — must error on miss, not create)"
vc_run miss "$BE" get "?./sub"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-get-create-on-miss: OK ==="
