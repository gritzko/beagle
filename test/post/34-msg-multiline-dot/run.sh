#!/bin/sh
#  34-msg-multiline-dot — POST-002: `be post '#<multi-line dotted body>'`
#  commits the body VERBATIM and is NOT refused as a path-form URI.
#
#  The bug: a `#`-led arg was classified `starts_uri` and fed to the
#  RFC-strict URILexer, whose fragment rule (`*( pchar / "/" / "?" )`)
#  stops at the first newline.  So a multi-line message's tail — which
#  has no `#` and contains a `.` (e.g. a `Co-Authored-By:` trailer with
#  an `a@b.c` address) — spilled into `u->path`, and POST then refused
#  it: "path-form URI ... not allowed".  Only the legacy `-m` flag
#  worked.  The fix routes a `#`-led arg's body straight into the
#  fragment slot verbatim, pre-parse (DOGNormalizeArg), grammar
#  untouched.
#
#  Asserts:
#    1. `be post '#a\n\nCo-Authored-By: X <a@b.c>'` is NOT refused.
#    2. The committed message is the body verbatim (subject + trailer).
#    3. Legacy `-m '<same body>'` commits the same message.

. "$(dirname "$0")/../../lib/branches.sh"

#  The repro body: subject + blank line + dotted Co-Authored-By trailer.
SUBJ='add multiline support'
TRAILER='Co-Authored-By: X <a@b.c>'
MSG="$SUBJ

$TRAILER"

# --- 1. `be post '#<multiline dotted>'` must commit, not refuse -------
cp "$CASE/01.greet.txt" greet.txt
"$BE" put greet.txt >/dev/null

"$BE" post "#$MSG" >"$ETMP/p1.out" 2>"$ETMP/p1.err" \
    || fail "be post '#<multiline>' refused; err: $(cat $ETMP/p1.err)"

#  Belt-and-suspenders: the path-form refusal must NOT appear.
grep -qi 'path-form URI' "$ETMP/p1.err" \
    && fail "post wrongly refused multiline msg as path-form: $(cat $ETMP/p1.err)"

H1=$(head_hex)
[ -n "$H1" ] || fail "be post '#<multiline>' did not create a commit"

"$KEEPER" get ".#$H1" >"$ETMP/c1.out" 2>"$ETMP/c1.err" \
    || fail "keeper get .#$H1 failed: $(cat $ETMP/c1.err)"

#  The body sits after the first blank line (header / body split).
#  Subject is the first body line; the dotted trailer must appear
#  verbatim somewhere in the body.
SUBJECT=$(awk '/^$/{p=1; next} p { print; exit }' "$ETMP/c1.out")
[ "$SUBJECT" = "$SUBJ" ] \
    || fail "subject should be '$SUBJ', got '$SUBJECT'; commit: $(cat $ETMP/c1.out)"
grep -qF "$TRAILER" "$ETMP/c1.out" \
    || fail "Co-Authored-By trailer missing from commit; commit: $(cat $ETMP/c1.out)"

note "post '#<multiline dotted>' committed verbatim (not path-form), tip=$H1"

# --- 2. legacy `-m '<same body>'` commits the same message -----------
#  Make a fresh change so the second post has something to commit.
printf 'hello again\n' >>greet.txt
"$BE" put greet.txt >/dev/null

"$BE" post -m "$MSG" >"$ETMP/p2.out" 2>"$ETMP/p2.err" \
    || fail "be post -m '<multiline>' refused; err: $(cat $ETMP/p2.err)"

H2=$(head_hex)
[ -n "$H2" ] && [ "$H2" != "$H1" ] \
    || fail "be post -m '<multiline>' did not create a new commit (H2=$H2)"

"$KEEPER" get ".#$H2" >"$ETMP/c2.out" 2>"$ETMP/c2.err" \
    || fail "keeper get .#$H2 failed: $(cat $ETMP/c2.err)"

SUBJECT2=$(awk '/^$/{p=1; next} p { print; exit }' "$ETMP/c2.out")
[ "$SUBJECT2" = "$SUBJ" ] \
    || fail "(-m) subject should be '$SUBJ', got '$SUBJECT2'"
grep -qF "$TRAILER" "$ETMP/c2.out" \
    || fail "(-m) Co-Authored-By trailer missing; commit: $(cat $ETMP/c2.out)"

note "legacy -m commits the same body, tip=$H2"
echo "=== post/34-msg-multiline-dot: OK ==="
