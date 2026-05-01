#!/bin/sh
#  be-get-overlay-delete-dirty.sh — `be get T2` from a wt at T1 where
#  T2 has removed `d.txt`, and the wt copy of `d.txt` is dirty.
#  Strictness increase: the merge classifies this as a real change
#  (path in baseline only) and refuses, preventing silent loss of dirty
#  edits to a vanishing file.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: T1 (d.txt + k.txt), T2 (k.txt only); wt at T1 with d.txt dirty"
vc_fresh_wt
echo "k stable" > k.txt
echo "d v1"     > d.txt
"$BE" post 'v1 msg' >/dev/null
T1=$(sp_head_hex)

#  Drop d.txt and post: T2 has only k.txt.
sleep 0.1
"$BE" delete d.txt >/dev/null
"$BE" post 'v2 msg' >/dev/null
T2=$(sp_head_hex)

#  Switch back to T1 so the wt has d.txt again, then dirty it.
"$BE" get "$T1" >/dev/null
sleep 0.1
echo "d user edit $(date +%N)" >> d.txt

vc_snapshot before

vc_step "be get $T2 — dirty d.txt would vanish → refused"
vc_run delete_dirty "$BE" get "$T2"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr delete_dirty "GET refused"
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-get-overlay-delete-dirty: OK ==="
