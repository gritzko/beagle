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

# --- 5. patch squash child into parent ---
#
#  Increment 2 — exercise the spec-aligned PATCH verb:
#  re-create a child, give it a multi-commit stack, then squash it
#  into trunk via `be patch` + `be post`.  Per VERBS.md §PATCH and
#  Invariant 2, PATCH erases provenance: the next POST emits a
#  *single-parent* commit on cur (no `&theirs`, no merge commit), and
#  the child branch is left untouched.
#
#  Both `be patch ?./fix1` (relative) and `be patch ?fix1` (absolute)
#  are accepted: sniff/PATCH absolutises the query against the wt's
#  current branch the same way POST/GET do.  We use the relative form
#  here to exercise the parse path.
# ------------------------------------------------------------------

# ------------------------------------------------------------------
# 10. re-create child + stack two commits on it (C1, C2)
# ------------------------------------------------------------------
echo "=== 10. rebuild ?fix1 with a 2-commit stack ==="
"$BE" post "?./fix1" >/dev/null \
    || fail "be post ?./fix1 (re-create) failed"
"$BE" get "?fix1" >/dev/null \
    || fail "be get ?fix1 (re-switch) failed"
[ "$(cur_branch)" = "fix1" ] \
    || fail "wt should be on ?fix1 after re-switch; got '$(cur_branch)'"

sleep 0.2
echo "a v1 (fix1)" > a.txt
"$BE" put a.txt >/dev/null \
    || fail "be put a.txt on ?fix1 failed"
"$BE" post fix1 c1 >/dev/null \
    || fail "be post fix1 c1 failed"
C1=$(head_hex)
[ -n "$C1" ] && [ "$C1" != "$T1" ] \
    || fail "C1 not advanced past T1 (got '$C1')"

sleep 0.2
echo "b v1 (fix1)" > b.txt
"$BE" put b.txt >/dev/null \
    || fail "be put b.txt on ?fix1 failed"
"$BE" post fix1 c2 >/dev/null \
    || fail "be post fix1 c2 failed"
C2=$(head_hex)
[ -n "$C2" ] && [ "$C2" != "$C1" ] \
    || fail "C2 not advanced past C1 (got '$C2')"
note "?fix1 stack: T1 -> C1=$C1 -> C2=$C2"

# ------------------------------------------------------------------
# 11. switch back to trunk; capture pre-patch trunk tip
# ------------------------------------------------------------------
echo "=== 11. switch back to trunk; T_pre ==="
"$BE" get "?.." >/dev/null \
    || fail "be get ?.. (back to trunk) failed"
[ "$(cur_branch)" = "" ] \
    || fail "wt should be on trunk; got '$(cur_branch)'"
T_pre=$(head_hex)
[ "$T_pre" = "$T1" ] \
    || fail "trunk tip pre-patch should be T1=$T1; got T_pre=$T_pre"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T1" ] \
    || fail "trunk REFS pre-patch should be T1=$T1; got $TRUNK_REFS"
[ ! -e a.txt ] || fail "a.txt should not be on trunk before patch"
[ ! -e b.txt ] || fail "b.txt should not be on trunk before patch"
note "trunk tip T_pre=$T_pre; wt has only x.txt"

# ------------------------------------------------------------------
# 12. be patch ?./fix1 — absorb child's full stack into trunk's wt
# ------------------------------------------------------------------
echo "=== 12. be patch ?./fix1 — absorb stack into wt ==="
PATCH_OUT="$TMP/patch.err"
"$BE" patch "?./fix1" 2>"$PATCH_OUT" >/dev/null \
    || fail "be patch ?./fix1 failed (stderr: $(cat "$PATCH_OUT"))"
grep -q '^sniff: patch:' "$PATCH_OUT" \
    || fail "no 'sniff: patch:' summary in stderr: $(cat "$PATCH_OUT")"
