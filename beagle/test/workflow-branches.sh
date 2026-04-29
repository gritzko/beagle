#!/bin/sh
#  workflow-branches.sh — end-to-end branch lifecycle through the
#  `be` dispatcher.  Companion to workflow.sh; that one covers the
#  put / post / delete file-level dispatch on a single (trunk) branch.
#  This one walks the branch verbs (create, switch, commit-on-child,
#  switch-back, delete) so we catch wiring regressions across
#  beagle / sniff / keeper / graf for the multi-branch path.
#
#  --- coverage ---
#  Increment 1 only — branch creation + basic edit + commit on child
#  + deletion.  Specifically:
#    1.  fresh wt + first post on trunk (baseline T1)
#    2.  `be post ?./fix1`              — create child branch
#    3.  `be get  ?fix1`                — switch wt to child
#    4.  edit a tracked file on child
#    5.  `be put <file>` then `be post <msg>` — commit on child
#    6.  verify ?fix1 tip advanced; trunk tip unchanged
#    7.  `be get  ?..`                  — switch wt back to parent
#    8.  `be delete ?fix1`              — drop the branch
#    9.  verify ?fix1 row gone from REFS; trunk tip unchanged
#
#  --- deferred (NOT in this test) ---
#    Increment 2: PATCH squash semantics — `be patch ?other` should
#                 leave baseline single-tip after a single-parent
#                 commit on next POST (BRANCHES.state.md §PATCH).
#    Increment 3: POST rebase / cascade — non-ff POST that rebases
#                 cur's stack onto target's tip with patch-id dedup
#                 (BRANCHES.state.md §"Cascade rebase (gap)").
#
#  --- TODO(spec): pre-spec verb gaps surfaced while wiring this up ---
#    * Bare `be post` on a child branch with staged put rows prints a
#      dry-run summary ("M a.txt / N change(s)") and does NOT write a
#      commit row — the new commit only lands when a message is given
#      (`be post <msg>`).  Spec wants bare POST to commit on cur.
#      Workaround: pass a message to force the commit.  Once bare POST
#      commits, switch step 5 to a bare `be post`.
#    * `be post ?./fix1` writes the REFS row but does NOT mkdir
#      `<store>/fix1/` (BRANCHES.state.md §"Branch creation"
#      KEEPCreateBranch GAP).  We assert REFS only; the dir-presence
#      assertion is left as a TODO below for when keeper grows the
#      per-branch shard.
#
#      BIN=build-debug/bin sh beagle/test/workflow-branches.sh

set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
BE="$BIN/be"
KEEPER="$BIN/keeper"
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TMP=${TMP:-$HOME/tmp/run-$(date +%Y%m%d-%H%M%S)}
TEST_ID=${TEST_ID:-BEworkflowBranches}
TMP=$TMP/$TEST_ID
mkdir -p "$TMP"
trap 'rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true' EXIT INT TERM

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }
skip() { echo "  - SKIP: $*"; }

