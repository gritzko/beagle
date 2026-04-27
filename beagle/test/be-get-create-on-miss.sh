#!/bin/sh
#  be-get-create-on-miss.sh — `be get ?./sub` from wt on `?feat`,
#  where `?feat/sub` doesn't exist, forks the child branch at the
#  current tip and switches the wt onto it.  Refs gain one row;
#  sniff gains one `get` row; wt content unchanged; baseline switches
#  to feat/sub.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + ?feat label, wt switched onto feat"
vc_fresh_wt
sp_seed_trunk
sp_label_feat
sp_switch_feat            # baseline (feat, T1)

vc_snapshot before

vc_step "be get ?./sub  (relative — should create feat/sub on miss)"
vc_run create "$BE" get "?./sub"

vc_snapshot after

vc_assert_exit 0
vc_assert_appended sniff "^get	\\?feat/sub#"
vc_assert_appended refs  "^\\?feat/sub	"
vc_assert_unchanged wt
vc_assert_baseline "feat/sub" "$T1"

echo "=== be-get-create-on-miss: OK ==="