[ -f a.txt ] || fail "a.txt missing in trunk wt after patch"
[ -f b.txt ] || fail "b.txt missing in trunk wt after patch"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T_pre" ] \
    || fail "trunk REFS moved by PATCH: $T_pre -> $TRUNK_REFS (must wait for POST)"
note "patch landed a.txt + b.txt in wt; trunk REFS still T_pre"

# ------------------------------------------------------------------
# 13. be put + be post squash — single-parent commit on trunk
#
#     Selective mode: explicit puts before POST so the bare-POST
#     dry-run pre-spec gap (see §5 TODO) doesn't bite us.
# ------------------------------------------------------------------
echo "=== 13. be put a.txt + be put b.txt + be post squash ==="
"$BE" put a.txt >/dev/null \
    || fail "be put a.txt (post-patch) failed"
"$BE" put b.txt >/dev/null \
    || fail "be put b.txt (post-patch) failed"
"$BE" post squash >/dev/null \
    || fail "be post squash failed"
T_squash=$(head_hex)
[ -n "$T_squash" ] || fail "no trunk tip after post squash"
[ "$T_squash" != "$T_pre" ] \
    || fail "post squash did not advance trunk tip past T_pre=$T_pre"
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T_squash" ] \
    || fail "trunk REFS should be T_squash=$T_squash; got $TRUNK_REFS"
note "trunk tip advanced to T_squash=$T_squash"

# ------------------------------------------------------------------
# 14. assert single-parent + child untouched (load-bearing)
# ------------------------------------------------------------------
echo "=== 14. verify squash invariants ==="
PARENTS=$("$KEEPER" get ".#$T_squash" 2>/dev/null \
            | grep -c '^parent ' || true)
[ "$PARENTS" = "1" ] \
    || fail "T_squash has $PARENTS parent line(s); want exactly 1 (single-parent squash)"
PARENT_SHA=$("$KEEPER" get ".#$T_squash" 2>/dev/null \
                | awk '/^parent / { print $2; exit }')
[ "$PARENT_SHA" = "$T_pre" ] \
    || fail "T_squash parent is $PARENT_SHA; want T_pre=$T_pre"
note "T_squash has exactly 1 parent, == T_pre (no merge commit)"

#  Orthogonality: PATCH must not touch the child branch.
FIX1_REFS=$(ref_tip "?fix1")
[ "$FIX1_REFS" = "$C2" ] \
    || fail "?fix1 tip moved across PATCH/POST: $C2 -> $FIX1_REFS"
note "?fix1 tip still at C2=$C2 (child untouched by PATCH)"

# ------------------------------------------------------------------
# 15. cleanup: drop ?fix1 so the test is idempotent on re-runs
# ------------------------------------------------------------------
echo "=== 15. cleanup: be delete ?fix1 ==="
"$BE" delete "?fix1" >/dev/null \
    || fail "be delete ?fix1 (cleanup) failed"
[ -z "$(ref_tip "?fix1")" ] \
    || fail "?fix1 still in REFS after cleanup delete"
[ ! -e .dogs/fix1 ] || fail ".dogs/fix1 left behind after cleanup delete"
note "?fix1 cleaned up"

# ------------------------------------------------------------------
# --- 6. rebase scenarios ---
#
# Increment 3 (Stage 2 phase-2 promote): a non-ff `be post` rebases
# the new commit onto the live REFS tip when the branch advanced out
# from under us.  We exercise the simplest shape — same-branch
# divergence on trunk via a second wt sharing one keeper store.
#
# DEFERRED (TODO(spec)):
#   * two-level cascade (?fix1 + ?fix1/sub) once the cascade walker
#     lands.  Today only cur's just-built commit is replayed; the
#     descendant cascade is not yet wired.  See sniff/POST.c
#     "TODO(spec): cross-branch promote" comment at the rebase site.
#   * `?..` rebase (parent absorbs cur with cur auto-sync) — needs
#     cross-branch promote dispatch.
# ------------------------------------------------------------------

