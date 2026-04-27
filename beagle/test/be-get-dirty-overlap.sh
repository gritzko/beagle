#!/bin/sh
#  be-get-dirty-overlap.sh — same-branch `be get T1` from a wt at T2
#  with x.txt dirty refuses with SNIFFOVRL (target tree contains a
#  file that on disk has an unattributed mtime).  All-or-nothing:
#  observables unchanged after the refusal.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: two trunk tips; wt at T2 with dirty x.txt"
vc_fresh_wt
sp_seed_two_tips          # exports T1, T2 (wt now at T2)
sp_make_dirty x.txt

vc_snapshot before

vc_step "be get $T1 — overlap with dirty file → refused"
vc_run overlap "$BE" get "$T1"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr overlap "GET refused"
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged wt
vc_assert_unchanged baseline

echo "=== be-get-dirty-overlap: OK ==="
