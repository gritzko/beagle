#!/bin/sh
#  post/44-sub-push-git-parent-relative — SUBS-020 POST mirror, CASE 3.
#
#  `be post` to a GIT parent whose URI does NOT end in `.git` must
#  recurse the push into each mounted sub and send the sub commit to the
#  PARENT-RELATIVE URI — the sub's `.gitmodules` url (which may be
#  relative, e.g. `../sub`) resolved against the parent SOURCE URI (same
#  host, path off the parent), the SAME address GET case 3 computes
#  (sniff/SUBS.c SNIFFSubCandidateGitRel).  Push and fetch are symmetric
#  so the sub is tracked alongside the parent.
#
#  Reuses the GET-side resolver verbatim (beagle/BE.cli.c
#  bepushgit_recurse_cb → SNIFFSubCandidateGitRel); no re-rolled relative
#  logic.  The synthetic be-only sub coordinate (`?/<sub>/.<parent>`) has
#  no git counterpart, so the push lands on the remote's default branch
#  (keeper POST-013 git_wire_default + the sub-cur project-strip in
#  keeper/KEEP.exe.c).
#
#  ssh://localhost, non-`.git` git parent (needs WITH_SSH).  PROOF: two
#  DISTINCT bare sub repos — `repos/sub` (the parent-relative `../sub`
#  target) and `official_sub` (an absolute url).  The sub is mounted via
#  the absolute url (so the ssh clone succeeds), then `.gitmodules` in
#  the worktree is rewritten to the RELATIVE `../sub` so the PUSH must
#  resolve parent-relative.  Assert the sub tip lands in `repos/sub`
#  (parent-relative), NOT `official_sub`.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  ssh-side path resolution needs $SCRATCH under $HOME (mirrors
#  submodules.sh).  case.sh's $SCRATCH already sits under $HOME/tmp.
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
# 1. two DISTINCT bare sub repos:
#      repos/sub      — the PARENT-RELATIVE target (../sub off repos/par)
#      official_sub   — an ABSOLUTE url, used only to MOUNT (clone) the
#                       sub so the ssh GET succeeds.
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/repos"
git init --bare -q "$SCRATCH/repos/sub"
git init --bare -q "$SCRATCH/official_sub"
mkg subseed || { echo "FAIL(setup): git init subseed"; exit 1; }
printf 'sub payload v1\n' > subseed/core.c
git -C subseed add -A; git -C subseed commit -qm sub1
git -C subseed push -q "$SCRATCH/repos/sub"    master:master
git -C subseed push -q "$SCRATCH/official_sub" master:master

# ---------------------------------------------------------------------
# 2. bare NON-`.git` parent `repos/par`, gitlink `sub`.  `.gitmodules`
#    points at the ABSOLUTE official_sub url for the initial mount.
# ---------------------------------------------------------------------
git init --bare -q "$SCRATCH/repos/par"
mkg pseed || { echo "FAIL(setup): git init pseed"; exit 1; }
printf 'p\n' > pseed/p.txt
git -C pseed -c protocol.file.allow=always submodule add -q \
    "$SCRATCH/repos/sub" sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add sub → pseed"; exit 1; }
printf '[submodule "sub"]\n\tpath = sub\n\turl = ssh://localhost/%s/official_sub\n' \
    "$REL" > pseed/.gitmodules
git -C pseed add -A; git -C pseed commit -qm p
git -C pseed push -q "$SCRATCH/repos/par" master:master

PAR_URL="ssh://localhost/$REL/repos/par"

# ---------------------------------------------------------------------
# 3. clone the non-`.git` git parent over ssh; sub mounts via the
#    absolute url.
# ---------------------------------------------------------------------
mkdir -p wt/.be
( cd wt && "$BE" get "$PAR_URL?master" >../01.get.out 2>../01.get.err ) \
    || { cat 01.get.err >&2; echo "FAIL(setup): ssh clone non-.git git parent" >&2; exit 1; }
[ -f wt/sub/core.c ] \
    || { echo "FAIL(setup): sub not mounted" >&2
         grep 'sub fetch try=' 01.get.err >&2 || true; exit 1; }

# ---------------------------------------------------------------------
# 4. rewrite the worktree `.gitmodules` to the RELATIVE `../sub` url so
#    the PUSH must resolve parent-relative (== repos/sub), then dirty +
#    recursive local commit (parks the sub on `?/sub/.par`).
# ---------------------------------------------------------------------
printf '[submodule "sub"]\n\tpath = sub\n\turl = ../sub\n' > wt/.gitmodules
sleep 0.02
cat >> wt/sub/core.c <<'EOF'

void sub_extra(void) { }
EOF
( cd wt && "$BE" post '#sync' >../02.post.out 2>../02.post.err ) \
    || { cat 02.post.err >&2; echo "FAIL: recursive local commit" >&2; exit 1; }

sub_local=$(grep -oE '#[0-9a-f]{40}' wt/sub/.be | tail -1 | tr -d '#')
[ -n "$sub_local" ] || { echo "FAIL: no sub local tip" >&2; exit 1; }

rel_pre=$(git --git-dir="$SCRATCH/repos/sub"    rev-parse master 2>/dev/null)
off_pre=$(git --git-dir="$SCRATCH/official_sub" rev-parse master 2>/dev/null)

# ---------------------------------------------------------------------
# 5. THE PUSH — `be post ssh://…/repos/par` (non-`.git`).  The sub push
#    MUST land its fresh tip PARENT-RELATIVE in `repos/sub`, NOT in the
#    absolute `official_sub`.
# ---------------------------------------------------------------------
( cd wt && "$BE" post "$PAR_URL" >../03.push.out 2>../03.push.err )
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: parent push exited $rc" >&2
                   cat 03.push.err >&2; exit 1; }

rel_post=$(git --git-dir="$SCRATCH/repos/sub"    rev-parse master 2>/dev/null)
off_post=$(git --git-dir="$SCRATCH/official_sub" rev-parse master 2>/dev/null)

[ "$rel_post" = "$sub_local" ] \
    || { echo "FAIL: sub not pushed PARENT-RELATIVE to repos/sub" >&2
         echo "  want=$sub_local got=$rel_post (pre=$rel_pre)" >&2
         echo "  official_sub=$off_post (pre=$off_pre)" >&2
         echo "  push stderr:" >&2; cat 03.push.err >&2; exit 1; }

[ "$off_post" = "$off_pre" ] \
    || { echo "FAIL: sub wrongly pushed to the ABSOLUTE official_sub" >&2
         echo "  official_sub moved $off_pre → $off_post" >&2; exit 1; }

echo "post/44-sub-push-git-parent-relative: sub pushed to the parent-relative url"
