#!/bin/sh
#  branches/08-conflict — `be patch ?..#` from a child branch reports
#  a conflict when both sides edited the same line.  Patch exits
#  non-zero, leaves conflict markers in the wt; subsequent `be post`
#  refuses.  Neither ?fix1 nor trunk's REFS move.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

echo "=== 30. cross-branch rebase conflict ==="
cd "$WT"

# Seed conflict.txt baseline on trunk.
sleep 0.01
printf 'the quick fox\n' > conflict30.txt
"$BE" put conflict30.txt >/dev/null || fail "§30: put conflict30.txt failed"
"$BE" post '30-base msg' >/dev/null || fail "§30: post 30-base failed"
T30_BASE=$(head_hex)

# Create ?fix1 with the OURS edit ("QUICK").
"$BE" put "?./fix1" >/dev/null || fail "§30: create ?fix1 failed"
"$BE" get "?fix1" >/dev/null || fail "§30: switch to ?fix1 failed"
sleep 0.01
printf 'the QUICK fox\n' > conflict30.txt
"$BE" put conflict30.txt >/dev/null || fail "§30: put OURS on ?fix1 failed"
"$BE" post 'fix1-ours msg' >/dev/null || fail "§30: post fix1-ours failed"
F1_OURS=$(head_hex)
F1_REFS_PRE=$(ref_tip "?fix1")

# Advance trunk via a peer wt with the THEIRS edit ("slow").
WT30="$ETMP/wt30"
mkdir -p "$WT30"
cp "$WT/.be/wtlog" "$WT30/.be"
( cd "$WT30" && "$BE" get "?" >/dev/null ) \
    || fail "§30: WT30 trunk checkout failed"
( cd "$WT30" && sleep 0.01 && printf 'the slow fox\n' > conflict30.txt \
    && "$BE" put conflict30.txt >/dev/null \
    && "$BE" post '30-theirs msg' >/dev/null ) \
    || fail "§30: WT30 trunk advance failed"
T30_NEW=$(ref_tip "?")
[ "$T30_NEW" != "$T30_BASE" ] || fail "§30: trunk didn't advance"
note "§30: trunk advanced T30_BASE=$T30_BASE -> T30_NEW=$T30_NEW"

# `be patch ?..#` should hit a 3-way conflict on the shared line.
set +e
"$BE" patch "?..#" 2>"$ETMP/p30.err" >"$ETMP/p30.out"
EC30=$?
set -e

[ "$EC30" != "0" ] \
    || { cat "$ETMP/p30.err" >&2; fail "§30: be patch ?..# should fail on conflict"; }
grep -qE '[[:space:]]+conflict[[:space:]]+(\./)?conflict30\.txt$' \
        "$ETMP/p30.out" \
    || fail "§30: status row should report 'conflict conflict30.txt'; got: $(cat $ETMP/p30.out $ETMP/p30.err)"

# Wt must have token-level 4-char conflict markers.
grep -F '<<<<' conflict30.txt \
    || fail "§30: expected '<<<<' marker in conflict30.txt"
grep -F '>>>>' conflict30.txt \
    || fail "§30: expected '>>>>' marker in conflict30.txt"

# POST must refuse — conflict markers in tracked file.
set +e
"$BE" post 'should not commit' 2>"$ETMP/p30.post.err" >/dev/null
EC30_POST=$?
set -e
[ "$EC30_POST" != "0" ] \
    || fail "§30: be post should refuse with conflict markers present"

# Neither ref moves across the failed patch/post.
T30_REFS_AFTER=$(ref_tip "?")
F1_REFS_AFTER=$(ref_tip "?fix1")
[ "$T30_REFS_AFTER" = "$T30_NEW" ] \
    || fail "§30: trunk REFS moved across failed patch/post"
[ "$F1_REFS_AFTER" = "$F1_REFS_PRE" ] \
    || fail "§30: ?fix1 REFS moved across failed patch/post"

note "§30 OK: conflict detected; REFS unchanged"

echo "=== 31. cascade conflict (smoke; descendant pass deferred) ==="
skip "§31 cascade-into-descendant after auto-sync — cascade walker skip semantics"

echo "=== branches/08-conflict: OK ==="
