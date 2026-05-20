#!/bin/sh
#  get/06-branch-switch-shards — `be get ?<branch>` switches the wt
#  across sibling branches, keeping each branch's pack log inside its
#  own `.be/<branch>/` shard.  Reads must satisfy the cross-branch
#  WEAVE invariant: graf walks parent chains via KEEPGet across PAST
#  (sibling branches collapsed into the read-only set) and DATA (the
#  active leaf).
#
#  Layout assertion (KEEP.h "Branch-aware object store" + abc/Bx.h
#  §PastDataS):
#
#     .be/                            trunk's own pack log
#       NNNNNNNNNN.keeper
#       NNNNNNNNNN.keeper.idx
#       feat/                         created by `be put ?./feat`
#         NNNNNNNNNN.keeper           every post on ?feat lands here
#         NNNNNNNNNN.keeper.idx
#       other/                        created by `be put ?./other`
#         NNNNNNNNNN.keeper           every post on ?other lands here
#         NNNNNNNNNN.keeper.idx
#
#  Crucially: posting on ?other must NOT extend trunk's or ?feat's
#  pack inventory — branch posts are leaf-only writes (KEEPCompact
#  rule).  Then `be get ?feat` after working on ?other must still
#  resolve ?feat's tip tree (its objects live in `.be/$P/feat/`, which
#  the new keeper open registers as PAST).

. "$(dirname "$0")/../../lib/case.sh"

# Anchor project shard at .be/$P/ so subsequent be invocations
# don't derive the project name from the first URI's basename.
"$BE" put "?/$P/" 2>/dev/null || true

# ====================================================================
# 1. trunk baseline + snapshot of trunk's pack inventory
# ====================================================================
echo "trunk v1" > x.txt
"$BE" put x.txt    > /dev/null
"$BE" post 't0'    > /dev/null
TRUNK_PACKS=$(ls -1 .be/$P/*.keeper 2>/dev/null | sort)
[ -n "$TRUNK_PACKS" ] || {
    echo "FAIL: no trunk pack after baseline post" >&2
    ls -la .be >&2
    exit 1
}

# ====================================================================
# 2. ?feat branch: switch + commit
# ====================================================================
"$BE" put '?./feat'     > /dev/null
[ -d .be/$P/feat ] || { echo "FAIL: .be/$P/feat/ missing after create" >&2; exit 1; }
"$BE" get  '?feat'      > /dev/null
sleep 0.02
echo "feat v2"  > x.txt
"$BE" put  x.txt        > /dev/null
"$BE" post 'feat msg'   > /dev/null

#  ?feat's commit must land at .be/$P/feat/*.keeper — not trunk.
FEAT_PACKS=$(ls -1 .be/$P/feat/*.keeper 2>/dev/null | sort)
[ -n "$FEAT_PACKS" ] || {
    echo "FAIL: no pack at .be/$P/feat/*.keeper after post on ?feat" >&2
    ls -laR .be >&2
    exit 1
}
[ "$(ls -1 .be/$P/*.keeper 2>/dev/null | sort)" = "$TRUNK_PACKS" ] || {
    echo "FAIL: trunk packs changed after post on ?feat" >&2
    exit 1
}

# ====================================================================
# 3. switch back to trunk, then create + commit ?other (sibling of feat)
# ====================================================================
"$BE" get  '?..'         > /dev/null
[ "$(cat x.txt)" = "trunk v1" ] || {
    echo "FAIL: trunk x.txt should be 'trunk v1' after switch back" >&2
    cat x.txt >&2; exit 1
}
"$BE" put  '?./other'    > /dev/null
[ -d .be/$P/other ] || { echo "FAIL: .be/$P/other/ missing" >&2; exit 1; }
"$BE" get  '?other'      > /dev/null
sleep 0.02
echo "other v3" > x.txt
"$BE" put  x.txt         > /dev/null
"$BE" post 'other msg'   > /dev/null

OTHER_PACKS=$(ls -1 .be/$P/other/*.keeper 2>/dev/null | sort)
[ -n "$OTHER_PACKS" ] || {
    echo "FAIL: no pack at .be/$P/other/*.keeper after post on ?other" >&2
    ls -laR .be >&2
    exit 1
}
[ "$(ls -1 .be/$P/*.keeper 2>/dev/null | sort)" = "$TRUNK_PACKS" ] || {
    echo "FAIL: trunk packs changed after post on ?other" >&2
    exit 1
}

# ====================================================================
# 4. THE TEST: cross-shard switch ?other → ?feat.  ?feat's objects
#    live in .be/$P/feat/, which the keeper open must register as PAST.
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
# 6. final layout: every branch's packs sit inside its own shard.
# ====================================================================
[ -n "$(ls -1 .be/$P/feat/*.keeper 2>/dev/null)" ]  || {
    echo "FAIL: .be/$P/feat/*.keeper missing in final layout" >&2
    ls -la .be/$P/feat >&2; exit 1
}
[ -n "$(ls -1 .be/$P/other/*.keeper 2>/dev/null)" ] || {
    echo "FAIL: .be/$P/other/*.keeper missing in final layout" >&2
    ls -la .be/$P/other >&2; exit 1
}
[ "$(ls -1 .be/$P/*.keeper 2>/dev/null | sort)" = "$TRUNK_PACKS" ] || {
    echo "FAIL: trunk packs grew after the cross-shard switches" >&2
    exit 1
}
