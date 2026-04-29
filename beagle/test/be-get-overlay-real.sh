#!/bin/sh
#  be-get-overlay-real.sh — `be get T1` from a wt at T2 with `a.txt`
#  dirty.  T1 and T2 differ in `a.txt`; the dirty path IS in the diff
#  set, so the overlap check still refuses.  Regression guard: the
#  merge-driven check must not loosen this case.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: T1 (a v1, b stable), T2 (a v2, b stable); wt at T2 with a.txt dirty"
vc_fresh_wt
echo "a v1" > a.txt
echo "b stable" > b.txt
"$BE" post v1 >/dev/null
T1=$(sp_head_hex)

sleep 0.1
echo "a v2" > a.txt
"$BE" post v2 >/dev/null
T2=$(sp_head_hex)

sleep 0.1
echo "a user edit $(date +%N)" >> a.txt

vc_snapshot before

vc_step "be get $T1 — a.txt dirty AND in the diff set → refused"
vc_run overlap "$BE" get "$T1"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr overlap "GET refused"
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-get-overlay-real: OK ==="
