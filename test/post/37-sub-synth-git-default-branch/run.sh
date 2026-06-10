#!/bin/sh
#  post/37-sub-synth-git-default-branch — POST-013.
#
#  A mounted beagle submodule whose cur is the be-only synthetic
#  coordinate `?/<sub>/.<parent>` (the normal state after a parent POST)
#  pushed to a GIT remote with NO explicit `?branch` must DEFAULT to the
#  remote's own default branch (advertised symref HEAD), NOT refuse and
#  NOT carry the synthetic coordinate onto the git wire.
#
#  This case complements post/25 (which proves the `master`-default
#  resolution) by exercising BOTH halves of the branch-resolution
#  decision against the SAME `.gitmodules`-declared git remote:
#
#    (A) DEFAULT  — `be post <git-remote>` (no `?branch`) resolves to the
#                   remote's advertised default branch `main` (here the
#                   sub's declared upstream defaults to `main`, distinct
#                   from post/25's `master`, so a hard-coded `master`
#                   fallback would mis-resolve).
#    (B) EXPLICIT — `be post <git-remote>?other` still pushes to the
#                   named ref `other`, unchanged by the POST-013 default.
#
#  The sub's official upstream URL/branch is the one declared in a
#  `.gitmodules` entry (parent-side); the user names it on the CLI.
#  Fully offline (file://), no ssh — runs under WITH_SSH=OFF.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

# --- 1. the sub's official upstream: a bare git remote on `main` -----
#  Two branches advertised (`main` + `other`); HEAD → main.  A push that
#  blindly picked the first head, or hard-coded `master`, would mis-land.
git init -q --bare -b main subdecl.git \
    || { echo "FAIL(setup): git init --bare subdecl.git" >&2; exit 1; }
git -C subdecl.git config protocol.file.allow always
git -C subdecl.git config receive.denyCurrentBranch ignore

git init -q -b main seed
git -C seed config user.email t@t
git -C seed config user.name  T
printf 'decl\n' > seed/d.txt
git -C seed add -A
git -C seed commit -qm s
git -C seed remote add origin "$SCRATCH/subdecl.git"
git -C seed -c protocol.file.allow=always push -q origin main \
    || { echo "FAIL(setup): seed push main" >&2; exit 1; }
#  A second branch `other`, behind `main`, to verify explicit `?other`.
git -C seed branch other main
git -C seed -c protocol.file.allow=always push -q origin other \
    || { echo "FAIL(setup): seed push other" >&2; exit 1; }
#  Ensure the bare repo's HEAD symref points at main (default branch).
git -C subdecl.git symbolic-ref HEAD refs/heads/main

#  This is the `.gitmodules` entry that declares the sub's official
#  remote — the parent would carry it; the user passes the same URL.
gm_url="file://localhost${SCRATCH}/subdecl.git"

# --- 2. mount: clone the declared git source into a beagle worktree --
mkdir -p wt/.be
( cd wt && "$BE" get --nosub "file://$SCRATCH/subdecl.git" \
        >../02.get.out 2>../02.get.err ) \
    || { echo "FAIL: clone declared git source" >&2
         cat 02.get.err >&2; exit 1; }
[ -f wt/d.txt ] || { echo "FAIL: clone did not check out d.txt" >&2; exit 1; }

# --- 3. parent POST puts the sub on the synthetic dot-coordinate -----
printf 'decl2\n' >> wt/d.txt
( cd wt && "$BE" post '#dot' "?/subdecl/.parentproj" \
        >../03.dotpost.out 2>../03.dotpost.err ) \
    || { echo "FAIL: post onto synthetic dot-branch" >&2
         cat 03.dotpost.err >&2; exit 1; }
grep -q 'post	?/subdecl/.parentproj#' wt/.be/wtlog \
    || { echo "FAIL: wt not on the synthetic dot-branch" >&2
         cat wt/.be/wtlog >&2; exit 1; }
cur=$(awk -F'\t' '$2=="post" { h=$3; sub(/^.*#/, "", h); last=h }
                  END { print last }' wt/.be/wtlog)
[ -n "$cur" ] || { echo "FAIL: no cur tip after dot commit" >&2; exit 1; }

# --- 4A. THE DEFAULT CHECK: be post <git-remote> (no ?branch) --------
main_before=$(git -C subdecl.git rev-parse main)
other_before=$(git -C subdecl.git rev-parse other)
( cd wt && "$BE" post "$gm_url" \
        >../04.gitpush.out 2>../04.gitpush.err )
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: synthetic→git default post exited $rc" >&2
                   cat 04.gitpush.err >&2; exit 1; }
! grep -q 'funny ref' 04.gitpush.err \
    || { echo "FAIL: forged a funny ref on the git wire" >&2
         cat 04.gitpush.err >&2; exit 1; }
git -C subdecl.git show-ref | grep -q 'refs/heads/\.' \
    && { echo "FAIL: forged a dot-coordinate ref on the git remote" >&2
         git -C subdecl.git show-ref >&2; exit 1; }
#  main (the remote's advertised default / HEAD) advanced to cur.
main_after=$(git -C subdecl.git rev-parse main)
[ "$main_after" = "$cur" ] \
    || { echo "FAIL: default push did not land on remote default 'main'" >&2
         printf 'before:%s\ncur:%s\nafter:%s\n' \
                "$main_before" "$cur" "$main_after" >&2
         cat 04.gitpush.err >&2; exit 1; }
#  `other` was NOT touched — the default resolved to HEAD/main, not
#  the first or every advertised head.
other_mid=$(git -C subdecl.git rev-parse other)
[ "$other_mid" = "$other_before" ] \
    || { echo "FAIL: default push also moved non-default branch 'other'" >&2
         exit 1; }

# --- 4B. EXPLICIT CHECK: be post <git-remote>?other still works ------
#  Advance cur once more so `other` (currently behind) FF-advances.
printf 'decl3\n' >> wt/d.txt
( cd wt && "$BE" post '#dot2' "?/subdecl/.parentproj" \
        >../05.dotpost.out 2>../05.dotpost.err ) \
    || { echo "FAIL: second dot post" >&2; cat 05.dotpost.err >&2; exit 1; }
cur2=$(awk -F'\t' '$2=="post" { h=$3; sub(/^.*#/, "", h); last=h }
                   END { print last }' wt/.be/wtlog)
( cd wt && "$BE" post "${gm_url}?other" \
        >../06.explicit.out 2>../06.explicit.err )
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: explicit ?other push exited $rc" >&2
                   cat 06.explicit.err >&2; exit 1; }
other_after=$(git -C subdecl.git rev-parse other)
[ "$other_after" = "$cur2" ] \
    || { echo "FAIL: explicit ?other did not land on 'other'" >&2
         printf 'cur2:%s\nother_after:%s\n' "$cur2" "$other_after" >&2
         cat 06.explicit.err >&2; exit 1; }

echo "post/37-sub-synth-git-default-branch: OK (default→main $main_before..$cur; explicit ?other→$cur2)"
