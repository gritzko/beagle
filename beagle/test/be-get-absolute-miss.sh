#!/bin/sh
#  be-get-absolute-miss.sh — `be get ?ghost` on a non-existent
#  absolute branch errors out.  All observables intact (GET never
#  creates — POST is the only branch-creator, per VERBS.md).

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk only; ?ghost has no REFS row"
vc_fresh_wt
sp_seed_trunk

vc_snapshot before

vc_step "be get ?ghost — must error (no auto-create on absolute)"
vc_run miss "$BE" get "?ghost"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-get-absolute-miss: OK ==="
