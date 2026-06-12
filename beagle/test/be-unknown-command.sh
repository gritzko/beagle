#!/bin/sh
#  be-unknown-command.sh — BE-002.  A mistyped verb/projector must NOT
#  be silently handed to bro (which printed a contextless `cannot open
#  <word>: FILENONE` and exited 0).  `be` owns the diagnostic: a "did
#  you mean" line for a near miss, a generic line for far-off garbage,
#  and ALWAYS a non-zero exit.  Real files / projectors / slashed
#  misses are regression-guarded to still behave as before.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

vc_step "setup: a trunk with a real README.md to page"
vc_fresh_wt
sp_seed_trunk
echo "real file body" > README.md
"$BE" put README.md >/dev/null
"$BE" post 'add readme' >/dev/null

# --- mistyped verb/projector → did-you-mean + non-zero exit ----------

vc_step "be difff → did you mean 'diff', non-zero exit"
vc_run difff "$BE" difff
vc_assert_exit nonzero
vc_assert_stderr difff "did you mean 'diff'"

vc_step "be psot → did you mean 'post'"
vc_run psot "$BE" psot
vc_assert_exit nonzero
vc_assert_stderr psot "did you mean 'post'"

vc_step "be stauts → did you mean 'status'"
vc_run stauts "$BE" stauts
vc_assert_exit nonzero
vc_assert_stderr stauts "did you mean 'status'"

# --- far-off garbage → generic line, no bogus suggestion -------------

vc_step "be totallynotacommand → no such file or beagle command"
vc_run garbage "$BE" totallynotacommand
vc_assert_exit nonzero
vc_assert_stderr garbage "no such file or beagle command"
#  Must NOT fabricate a suggestion for unrelated garbage.
if grep -qF "did you mean" "$TMP/stderr.garbage"; then
    vc_fail "garbage produced a bogus 'did you mean' suggestion"
fi

# --- regression guards: real targets still reach bro unchanged -------

vc_step "be README.md (real file) still pages, exit 0"
vc_run readme "$BE" README.md
vc_assert_exit 0
#  The file body must appear in stdout (plain HUNK render, non-tty).
grep -qF "real file body" "$TMP/stdout.readme" \
    || vc_fail "be README.md did not page the file body"

vc_step "be status (real projector) still runs, exit 0"
vc_run status "$BE" status
vc_assert_exit 0

vc_step "be diff: (real projector scheme) still runs, exit 0"
vc_run diffp "$BE" "diff:"
vc_assert_exit 0

vc_step "be src/missing.c (slashed miss) → generic line, no suggestion"
vc_run slashmiss "$BE" src/missing.c
vc_assert_exit nonzero
#  A slashed missing path is a real missing FILE, not a command typo —
#  it must NOT carry a 'did you mean' suggestion.
if grep -qF "did you mean" "$TMP/stderr.slashmiss"; then
    vc_fail "slashed miss produced a bogus 'did you mean' suggestion"
fi

echo "=== be-unknown-command: OK ==="
