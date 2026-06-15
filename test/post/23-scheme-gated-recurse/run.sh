#!/bin/sh
#  post/23-scheme-gated-recurse — submodule recursion is UNCONDITIONAL
#  for local work.  There is no scheme gate and no `--sub` flag: a
#  git-sourced parent recurses the local commit (and the bare put/delete
#  stage-all) into mounted subs exactly like a keeper-sourced one.  The
#  git REMOTE part is the only thing that can't recurse, and that is the
#  push (post/16), not the local commit.  Fully offline (file://), no ssh.
#
#    A. git source (`file:…parA.git`): `be get` mounts the sub by default
#       (no flag, no skip marker).  Dirty the sub, then bare `be put`
#       stages it (recurses, no skip marker) and `be post '#msg'`
#       recurses the commit — a `be: post vsub` line, the sub commits,
#       the parent stages the gitlink bump and commits.
#    B. keeper source (file:// beagle store): same — recurses by default.

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

# --- A. git source: post recurses the local commit into the sub ------
mkg parA.git || { echo "FAIL(setup): git init parA.git"; exit 1; }
printf 'parA\n' > parA.git/p.txt
git -C parA.git -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" vsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (A)"; exit 1; }
git -C parA.git add -A; git -C parA.git commit -qm parA

mkdir -p wtA/.be
#  No --sub: a git source recurses and mounts the sub by default.
( cd wtA && "$BE" get "file:$SCRATCH/parA.git" >../A.get.out 2>../A.get.err ) \
    || { echo "FAIL(A): seed git get" >&2; cat A.get.err >&2; exit 1; }
[ -f wtA/vsub/s.txt ] \
    || { echo "FAIL(A): git source did NOT mount the sub by default" >&2
         cat A.get.err >&2; exit 1; }
! grep -q 'skipped (git source' A.get.err \
    || { echo "FAIL(A): obsolete git-source skip marker on get" >&2
         cat A.get.err >&2; exit 1; }

#  Dirty BOTH levels, then bare `be put`: the parent stages its own
#  change AND recurses the stage-all into the dirty sub — no skip marker.
printf 'parA2\n'   >> wtA/p.txt
printf 'subdirty\n' >> wtA/vsub/s.txt
( cd wtA && "$BE" put >../A.put.out 2>../A.put.err ) \
    || { echo "FAIL(A): bare put" >&2; cat A.put.err >&2; exit 1; }
! grep -q 'skipped (git source' A.put.err \
    || { echo "FAIL(A): obsolete git-source skip marker on bare put" >&2
         cat A.put.err >&2; exit 1; }

#  Local commit MUST descend into the sub.
( cd wtA && "$BE" post '#sub local commit' >../A.post.out 2>../A.post.err )
rcA=$?
[ "$rcA" = 0 ] || { echo "FAIL(A): post exited $rcA" >&2; cat A.post.err >&2; exit 1; }
grep -q '^be: post vsub' A.post.err \
    || { echo "FAIL(A): post did NOT recurse into the sub on a git source" >&2
         cat A.post.err >&2; exit 1; }
! grep -q 'skipped (git source' A.post.err \
    || { echo "FAIL(A): obsolete git-source skip marker on post" >&2
         cat A.post.err >&2; exit 1; }
#  The sub actually committed to a NEW sha and the parent staged the
#  gitlink bump (a `put vsub#<hex>` row in the parent's wtlog) before
#  committing.  `wtA/vsub/.be` is the mount ANCHOR file, not a shard —
#  the proof lives in the parent's wtlog.
grep -q '	put	vsub#' wtA/.be/wtlog \
    || { echo "FAIL(A): sub recursion fired but the parent never bumped the gitlink" >&2
         cat wtA/.be/wtlog >&2; exit 1; }
#  The sub's own change rode back as one relayed per-module table hunk
#  (BRO-002): a `vsub` module header (the rebased hunk uri) followed by
#  the sub's own change row underneath it.  BRO-003: the relay rebases
#  the per-row path column under the mount point too, so the row reads
#  `mod vsub/s.txt`, not a bare `mod s.txt`.
grep -q 'vsub' A.post.out && grep -qE '[[:space:]]mod[[:space:]]+vsub/s\.txt' A.post.out \
    || { echo "FAIL(A): sub change not relayed into the parent's report" >&2
         cat A.post.out >&2; exit 1; }

# --- B. keeper source: post recurses identically --------------------
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
#  asserts; downstream sub-commit outcome is exercised by case A.
( cd wtB && "$BE" post '#cascade' >../B.post.out 2>../B.post.err ) || true
! grep -q 'skipped (git source' B.post.err \
    || { echo "FAIL(B): obsolete git-source skip marker on keeper post" >&2
         cat B.post.err >&2; exit 1; }
grep -q '^be: post sub' B.post.err \
    || { echo "FAIL(B): keeper-source post did NOT recurse into the dirty sub" >&2
         cat B.post.err >&2; exit 1; }

echo "post/23-scheme-gated-recurse: OK"
