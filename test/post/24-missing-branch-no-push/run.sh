#!/bin/sh
#  post/24-missing-branch-no-push — `be post //host?<branch>` where the
#  target branch does NOT exist locally must ABORT before any wire push.
#
#  DIS-020: `POSTNONE` was overloaded — returned both for the benign
#  "nothing to post / in sync" case (which `be` treats as no-op-OK and
#  continues past) AND for the hard error "target branch does not exist"
#  (sniff/POST.c POSTPromote).  Because `be` swallowed `POSTNONE` as a
#  no-op (its low byte == NONE's), the POST plan kept going to
#  BEActKeeperPush and attempted a doomed wire push (WIRECLNFF / unknown
#  remote).  The fix introduces a distinct `NOBRANCH` ok64 for the
#  missing-branch case; its low byte differs from NONE so `be`'s plan
#  runner aborts on it (BEDOGEXIT) and never reaches the push.
#
#  Local-only repro (no ssh needed): a fresh wt with one commit, then
#  `be post //localhost?nonexistent`.  The `//localhost` authority is
#  what would normally trigger BEActKeeperPush; the `?nonexistent`
#  target has no local branch.
#
#  Asserts:
#    * `be post //localhost?nonexistent` exits NON-ZERO.
#    * sniff reports the missing branch (`NOBRANCH` / "does not exist").
#    * NO wire push was attempted — the keeper stage never ran, so its
#      `keeper: unknown remote ...` line is ABSENT from stderr.

. "$(dirname "$0")/../../lib/case.sh"

#  Seed one commit so the wt has a real cur tip (POSTPromote resolves
#  cur before classifying the missing target).
sleep 0.02; echo "hello" > a.txt
"$BE" put a.txt > 01.put.got.out 2> 01.put.got.err
"$BE" post '#first' > 02.post.got.out 2> 02.post.got.err
#  POST-018: commit confirmation is a ROWS `post ?<hashlet>#<subj>` row.
grep -qE 'post[[:space:]]+\??[0-9a-f]{6,}#first' 02.post.got.err || {
    echo "post/24: seed commit did not land:" >&2
    cat 02.post.got.err >&2
    exit 1
}

#  The check: POST onto a non-existent branch with a transport
#  authority.  Bypass `mustnt` so we keep stderr for grepping.  Must
#  exit non-zero AND must NOT have reached the keeper push stage.
if "$BE" post '//localhost?nonexistent' \
        > 03.post.got.out 2> 03.post.got.err; then
    echo "post/24: be post //localhost?nonexistent unexpectedly succeeded" >&2
    cat 03.post.got.err >&2
    exit 1
fi

#  sniff must have surfaced the missing branch as the hard NOBRANCH
#  code, not the no-op NONE class.
grep -q 'does not exist' 03.post.got.err || {
    echo "post/24: expected 'does not exist' in stderr, got:" >&2
    cat 03.post.got.err >&2
    exit 1
}
grep -q 'NOBRANCH' 03.post.got.err || {
    echo "post/24: expected 'NOBRANCH' error in stderr, got:" >&2
    cat 03.post.got.err >&2
    exit 1
}

#  The plan must have aborted at the sniff stage — BEActKeeperPush
#  (the wire push) must NEVER have fired.  With the bug, POSTNONE was
#  swallowed and keeper ran, printing `keeper: unknown remote //...`.
if grep -q '^keeper:' 03.post.got.err; then
    echo "post/24: keeper push fired after missing-branch abort (bug):" >&2
    cat 03.post.got.err >&2
    exit 1
fi

echo "post/24: missing-branch POST aborted with NOBRANCH, no wire push"
