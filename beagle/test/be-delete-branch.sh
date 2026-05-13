#!/bin/sh
#  be-delete-branch.sh — `be delete ?feat` from a wt on trunk drops
#  the label.  Refs section loses one row (tombstone hides it via
#  refs_each_store filter); sniff and wt unchanged; baseline still
#  trunk.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + ?feat label; wt stays on trunk"
vc_fresh_wt
sp_seed_trunk
sp_label_feat             # ?feat present; wt on trunk

vc_snapshot before

vc_step "be delete ?feat — tombstone the label"
vc_run del "$BE" delete "?feat"

vc_snapshot after

vc_assert_exit 0
vc_assert_unchanged sniff
vc_assert_removed refs "^\\?feat	"
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-delete-branch: OK ==="
