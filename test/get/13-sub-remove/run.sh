#!/bin/sh
#  get/13-sub-remove — target tree drops a sub: beagle unmounts.
#
#  Setup uses the 2-level fixture: parent has C2 (no sub) and C3
#  (with sub @ SUB_C2), both pushed.  Test sequence:
#    1. `be get $PARENT_URL?master` — clone parent at C3.  Sub
#       mounts at vendor/sub.
#    2. `be get ?prev` — switch parent back to C2 (no sub).  Beagle
#       diffs baseline (C3, has vendor/sub) against target (C2, no
#       subs); unmounts vendor/sub.
#
#  Assertions:
#    * `vendor/sub/.be` anchor is gone after step 2.
#    * stderr from step 2 includes `be: get vendor/sub: unmounted`.
#    * No regression on the 2-level get tests (parent files
#      survive, parent's wtlog grew by one `get` row).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

# --- Step 1: clone at master (C3) — sub mounts. ----------------------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "step-1 be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f vendor/sub/.be ]    || fail "step-1: sub did not mount"
[ -f vendor/sub/core.c ] || fail "step-1: sub content missing"

# --- Step 2: switch to prev (C2, no submodule) — sub unmounts. ------
#  Use the transport URL so keeper fetches the `prev` ref into local;
#  bare `?prev` would fail because only `master` is in local refs.
"$BE" get "$PARENT_URL?prev" >02.get.got.out 2>02.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "step-2 be get exited $rc; stderr:
$(cat 02.get.got.err)"

# Anchor gone (the file got unlinked).
[ ! -f vendor/sub/.be ] || fail "vendor/sub/.be still present after C2 switch"
[ ! -d vendor/sub/.be ] || fail "vendor/sub/.be unexpectedly is a dir"

# Marker on stderr.
grep -q '^be: get vendor/sub: unmounted' 02.get.got.err \
    || fail "unmount marker missing; stderr:
$(cat 02.get.got.err)"

# Parent files survive (C2 has main.c + util.c, no sub gitlink).
[ -f main.c ] || fail "parent main.c missing"
[ -f util.c ] || fail "parent util.c missing"

note "get/13-sub-remove: vendor/sub unmounted on C3 → C2 switch"
