#!/bin/sh
#  be-get-overlay-clean.sh — `be get T1` from a wt at T2 with `b.txt`
#  dirty.  T1 and T2 differ only in `a.txt`; `b.txt` is byte-identical
#  in both trees.  The merge-driven overlap check classifies `b.txt` as
#  a no-op overlay; the WRITE pass leaves the dirty bytes alone and
#  `a.txt` reverts to its T1 content.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: T1 (a v1, b stable), T2 (a v2, b stable); wt at T2; b.txt dirty"
vc_fresh_wt
echo "a v1" > a.txt
echo "b stable" > b.txt
"$BE" post 'v1 msg' >/dev/null
T1=$(sp_head_hex)

sleep 0.1
echo "a v2" > a.txt
"$BE" post 'v2 msg' >/dev/null
T2=$(sp_head_hex)

#  Make b.txt dirty: append unique bytes so its sha differs from baseline.
sleep 0.1
echo "b user edit $(date +%N)" >> b.txt
DIRTY_B_SHA=$(sha1sum b.txt | awk '{print $1}')

vc_snapshot before

vc_step "be get $T1 — b.txt dirty but unchanged across T1↔T2 → succeeds"
vc_run overlay "$BE" get "$T1"

vc_snapshot after

vc_assert_exit 0
vc_assert_appended sniff "^get	\\?#${T1}"

#  a.txt reverted to T1 content "a v1".
got_a=$(awk 'NR==1' a.txt)
[ "$got_a" = "a v1" ] || vc_fail "a.txt not reverted to T1 (got '$got_a')"

#  b.txt still carries the user's dirty edit (sha unchanged from before).
got_b_sha=$(sha1sum b.txt | awk '{print $1}')
[ "$got_b_sha" = "$DIRTY_B_SHA" ] \
    || vc_fail "b.txt clobbered (sha $got_b_sha, want $DIRTY_B_SHA)"

vc_note "a.txt reverted, b.txt dirty edits preserved"
echo "=== be-get-overlay-clean: OK ==="
