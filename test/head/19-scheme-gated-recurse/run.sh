#!/bin/sh
#  head/19-scheme-gated-recurse — POST-001 phase 2 for the LOCAL-verb
#  path: `be head` (no transport URI) decides sub recursion off the
#  project's PERSISTED source scheme (REFSSourceScheme, line-1 get row).
#  A git-sourced checkout does NOT recurse into subs; a keeper-sourced
#  checkout does.  Fully offline (file://), no ssh.
#
#    A. git source (`file:…parA.git`, --sub forces the initial mount so
#       there's a sub on disk), then bare `be head`: the LOCAL head must
#       NOT recurse — persisted source is git → git-source skip marker,
#       no `be: head <sub>` recursion line.
#    B. keeper source (file:// beagle store), bare `be head`: recurses —
#       a `be: head <sub>` line appears, no git-source skip marker.

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

# --- A. git source: local head must NOT recurse ----------------------
mkg parA.git || { echo "FAIL(setup): git init parA.git"; exit 1; }
printf 'parA\n' > parA.git/p.txt
git -C parA.git -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" vsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (A)"; exit 1; }
git -C parA.git add -A; git -C parA.git commit -qm parA

mkdir -p wtA/.be
#  --sub on the initial get forces the sub mount (git source) so the
#  later LOCAL head has a mounted sub to (not) recurse into.
( cd wtA && "$BE" get --sub "file:$SCRATCH/parA.git" >../A.get.out 2>../A.get.err ) \
    || { echo "FAIL(A): seed git get --sub" >&2; cat A.get.err >&2; exit 1; }
[ -f wtA/vsub/s.txt ] || { echo "FAIL(A): seed sub not mounted" >&2; exit 1; }

( cd wtA && "$BE" head >../A.head.out 2>../A.head.err )
grep -q 'skipped (git source' A.head.err \
    || { echo "FAIL(A): local head on a git source should print the skip marker" >&2
         cat A.head.err >&2; exit 1; }
! grep -q '^be: head vsub' A.head.err \
    || { echo "FAIL(A): local head recursed into the sub on a git source" >&2
         cat A.head.err >&2; exit 1; }

# --- B. keeper source: local head recurses ---------------------------
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

( cd wtB && "$BE" head >../B.head.out 2>../B.head.err )
! grep -q 'skipped (git source' B.head.err \
    || { echo "FAIL(B): keeper-source local head emitted the git-source skip marker" >&2
         cat B.head.err >&2; exit 1; }
grep -q '^be: head sub' B.head.err \
    || { echo "FAIL(B): keeper-source local head did NOT recurse into the sub" >&2
         cat B.head.err >&2; exit 1; }

echo "head/19-scheme-gated-recurse: OK"
