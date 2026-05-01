#!/bin/sh
#  be-post-empty.sh — `be post msg` after a successful post, with
#  no wt changes since, refuses with SNIFFNOOP.  All observables
#  intact.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + one post; wt clean (no changes since)"
vc_fresh_wt
sp_seed_trunk

vc_snapshot before

vc_step "be post noop — should refuse (no changes since base)"
vc_run noop "$BE" post 'noop msg'

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr noop "no changes since base"
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-post-empty: OK ==="
