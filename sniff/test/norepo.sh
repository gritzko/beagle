#!/bin/sh
#  norepo.sh — bare `sniff` (status) outside any beagle repo must be a
#  harmless refusal, NOT a silent `.be/` bootstrap.
#
#  Regression: a read-only status invocation (`be` / `sniff` with no
#  verb) was classified rw, so HOMEOpen(rw=YES) laid down
#  `<cwd>/.be/{refs,wtlog}` in whatever directory it ran in.  Run once
#  in $HOME and every later bare `be` under $HOME walks up to that
#  stray anchor, treats all of $HOME as the worktree, and enumerates
#  it — which reads as a hang.  Bare status must:
#    1. NOT create a `.be/` in a non-repo dir, and
#    2. refuse cleanly when no `.be/` exists in any ancestor.
#
#  Run: BIN=build-debug/bin sh sniff/test/norepo.sh
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

# This test asserts "no .be in any ANCESTOR dir".  The suite pins TMP
# under $HOME, where a real `~/.be` store may live — the cwd walk-up
# would then find it and (mis)treat $HOME as a worktree, hanging bare
# sniff.  Anchor our scratch under /tmp instead (no ssh here, so no
# $HOME dependency); its ancestor chain up to / has no `.be`.
TMP=${TMPDIR:-/tmp}/be-tests-norepo
TEST_ID=${TEST_ID:-SNIFFnorepo}
TMP=$TMP/$TEST_ID/$$
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM
mkdir -p "$TMP"

SNIFF="$BIN/sniff"
fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

# ====================================================================
# A fresh dir that has NO .be in itself or any ancestor of $TMP.
# ====================================================================
mkdir -p "$TMP/loose"
cd "$TMP/loose"
echo "hello" > somefile.txt

echo "=== 1. bare sniff in a non-repo dir refuses, creates no .be ==="
#  Must terminate (no hang) and must not bootstrap a store.
out=$(timeout 30 "$SNIFF" 2>&1) && rc=0 || rc=$?
{ [ "$rc" -eq 124 ] || [ "$rc" -eq 143 ]; } && fail "bare sniff hung (timeout) in a non-repo dir"
[ "$rc" -ne 0 ]   || fail "bare sniff should refuse outside a repo; rc=$rc, out=$out"
[ ! -e .be ]      || fail "bare sniff must NOT create a .be/ in a non-repo dir"
note "refused (rc=$rc), no .be/ bootstrapped"

# ====================================================================
# Same in a deeper subdir: still no ancestor .be, still refuses.
# ====================================================================
echo "=== 2. bare sniff in a nested non-repo subdir also refuses ==="
mkdir -p deep/er
cd deep/er
out=$(timeout 30 "$SNIFF" 2>&1) && rc=0 || rc=$?
{ [ "$rc" -eq 124 ] || [ "$rc" -eq 143 ]; } && fail "bare sniff hung (timeout) in nested non-repo dir"
[ "$rc" -ne 0 ]   || fail "bare sniff should refuse in nested non-repo dir; out=$out"
[ ! -e "$TMP/loose/.be" ] || fail "no .be/ may appear anywhere up the tree"
note "nested refusal, tree untouched"

# ====================================================================
# A `.be/` dir with an EMPTY wtlog: the repo exists but no worktree is
# anchored here (row 0's `repo` URI never got written).  Bare status
# must refuse (NOHOME), not treat the dir as a worktree and enumerate
# its tree — the exact shape of the stray-$HOME/.be regression.
# ====================================================================
echo "=== 2b. bare sniff on a .be/ with empty wtlog refuses ==="
cd "$TMP/loose"
mkdir -p emptywt && cd emptywt
mkdir -p .be
: > .be/wtlog
: > .be/refs
echo "payload" > big.txt
out=$(timeout 30 "$SNIFF" 2>&1) && rc=0 || rc=$?
{ [ "$rc" -eq 124 ] || [ "$rc" -eq 143 ]; } && fail "bare sniff hung (timeout) on empty-wtlog .be/"
[ "$rc" -ne 0 ] || fail "bare sniff should refuse on an empty-wtlog .be/; out=$out"
note "empty-wtlog .be/ refused (rc=$rc), no tree walk"

# ====================================================================
# Sanity: an actual repo still prints status fine (no over-refusal).
# ====================================================================
echo "=== 3. bare sniff inside a real repo still works ==="
cd "$TMP/loose"
mkdir -p r
cd r
echo "alpha" > a.txt
"$SNIFF" post -m "init" >/dev/null || fail "post bootstrap should succeed"
[ -e .be ] || fail "post should have created the store"
"$SNIFF" >/dev/null 2>&1 || fail "bare sniff inside a real repo should succeed"
note "real-repo status OK"

echo "=== sniff norepo: OK ==="
