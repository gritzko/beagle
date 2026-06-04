#!/bin/sh
#  get/33-scheme-gated-recurse — POST-001 phase 2: submodule recursion
#  is the DEFAULT only for a keeper/beagle source.  A git source
#  (here: a `file:…par.git` path — `.git` suffix marks it git, so
#  be_post_target_is_keeper says NO) does NOT recurse by default; the
#  explicit `--sub` flag forces it.  Fully offline (file://), no ssh.
#
#  Three scenarios, all hermetic:
#    A. git source, declared sub with a DEAD .gitmodules URL → `be get`
#       SKIPS the sub (prints the git-source marker), exits 0, and
#       never reaches the unreachable URL.  (Offline-failure-becomes-
#       success: the whole point of the scheme gate.)
#    B. git source + `--sub` → recursion is forced; the sub mounts.
#    C. keeper source (file:// beagle store) → recurses by DEFAULT;
#       the sub mounts with no flag.

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
# A. git source with a DEAD submodule URL — must SKIP, not fetch.
#    `parA.git` has a `.git` basename → be_post_target_is_keeper = NO.
# =====================================================================
mkg parA.git || { echo "FAIL(setup): git init parA.git"; exit 1; }
printf 'parA\n' > parA.git/p.txt
git -C parA.git -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" vsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (A)"; exit 1; }
#  Point the declared URL at an unreachable path: if the gate ever
#  regresses to recurse, the fetch hits this and the test fails loudly.
cat > parA.git/.gitmodules <<EOF
[submodule "vsub"]
	path = vsub
	url = file:///nonexistent-dead-$$/dead.git
EOF
git -C parA.git add -A; git -C parA.git commit -qm parA

mkdir -p wtA/.be
( cd wtA && "$BE" get "file:$SCRATCH/parA.git" >../A.out 2>../A.err )
rcA=$?
[ "$rcA" = 0 ] \
    || { echo "FAIL(A): git source get exited $rcA (sub skip should keep it green)" >&2
         cat A.err >&2; exit 1; }
[ -f wtA/p.txt ] \
    || { echo "FAIL(A): parent p.txt missing" >&2; exit 1; }
grep -q 'skipped (git source' A.err \
    || { echo "FAIL(A): expected git-source skip marker on stderr" >&2
         cat A.err >&2; exit 1; }
[ ! -e wtA/vsub/s.txt ] \
    || { echo "FAIL(A): sub materialised on a git source without --sub" >&2; exit 1; }
[ ! -e wtA/vsub/.be ] \
    || { echo "FAIL(A): sub mounted on a git source without --sub" >&2; exit 1; }
#  Proof it never reached the dead URL: no fetch/wire error recorded.
! grep -qiE 'dead\.git|nonexistent-dead|WIRECLFL|WIREFAIL' A.err \
    || { echo "FAIL(A): get reached the dead submodule URL (gate regressed)" >&2
         cat A.err >&2; exit 1; }

# =====================================================================
# B. git source + --sub — recursion FORCED; sub mounts.
#    parB.git pins a REACHABLE sub url so the forced fetch succeeds.
# =====================================================================
mkg parB.git || { echo "FAIL(setup): git init parB.git"; exit 1; }
printf 'parB\n' > parB.git/p.txt
git -C parB.git -c protocol.file.allow=always \
    submodule add -q "$SCRATCH/sub" vsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add (B)"; exit 1; }
git -C parB.git add -A; git -C parB.git commit -qm parB

mkdir -p wtB/.be
( cd wtB && "$BE" get --sub "file:$SCRATCH/parB.git" >../B.out 2>../B.err )
rcB=$?
[ "$rcB" = 0 ] \
    || { echo "FAIL(B): git source --sub get exited $rcB" >&2; cat B.err >&2; exit 1; }
[ -f wtB/vsub/s.txt ] \
    || { echo "FAIL(B): --sub did not force sub recursion on a git source" >&2
         cat B.err >&2; exit 1; }
match sub/s.txt wtB/vsub/s.txt

# =====================================================================
# C. keeper source (file:// beagle store) — recurses by DEFAULT.
#    Build a plain (non-`.git`) git parent `parC` whose sub is added by
#    `git submodule add` (so the gitlink path == the sub url basename
#    `sub`, and the beagle clone titles its shard `sub`).  Clone it
#    into a beagle store K (git source → git's own recursion mounts the
#    sub there), then re-get the beagle project with NO flag: the
#    keeper-source default recurses.  Mirrors get/31's working keeper
#    leg.
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
[ -f wtC/p.txt ] \
    || { echo "FAIL(C): keeper parent p.txt missing" >&2; exit 1; }
[ -f wtC/sub/s.txt ] \
    || { echo "FAIL(C): keeper source did NOT recurse into the sub by default" >&2
         cat C.err >&2; find wtC ! -path '*/.be/*' >&2; exit 1; }
match K/sub/s.txt wtC/sub/s.txt
#  No git-source skip marker on a keeper source.
! grep -q 'skipped (git source' C.err \
    || { echo "FAIL(C): keeper source emitted the git-source skip marker" >&2
         cat C.err >&2; exit 1; }

echo "get/33-scheme-gated-recurse: OK"
