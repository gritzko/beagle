#!/bin/sh
#  post/25-dot-branch-no-git-wire — DIS-019.  A be-only synthetic
#  dot-coordinate branch (`?/<sub>/.<parent>[/<br>]`, branch[0]=='.';
#  the form a mounted beagle submodule sits on) must NEVER reach a git
#  wire.  Posting such a worktree DIRECTLY to a git remote would derive
#  the dot-branch as the wire refname and send `refs/heads/.<…>`, which
#  the git peer rejects as a "funny ref" — but only AFTER a full pack
#  build (WIRECLFL / BEDOGEXIT).
#
#  The fix (keeper/KEEP.exe.c keeper_post): when the resolved branch is
#  a dot-coordinate AND the remote is a git wire (DOGIsGitTransport, or
#  a local file:// path that is a git repo), print a one-line note and
#  return OK — build NO pack, forge NO ref.  The local commit (sniff)
#  already happened; a beagle sub can't live in a git remote.
#
#  Fully offline (file://), no ssh — runs under WITH_SSH=OFF.
#
#    1. Bare git remote `subA.git`; seed one commit on master.
#    2. Clone it into a beagle worktree (git source → keeper dest).
#    3. `be post '#…' ?/<proj>/.<parent>` — commit onto a SYNTHETIC
#       dot-coordinate branch, putting cur on `?/subA/.parentproj`.
#    4. `be post file://localhost/<subA.git>` — direct git-remote post.
#       MUST: exit 0, build no pack, forge no `refs/heads/.parentproj`,
#       print the be-only-coordinate note, leave the bare repo's refs
#       byte-identical.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

# --- 1. bare git remote + one commit ---------------------------------
git init -q --bare -b master subA.git \
    || { echo "FAIL(setup): git init --bare subA.git" >&2; exit 1; }
git -C subA.git config protocol.file.allow always
#  A git peer rejects a funny ref regardless of denyCurrentBranch; set
#  it anyway so a (buggy) normal-looking push wouldn't trip on that
#  instead and mask the funny-ref check.
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

# --- 4. THE CHECK: direct git-remote post of the dot-branch wt -------
refs_before=$(git -C subA.git show-ref | sort)
( cd wt && "$BE" post "file://localhost${SCRATCH}/subA.git" \
        >../04.gitpush.out 2>../04.gitpush.err )
rc=$?

#  (a) MUST exit clean — no WIRECLFL / BEDOGEXIT.
[ "$rc" = 0 ] || { echo "FAIL: dot-branch git post exited $rc (expected 0)" >&2
                   cat 04.gitpush.err >&2; exit 1; }

#  (b) MUST NOT have built a pack (the whole point — bail BEFORE the
#  expensive, doomed pack build).
! grep -q 'pack built' 04.gitpush.err \
    || { echo "FAIL: a pack was built for a doomed dot-branch git push" >&2
         cat 04.gitpush.err >&2; exit 1; }

#  (c) MUST NOT have forged / attempted a funny ref.
! grep -q 'funny ref' 04.gitpush.err \
    || { echo "FAIL: forged a funny ref refs/heads/.<…> on the git wire" >&2
         cat 04.gitpush.err >&2; exit 1; }

#  (d) MUST print the be-only-coordinate note so the user knows why.
grep -q 'be-only synthetic coordinate' 04.gitpush.err \
    || { echo "FAIL: missing the be-only-coordinate skip note" >&2
         cat 04.gitpush.err >&2; exit 1; }

#  (e) the bare git remote's refs are byte-identical — nothing pushed.
refs_after=$(git -C subA.git show-ref | sort)
[ "$refs_before" = "$refs_after" ] \
    || { echo "FAIL: dot-branch post mutated the git remote's refs" >&2
         printf 'before:\n%s\nafter:\n%s\n' "$refs_before" "$refs_after" >&2
         exit 1; }

echo "post/25-dot-branch-no-git-wire: OK"
