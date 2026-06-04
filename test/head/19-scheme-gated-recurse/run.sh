#!/bin/sh
#  head/19-scheme-gated-recurse — submodule recursion is UNCONDITIONAL.
#  `be head` (a local, read-only dry-run) reports every mounted sub
#  regardless of the project's source scheme — there is no scheme gate
#  and no `--sub` flag.  A git-sourced checkout recurses its local head
#  exactly like a keeper-sourced one; only the per-sub REMOTE ahead/
#  behind can't recurse into a git peer.  Fully offline (file://), no ssh.
#
#    A. git source (`file:…parA.git`): `be get` mounts the sub by default,
#       then bare `be head` recurses — a `be: head vsub` line, no
#       git-source skip marker.
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

# --- A. git source: local head recurses ------------------------------
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
[ -f wtA/vsub/s.txt ] || { echo "FAIL(A): seed sub not mounted" >&2; exit 1; }

#  The recursion FIRING (the `be: head vsub` line) is what this case
#  asserts; a detached sub's own head may resolve to nothing, so don't
#  let a non-zero exit abort under set -e.
( cd wtA && "$BE" head >../A.head.out 2>../A.head.err ) || true
grep -q '^be: head vsub' A.head.err \
    || { echo "FAIL(A): local head did NOT recurse into the sub on a git source" >&2
         cat A.head.err >&2; exit 1; }
! grep -q 'skipped (git source' A.head.err \
    || { echo "FAIL(A): obsolete git-source skip marker on local head" >&2
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

( cd wtB && "$BE" head >../B.head.out 2>../B.head.err ) || true
! grep -q 'skipped (git source' B.head.err \
    || { echo "FAIL(B): obsolete git-source skip marker on keeper head" >&2
         cat B.head.err >&2; exit 1; }
grep -q '^be: head sub' B.head.err \
    || { echo "FAIL(B): keeper-source local head did NOT recurse into the sub" >&2
         cat B.head.err >&2; exit 1; }

echo "head/19-scheme-gated-recurse: OK"
