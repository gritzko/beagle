#!/bin/sh
#  branches/08-conflict — extracted from workflow-branches.sh stages 30-31.
#  Cross-branch promote conflict abort; cascade-conflict (deferred).

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

echo "=== 30. cross-branch promote conflict ==="
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
# Secondary wt: `.be` is a regular file = its own wtlog seeded from
# the primary's (row-0 `repo` URI names the shared store).
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

# Snapshot wt content (cur on ?fix1) before the conflicting promote.
WT_FILE_PRE=$(sha1sum conflict30.txt 2>/dev/null | awk '{print $1}')

set +e
"$BE" post "?.." 2>"$ETMP/p30.err" >/dev/null
EC30=$?
set -e

[ "$EC30" != "0" ] \
    || { cat "$ETMP/p30.err" >&2; fail "§30: be post ?.. should have aborted"; }
grep -q 'rebase aborted' "$ETMP/p30.err" \
    || fail "§30: stderr should mention 'rebase aborted'"
T30_REFS_AFTER=$(ref_tip "?")
F1_REFS_AFTER=$(ref_tip "?fix1")
[ "$T30_REFS_AFTER" = "$T30_NEW" ] \
    || fail "§30: trunk REFS moved across aborted post"
[ "$F1_REFS_AFTER" = "$F1_REFS_PRE" ] \
    || fail "§30: ?fix1 REFS moved across aborted post"
WT_FILE_POST=$(sha1sum conflict30.txt 2>/dev/null | awk '{print $1}')
[ "$WT_FILE_POST" = "$WT_FILE_PRE" ] \
    || fail "§30: wt conflict30.txt changed across aborted post"
note "§30 OK: conflict aborted; REFS + wt unchanged"

echo "=== 31. cascade conflict (smoke; descendant pass deferred) ==="
skip "§31 cascade-into-descendant after auto-sync — cascade walker skip semantics"

echo "=== branches/08-conflict: OK ==="
