#!/bin/sh
#  be-delete-branch-resurrect.sh — `be delete ?feat` then
#  `be post ?feat` brings the label back.  Verifies the tombstone
#  doesn't permanently shadow the key (a later post supersedes it).

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + ?feat label; tombstone ?feat"
vc_fresh_wt
sp_seed_trunk
sp_label_feat
"$BE" delete "?feat" >/dev/null

vc_snapshot before
#  Sanity: ?feat must be hidden in the BEFORE refs (tombstone filter).
if vc_section before refs | grep -qE '^\?feat	'; then
    vc_fail "?feat still surfaced in refs after tombstone — wrong setup"
fi
vc_note "?feat hidden in REFS after tombstone"

vc_step "be post ?feat — should resurrect the label"
vc_run resurrect "$BE" post "?feat"

vc_snapshot after

vc_assert_exit 0
vc_assert_appended refs "^\\?feat	"
vc_note "?feat resurrected"

echo "=== be-delete-branch-resurrect: OK ==="
