#!/bin/sh
#  head/07-sub-fetch-recursive — `be head ssh://parent.git?master`
#  on a pre-mounted 3-level forest triggers a remote-ref fetch at
#  EVERY level (parent + sub + leaf), each using its own URL.
#
#  Setup:
#    1. sub-deep.sh seeds parent + sub + leaf bare repos.
#    2. `be get $PARENT_URL?master` mounts all three locally.
#    3. `be head ssh://...parent.git?master` — should fetch each
#       project's remote in turn.
#
#  Test contract:
#    * exit 0.
#    * `keeper: fetched N ref(s)` appears at least three times
#      (one per project — parent, sub, leaf).
#    * `be: head .` outer marker present (lazy emit on first sub).
#    * Recursion markers for both vendor/sub and vendor/leaf.

. "$(dirname "$0")/../../lib/sub-deep.sh"

mkdir wt && cd wt

# --- Seed checkout (mounts all three levels). ------------------------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "seed be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f vendor/sub/vendor/leaf/leaf.txt ] \
    || fail "seed: 3-level mount didn't materialise"

# --- be head ssh://...parent.git?master — fetch every level's refs. -
"$BE" head "$PARENT_URL?master" >02.head.got.out 2>02.head.got.err
rc=$?
[ "$rc" = 0 ] || fail "be head exited $rc; stderr:
$(cat 02.head.got.err)"

# Outer + sub + leaf markers (proves recursion fired all the way down).
grep -q '^be: head [.]'         02.head.got.err \
    || fail "outer marker missing; stderr:
$(cat 02.head.got.err)"
grep -q '^be: head vendor/sub'  02.head.got.err \
    || fail "sub marker missing"
grep -q '^be: head vendor/leaf' 02.head.got.err \
    || fail "leaf marker missing — recursion didn't reach 3rd level"

# At least three fetch attempts — one per project.  WIREDBG's
# `match_advert: picked sha=…` is emitted once per upload-pack
# session; `Total N` is the git-side count of objects transferred.
# Either is a stable per-fetch marker.
nfetch=$(grep -c 'match_advert' 02.head.got.err)
[ "$nfetch" -ge 3 ] \
    || fail "expected >=3 fetch sessions, got $nfetch; stderr:
$(cat 02.head.got.err)"

note "head/07-sub-fetch-recursive: $nfetch fetches across 3 projects"
