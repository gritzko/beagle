#!/bin/sh
#  get/33-scheme-gated-recurse — submodule recursion is UNCONDITIONAL.
#  `be get` mounts declared subs by default for EVERY source; there is
#  no scheme gate and no `--sub` flag.  A sub is fetched from its OWN
#  `.gitmodules` URL (its own remote), so a dead URL is a real fetch
#  failure (git-consistent: `git clone --recursive` fails too), and the
#  ONLY opt-out is `--nosub`.  Fully offline (file://), no ssh.
#
#    A. git source, REACHABLE sub → recurses + mounts by default, no
#       skip marker, exit 0.
#    B. git source, DEAD sub URL → recursion fires and the fetch fails
#       (get exits non-zero, no silent skip); `--nosub` is the escape
#       hatch — sub left unmounted, parent checked out, exit 0.
#    C. keeper source (file:// beagle store) → recurses by default.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap
#  as their own fresh stores (mirrors get/31).
rm -rf "$SCRATCH/.be"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {  # $1 = repo dir
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

# --- shared sub upstream ---------------------------------------------
mkg sub || { echo "FAIL(setup): git init sub"; exit 1; }
printf 'subblob\n' > sub/s.txt
git -C sub add -A; git -C sub commit -qm sub

# =====================================================================
# A. git source, REACHABLE sub — recurses + mounts by default.
# =====================================================================
mkg parA.git || { echo "FAIL(setup): git init parA.git"; exit 1; }
printf 'parA\n' > parA.git/p.txt
git -C parA.git -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" vsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (A)"; exit 1; }
git -C parA.git add -A; git -C parA.git commit -qm parA

mkdir -p wtA/.be
( cd wtA && "$BE" get "file:$SCRATCH/parA.git" >../A.out 2>../A.err )
rcA=$?
[ "$rcA" = 0 ] \
    || { echo "FAIL(A): git source get exited $rcA" >&2; cat A.err >&2; exit 1; }
[ -f wtA/p.txt ]      || { echo "FAIL(A): parent p.txt missing" >&2; exit 1; }
[ -f wtA/vsub/s.txt ] \
    || { echo "FAIL(A): git source did NOT mount the sub by default" >&2
         cat A.err >&2; exit 1; }
match sub/s.txt wtA/vsub/s.txt
! grep -q 'skipped (git source' A.err \
    || { echo "FAIL(A): obsolete git-source skip marker" >&2; cat A.err >&2; exit 1; }

# =====================================================================
# B. git source, DEAD sub URL — recursion fires, fetch fails (no silent
#    skip); `--nosub` is the only opt-out.
# =====================================================================
mkg parB.git || { echo "FAIL(setup): git init parB.git"; exit 1; }
printf 'parB\n' > parB.git/p.txt
git -C parB.git -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" vsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (B)"; exit 1; }
cat > parB.git/.gitmodules <<EOF
[submodule "vsub"]
	path = vsub
	url = file:///nonexistent-dead-$$/dead.git
EOF
git -C parB.git add -A; git -C parB.git commit -qm parB

#  B1: default recurse → the dead fetch is reached and fails the get.
#  Guard with `if` (not `cmd; rc=$?`) so set -e doesn't abort on the
#  expected non-zero exit.
mkdir -p wtB/.be
if ( cd wtB && "$BE" get "file:$SCRATCH/parB.git" >../B.out 2>../B.err ); then
    echo "FAIL(B1): dead sub URL should fail the get (no silent skip)" >&2
    cat B.err >&2; exit 1
fi
[ -f wtB/p.txt ] || { echo "FAIL(B1): parent p.txt missing" >&2; exit 1; }
grep -qiE 'dead\.git|nonexistent-dead|WIRECLFL' B.err \
    || { echo "FAIL(B1): recursion did not reach the sub fetch" >&2; cat B.err >&2; exit 1; }

#  B2: --nosub is the escape hatch — sub left unmounted, get succeeds.
mkdir -p wtB2/.be
( cd wtB2 && "$BE" get --nosub "file:$SCRATCH/parB.git" >../B2.out 2>../B2.err )
rcB2=$?
[ "$rcB2" = 0 ] \
    || { echo "FAIL(B2): --nosub get exited $rcB2" >&2; cat B2.err >&2; exit 1; }
[ -f wtB2/p.txt ]       || { echo "FAIL(B2): parent p.txt missing" >&2; exit 1; }
[ ! -e wtB2/vsub/.be ]  || { echo "FAIL(B2): --nosub still mounted the sub" >&2; exit 1; }
grep -q 'skipped (--nosub' B2.err \
    || { echo "FAIL(B2): expected the --nosub skip marker" >&2; cat B2.err >&2; exit 1; }

# =====================================================================
# C. keeper source (file:// beagle store) — recurses by DEFAULT.
# =====================================================================
mkg parC || { echo "FAIL(setup): git init parC"; exit 1; }
printf 'parC\n' > parC/p.txt
git -C parC -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (C)"; exit 1; }
git -C parC add -A; git -C parC commit -qm parC

mkdir -p K/.be
( cd K && "$BE" get "file:$SCRATCH/parC" >../K1.out 2>../K1.err ) \
    || { echo "FAIL(setup): clone git parC into keeper store K" >&2; cat K1.err >&2; exit 1; }
[ -f K/.be/parC/refs ] \
    || { echo "FAIL(setup): K missing parC shard" >&2; ls -R K/.be | head -30 >&2; exit 1; }
[ -f K/sub/s.txt ] \
    || { echo "FAIL(setup): K sub not checked out (git-recursion clone)" >&2; exit 1; }

mkdir -p wtC/.be
( cd wtC && "$BE" get "file://$SCRATCH/K?/parC" >../C.out 2>../C.err )
rcC=$?
[ "$rcC" = 0 ] \
    || { echo "FAIL(C): keeper source get exited $rcC" >&2; cat C.err >&2; exit 1; }
[ -f wtC/p.txt ]      || { echo "FAIL(C): keeper parent p.txt missing" >&2; exit 1; }
[ -f wtC/sub/s.txt ] \
    || { echo "FAIL(C): keeper source did NOT recurse into the sub by default" >&2
         cat C.err >&2; find wtC ! -path '*/.be/*' >&2; exit 1; }
match K/sub/s.txt wtC/sub/s.txt
! grep -q 'skipped (git source' C.err \
    || { echo "FAIL(C): obsolete git-source skip marker on a keeper source" >&2
         cat C.err >&2; exit 1; }

echo "get/33-scheme-gated-recurse: OK"
