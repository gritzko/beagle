#!/bin/sh
#  be-get-restore-wiped.sh — `be get` on a wiped wt restores files
#  from the recorded baseline tip.  Wt content is rebuilt; sniff
#  appends one `get` row; refs/baseline are unchanged shape (still
#  trunk at the same tip).

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + commit, then wipe wt files"
vc_fresh_wt
sp_seed_trunk             # exports T1 (= the only tip)
sp_wipe_wt                # rm tracked files; .be/wtlog baseline still T1

vc_snapshot before

vc_step "be get ? — restore trunk from REFS tip"
vc_run restore "$BE" get "?"

vc_snapshot after

vc_assert_exit 0
vc_assert_appended sniff "^get	"
vc_assert_unchanged refs
vc_assert_baseline "" "$T1"     # branch still trunk (empty), tip still T1

#  wt section: x.txt back on disk with the original content+sha.
got=$(vc_section after wt | grep -F './x.txt')
[ -n "$got" ] || vc_fail "x.txt not restored on disk"
vc_note "x.txt restored at $T1"

echo "=== be-get-restore-wiped: OK ==="
