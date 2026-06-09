#!/bin/sh
#  post/36-noop-promote-quiet — POST-012 repro.
#
#  On a no-op label promote (`be post ?<branch>` where <branch> already
#  contains cur), POSTPromote takes the "nothing to promote" early-out
#  and used to ALWAYS print
#      sniff: post: nothing to promote (?<branch> already contains cur)
#  even in the submodule-recursion contexts, where the parent's
#  `0 change(s)` summary already conveys the no-op and the per-sub line
#  reads like an error (POST-012 surfaced as "what does this mean?").
#
#  The two recursion contexts the parent `be post` drives a sub through
#  (beagle/BE.cli.c):
#    * local-commit recursion  — child spawned with `-q`  (SNIFF.quiet)
#    * beagle push relay        — child forced `--tlv`     (HUNKOutTLV)
#  Both must stay quiet on a no-op sub (report a sub ONLY when it
#  promotes a real change — the verb report-channels rule).  A DIRECT
#  interactive `be post ?branch` no-op still prints the line (useful
#  feedback for a hand-typed command).
#
#  This case exercises POSTPromote directly via the two recursion-mode
#  flags, so it is hermetic and ssh-free:
#    1. NO-OP promote: assert the line is PRINTED interactively (plain),
#       SILENT under `-q` and under `--tlv`.
#    2. POSITIVE (don't over-suppress): a REAL promote that advances the
#       target still reports `? -> <sha>` under `-q` and the no-op line
#       never appears.

. "$(dirname "$0")/../../lib/branches.sh"

# --- setup: trunk with one commit, plus a sibling label ?feat at cur ---
echo "x v1" > x.txt
"$BE" post 'v1 msg' >/dev/null || fail "seed post failed"
"$BE" put "?feat"   >/dev/null || fail "be put ?feat failed"

#  feat now sits at cur's tip — promoting it is a no-op that hits the
#  POSTPromote "already contains cur" branch.

# --- 1a. interactive (plain) no-op promote: line MUST print -----------
"$BE" post "?feat" >n_plain.out 2>n_plain.err \
    || fail "interactive no-op promote exited nonzero"
grep -q "nothing to promote (?feat already contains cur)" n_plain.err \
    || fail "interactive no-op should print the 'already contains cur' line; stderr:
$(cat n_plain.err)"
note "1a OK: interactive no-op promote prints the line"

# --- 1b. local-commit recursion (`-q`): line MUST be silent -----------
"$BE" post -q "?feat" >n_q.out 2>n_q.err \
    || fail "`-q` no-op promote exited nonzero"
if grep -q "already contains cur" n_q.err; then
    fail "POST-012: '-q' no-op promote must NOT print 'already contains cur'; stderr:
$(cat n_q.err)"
fi
note "1b OK: -q (local-commit recursion) no-op promote is silent"

# --- 1c. push relay (`--tlv`): line MUST be silent --------------------
"$BE" post --tlv "?feat" >n_tlv.out 2>n_tlv.err \
    || fail "`--tlv` no-op promote exited nonzero"
if grep -q "already contains cur" n_tlv.err; then
    fail "POST-012: '--tlv' no-op promote must NOT print 'already contains cur'; stderr:
$(cat n_tlv.err)"
fi
#  The no-op line must not leak onto the captured TLV stdout stream either.
if grep -q "already contains cur" n_tlv.out; then
    fail "POST-012: '--tlv' no-op promote leaked the line onto stdout; stdout:
$(cat n_tlv.out)"
fi
note "1c OK: --tlv (push relay) no-op promote is silent"

# --- 2. POSITIVE: a REAL promote still reports under recursion --------
#  Put cur on a child ?fix1, commit, then `be post ?..` FF-advances the
#  parent (trunk) to cur's tip — a genuine promote, NOT a no-op.
TRUNK_PRE=$(ref_tip "?")
"$BE" put "?./fix1" >/dev/null || fail "be put ?./fix1 failed"
"$BE" get "?fix1"   >/dev/null || fail "be get ?fix1 failed"
sleep 0.01
echo "f1 v1" > f1.txt
"$BE" put f1.txt >/dev/null || fail "be put f1.txt failed"
"$BE" post 'fix1 c1' >/dev/null || fail "be post fix1 c1 failed"
FIX_TIP=$(ref_tip "?fix1")
[ -n "$FIX_TIP" ] && [ "$FIX_TIP" != "$TRUNK_PRE" ] \
    || fail "?fix1 didn't advance"

"$BE" post -q "?.." >promote.out 2>promote.err \
    || fail "real promote ?.. exited nonzero; stderr:
$(cat promote.err)"
#  Real promote MUST still report (don't over-suppress).
grep -q "? -> $FIX_TIP" promote.err \
    || fail "real promote under -q should still report '? -> <sha>'; stderr:
$(cat promote.err)"
#  …and it must NOT be the suppressed no-op line.
if grep -q "already contains cur" promote.err; then
    fail "real promote misclassified as no-op; stderr:
$(cat promote.err)"
fi
TRUNK_POST=$(ref_tip "?")
[ "$TRUNK_POST" = "$FIX_TIP" ] \
    || fail "real promote did not advance trunk ($TRUNK_PRE -> $TRUNK_POST, want $FIX_TIP)"
note "2 OK: real promote advances trunk and still reports under -q"

note "post/36-noop-promote-quiet: no-op promote silent under -q/--tlv, loud interactively; real promote still reports"
