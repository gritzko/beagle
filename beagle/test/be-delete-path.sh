#!/bin/sh
#  be-delete-path.sh — `be delete x.txt` (path-form) appends one
#  `delete x.txt` row to .be/wtlog.  No keeper writes, no refs change.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk with x.txt committed"
vc_fresh_wt
sp_seed_trunk

vc_snapshot before

vc_step "be delete x.txt — path-form delete (stage)"
vc_run del "$BE" delete x.txt

vc_snapshot after

vc_assert_exit 0
vc_assert_appended sniff "^delete	x.txt$"
vc_assert_unchanged refs
vc_assert_unchanged baseline

echo "=== be-delete-path: OK ==="
