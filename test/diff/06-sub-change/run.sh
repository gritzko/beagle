#!/bin/sh
#  diff/06-sub-change — SUBS-012.  The `diff:` projector aggregates
#  every mounted submodule's working-tree diff into one merged,
#  path-prefixed hunk stream — exactly like the verb reports.  A dirty
#  *parent* file and a dirty *sub* file must BOTH appear; `--nosub`
#  suppresses the sub side.
#
#  Same machinery as SUBS-011: BEProjector fans whole-tree projectors
#  (diff/tree/refs + search) into mounted subs via BERelaySub.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f main.c ]            || fail "parent main.c missing after get"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing after get"

#  Dirty BOTH the parent and the sub with distinct sentinels.
printf '\n// DIRTYPARENT\n' >> main.c
printf '\n// DIRTYSUB\n'    >> vendor/sub/core.c

#  diff: must show the parent change AND the path-prefixed sub change.
"$BE" diff: >02.diff.got.out 2>02.diff.got.err
rc=$?
[ "$rc" = 0 ] || fail "be diff: exited $rc; stderr:
$(cat 02.diff.got.err)
stdout:
$(cat 02.diff.got.out)"

grep -q "DIRTYPARENT" 02.diff.got.out \
    || fail "diff: missing parent change; stdout:
$(cat 02.diff.got.out)"
grep -q "DIRTYSUB" 02.diff.got.out \
    || fail "diff: did not aggregate the sub working-tree change; stdout:
$(cat 02.diff.got.out)"
grep -q "vendor/sub/core.c" 02.diff.got.out \
    || fail "diff: sub hunk not path-prefixed under vendor/sub/; stdout:
$(cat 02.diff.got.out)"

#  --nosub suppresses the sub side; the parent change still shows.
"$BE" diff: --nosub >03.diff.got.out 2>03.diff.got.err
rc=$?
[ "$rc" = 0 ] || fail "be diff: --nosub exited $rc; stderr:
$(cat 03.diff.got.err)"
grep -q "DIRTYPARENT" 03.diff.got.out \
    || fail "diff: --nosub dropped the parent change; stdout:
$(cat 03.diff.got.out)"
if grep -q "DIRTYSUB" 03.diff.got.out; then
    fail "diff: --nosub still aggregated the sub change; stdout:
$(cat 03.diff.got.out)"
fi

note "diff/06: diff: aggregates sub working-tree change, --nosub opts out"
