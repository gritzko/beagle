#!/bin/sh
#  get/06-branch-switch-shards — under the FLAT store layout, `be get
#  ?<branch>` switches the wt across sibling branches that all share the
#  ONE project pool `.be/$P/` (no per-branch object dir).  Reads must
#  satisfy the cross-branch WEAVE invariant: graf walks parent chains via
#  KEEPGet across the shared pool.
#
#  Flat layout: every commit (trunk or branch) lands in the single
#  project shard `.be/$P/`; branch tips are REFS rows.  This case keeps
#  the behavioural invariant: switching ?feat <-> ?other resolves the
#  correct content each way.

. "$(dirname "$0")/../../lib/case.sh"

# Anchor project shard at .be/$P/ so subsequent be invocations
# don't derive the project name from the first URI's basename.
"$BE" put "?/$P/" 2>/dev/null || true

# ====================================================================
# 1. trunk baseline (flat shard: .be/$P/)
# ====================================================================
echo "trunk v1" > x.txt
"$BE" put x.txt    > /dev/null
"$BE" post 't0'    > /dev/null
[ -n "$(ls -1 .be/$P/*.keeper 2>/dev/null)" ] || {
    echo "FAIL: no pack in project shard after baseline post" >&2
    ls -la .be >&2
    exit 1
}

# ====================================================================
# 2. ?feat branch: switch + commit (lands in the shared pool)
# ====================================================================
"$BE" put '?./feat'     > /dev/null
[ ! -d .be/$P/feat ] || { echo "FAIL: per-branch shard .be/$P/feat must not exist (flat layout)" >&2; exit 1; }
"$BE" get  '?feat'      > /dev/null
sleep 0.02
echo "feat v2"  > x.txt
"$BE" put  x.txt        > /dev/null
"$BE" post 'feat msg'   > /dev/null

# ====================================================================
# 3. switch back to trunk, then create + commit ?other (sibling of feat)
# ====================================================================
"$BE" get  '?..'         > /dev/null
[ "$(cat x.txt)" = "trunk v1" ] || {
    echo "FAIL: trunk x.txt should be 'trunk v1' after switch back" >&2
    cat x.txt >&2; exit 1
}
"$BE" put  '?./other'    > /dev/null
[ ! -d .be/$P/other ] || { echo "FAIL: per-branch shard .be/$P/other must not exist (flat layout)" >&2; exit 1; }
"$BE" get  '?other'      > /dev/null
sleep 0.02
echo "other v3" > x.txt
"$BE" put  x.txt         > /dev/null
"$BE" post 'other msg'   > /dev/null

# ====================================================================
# 4. THE TEST: cross-branch switch ?other → ?feat.  ?feat's objects
#    live in the shared pool .be/$P/, which the keeper open registers.
#    Without it, `be get ?feat` fails with 'object not found' (graf
#    can't resolve the feat tip tree).
# ====================================================================
"$BE" get '?feat' > 01.switch.got.out 2> 01.switch.got.err
SWITCH_RC=$?
[ "$SWITCH_RC" = "0" ] || {
    echo "FAIL: cross-shard 'be get ?feat' exited $SWITCH_RC" >&2
    echo "stdout:" >&2; cat 01.switch.got.out >&2
    echo "stderr:" >&2; cat 01.switch.got.err >&2
    exit 1
}
[ "$(cat x.txt)" = "feat v2" ] || {
    echo "FAIL: x.txt should be 'feat v2' after switch to ?feat; got:" >&2
    cat x.txt >&2; exit 1
}

# ====================================================================
# 5. and back the other way — ?feat → ?other.
# ====================================================================
"$BE" get '?other'                       > /dev/null
[ "$(cat x.txt)" = "other v3" ] || {
    echo "FAIL: x.txt should be 'other v3' after switch to ?other" >&2
    cat x.txt >&2; exit 1
}

# ====================================================================
# 6. final layout: one shared pool, no per-branch dirs; both refs
#    still resolve.
# ====================================================================
[ ! -d .be/$P/feat ]  || { echo "FAIL: per-branch shard .be/$P/feat must not exist (flat layout)" >&2; exit 1; }
[ ! -d .be/$P/other ] || { echo "FAIL: per-branch shard .be/$P/other must not exist (flat layout)" >&2; exit 1; }
"$BE" get '?feat'  > /dev/null || { echo "FAIL: ?feat no longer resolves" >&2; exit 1; }
"$BE" get '?other' > /dev/null || { echo "FAIL: ?other no longer resolves" >&2; exit 1; }
