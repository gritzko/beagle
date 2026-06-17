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

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

#  `be get parent` — outer + sub should succeed, leaf should fail.
#  set -e is on; the intentional non-zero exit needs a `||` capture.
#  Run in --tlv so the relay (not stderr markers, now ABC_TRACE-gated)
#  proves outer + sub recursion happened.
rc=0
"$BE" get "$PARENT_URL?master" --tlv >01.get.got.out 2>01.get.got.err || rc=$?
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

#  Recursion fired at outer + sub level (compiler-style: the leaf
#  failure does NOT prevent outer/sub from reporting).  Proven via the
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

#  Some indication of the leaf failure on stderr (form is best-effort —
#  the exact error chain depends on which fetch primitive trips first).
grep -qiE 'fetch|leaf|fail|error' 01.get.got.err \
    || fail "no leaf-failure indication on stderr"

note "head/18-sub-leaf-unreachable: parent+sub mounted; leaf reported, exit $rc"
