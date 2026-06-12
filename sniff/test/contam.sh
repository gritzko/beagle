#!/bin/sh
#  contam.sh — POST-016 regression: a selective `sniff post` must commit
#  EXACTLY the explicitly-staged set, never a file whose mtime still
#  resolves to an OUT-OF-SCOPE patch/put row left over from a prior
#  (aborted or already-committed) operation.
#
#  The wrecked-repo failure: a leftover `patch` row stamped `stray.txt`;
#  a later `be put a.txt` + `be post` then swept `stray.txt` into the
#  commit because POST classified it by its stale patch stamp, ignoring
#  selective scope.  The same contamination folded into the NEXT post
#  too (KEEP files keep their old stamp, so the patch row stays the
#  owning row of the stray file's mtime).
#
#  Scenarios:
#    1. A patch absorbed + committed; then a SECOND selective post that
#       stages only an unrelated file must NOT re-commit the patched
#       file (its stamp is now out of scope).
#    2. A selective `be put fileA` + `be post` with an unrelated dirty
#       (mtime-drifted) tracked file commits ONLY fileA.
#
#  Run: BIN=build-debug/bin sh sniff/test/contam.sh
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TEST_ID=${TEST_ID:-SNIFFcontam}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

head_hex() {
    awk -F'\t' '$2=="post" || $2=="get" || $2=="patch" { last=$3 }
                END {
                    h = last
                    sub(/^[^#]*#/, "", h)
                    if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
                }' .be/wtlog
}

#  YES (exit 0) iff <path> is a leaf of commit <sha>'s tree.
in_commit() {
    sha=$1; path=$2
    keeper ls-files ".#$sha" 2>/dev/null \
        | awk -F'\t' -v p="$path" '$2==p { found=1 } END { exit found?0:1 }'
}

# ====================================================================
# Scenario 1 — a leftover (out-of-scope) patch stamp must not
#              contaminate a later selective post.
#
# This mirrors the wrecked-`?main` failure: a `be patch` weave-merges a
# file (stamping it with the patch row's ts), a later `be get` resets the
# boundaries (dropping that patch row out of scope) while PRESERVING the
# dirty overlay AND its stale patch stamp, and then a selective
# `be put fileA` + `be post` sweeps the still-patch-stamped file into the
# unrelated commit.
# ====================================================================
echo "=== 1. leftover patch stamp does not contaminate later post ==="
WT="$TMP/wt1"
rs_wt_at "$WT"
echo "a base"     > a.txt
echo "stray base" > stray.txt
sniff post -m "base" >/dev/null
BASE=$(head_hex)
note "base=$BASE"

#  Side branch ?feat that edits stray.txt only.
sniff put "?feat" >/dev/null
sniff get "?feat" >/dev/null
sleep 0.1
echo "stray FEAT" > stray.txt
sniff put stray.txt >/dev/null
sniff post -m "feat: edit stray" >/dev/null
note "feat edited stray.txt"

#  Back to trunk; make a concurrent edit so feat is not already reachable.
sniff get "?" >/dev/null
grep -qF "stray base" stray.txt || fail "stray.txt not reset to base on trunk"
sleep 0.1
echo "a trunk" > a.txt
sniff put a.txt >/dev/null
sniff post -m "trunk: edit a" >/dev/null
T1=$(head_hex)
note "trunk tip after edit a = $T1"

#  Weave-merge feat into the wt: this stamps stray.txt with the patch
#  row's ts and leaves a `patch` row.  DO NOT post it.
sleep 0.1
sniff patch "?feat!" >/dev/null 2>&1 || fail "patch ?feat! failed"
grep -qF "stray FEAT" stray.txt || fail "patch did not bring feat's stray"
note "patched (stray.txt now dirty + patch-stamped, NOT committed)"

#  A same-branch `be get` resets the pd/patch boundary (the patch row is
#  now out of scope) but preserves the dirty stray overlay + its stamp.
sleep 0.1
sniff get "?" >/dev/null 2>&1 || fail "same-branch get failed"
note "get ? reset boundaries (patch row out of scope); stray still dirty"

#  Now a selective post that stages ONLY a.txt.  stray.txt's leftover
#  patch stamp is out of scope, so it must NOT be re-committed: the
#  selective commit's stray.txt sha must equal the parent's.
sleep 0.1
echo "a v2" > a.txt
sniff put a.txt >/dev/null
sniff post -m "edit a only" >/dev/null
ONLYA=$(head_hex)
[ "$ONLYA" != "$T1" ] || fail "no new commit for the a-only post"

A_NEW=$(keeper ls-files ".#$ONLYA" 2>/dev/null | awk -F'\t' '$2=="a.txt"{split($1,f," ");print f[3]}')
A_OLD=$(keeper ls-files ".#$T1"    2>/dev/null | awk -F'\t' '$2=="a.txt"{split($1,f," ");print f[3]}')
[ -n "$A_NEW" ] && [ "$A_NEW" != "$A_OLD" ] || fail "a.txt not updated in selective commit"
S_NEW=$(keeper ls-files ".#$ONLYA" 2>/dev/null | awk -F'\t' '$2=="stray.txt"{split($1,f," ");print f[3]}')
S_OLD=$(keeper ls-files ".#$T1"    2>/dev/null | awk -F'\t' '$2=="stray.txt"{split($1,f," ");print f[3]}')
[ "$S_NEW" = "$S_OLD" ] \
    || fail "stray.txt leaked into a-only selective commit (contamination): new=$S_NEW old=$S_OLD"
note "selective commit changed ONLY a.txt; leftover patch stamp ignored"

# ====================================================================
# Scenario 2 — an unrelated mtime-drifted tracked file stays out of a
#              selective `put fileA` + `post`.
# ====================================================================
echo "=== 2. unrelated dirty tracked file excluded from selective post ==="
WT="$TMP/wt2"
rs_wt_at "$WT"
echo "a base" > a.txt
echo "b base" > b.txt
sniff post -m "base" >/dev/null
P0=$(head_hex)

#  Make b.txt genuinely dirty (mtime ∉ stamp-set), stage only a.txt.
sleep 0.1
echo "a v2" > a.txt
echo "b DIRTY UNSTAGED" > b.txt
sniff put a.txt >/dev/null
sniff post -m "edit a only" >/dev/null
SEL=$(head_hex)
[ "$SEL" != "$P0" ] || fail "no new commit"

B_NEW=$(keeper ls-files ".#$SEL" 2>/dev/null | awk -F'\t' '$2=="b.txt"{split($1,f," ");print f[3]}')
B_OLD=$(keeper ls-files ".#$P0"  2>/dev/null | awk -F'\t' '$2=="b.txt"{split($1,f," ");print f[3]}')
[ "$B_NEW" = "$B_OLD" ] \
    || fail "b.txt (dirty, unstaged) leaked into selective commit"
note "selective commit excluded the dirty unstaged b.txt"

echo
echo "=== POST-016 contamination: OK ==="
