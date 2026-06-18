#!/bin/sh
#  get/53-file-subs-git-absolute — SUBS-024, GET CASE 4.
#
#  `be get` from a GIT parent whose URI does NOT end in `.git`, where the
#  sub's `.gitmodules` url is ABSOLUTE, must add a `<src>/<subpath>`
#  candidate (the sub's worktree PATH appended to the parent SOURCE URI)
#  BEFORE the raw official url.  An absolute url resolves-to-self under
#  SNIFFSubCandidateGitRel (case 3 → NONE), so prior to SUBS-024 only the
#  declared (possibly unreachable) official url was ever tried.  Case 4
#  is the push/fetch mirror of post/45.
#
#  ssh://localhost, non-`.git` git parent (needs WITH_SSH).  PROOF is a
#  byte-exact checkout in a layout that ONLY case 4 can satisfy: the sub
#  content lives NESTED at `repos/par/sub` (the `<src>/<subpath>`
#  candidate), while the declared official url points at an EMPTY bare
#  repo with NO matching ref — so the gitlink pin can be landed only by
#  the nested candidate.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || { echo "FAIL: HOME unset" >&2; exit 1; }
case "$SCRATCH" in "$HOME"/*) ;; *) echo "SKIP: SCRATCH not under HOME" >&2; exit 0;; esac
REL=${SCRATCH#"$HOME"/}

rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

# ---------------------------------------------------------------------
# 1. bare NON-`.git` parent `repos/par` with the sub content NESTED at
#    `repos/par/sub` (the <src>/<subpath> case-4 candidate).  A SEPARATE
#    EMPTY bare `official_sub` is the declared absolute url — it has NO
#    ref carrying the gitlink pin, so only case 4 can land it.
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/repos"
git init --bare -q "$SCRATCH/repos/par"
git init --bare -q "$SCRATCH/repos/par/sub"
git init --bare -q "$SCRATCH/official_sub"        # EMPTY — no pin here
mkg subseed || { echo "FAIL(setup): git init subseed"; exit 1; }
printf 'sub-body\n' > subseed/s.txt
git -C subseed add -A; git -C subseed commit -qm sub
git -C subseed push -q "$SCRATCH/repos/par/sub" master:master
SUB_PIN=$(git -C subseed rev-parse master)

# ---------------------------------------------------------------------
# 2. seed the parent gitlink `sub` at $SUB_PIN; `.gitmodules` declares
#    the ABSOLUTE (empty) official_sub url — the case-4 trigger.
# ---------------------------------------------------------------------
mkg pseed || { echo "FAIL(setup): git init pseed"; exit 1; }
printf 'par\n' > pseed/p.txt
#  Stage the gitlink at the exact pin without needing official_sub to
#  carry it: add the gitlink path entry by hand.
git -C pseed update-index --add --cacheinfo 160000,"$SUB_PIN",sub
printf '[submodule "sub"]\n\tpath = sub\n\turl = ssh://localhost/%s/official_sub\n' \
    "$REL" > pseed/.gitmodules
git -C pseed add .gitmodules p.txt
git -C pseed commit -qm par
git -C pseed push -q "$SCRATCH/repos/par" master:master

PAR_URL="ssh://localhost/$REL/repos/par"

# ---------------------------------------------------------------------
# 3. clone the non-`.git` parent over ssh.  The official url is empty,
#    so the gitlink pin lands ONLY via the `<src>/<subpath>` case-4
#    candidate (repos/par/sub).  A byte-exact checked-out sub PROVES it.
# ---------------------------------------------------------------------
mkdir -p wt/.be
( cd wt && "$BE" get "$PAR_URL?master" >../03.get.out 2>../03.get.err ) \
    || { cat 03.get.err >&2
         echo "FAIL: clone non-.git git parent w/ absolute sub url" >&2
         exit 1; }

[ -f wt/sub/s.txt ] \
    || { echo "FAIL: sub not checked out (case-4 <src>/<subpath> not tried)" >&2
         cat 03.get.err >&2; exit 1; }
match subseed/s.txt wt/sub/s.txt

echo "get/53-file-subs-git-absolute: sub fetched from <src>/<subpath>"
