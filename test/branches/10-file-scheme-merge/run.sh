#!/bin/sh
#  branches/10-file-scheme-merge — sibling worktree wired via the
#  `file:` scheme, edits + commits on a child branch, then primary
#  patches + posts the branch's stack into trunk (VERBS.md
#  §"Worktree management" Example 2 + Example 3 merge step).
#
#  Walks the full happy path:
#    1.  primary on trunk + seed commit (T1)
#    2.  primary creates ?fix1 (`be post ?./fix1`)
#    3.  sibling wt wired to primary's store on ?fix1
#        (`be get file:<primary>?fix1`)
#    4.  sibling edits + commits on ?fix1 → tip advances
#    5.  primary patches ?fix1 into trunk wt
#    6.  primary posts merge → single-parent commit on trunk
#    7.  ?fix1 untouched; trunk advanced to the merge

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# 1. primary trunk seed
echo "v1" > x.txt
"$BE" post 'seed msg' >/dev/null || fail "primary seed post failed"
T1=$(head_hex)
[ -n "$T1" ] || fail "no trunk tip after seed"
[ "$(cur_branch)" = "" ] || fail "primary should be on trunk"
note "primary trunk T1=$T1"

# 2. primary creates ?fix1 (cur stays on trunk)
"$BE" put "?./fix1" >/dev/null || fail "be post ?./fix1 failed"
[ -d ".dogs/fix1" ] || fail ".dogs/fix1 shard missing"
[ "$(ref_tip "?fix1")" = "$T1" ] \
    || fail "?fix1 should fork at T1"
[ "$(cur_branch)" = "" ] || fail "primary cur should still be trunk"
note "?fix1 created at T1; primary cur unchanged"

# 3. sibling wt — `be get file:<primary>` then switch to ?fix1
#
#  TODO(spec): `be get file:$WT?fix1` should honour the ?ref and
#  land the sibling directly on ?fix1, but BEGetWorktree currently
#  rewrites the URI to primary's current sha (ignoring ?fix1).  Two
#  steps for now: wire the sibling, then `be get ?fix1` switches it.
WT2="$ETMP/wt2"
mkdir -p "$WT2"
( cd "$WT2" && "$BE" get "file:$WT" >/dev/null 2>"$ETMP/wt2-get.err" ) \
    || { cat "$ETMP/wt2-get.err"; fail "be get file: failed"; }
[ -L "$WT2/.dogs" ] || fail "$WT2/.dogs not a symlink"
tgt=$(readlink "$WT2/.dogs")
[ "$tgt" = "$WT/.dogs" ] \
    || fail "$WT2/.dogs -> $tgt, expected $WT/.dogs"
[ -f "$WT2/.sniff" ] && [ ! -L "$WT2/.sniff" ] \
    || fail "$WT2/.sniff should be a local file"
[ -f "$WT2/x.txt" ] || fail "x.txt not checked out in WT2"
note "WT2 wired to $WT/.dogs (on trunk @ T1)"

( cd "$WT2" && "$BE" get "?fix1" >/dev/null 2>"$ETMP/wt2-switch.err" ) \
    || { cat "$ETMP/wt2-switch.err"; fail "WT2 switch to ?fix1 failed"; }
( cd "$WT2" && [ "$(cur_branch)" = "fix1" ] ) \
    || fail "WT2 should be on fix1 after switch"
note "WT2 switched to ?fix1"

# 4. sibling commits on ?fix1
( cd "$WT2" && sleep 0.01 && echo "extra" > extra.txt \
    && "$BE" put extra.txt >/dev/null \
    && "$BE" post 'fix1 c1' >/dev/null ) \
    || fail "WT2: edit/put/post on ?fix1 failed"
FIX1_TIP=$(ref_tip "?fix1")
[ -n "$FIX1_TIP" ] && [ "$FIX1_TIP" != "$T1" ] \
    || fail "?fix1 didn't advance past T1"
note "?fix1 advanced to $FIX1_TIP"

# 5. primary trunk still at T1, no extra.txt yet
[ "$(ref_tip "?")" = "$T1" ] \
    || fail "trunk REFS moved during sibling commit"
[ ! -e extra.txt ] \
    || fail "extra.txt should not be on trunk before patch"

# 6. primary: patch ?./fix1 — absorb fix1's stack into trunk wt
"$BE" patch "?./fix1" 2>"$ETMP/patch.err" >/dev/null \
    || { cat "$ETMP/patch.err"; fail "be patch ?./fix1 failed"; }
[ -f extra.txt ] || fail "extra.txt missing in trunk wt after patch"
[ "$(ref_tip "?")" = "$T1" ] \
    || fail "trunk REFS moved by PATCH (must wait for POST)"
note "patch landed extra.txt in trunk wt; trunk REFS still T1"

# 7. primary: put + post merge — single-parent commit on trunk
"$BE" put extra.txt >/dev/null \
    || fail "be put extra.txt failed"
"$BE" post 'merge fix1' >/dev/null \
    || fail "be post merge fix1 failed"
T_MERGE=$(head_hex)
[ -n "$T_MERGE" ] && [ "$T_MERGE" != "$T1" ] \
    || fail "trunk didn't advance past T1 after merge POST"
[ "$(ref_tip "?")" = "$T_MERGE" ] \
    || fail "trunk REFS != T_MERGE=$T_MERGE"
note "trunk advanced to T_MERGE=$T_MERGE"

# 8. invariants: single-parent merge; ?fix1 untouched
PARENTS=$("$KEEPER" get ".#$T_MERGE" 2>/dev/null \
            | grep -c '^parent ' || true)
[ "$PARENTS" = "1" ] \
    || fail "T_MERGE has $PARENTS parent(s); want exactly 1"
PARENT_SHA=$("$KEEPER" get ".#$T_MERGE" 2>/dev/null \
                | awk '/^parent / { print $2; exit }')
[ "$PARENT_SHA" = "$T1" ] \
    || fail "T_MERGE parent is $PARENT_SHA; want T1=$T1"
note "T_MERGE has 1 parent == T1 (no merge commit)"

[ "$(ref_tip "?fix1")" = "$FIX1_TIP" ] \
    || fail "?fix1 tip moved across PATCH/POST: $FIX1_TIP -> $(ref_tip "?fix1")"
note "?fix1 still at $FIX1_TIP (untouched by primary's merge)"

echo "=== branches/10-file-scheme-merge: OK ==="
