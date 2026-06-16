#!/bin/sh
#  post/43-sub-push-git-official-url — SUBS-020 POST mirror, CASE 2.
#
#  `be post` to a GIT parent whose URI ENDS in `.git` must recurse the
#  push into each mounted sub and send the sub commit to its OFFICIAL
#  `.gitmodules` URL — the sub lives in its own independently-hosted git
#  remote (no path computation).  This MIRRORS the landed GET resolution
#  (sniff/SUBS.c SNIFFSubSrcEndsGit case 2): push and fetch are
#  symmetric so the sub is tracked on the other end.
#
#  Bug (pre-SUBS-020-Part-2): a git-peer push did NOT recurse the sub
#  push at all (BEActSubsPost fell through to local-commit only — see
#  the old "Git peer: do nothing" comment).  The parent's tree was
#  pushed referencing a sub sha that the sub's remote never received, so
#  a fresh recursive clone would dangle.  Fix: route the sub push
#  destination by parent kind through the SAME GET resolver
#  (beagle/BE.cli.c bepushgit_recurse_cb + sniff SNIFFSubSrcEndsGit /
#  SNIFFSubCandidateGitRel).
#
#  Fully local (file://), no ssh.  PROOF: after the parent `.git` push,
#  the sub's OFFICIAL bare git repo (`sub.git`, the `.gitmodules` url)
#  holds the sub's freshly-committed tip on its default branch.
#
#  The synthetic be-only coordinate the sub commits onto
#  (`?/<sub>/.<parent>`) has no git counterpart; the push lands it on the
#  remote's default branch (keeper POST-013 git_wire_default), which the
#  sub-cur project-strip in keeper/KEEP.exe.c now reaches even when the
#  cur is the full `/<sub>/.<parent>` coordinate.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap as
#  their own fresh stores.
rm -rf "$SCRATCH/.be"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {  # $1 = repo dir — quiet init with file:// submodules allowed
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

# ---------------------------------------------------------------------
# 1. bare sub upstream `sub.git` — the OFFICIAL `.gitmodules` url and
#    push destination (independently hosted git remote).
# ---------------------------------------------------------------------
git init --bare -q "$SCRATCH/sub.git"
mkg subseed || { echo "FAIL(setup): git init subseed"; exit 1; }
printf 'sub payload v1\n' > subseed/core.c
git -C subseed add -A; git -C subseed commit -qm sub1
git -C subseed push -q "$SCRATCH/sub.git" master:master

# ---------------------------------------------------------------------
# 2. bare `.git` parent `par.git`, gitlink vendor/sub → sub.git, with an
#    ABSOLUTE file:// `.gitmodules` url (a real pushable URI).
# ---------------------------------------------------------------------
git init --bare -q "$SCRATCH/par.git"
mkg pseed || { echo "FAIL(setup): git init pseed"; exit 1; }
printf 'int main(void){return 0;}\n' > pseed/main.c
git -C pseed -c protocol.file.allow=always submodule add -q \
    "file://$SCRATCH/sub.git" vendor/sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add sub → pseed"; exit 1; }
printf '[submodule "vendor/sub"]\n\tpath = vendor/sub\n\turl = file://%s/sub.git\n' \
    "$SCRATCH" > pseed/.gitmodules
git -C pseed add -A; git -C pseed commit -qm p
git -C pseed push -q "$SCRATCH/par.git" master:master

# ---------------------------------------------------------------------
# 3. clone the `.git` git parent into a beagle worktree (the sub mounts).
# ---------------------------------------------------------------------
mkdir -p wt/.be
( cd wt && "$BE" get "file:$SCRATCH/par.git" >../01.get.out 2>../01.get.err ) \
    || { cat 01.get.err >&2; echo "FAIL(setup): clone .git git parent" >&2; exit 1; }
[ -f wt/vendor/sub/core.c ] \
    || { echo "FAIL(setup): sub not mounted" >&2
         grep 'sub fetch try=' 01.get.err >&2 || true; exit 1; }

# ---------------------------------------------------------------------
# 4. dirty ONLY the sub; recursive local commit (no push yet).  Parks
#    the sub on its synthetic coordinate `?/sub/.par`.
# ---------------------------------------------------------------------
sleep 0.02
cat >> wt/vendor/sub/core.c <<'EOF'

void sub_extra(void) { }
EOF
( cd wt && "$BE" post '#sync' >../02.post.out 2>../02.post.err ) \
    || { cat 02.post.err >&2; echo "FAIL: recursive local commit" >&2; exit 1; }

sub_local=$(grep -oE '#[0-9a-f]{40}' wt/vendor/sub/.be | tail -1 | tr -d '#')
[ -n "$sub_local" ] || { echo "FAIL: no sub local tip" >&2; exit 1; }

#  sub.git default-branch tip BEFORE the push.
sub_off_pre=$(git --git-dir="$SCRATCH/sub.git" rev-parse master 2>/dev/null)

# ---------------------------------------------------------------------
# 5. THE PUSH — `be post file://<par.git>`.  The sub push MUST land its
#    fresh tip on its OFFICIAL url `sub.git`.  Pre-fix: the sub is never
#    pushed; sub.git stays at $sub_off_pre while the parent tree (pushed
#    to par.git) references $sub_local — a dangling gitlink.
# ---------------------------------------------------------------------
( cd wt && "$BE" post "file://$SCRATCH/par.git" >../03.push.out 2>../03.push.err )
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: parent push exited $rc" >&2
                   echo "  stderr:" >&2; cat 03.push.err >&2; exit 1; }

# ---------------------------------------------------------------------
# 6. Assertion: sub.git's default branch advanced to the sub's local
#    tip — the sub was pushed to its OFFICIAL `.gitmodules` url.
# ---------------------------------------------------------------------
sub_off_post=$(git --git-dir="$SCRATCH/sub.git" rev-parse master 2>/dev/null)

[ "$sub_off_post" = "$sub_local" ] \
    || { echo "FAIL: sub not pushed to official url (sub.git)" >&2
         echo "  want=$sub_local got=$sub_off_post (pre=$sub_off_pre)" >&2
         echo "  push stderr:" >&2; cat 03.push.err >&2; exit 1; }

#  And the sub commit object is genuinely present in sub.git.
git --git-dir="$SCRATCH/sub.git" cat-file -t "$sub_local" >/dev/null 2>&1 \
    || { echo "FAIL: sub commit $sub_local absent from sub.git" >&2; exit 1; }

echo "post/40-sub-push-git-official-url: sub pushed to its official .gitmodules url"