echo "=== 16. setup secondary wt (WT2) sharing one keeper ==="
WT2="$TMP/wt2"
mkdir -p "$WT2"
ln -s "$WT/.dogs" "$WT2/.dogs"
cp "$WT/x.txt" "$WT2/x.txt"
cp "$WT/.sniff" "$WT2/.sniff"
T_pre_rebase=$T_squash
note "WT2 forked at trunk tip T_pre_rebase=$T_pre_rebase"

# Advance WT (primary) trunk by adding a new file.
echo "=== 17. WT advances trunk: a new commit lands first ==="
sleep 0.2
echo "racing-1" > racing.txt
"$BE" put racing.txt >/dev/null \
    || fail "WT: be put racing.txt failed"
"$BE" post racing-first >/dev/null \
    || fail "WT: be post racing-first failed"
T_advance=$(head_hex)
[ "$T_advance" != "$T_pre_rebase" ] \
    || fail "WT advance didn't change tip (got $T_advance)"
note "WT advanced trunk to T_advance=$T_advance"

# WT2's .sniff still references T_pre_rebase as its parent.  Edit a
# disjoint file and post — Stage 2 phase-2 promote rebases WT2's new
# commit onto T_advance.
echo "=== 18. WT2 posts on top of stale tip → rebase ==="
cd "$WT2"
sleep 0.2
echo "wt2-only" > wt2.txt
"$BE" put wt2.txt >/dev/null \
    || fail "WT2: be put wt2.txt failed"
"$BE" post wt2-rebase 2>"$TMP/wt2-rebase.err" >/dev/null \
    || { cat "$TMP/wt2-rebase.err"; fail "WT2: be post should have rebased"; }
T_rebased=$(head_hex)
[ -n "$T_rebased" ] && [ "$T_rebased" != "$T_advance" ] \
    && [ "$T_rebased" != "$T_pre_rebase" ] \
    || fail "WT2: rebased tip $T_rebased not distinct from T_advance/T_pre_rebase"
note "WT2 rebased onto T_advance; new trunk tip T_rebased=$T_rebased"

# Verify REFS advanced and the rebased commit's parent is T_advance.
TRUNK_REFS=$(ref_tip "?")
[ "$TRUNK_REFS" = "$T_rebased" ] \
    || fail "trunk REFS at $TRUNK_REFS; want T_rebased=$T_rebased"
PARENT_REBASED=$("$KEEPER" get ".#$T_rebased" 2>/dev/null \
                    | awk '/^parent / { print $2; exit }')
[ "$PARENT_REBASED" = "$T_advance" ] \
    || fail "T_rebased's parent is $PARENT_REBASED; want T_advance=$T_advance"
note "T_rebased.parent = T_advance (rebase landed on top)"

# Conflict abort: WT2 edits the SAME file as WT did.  The advance commit
# touched racing.txt; rewrite the wt to also edit racing.txt, then post.
# We need T_advance still in REFS — switch back to the primary wt to
# ensure the advance happened, then run the conflicting post on a
# fresh WT3 forked at T_pre_rebase.
echo "=== 19. WT3 conflict abort: edits racing.txt vs T_advance ==="
WT3="$TMP/wt3"
mkdir -p "$WT3"
ln -s "$WT/.dogs" "$WT3/.dogs"
# WT3 starts at T_rebased (current state of REFS); rewind .sniff to a
# stale baseline so the conflict path fires.  Easiest: copy WT2's
# original (pre-rebase) .sniff snapshot — but we already advanced past
# that.  Skip this aggressive scenario on the workflow path; the
# unit/integration covers it via the tooling layer.
skip "explicit conflict-abort scenario deferred — needs scripted .sniff rewind"

cd "$WT"
echo "=== workflow-branches: increment 1 + 2 + 3 OK ==="
