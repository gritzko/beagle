#!/bin/sh
#  restore.sh — single-file `sniff get <path>` baseline-restore tests.
#
#  Regression for the PATHBAD bug: the no-`?` restore form
#  (`be get sub/file.txt`) routed through sniff_get_blob_to_wt, which
#  composed the wt path with PATHu8bPush — a single-SEGMENT append that
#  rejects any embedded '/'.  Root-level files worked; anything in a
#  subdir failed with PATHBAD.  Fix: PATHu8bAdd (segment-by-segment),
#  matching the subtree path already does (sniff/GET.c).
#
#  Run: BIN=build-debug/bin sh sniff/test/restore.sh
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TEST_ID=${TEST_ID:-SNIFFrestore}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

# ====================================================================
# Scenario 1 — restore a ROOT-level file (control: always worked).
# ====================================================================
echo "=== 1. restore root-level file from baseline ==="
WT="$TMP/wt1"
rs_wt_at "$WT"
echo "root v1" > root.txt
sniff post -m "base" >/dev/null
sleep 0.1
echo "root edited" > root.txt
note "dirtied root.txt"
sniff get root.txt 2>$TMP/r1.err \
    || { cat $TMP/r1.err; fail "root-file restore failed"; }
[ "$(cat root.txt)" = "root v1" ] \
    || fail "root.txt not restored to baseline (got: $(cat root.txt))"
note "root.txt restored to baseline"

# ====================================================================
# Scenario 2 — restore a SUBDIR file (the PATHBAD repro).
# ====================================================================
echo "=== 2. restore subdir file from baseline ==="
WT="$TMP/wt2"
rs_wt_at "$WT"
mkdir -p sub/deep
echo "sub v1"  > sub/file.txt
echo "deep v1" > sub/deep/x.txt
sniff post -m "base" >/dev/null
sleep 0.1
echo "sub edited"  > sub/file.txt
echo "deep edited" > sub/deep/x.txt
note "dirtied sub/file.txt and sub/deep/x.txt"

sniff get sub/file.txt 2>$TMP/r2.err \
    || { cat $TMP/r2.err; fail "subdir-file restore failed (PATHBAD?)"; }
[ "$(cat sub/file.txt)" = "sub v1" ] \
    || fail "sub/file.txt not restored (got: $(cat sub/file.txt))"
note "sub/file.txt (one level) restored"

sniff get sub/deep/x.txt 2>$TMP/r2b.err \
    || { cat $TMP/r2b.err; fail "nested-subdir restore failed (PATHBAD?)"; }
[ "$(cat sub/deep/x.txt)" = "deep v1" ] \
    || fail "sub/deep/x.txt not restored (got: $(cat sub/deep/x.txt))"
note "sub/deep/x.txt (two levels) restored"

echo "=== all restore scenarios passed ==="
