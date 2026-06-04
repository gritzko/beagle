#!/bin/sh
#  get/37-sub-pin-present-offline — a submodule whose pin is ALREADY in
#  the local store mounts offline, even when its declared source is
#  unreachable.
#
#  Beagle's submodule fetch (sniff/SUBS.c §SNIFFSubMount) round-trips
#  the declared `.gitmodules` URL for a git-sourced parent (the
#  parent-source candidates only kick in for a beagle remote).  When
#  that URL is down but the pinned commit is already present in the
#  sub-shard `<store>/.be/<basename>/`, the fetch is pointless: the
#  short-circuit skips the wire entirely and checks out from the local
#  store.  This mirrors the real case of a `~/.be` worktree whose
#  submodule objects were fetched into the shared store out of band
#  while GitHub was unreachable.
#
#  PROOF: the git upstream is DELETED before the re-mount, so the only
#  place the sub's objects survive is the local store shard.  Fully
#  local (file:), no ssh.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so B1 below bootstraps as its
#  own fresh store.
rm -rf "$SCRATCH/.be"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {  # $1 = repo dir — quiet init, file: submodules allowed
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

# ---------------------------------------------------------------------
# 1. git `sub` (one commit) and git `par` gitlinking vendor/sub.
# ---------------------------------------------------------------------
mkg sub || { echo "FAIL(setup): git init sub"; exit 1; }
printf 'sub payload v1\n' > sub/sub.txt
git -C sub add -A; git -C sub commit -qm sub

mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/sub" vendor/sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add sub → par"; exit 1; }
git -C par add -A; git -C par commit -qm par

# ---------------------------------------------------------------------
# 2. clone git `par` into beagle store B1 WITH --sub: the parent
#    checks out AND the sub's pin lands in B1/.be/sub.
# ---------------------------------------------------------------------
mkdir -p B1/.be
( cd B1 && "$BE" get --sub "file:$SCRATCH/par" >../02.b1.out 2>../02.b1.err ) \
    || { cat 02.b1.err >&2; echo "FAIL(setup): clone par into B1" >&2; exit 1; }
[ -f B1/vendor/sub/sub.txt ] \
    || { echo "FAIL(setup): sub not mounted in B1" >&2; cat 02.b1.err >&2; exit 1; }
[ -f B1/.be/sub/refs ] \
    || { echo "FAIL(setup): B1 missing sub shard" >&2; ls -R B1/.be >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. UNMOUNT vendor/sub (keep the B1/.be/sub shard with the pin), then
#    DELETE the git upstream — the declared URL is now unreachable.
# ---------------------------------------------------------------------
rm -rf B1/vendor/sub
rm -rf "$SCRATCH/sub"
[ -f B1/.be/sub/refs ] || { echo "FAIL(setup): sub shard vanished" >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. THE TEST: re-checkout trunk WITH --sub.  vendor/sub is gone, so
#    SubMount runs again; the declared URL is dead, but the pin is in
#    B1/.be/sub → skip-fetch → offline mount.
# ---------------------------------------------------------------------
( cd B1 && "$BE" get --sub '?master' >../04.remount.out 2>../04.remount.err ) \
    || { cat 04.remount.err >&2; echo "FAIL: offline re-mount failed" >&2; exit 1; }

grep -q 'pin present in sub-shard' 04.remount.err \
    || { echo "FAIL: skip-fetch path not taken" >&2; cat 04.remount.err >&2; exit 1; }
[ -f B1/vendor/sub/sub.txt ] \
    || { echo "FAIL: sub not re-mounted offline" >&2; cat 04.remount.err >&2; exit 1; }
grep -qx 'sub payload v1' B1/vendor/sub/sub.txt \
    || { echo "FAIL: re-mounted sub.txt content wrong" >&2; cat B1/vendor/sub/sub.txt >&2; exit 1; }

echo "get/37-sub-pin-present-offline: OK"
