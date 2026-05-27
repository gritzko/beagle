#!/bin/sh
#  get/18-sub-leaf-unreachable — `be get` on a 3-level forest whose
#  leaf URL is unreachable.  Identical scenario to head/18 but on
#  the GET path: the depth-2 sub MUST mount cleanly even though
#  the deeper recursion fails on the leaf.  Beagle's split-mount
#  design (sniff sub-mount does anchor+fetch+checkout only, beagle
#  recurses separately) is what makes this true.

. "$(dirname "$0")/../../lib/sub-deep.sh"

#  Break the leaf URL: ssh-side upload-pack will report "not a git
#  repository" → WIRE fetch fails → `sniff sub-mount ./vendor/leaf#…`
#  fails → beagle aggregates worst exit; outer + sub stay intact.
rm -rf "$LEAF_BARE"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

#  Intentional non-zero exit; capture with `||` so `set -e` doesn't
#  abort the script.
rc=0
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err || rc=$?
[ "$rc" != 0 ] || fail "expected non-zero exit; got $rc; stderr:
$(cat 01.get.got.err)"

#  Parent + sub fully mounted (their fetches were fine).
[ -f main.c ]                       || fail "parent main.c missing"
[ -f vendor/sub/sub.txt ]           || fail "sub.txt missing"
[ -f vendor/sub/.be ]               || fail "sub anchor missing"
[ ! -d vendor/sub/.be ]             || fail "sub .be should be a file"

#  Leaf NOT mounted — anchor absent, payload absent.
[ ! -f vendor/sub/vendor/leaf/.be ] \
    || fail "leaf anchor unexpectedly present"
[ ! -f vendor/sub/vendor/leaf/leaf.txt ] \
    || fail "leaf payload unexpectedly present"

#  Markers fired at outer + sub level despite the leaf failure
#  (compiler-style aggregation, not short-circuit).
grep -q '^be: get [.]'         01.get.got.err \
    || fail "outer marker missing; stderr:
$(cat 01.get.got.err)"
grep -q '^be: get vendor/sub'  01.get.got.err \
    || fail "sub marker missing"

#  Leaf failure surfaces in stderr.
grep -qiE 'fetch|leaf|fail|error' 01.get.got.err \
    || fail "no leaf-failure indication on stderr"

note "get/18-sub-leaf-unreachable: parent+sub mounted, leaf reported, exit $rc"
