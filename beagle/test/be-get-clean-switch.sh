#!/bin/sh
#  be-get-clean-switch.sh — `be get ?feat` from a clean wt on trunk
#  switches the wt to feat (same tip).  Sniff appends one `get` row;
#  refs unchanged; wt content unchanged (same tip); baseline.branch
#  flips from "" (trunk) to "feat".

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + ?feat label at the same tip; wt clean on trunk"
vc_fresh_wt
sp_seed_trunk
sp_label_feat             # ?feat -> T1, wt still on trunk

vc_snapshot before

vc_step "be get ?feat"
vc_run switch "$BE" get "?feat"

vc_snapshot after

vc_assert_exit 0
vc_assert_appended sniff "^get	\\?feat#"
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_baseline "feat" "$T1"

echo "=== be-get-clean-switch: OK ==="
