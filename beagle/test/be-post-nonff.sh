#!/bin/sh
#  be-post-nonff.sh — `be post msg` against a REFS tip on an unrelated
#  lineage (no common ancestor) cannot be rebased and surfaces as a
#  rebase-aborted failure.  Stage 2 phase-2 promote: same-branch
#  divergence triggers GRAFRebase; unrelated-lineage tips fail the
#  parent-chain walk and the rebase aborts.  Pre-Stage-2 this returned
#  SNIFFNOFF; post-Stage-2 the same poison input still fails (no merge
#  base ⇒ GRAFFAIL surfaces from rebase).
#  Simulated by appending a fake `?#deadbeef…` row to .dogs/refs.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: two trunk tips; poison REFS with unrelated tip"
vc_fresh_wt
sp_seed_two_tips
sp_poison_refs "?"        # trunk now has a fake unrelated tip

vc_snapshot before

vc_step "be post v3 — non-ff against unrelated REFS tip → refused"
sleep 0.1
echo "x v3" > x.txt
vc_run nonff "$BE" post v3

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr nonff "rebase aborted"
vc_assert_unchanged sniff
vc_assert_unchanged refs
vc_assert_unchanged baseline

echo "=== be-post-nonff: OK ==="
