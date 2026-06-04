#!/bin/sh
#  post/23-scheme-gated-recurse — POST-001 phase 2 for the POST
#  local-commit path and the bare put/delete relay.  A git-sourced
#  parent does NOT recurse the local commit into mounted subs (the
#  persisted-source scheme is git); a keeper-sourced parent does.
#  Fully offline (file://), no ssh.
#
#    A. git source: dirty BOTH parent and sub, then `be post '#msg'`.
#       The commit must NOT recurse into the sub — no `be: post <sub>`
#       line; the sub stays dirty/uncommitted; the parent commits.
#       Also `be put` (bare stage-all) prints the git-source skip
#       marker and does not recurse.
#    B. keeper source: dirty the sub, then `be post '#msg'` recurses —
#       a `be: post <sub>` line appears.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

mkg sub || { echo "FAIL(setup): git init sub"; exit 1; }
printf 'subblob\n' > sub/s.txt
git -C sub add -A; git -C sub commit -qm sub

# --- A. git source: post must NOT recurse into the sub ---------------
mkg parA.git || { echo "FAIL(setup): git init parA.git"; exit 1; }
printf 'parA\n' > parA.git/p.txt
git -C parA.git -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" vsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (A)"; exit 1; }
git -C parA.git add -A; git -C parA.git commit -qm parA

mkdir -p wtA/.be
( cd wtA && "$BE" get --sub "file:$SCRATCH/parA.git" >../A.get.out 2>../A.get.err ) \
    || { echo "FAIL(A): seed git get --sub" >&2; cat A.get.err >&2; exit 1; }
[ -f wtA/vsub/s.txt ] || { echo "FAIL(A): seed sub not mounted" >&2; exit 1; }

#  Dirty both levels.
printf 'parA2\n'   >> wtA/p.txt
printf 'subdirty\n' >> wtA/vsub/s.txt

#  Bare `be put` (stage-all relay): git source → skip marker, no recurse.
( cd wtA && "$BE" put >../A.put.out 2>../A.put.err )
grep -q 'skipped (git source' A.put.err \
    || { echo "FAIL(A): bare put on a git source should print the skip marker" >&2
         cat A.put.err >&2; exit 1; }

#  Local commit must not descend into the sub.
( cd wtA && "$BE" post '#parent-only' >../A.post.out 2>../A.post.err )
rcA=$?
[ "$rcA" = 0 ] || { echo "FAIL(A): post exited $rcA" >&2; cat A.post.err >&2; exit 1; }
! grep -q '^be: post vsub' A.post.err \
    || { echo "FAIL(A): post recursed into the sub on a git source" >&2
         cat A.post.err >&2; exit 1; }
! grep -q 'aborting parent commit' A.post.err \
    || { echo "FAIL(A): post aborted on sub recursion (git source should not recurse)" >&2
         cat A.post.err >&2; exit 1; }

# --- B. keeper source: post recurses into the dirty sub --------------
mkg parC || { echo "FAIL(setup): git init parC"; exit 1; }
printf 'parC\n' > parC/p.txt
git -C parC -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (B)"; exit 1; }
git -C parC add -A; git -C parC commit -qm parC

mkdir -p K/.be
( cd K && "$BE" get "file:$SCRATCH/parC" >../K.out 2>../K.err ) \
    || { echo "FAIL(setup): clone git parC into keeper store K" >&2; cat K.err >&2; exit 1; }

mkdir -p wtB/.be
( cd wtB && "$BE" get "file://$SCRATCH/K?/parC" >../B.get.out 2>../B.get.err ) \
    || { echo "FAIL(B): keeper seed get" >&2; cat B.get.err >&2; exit 1; }
[ -f wtB/sub/s.txt ] || { echo "FAIL(B): keeper seed sub not mounted" >&2; exit 1; }

printf 'subdirty\n' >> wtB/sub/s.txt
#  The recursion FIRING (the `be: post sub` line) is what this case
#  asserts; the keeper-cloned sub itself lands detached, so its own
#  commit may refuse (DIS-009) — that downstream outcome is unrelated
#  to the scheme gate, so don't let a non-zero exit abort under set -e.
( cd wtB && "$BE" post '#cascade' >../B.post.out 2>../B.post.err ) || true
! grep -q 'skipped (git source' B.post.err \
    || { echo "FAIL(B): keeper-source post emitted the git-source skip marker" >&2
         cat B.post.err >&2; exit 1; }
grep -q '^be: post sub' B.post.err \
    || { echo "FAIL(B): keeper-source post did NOT recurse into the dirty sub" >&2
         cat B.post.err >&2; exit 1; }

echo "post/23-scheme-gated-recurse: OK"
