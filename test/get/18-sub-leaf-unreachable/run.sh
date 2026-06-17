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
#  abort the script.  Run in --tlv so the relay (not stderr markers,
#  now ABC_TRACE-gated) proves outer + sub recursion happened.
rc=0
"$BE" get "$PARENT_URL?master" --tlv >01.get.got.out 2>01.get.got.err || rc=$?
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

#  Recursion fired at outer + sub level despite the leaf failure
#  (compiler-style aggregation, not short-circuit).  Proven via the
#  --tlv relay (GET-026): the parent emits its own rows (e.g. `main.c`),
#  THEN relays the sub with rows rebased under the mount
#  (`vendor/sub?<hashlet>`); pre-order ⇒ parent row precedes the first
#  `vendor/sub` row.  TLV is NUL-laden binary; split on NUL into lines
#  so plain `grep -n` yields stream order.
LC_ALL=C tr '\0' '\n' < 01.get.got.out > 01.tlv.lines
LC_ALL=C grep -aq 'main\.c'    01.tlv.lines \
    || fail "no parent 'main.c' row in --tlv relay:
$(cat -v 01.tlv.lines | head -c 400)"
LC_ALL=C grep -aq 'vendor/sub' 01.tlv.lines \
    || fail "no relayed 'vendor/sub' sub hunk in --tlv stream:
$(cat -v 01.tlv.lines | head -c 400)"
parent_ln=$(LC_ALL=C grep -anE 'main\.c'    01.tlv.lines | head -1 | cut -d: -f1)
sub_ln=$(   LC_ALL=C grep -anE 'vendor/sub' 01.tlv.lines | head -1 | cut -d: -f1)
[ -n "$parent_ln" ] && [ -n "$sub_ln" ] && [ "$parent_ln" -lt "$sub_ln" ] \
    || fail "expected parent row (ln=$parent_ln) before sub hunk (ln=$sub_ln) in --tlv relay"

#  Leaf failure surfaces in stderr.
grep -qiE 'fetch|leaf|fail|error' 01.get.got.err \
    || fail "no leaf-failure indication on stderr"

note "get/18-sub-leaf-unreachable: parent+sub mounted, leaf reported, exit $rc"