#  Latest sniff baseline row's URI sha (post|get|patch).
head_hex() {
    awk -F'\t' '$2=="post"||$2=="get"||$2=="patch" { last=$3 }
                END {
                    h = last; sub(/^[^#]*#/, "", h)
                    if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
                }' .sniff
}

#  Branch portion (between leading `?` and `#`) of the latest row.
cur_branch() {
    awk -F'\t' '$2=="post"||$2=="get"||$2=="patch" { last=$3 }
                END {
                    q = last; sub(/#.*/, "", q); sub(/^\?/, "", q)
                    print q
                }' .sniff
}

#  Tip recorded for KEY in `keeper refs` output.  Empty if KEY absent.
ref_tip() {
    "$KEEPER" refs 2>/dev/null | awk -v k="$1" '
        { sub(/^[[:space:]]+/, "")
          tab = index($0, "\t"); if (tab == 0) next
          kf = substr($0, 1, tab - 1); if (kf != k) next
          n = split($0, toks, /[[:space:]]+/)
          v = toks[n]; sub(/^\?/, "", v); print v; exit
        }'
}

WT="$TMP/wt"
mkdir -p "$WT"; cd "$WT"

# ------------------------------------------------------------------
# 1. seed trunk: first commit gives us T1
# ------------------------------------------------------------------
echo "=== 1. trunk baseline ==="
echo "x v1" > x.txt
"$BE" post v1 >/dev/null
T1=$(head_hex)
[ -n "$T1" ] || fail "no trunk tip after first post"
[ "$(cur_branch)" = "" ] || fail "expected trunk (empty branch), got '$(cur_branch)'"
TRUNK_REFS_T1=$(ref_tip "?")
[ "$TRUNK_REFS_T1" = "$T1" ] \
    || fail "trunk REFS tip $TRUNK_REFS_T1 != T1=$T1"
note "trunk T1=$T1"

# ------------------------------------------------------------------
# 2. create child branch via POST  (be post ?./fix1)
# ------------------------------------------------------------------
echo "=== 2. be post ?./fix1 — create child ==="
"$BE" post "?./fix1" >/dev/null \
    || fail "be post ?./fix1 failed (pre-spec: POST must create on miss)"
FIX1_REFS=$(ref_tip "?fix1")
[ -n "$FIX1_REFS" ] \
    || fail "?fix1 not in REFS after be post ?./fix1"
[ "$FIX1_REFS" = "$T1" ] \
    || fail "?fix1 should fork at T1=$T1; got $FIX1_REFS"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T1" ] \
    || fail "trunk REFS moved by branch create: $T1 -> $TRUNK_REFS"
note "?fix1 forked at $FIX1_REFS; trunk unchanged"

# Assert the per-branch shard dir exists — POSTSetLabel now calls
# KEEPCreateBranch before writing the REFS row.
[ -d .dogs/fix1 ] || fail ".dogs/fix1 shard missing after be post ?./fix1"
note ".dogs/fix1 shard materialised"

# ------------------------------------------------------------------
# 3. switch wt to the child branch  (be get ?fix1)
# ------------------------------------------------------------------
echo "=== 3. be get ?fix1 — switch wt ==="
"$BE" get "?fix1" >/dev/null \
    || fail "be get ?fix1 failed"
[ "$(cur_branch)" = "fix1" ] \
    || fail "wt baseline branch should be 'fix1', got '$(cur_branch)'"
[ "$(head_hex)" = "$T1" ] \
    || fail "wt tip should still be T1 right after switch"
note "wt now on ?fix1 at T1"

# ------------------------------------------------------------------
# 4. edit a tracked file on the child
# ------------------------------------------------------------------
echo "=== 4. edit on child branch ==="
sleep 0.2                             # distinct mtime
echo "x v2 (fix1)" > x.txt
note "x.txt modified on ?fix1"

# ------------------------------------------------------------------
# 5. stage + commit on the child branch
#
#    TODO(spec): bare `be post` on a child with staged puts is dry-run
#                only today (prints "M ... / N change(s)" without
#                writing a commit row).  Pass a message to force the
#                commit.  Switch back to bare POST when sniff/POST is
#                spec-aligned.
# ------------------------------------------------------------------
echo "=== 5. be put + be post on ?fix1 ==="
"$BE" put x.txt >/dev/null \
    || fail "be put x.txt on ?fix1 failed"
"$BE" post fix1 v2 >/dev/null \
    || fail "be post on ?fix1 failed"
T2=$(head_hex)
[ -n "$T2" ] || fail "no tip after post on ?fix1"
[ "$T2" != "$T1" ] || fail "post on ?fix1 did not advance tip"
note "?fix1 tip advanced to $T2"

# ------------------------------------------------------------------
# 6. verify: ?fix1 moved to T2; trunk still at T1
# ------------------------------------------------------------------
echo "=== 6. verify branch tips ==="
FIX1_REFS=$(ref_tip "?fix1")
TRUNK_REFS=$(ref_tip "?")
[ "$FIX1_REFS" = "$T2" ] \
    || fail "REFS ?fix1 should be T2=$T2; got $FIX1_REFS"
[ "$TRUNK_REFS" = "$T1" ] \
    || fail "REFS ? should be T1=$T1; got $TRUNK_REFS (trunk moved!)"
note "REFS: ?=$T1 ?fix1=$T2"

# ------------------------------------------------------------------
# 7. switch back to trunk via ?..
# ------------------------------------------------------------------
echo "=== 7. be get ?.. — switch back to trunk ==="
"$BE" get "?.." >/dev/null \
    || fail "be get ?.. failed"
[ "$(cur_branch)" = "" ] \
    || fail "wt should be on trunk (empty branch); got '$(cur_branch)'"
[ "$(head_hex)" = "$T1" ] \
    || fail "wt tip on trunk should be T1=$T1; got $(head_hex)"
note "wt back on trunk at T1"

# ------------------------------------------------------------------
# 8. delete the child branch
# ------------------------------------------------------------------
echo "=== 8. be delete ?fix1 ==="
"$BE" delete "?fix1" >/dev/null \
    || fail "be delete ?fix1 failed"

# ------------------------------------------------------------------
# 9. verify: ?fix1 gone from REFS; trunk tip unchanged; shard dir
#    removed (DELBranch now calls KEEPBranchDrop after the REFS
#    tombstone).
# ------------------------------------------------------------------
echo "=== 9. verify deletion ==="
FIX1_REFS=$(ref_tip "?fix1")
[ -z "$FIX1_REFS" ] \
    || fail "?fix1 still visible in REFS after delete: $FIX1_REFS"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T1" ] \
    || fail "trunk REFS moved across delete: $T1 -> $TRUNK_REFS"
[ ! -e .dogs/fix1 ] || fail ".dogs/fix1 left behind after delete"
note ".dogs/fix1 shard removed by KEEPBranchDrop"
note "?fix1 deleted; trunk unchanged at T1=$T1"

echo "=== workflow-branches: increment 1 OK ==="
