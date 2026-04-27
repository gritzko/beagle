#!/bin/sh
#  be-put-path.sh — `be put a.txt` appends one `put a.txt` row to
#  .sniff and changes nothing else.  No keeper writes, no refs row.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk; create an untracked a.txt on disk"
vc_fresh_wt
sp_seed_trunk
echo "alpha" > a.txt

vc_snapshot before

vc_step "be put a.txt"
vc_run put "$BE" put a.txt

vc_snapshot after

vc_assert_exit 0
vc_assert_appended sniff "^put	a.txt$"
vc_assert_unchanged refs
vc_assert_unchanged baseline

echo "=== be-put-path: OK ==="
