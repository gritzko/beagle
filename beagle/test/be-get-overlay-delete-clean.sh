#!/bin/sh
#  be-get-overlay-delete-clean.sh — `be get T2` from a wt at T1 where
#  T2 has removed `d.txt` and the wt copy of `d.txt` is clean (no dirty
#  edits since the last GET).  GET succeeds; `d.txt` vanishes.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: T1 (d.txt + k.txt), T2 (k.txt only); wt at T1, d.txt clean"
vc_fresh_wt
echo "k stable" > k.txt
echo "d v1"     > d.txt
"$BE" post 'v1 msg' >/dev/null
T1=$(sp_head_hex)

sleep 0.1
"$BE" delete d.txt >/dev/null
"$BE" post 'v2 msg' >/dev/null
T2=$(sp_head_hex)

#  Restore T1 so d.txt is on disk and stamped (clean).
"$BE" get "$T1" >/dev/null

vc_snapshot before

vc_step "be get $T2 — d.txt clean → vanishes"
vc_run delete_clean "$BE" get "$T2"

vc_snapshot after

vc_assert_exit 0
#  DIS-009: bare-sha `be get <sha>` → DETACHED row `?<sha>` (sha in
#  QUERY, empty fragment), not trunk-state `?#<sha>`.  This case tests
#  the delete-overlay WRITE behavior (d.txt vanishes); shape updated to
#  the detached form per the model.
vc_assert_appended sniff "^get	\\?${T2}"

[ ! -e d.txt ] || vc_fail "d.txt still on disk after GET T2"
[ -f k.txt ]   || vc_fail "k.txt missing after GET T2"

vc_note "d.txt removed, k.txt preserved"
echo "=== be-get-overlay-delete-clean: OK ==="
