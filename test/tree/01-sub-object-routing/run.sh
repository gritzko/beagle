#!/bin/sh
#  tree/01-sub-object-routing — SUBS-012.  Object/tree projectors whose
#  PATH lands inside a mounted submodule resolve in the SUB shard, not
#  the parent (whose tree holds only a 160000 gitlink at that path).
#
#    tree:vendor/sub?master           → descends into the sub tree
#    blob:vendor/sub/core.c?master    → the sub blob's bytes
#
#  BEProjectorRouteToMount detects the path-in-mount, rewrites the
#  projector mount-relative, and relays it into the sub (BERelaySub
#  re-prefixes the hunk URI under the mount path).  `--nosub` opts out
#  (the path then stays parent-bound and fails as before).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing after get"

#  Pre-fix, both of these returned PROJFAIL / KEEPNONE (parent-only).

#  tree:vendor/sub?master must list the sub's tree entries (core.c +
#  helper.c live at the sub root), not fail at the gitlink.
"$BE" tree:vendor/sub?master >02.tree.got.out 2>02.tree.got.err
rc=$?
[ "$rc" = 0 ] || fail "be tree:vendor/sub?master exited $rc; stderr:
$(cat 02.tree.got.err)
stdout:
$(cat 02.tree.got.out)"
grep -q "core.c"   02.tree.got.out || fail "tree: did not descend into sub (no core.c); stdout:
$(cat 02.tree.got.out)"
grep -q "helper.c" 02.tree.got.out || fail "tree: did not descend into sub (no helper.c); stdout:
$(cat 02.tree.got.out)"

#  blob:vendor/sub/core.c?master must emit the SUB blob bytes
#  (sub/core.c carries the `sub_counter` symbol, never in the parent).
"$BE" blob:vendor/sub/core.c?master >03.blob.got.out 2>03.blob.got.err
rc=$?
[ "$rc" = 0 ] || fail "be blob:vendor/sub/core.c?master exited $rc; stderr:
$(cat 03.blob.got.err)
stdout:
$(cat 03.blob.got.out)"
grep -q "sub_counter" 03.blob.got.out \
    || fail "blob: did not resolve the sub blob (no sub_counter); stdout:
$(cat 03.blob.got.out)"

#  A parent-shard blob is unaffected by the routing.
"$BE" blob:main.c?master >04.pblob.got.out 2>04.pblob.got.err
rc=$?
[ "$rc" = 0 ] || fail "be blob:main.c?master (parent) exited $rc; stderr:
$(cat 04.pblob.got.err)"
grep -q "main" 04.pblob.got.out \
    || fail "blob:main.c?master parent blob empty; stdout:
$(cat 04.pblob.got.out)"

#  --nosub opts out: the sub-path projector stays parent-bound and
#  fails (the parent shard has only a gitlink there), exit non-zero.
rc=0
"$BE" blob:vendor/sub/core.c?master --nosub >05.nosub.got.out 2>05.nosub.got.err || rc=$?
[ "$rc" != 0 ] \
    || fail "blob: --nosub unexpectedly resolved a sub path; stdout:
$(cat 05.nosub.got.out)"

note "tree/01: tree:/blob: route a sub path into the sub shard, --nosub opts out"
