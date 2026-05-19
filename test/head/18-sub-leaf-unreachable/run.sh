#!/bin/sh
#  head/18-sub-leaf-unreachable — `be get` on a 3-level forest
#  whose deepest project URL is unreachable.  Validates the plan's
#  compiler-style aggregation: outer + sub mount cleanly, the
#  unreachable leaf is reported, the top-level exit code is non-zero.
#
#  Reachability is broken by deleting the leaf bare repo AFTER
#  seeding but BEFORE the test's `be get`.  ssh-side
#  `git-upload-pack` returns an error → keeper's WIRE fetch fails →
#  `sniff sub-mount` for the leaf fails → beagle aggregates worst
#  exit; outer + sub stay intact.

. "$(dirname "$0")/../../lib/sub-deep.sh"

#  Break the leaf URL: remove the bare repo so ssh's upload-pack
#  can't find it.  Leaving the URL in sub's .gitmodules — the test
#  is about FETCH-time unreachability, not declaration absence.
rm -rf "$LEAF_BARE"
[ ! -e "$LEAF_BARE" ] || fail "leaf bare did not get removed"

mkdir wt && cd wt

#  `be get parent` — outer + sub should succeed, leaf should fail.
#  set -e is on; the intentional non-zero exit needs a `||` capture.
rc=0
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err || rc=$?
#  Top-level non-zero from the failed leaf propagating up.
[ "$rc" != 0 ] || fail "expected non-zero exit; got $rc; stderr:
$(cat 01.get.got.err)"

#  Parent + sub mounted normally.
[ -f main.c ]                       || fail "parent main.c missing"
[ -f vendor/sub/sub.txt ]           || fail "sub.txt missing"
[ -f vendor/sub/.be ]               || fail "sub anchor missing"

#  Leaf did NOT mount — anchor absent, directory either missing or
#  empty (depending on whether sniff's mkdir step ran before the
#  fetch failed and rolled back).
[ ! -f vendor/sub/vendor/leaf/.be ] \
    || fail "leaf should not be mounted; anchor unexpectedly present"
[ ! -f vendor/sub/vendor/leaf/leaf.txt ] \
    || fail "leaf should not be mounted; leaf.txt unexpectedly present"

#  Markers fired at outer + sub level (compiler-style: the leaf
#  failure does NOT prevent outer/sub from reporting).
grep -q '^be: get [.]'         01.get.got.err \
    || fail "outer marker missing; stderr:
$(cat 01.get.got.err)"
grep -q '^be: get vendor/sub'  01.get.got.err \
    || fail "sub marker missing"

#  Some indication of the leaf failure on stderr (form is best-effort —
#  the exact error chain depends on which fetch primitive trips first).
grep -qiE 'fetch|leaf|fail|error' 01.get.got.err \
    || fail "no leaf-failure indication on stderr"

note "head/18-sub-leaf-unreachable: parent+sub mounted; leaf reported, exit $rc"
