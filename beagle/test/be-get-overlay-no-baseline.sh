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
PRIMARY="$(pwd)"

#  Fresh secondary wt: `.be` is a regular FILE = its own wtlog, with
#  only a row-0 `repo` anchor naming the primary store.  Keeper opens
#  via the anchor; the wtlog has no baseline get/post row yet.  Drop
#  a foreign x.txt onto disk before GET to trip the overlap check.
mkdir "$TMP/wt2"
cd "$TMP/wt2"
ts=$("$BE" head 2>/dev/null | awk '{print $1; exit}')   # any ron60-ms is fine
[ -n "$ts" ] || ts="0000000000"
printf '%s\trepo\tfile://%s/.be/\n' "$ts" "$PRIMARY" > .be
echo "foreign content" > x.txt

vc_snapshot before

vc_step "be get $T1 — foreign x.txt at a target-tree path → refused"
vc_run nobase "$BE" get "$T1"

vc_snapshot after

vc_assert_exit nonzero
vc_assert_stderr nobase "GET refused"
vc_assert_unchanged wt
#  No `get` row should have been appended.  (The seed `repo` row at
#  row 0 stays; that's separate from any attempt at the actual checkout.)
got_row=$(awk -F'\t' '$2=="get"' .be 2>/dev/null || true)
[ -z "$got_row" ] || vc_fail "unexpected get row appended: $got_row"

echo "=== be-get-overlay-no-baseline: OK ==="
