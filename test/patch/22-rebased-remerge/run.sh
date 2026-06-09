#!/bin/sh
#  22-rebased-remerge — content absorbed *without a reachability edge*
#  (cherry-pick's `picked` trailer, or a foster the merge's ancestor-
#  skip can't see) and then re-encountered through a MERGE produces a
#  SPURIOUS conflict.
#
#  Root cause: the absorbing commit re-inserts the picked commit's
#  tokens under its OWN WEAVE birth-id `(seq,pos)`.  `picked` trailers
#  are dedup-only and do NOT participate in reachability (https://replicated.wiki/html/wiki/Verbs.html
#  §PATCH "Ancestor-skip walk"), so the merge does NOT skip the
#  original commit — it replays it, and WEAVE sees the same tokens with
#  two different birth-ids (ours = the absorbing commit, theirs = the
#  original) → frames them as concurrent inserts → false conflict, even
#  when both sides are byte-identical (cf. graf/NEIL.c
#  `<<<<a_carve(u32||||a_carve(u32>>>>`).  This is the same identity-
#  re-stamp that makes a rebased (foster) remote un-FF-pushable.
#
#  Topology:
#       T0 ── P1(picked C1)        <- cur (trunk)
#         \
#          C1 ── C2                <- ?feat   (C1 edits alpha, C2 beta)
#
#  ours(P1) and theirs(C2) BOTH carry C1's alpha edit (identical
#  bytes); only C2's beta edit is new.  The merge must be CLEAN:
#  result == C2.
#
#  WILL_FAIL until re-absorbed content keeps an identity WEAVE can
#  match (or the merge falls back to byte-equality when birth-ids
#  differ but tokens match).

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.f.t0.c" f.c
"$BE" put f.c >/dev/null; "$BE" post 't0' >/dev/null
T0=$(head_hex)

# feat: C1 (edit alpha), C2 (edit beta on top of C1)
"$BE" put '?./feat' >/dev/null
"$BE" get '?feat'   >/dev/null
sleep 0.02; cp "$CASE/02.f.c1.c" f.c
"$BE" put f.c >/dev/null; "$BE" post 'c1 alpha' >/dev/null
C1=$(head_hex)
sleep 0.02; cp "$CASE/03.f.c2.c" f.c
"$BE" put f.c >/dev/null; "$BE" post 'c2 beta' >/dev/null
C2=$(head_hex)

# back to trunk at T0
"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T0" ] || fail "should be at T0, got $(head_hex)"

# Step 1: cherry-pick C1 onto trunk (picked trailer, NO reachability
# edge), then POST.  Now trunk carries C1's alpha edit under P1's
# birth-id, but C1 is not an ancestor of cur.
sleep 0.02
"$BE" patch "#$C1" >"$ETMP/p1.out" 2>"$ETMP/p1.err" \
    || fail "cherry-pick C1 failed: $(cat $ETMP/p1.err)"
"$BE" post >/dev/null 2>"$ETMP/p1post.err" \
    || fail "post after cherry-pick failed: $(cat $ETMP/p1post.err)"
P1=$(head_hex)
match "$CASE/02.f.c1.c" f.c        # wt now carries C1's alpha edit

# Step 2: merge ?feat (tip C2) into trunk.  alpha() is identical on
# both sides; C1 is not reachability-deduped, so the merge re-weaves
# it — must NOT conflict, only C2's beta() edit applies.
sleep 0.02
rc=0
"$BE" patch '?feat!' >"$ETMP/m.out" 2>"$ETMP/m.err" || rc=$?

# (1) no conflict markers may appear in f.c (alpha is the same bytes
#     on both sides — there is nothing to conflict over).
if grep -q '<<<<' f.c; then
    echo "FAIL: spurious conflict markers in f.c after re-merge:" >&2
    cat f.c >&2
    echo "--- patch stderr: ---" >&2; cat "$ETMP/m.err" >&2
    exit 1
fi
# (2) the patch must not report a content-conflict.
grep -q 'content-conflict=0' "$ETMP/m.err" \
    || { echo "FAIL: merge reported a (false) content-conflict:" >&2
         cat "$ETMP/m.err" >&2; exit 1; }
# (3) clean result == C2 (alpha=111 from both, beta=222 from theirs).
match "$CASE/03.f.c2.c" f.c
