#!/bin/sh
#  post/21-foster-ff-push — a foster-rebased tip must FF-push (be<->be).
#
#  `be patch ssh://origin?<br>#` (rebase-one) records the absorbed
#  remote commit as a `foster`, not a `parent`.  The push FF gate
#  `KEEPIsAncestor` must follow foster edges, else `be post //origin`
#  wrongly refuses a freshly-rebased tip as non-fast-forward — which
#  contradicts https://replicated.wiki/html/wiki/Invariants.html Invariant 9 ("divergence is resolved
#  client-side with PATCH + POST").  See FOSTER.plan.md (FF-push
#  companion).  (Server is keeper receive-pack, which CAS-es on
#  old_sha; a git peer would still refuse — foster is be-only.)
#
#  Topology:
#       A ── L1            <- cur (local, edits a())
#        \
#         B                <- origin/master (edits b())
#  rebase-one B onto local -> P (fosters B); P descends from B via the
#  foster edge, so `be post //localhost` must fast-forward origin to P.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "post/21: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in "$HOME"/*) ;; *) echo "post/21: SCRATCH not under HOME" >&2; exit 1;; esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"; SEED="$SCRATCH/seed"; REL_ORIGIN="$REL_SCRATCH/origin.git"
cd "$SCRATCH"

# 1. seed origin with A on master
git init --bare "$ORIGIN" >/dev/null
git init "$SEED" >/dev/null
git -C "$SEED" config user.email t@t; git -C "$SEED" config user.name T
git -C "$SEED" checkout -b master >/dev/null 2>&1 || true
cp "$CASE/01.A.c" "$SEED/f.c"; git -C "$SEED" add .; git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# 2. clone into wt via ssh (cur at A)
mkdir wt wt/.be && cd wt
"$BE" get "ssh://localhost/$REL_ORIGIN?master" >01.clone.out 2>01.clone.err
match "$CASE/01.A.c" f.c

# 3. local commit L1 (edits a())
sleep 0.02; cp "$CASE/02.L1.c" f.c
"$BE" post 'L1 edits a' >02.l1.out 2>02.l1.err

# 4. origin advances to B (edits b()) — a separate commit on top of A
cd ..
sleep 0.02; cp "$CASE/03.B.c" "$SEED/f.c"; git -C "$SEED" add .
git -C "$SEED" commit -qm B; git -C "$SEED" push -q "$ORIGIN" master:master
BSHA=$(git -C "$ORIGIN" rev-parse master)
cd wt

# 5. rebase-one B onto local -> P fosters B (disjoint edits merge clean).
#    DIS-030/031: next-one = bare `?master`; `#!` = reuse msg + forget.
sleep 0.02
"$BE" patch "ssh://localhost/$REL_ORIGIN?master" >03.patch.out 2>03.patch.err \
    || { echo "post/21: rebase-one failed" >&2; cat 03.patch.err >&2; exit 1; }
"$BE" post '#!' >04.post.out 2>04.post.err \
    || { echo "post/21: post after rebase failed" >&2; cat 04.post.err >&2; exit 1; }
# wt now carries both edits (a() from L1, b() from B).
grep -q 'return 1' f.c && grep -q 'return 2' f.c \
    || { echo "post/21: rebase didn't merge both edits:" >&2; cat f.c >&2; exit 1; }

# 6. THE CHECK: the foster-rebased tip must FF-push (no parent edge to
#    B exists — only a foster).  Pre-fix this refused as non-FF.
sleep 0.02
"$BE" post "//localhost" >05.push.out 2>05.push.err || {
    echo "post/21: be post //localhost refused a foster-rebased FF push" >&2
    cat 05.push.err >&2
    exit 1; }

# 7. origin's master fast-forwarded off B to the rebased tip.
TIP=$(git -C "$ORIGIN" rev-parse master)
[ "$TIP" != "$BSHA" ] || {
    echo "post/21: origin master did not advance past B ($BSHA)" >&2
    exit 1; }
