#!/bin/sh
#  be-post-path-reject.sh — POST refuses path-form URIs per VERBS.md
#  §POST.  POST takes only branch URIs (`?<branch>`) or is bare; paths
#  must go through `be put` first.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: trunk + one commit; dirty x.txt"
vc_fresh_wt
sp_seed_trunk
echo "edited" > x.txt

vc_snapshot before

vc_step "be post ./x.txt msg — should be refused (explicit path-form)"
#  Bareword `x.txt` is a fragment under VERBS.md §"Bareword
#  defaults" — POST's bareword default is fragment (commit msg).
#  The path-reject rule fires only for an EXPLICIT path-form URI
#  (`./x.txt`, `/x.txt`, or anything with a `/`).
if "$BE" post ./x.txt msg 2>$TMP/post-path.err; then
    cat $TMP/post-path.err
    vc_fail "be post path-form should have been refused"
fi
grep -q "path-form URI" $TMP/post-path.err \
    || vc_fail "expected path-form refusal; got: $(cat $TMP/post-path.err)"

vc_snapshot after

#  Refused POST must leave .be/wtlog and refs untouched.
vc_assert_unchanged sniff
vc_assert_unchanged refs

vc_note "path-form POST refused; wt state intact"
echo "=== be-post-path-reject: OK ==="
