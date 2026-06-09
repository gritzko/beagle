#!/bin/sh
#  be-patch-noop.sh — POST-011.  A `be patch` that absorbs ZERO changes
#  (the source commit is already reachable from cur) must record NO
#  `patch` row in `.be/wtlog`.  Pre-fix the noop row was appended
#  unconditionally, so two noop patches accumulated two dead rows and
#  the next `be post` died `POSTNOMSG` (cannot auto-resolve a message
#  from N empty patch rows) — blocking even an unrelated post, with no
#  `be` way to clear them short of hand-editing the wtlog.  See
#  https://replicated.wiki/html/wiki/PATCH.html §PATCH and
#  https://replicated.wiki/html/wiki/POST.html §"Message resolution".

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: absorb ?feat's commit by named sha + post (now reachable)"
vc_fresh_wt
sp_seed_trunk             # T1 on trunk
sp_label_feat             # ?feat at T1
sp_switch_feat            # wt on feat
sleep 0.1
echo "y" > y.txt
"$BE" put y.txt >/dev/null
"$BE" post 'add y on feat' >/dev/null
FEAT_HEAD=$(awk -F'\t' '$2=="post"{h=$3;sub(/^[^#]*#/,"",h);last=h} END{print last}' .be/wtlog)
[ -n "$FEAT_HEAD" ] || vc_fail "no FEAT_HEAD"

"$BE" get "?" >/dev/null  # back on trunk
sleep 0.1
#  Cherry-pick feat's commit by name, then post — feat's work is now
#  reachable from cur (via the picked commit), so any further patch of
#  the same sha is a true noop.
"$BE" patch "#$FEAT_HEAD" >/dev/null 2>&1
"$BE" post '#cherry feat!' >/dev/null 2>&1

vc_snapshot before

vc_step "be patch #<reachable-sha> — a true noop, must stage NO patch row"
vc_run patch1 "$BE" patch "#$FEAT_HEAD"

vc_snapshot after_patch1

vc_assert_exit 0
#  The crux: a noop patch appends no `patch` row — [sniff] section is
#  byte-identical before/after.
vc_assert_unchanged sniff before after_patch1
vc_note "first noop patch staged no row"

vc_step "be patch #<reachable-sha> AGAIN — still no row"
vc_run patch2 "$BE" patch "#$FEAT_HEAD"

vc_snapshot after_patch2

vc_assert_exit 0
vc_assert_unchanged sniff after_patch1 after_patch2
vc_note "second noop patch staged no row"

vc_step "be post — must NOT die POSTNOMSG (no dead patch rows to confuse it)"
vc_run post1 "$BE" post

vc_snapshot after_post

#  Pre-fix this exits non-zero with POSTNOMSG (2 patch rows, no msg).
#  Post-fix the noop patches left nothing to commit → a clean exit 0.
vc_assert_exit 0
#  Be explicit: the failure mode we are guarding against must NOT appear.
if grep -qF 'POSTNOMSG' "$TMP/stderr.post1"; then
    vc_fail "post died POSTNOMSG — noop patch rows poisoned the post"
fi
vc_note "post clean after two noop patches (no POSTNOMSG)"

echo "=== be-patch-noop: OK ==="
