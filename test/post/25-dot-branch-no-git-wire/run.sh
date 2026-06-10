#!/bin/sh
#  post/25-dot-branch-no-git-wire — DIS-019 + POST-013.
#
#  A be-only synthetic dot-coordinate branch (`?/<sub>/.<parent>[/<br>]`,
#  branch[0]=='.'; the normal cur of a mounted beagle submodule after a
#  parent POST) has no git counterpart — a git peer's receive-pack would
#  reject `refs/heads/.<…>` as a "funny ref" AFTER a full pack build.
#
#  DIS-019 used to REFUSE such a push outright ("be-only synthetic
#  coordinate; not pushing").  POST-013 refines that: the user DID name a
#  usable git target (`be post <git-remote>`), so instead of refusing we
#  DEFAULT to the remote's OWN default branch (advertised symref HEAD,
#  else main/master) — exactly what a `git push` of a HEAD checkout does.
#  No more `?main` friction; the dot-branch is forged onto NO ref, the
#  remote's default branch FF-advances to cur's tip.
#
#  The synthetic coordinate stays meaningful to a be peer (beagle
#  protocol), where the dot-branch is a real REFS tip — that path is
#  unchanged; this case only exercises the GIT-wire default.
#
#  Fully offline (file://), no ssh — runs under WITH_SSH=OFF.
#
#    1. Bare git remote `subA.git` on `master`; seed one commit.
#    2. Clone it into a beagle worktree (git source → keeper dest).
#    3. `be post '#…' ?/<proj>/.<parent>` — commit onto a SYNTHETIC
#       dot-coordinate branch, putting cur on `?/subA/.parentproj`.
#    4. `be post file://localhost/<subA.git>` — direct git-remote post,
#       NO `?branch`.  MUST: exit 0, push cur onto the remote's default
#       branch `master` (FF), forge NO `refs/heads/.parentproj`.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

# --- 1. bare git remote + one commit ---------------------------------
git init -q --bare -b master subA.git \
    || { echo "FAIL(setup): git init --bare subA.git" >&2; exit 1; }
git -C subA.git config protocol.file.allow always
git -C subA.git config receive.denyCurrentBranch ignore

git init -q -b master seed
git -C seed config user.email t@t
git -C seed config user.name  T
printf 'subblob\n' > seed/s.txt
git -C seed add -A
git -C seed commit -qm sub
git -C seed remote add origin "$SCRATCH/subA.git"
git -C seed -c protocol.file.allow=always push -q origin master \
    || { echo "FAIL(setup): seed push" >&2; exit 1; }

# --- 2. clone the git source into a beagle worktree ------------------
mkdir -p wt/.be
( cd wt && "$BE" get --nosub "file://$SCRATCH/subA.git" \
        >../02.get.out 2>../02.get.err ) \
    || { echo "FAIL: clone git source into beagle wt" >&2
         cat 02.get.err >&2; exit 1; }
[ -f wt/s.txt ] || { echo "FAIL: clone did not check out s.txt" >&2; exit 1; }

# --- 3. commit onto a SYNTHETIC dot-coordinate branch ----------------
#  This is what a parent-driven POST does to a mounted beagle sub:
#  it commits the sub onto `?/<subproj>/.<parentproj>[/<br>]`.  After
#  this, cur sits on the be-only dot-branch `.parentproj`.
printf 'subblob2\n' >> wt/s.txt
( cd wt && "$BE" post '#dot commit' "?/subA/.parentproj" \
        >../03.dotpost.out 2>../03.dotpost.err ) \
    || { echo "FAIL: post onto synthetic dot-branch" >&2
         cat 03.dotpost.err >&2; exit 1; }
#  cur's tip is now recorded against the dot-coordinate in the wtlog.
grep -q 'post	?/subA/.parentproj#' wt/.be/wtlog \
    || { echo "FAIL: wt not on the synthetic dot-branch" >&2
         cat wt/.be/wtlog >&2; exit 1; }
cur=$(awk -F'\t' '$2=="post" { h=$3; sub(/^.*#/, "", h); last=h }
                  END { print last }' wt/.be/wtlog)
[ -n "$cur" ] || { echo "FAIL: no cur tip after dot-branch commit" >&2; exit 1; }

# --- 4. THE CHECK: direct git-remote post of the dot-branch wt -------
master_before=$(git -C subA.git rev-parse master)
( cd wt && "$BE" post "file://localhost${SCRATCH}/subA.git" \
        >../04.gitpush.out 2>../04.gitpush.err )
rc=$?

#  (a) MUST exit clean — no WIRECLFL / BEDOGEXIT.
[ "$rc" = 0 ] || { echo "FAIL: dot-branch git post exited $rc (expected 0)" >&2
                   cat 04.gitpush.err >&2; exit 1; }

#  (b) MUST NOT have forged / attempted a funny ref.
! grep -q 'funny ref' 04.gitpush.err \
    || { echo "FAIL: forged a funny ref refs/heads/.<…> on the git wire" >&2
         cat 04.gitpush.err >&2; exit 1; }

#  (c) MUST NOT have created a `refs/heads/.parentproj` on the remote.
git -C subA.git show-ref | grep -q 'refs/heads/\.' \
    && { echo "FAIL: forged a dot-coordinate ref on the git remote" >&2
         git -C subA.git show-ref >&2; exit 1; }

#  (d) the remote's DEFAULT branch (master) FF-advanced to cur — the
#  POST-013 behaviour: a synthetic-coordinate git push defaults to the
#  remote's own default branch instead of refusing.
master_after=$(git -C subA.git rev-parse master)
[ "$master_after" = "$cur" ] \
    || { echo "FAIL: remote master did not advance to cur (POST-013)" >&2
         printf 'before:%s\ncur   :%s\nafter :%s\n' \
                "$master_before" "$cur" "$master_after" >&2
         cat 04.gitpush.err >&2; exit 1; }

echo "post/25-dot-branch-no-git-wire: OK (default-branch push: master $master_before -> $cur)"
