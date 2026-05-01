#!/bin/sh
#  be-get-overlay-no-baseline.sh — first checkout into a wt that has a
#  foreign file at a path the target tree owns.  No baseline ULOG row
#  → fallback path treats every target file as incoming → the foreign
#  on-disk content (unknown mtime) blocks the GET.  Regression guard
#  for the no-baseline branch of the merge-driven overlap check.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: build a tip in one wt; create a fresh wt sharing the store"
#  Build a commit and capture its sha in a primary wt.
vc_fresh_wt
echo "x v1" > x.txt
"$BE" post 'v1 msg' >/dev/null
T1=$(sp_head_hex)
PRIMARY_DOGS="$(pwd)/.dogs"

#  Fresh wt: empty .sniff, but `.dogs` symlinked to the primary store
#  so keeper can resolve T1.  Drop a foreign x.txt onto disk before GET.
mkdir "$TMP/wt2"
cd "$TMP/wt2"
ln -s "$PRIMARY_DOGS" .dogs
echo "foreign content" > x.txt

vc_snapshot before

vc_step "be get $T1 — foreign x.txt at a target-tree path → refused"
vc_run nobase "$BE" get "$T1"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr nobase "GET refused"
vc_assert_unchanged wt
#  No `get` row should have been appended.  (The bare `repo` row may
#  have been seeded by sniff's open path; that's separate from any
#  attempt at the actual checkout.)
got_row=$(awk -F'\t' '$2=="get"' .sniff 2>/dev/null || true)
[ -z "$got_row" ] || vc_fail "unexpected get row appended: $got_row"

echo "=== be-get-overlay-no-baseline: OK ==="
