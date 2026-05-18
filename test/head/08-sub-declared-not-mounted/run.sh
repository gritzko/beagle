#!/bin/sh
#  head/08-sub-declared-not-mounted — `.gitmodules` lists a sub the
#  user has removed from disk (unmount-by-rm-anchor).  HEAD must not
#  refuse — it reports "declared but not mounted" for that path and
#  proceeds.  Mirrors the spec's compiler-style aggregation: HEAD
#  never stops.
#
#  Setup:
#    1. Clone fixture (parent + sub mounted).
#    2. Remove the sub's secondary-wt anchor + (optionally) its
#       checked-out files, leaving `.gitmodules` intact in the wt.
#       Now `vendor/sub/.be` is gone; SubIsMount(parent_root,
#       "vendor/sub") returns NO; but the tree still declares it.
#    3. `be head ?` runs; reports the unmounted-but-declared path
#       without recursing into it (nothing to recurse into).
#
#  Output contract: one stderr line of the form
#      be: head vendor/sub: declared, not mounted
#  (any equivalent wording is fine as long as it carries the path
#  and the "not mounted" / "would mount" marker).
#
#  Status: WILL_FAIL until BEHead recursion lands.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt && cd wt
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
[ -f vendor/sub/.be ] || fail "fixture: sub anchor missing"
[ -f .gitmodules ]    || fail "fixture: parent .gitmodules missing"

# --- Unmount: remove anchor + sub wt; .gitmodules stays. -------------
rm -f  vendor/sub/.be
rm -rf vendor/sub
[ ! -e vendor/sub ] || fail "sub mount not fully removed"
grep -q 'vendor/sub' .gitmodules \
    || fail "fixture broken: .gitmodules lost vendor/sub entry"

# --- `be head ?` — reports declared-not-mounted, exits 0. ------------
"$BE" head '?' >02.head.got.out 2>02.head.got.err
rc=$?
[ "$rc" = 0 ] || fail "be head exited $rc; stderr:
$(cat 02.head.got.err)"

# Outer still reports.
grep -q '^be: head [.]' 02.head.got.err || fail "outer marker missing"

# Sub is declared-not-mounted: marker carries the path + a hint.
grep -Eq 'vendor/sub.*(not mounted|would mount|declared)' 02.head.got.err \
    || fail "expected 'vendor/sub' + not-mounted hint on stderr:
$(cat 02.head.got.err)"

# No actual recursion into the (absent) mount.
grep -q '^be: head vendor/sub$' 02.head.got.err \
    && fail "head should not recurse into an unmounted sub; stderr:
$(cat 02.head.got.err)"

note "head/08-sub-declared-not-mounted: reported, continued"
