#!/bin/sh
#  23-foster-remerge — a commit absorbed via rebase-one (FOSTER) and
#  re-encountered through a merge whose tip EXTENDS the foster'd
#  content must merge clean.  Unlike cherry-pick (22-rebased-remerge),
#  foster participates in reachability, so:
#    * the merge's ancestor-skip skips the foster'd commit, and
#    * the apply-closure (parent|foster) carries the foster'd tokens
#      with their ORIGINAL birth-ids,
#  so theirs's extending edit anchors cleanly on the foster'd tokens.
#  This guards the foster identity path (FOSTER.plan.md #2 merge-half /
#  #3): theirs EXTENDS C1's content, so a re-stamp would conflict in a
#  way the #1 byte-equality collapse could NOT hide.
#
#  Topology:
#       T0 ── P1(foster C1)        <- cur (trunk)
#         \
#          C1 ── C2                <- ?feat
#    C1: f() returns 100.  C2: extends to `return 100 + 7;`.
#
#  rebase-one C1 -> P1 fosters C1; merge ?feat (tip C2) must yield
#  `return 100 + 7;` with no conflict.

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.f.t0.c" f.c
"$BE" put f.c >/dev/null; "$BE" post 't0' >/dev/null
T0=$(head_hex)

# feat: C1 (return 100), C2 (extends to 100 + 7)
"$BE" put '?./feat' >/dev/null
"$BE" get '?feat'   >/dev/null
sleep 0.02; cp "$CASE/02.f.c1.c" f.c
"$BE" put f.c >/dev/null; "$BE" post 'c1 100' >/dev/null
sleep 0.02; cp "$CASE/03.f.c2.c" f.c
"$BE" put f.c >/dev/null; "$BE" post 'c2 extend' >/dev/null

# back to trunk at T0
"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T0" ] || fail "should be at T0, got $(head_hex)"

# rebase-one C1 onto trunk (foster), then POST.
sleep 0.02
"$BE" patch '?feat#' >"$ETMP/p1.out" 2>"$ETMP/p1.err" \
    || fail "rebase-one failed: $(cat $ETMP/p1.err)"
"$BE" post >/dev/null 2>"$ETMP/p1post.err" \
    || fail "post after rebase-one failed: $(cat $ETMP/p1post.err)"
match "$CASE/02.f.c1.c" f.c        # wt carries C1's `return 100;`

# merge ?feat (tip C2) — theirs extends C1's tokens; must merge clean.
sleep 0.02
rc=0
"$BE" patch '?feat#merge feat' >"$ETMP/m.out" 2>"$ETMP/m.err" || rc=$?

if grep -q '<<<<' f.c; then
    echo "FAIL: foster'd content re-stamped -> conflict on extend:" >&2
    cat f.c >&2; echo "--- stderr ---" >&2; cat "$ETMP/m.err" >&2
    exit 1
fi
grep -q 'content-conflict=0' "$ETMP/m.err" \
    || { echo "FAIL: merge reported a content-conflict:" >&2; cat "$ETMP/m.err" >&2; exit 1; }
match "$CASE/03.f.c2.c" f.c        # clean result == C2 (return 100 + 7)
