#!/bin/sh
#  get/31-file-subs-parent-source — `be get` from a BEAGLE repo pulls
#  every submodule from the SAME parent source, recursively
#  (parent → child → grandchild).
#
#  "If it has the parent, it has subs": a beagle store is multi-project
#  and holds the sub projects as siblings, addressed by `?/project`.
#  When the parent is a beagle remote, sniff resolves each sub against
#  the parent's source locator — <locator>?/<url-basename>, then
#  ?/<path-basename>, then the declared .gitmodules URL (sniff/SUBS.c).
#  keeper upload-pack serves the requested `?/project` shard (falling
#  back to the store's row-0 default only when none is given).
#
#  PROOF: the original submodule upstreams are DELETED before the
#  beagle re-clone, so the only source that still has par/ch/gc is the
#  beagle store B1.  All three levels materialising there proves the
#  subs came from the parent source.  Fully local (file://), no ssh.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap
#  as their own fresh stores.
rm -rf "$SCRATCH/.be"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {  # $1 = repo dir — quiet init with file:// submodules allowed
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

# ---------------------------------------------------------------------
# 1. three-level git chain: gc ← ch (gitlinks gc) ← par (gitlinks ch)
# ---------------------------------------------------------------------
mkg gc  || { echo "FAIL(setup): git init gc";  exit 1; }
printf 'gc\n' > gc/g.txt; git -C gc add -A; git -C gc commit -qm gc

mkg ch  || { echo "FAIL(setup): git init ch";  exit 1; }
printf 'ch\n' > ch/c.txt
git -C ch -c protocol.file.allow=always submodule add -q "$SCRATCH/gc" gcsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add gc → ch"; exit 1; }
git -C ch add -A; git -C ch commit -qm ch

mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/ch" chsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add ch → par"; exit 1; }
git -C par add -A; git -C par commit -qm par

# ---------------------------------------------------------------------
# 2. clone git `par` into a beagle store B1 → sibling shards par/ch/gc,
#    all three levels checked out.  (git source → git's own recursion.)
# ---------------------------------------------------------------------
#  Shield B1 with its own empty `.be/` so HOMEFindDogs anchors here
#  instead of walking up into an ancestor store (e.g. $HOME/.be under
#  a ctest scratch that lives below $HOME).
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >../01.b1.out 2>../01.b1.err ) \
    || { cat 01.b1.err >&2; echo "FAIL(setup): clone git par into B1" >&2; exit 1; }
for p in par ch gc; do
    [ -f "B1/.be/$p/refs" ] \
        || { echo "FAIL(setup): B1 missing project shard '$p'" >&2
             ls -R B1/.be 2>/dev/null | head -40 >&2; exit 1; }
done
[ -f B1/chsub/gcsub/g.txt ] \
    || { echo "FAIL(setup): B1 grandchild not checked out" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. THE PROOF — delete every original git upstream.  Now the only
#    place par/ch/gc exist is the beagle store B1.
# ---------------------------------------------------------------------
rm -rf gc ch par

# ---------------------------------------------------------------------
# 4. clone the BEAGLE store's `par` project into B2 (transport form,
#    triple-slash so authority is present → keeper upload-pack).  Each
#    submodule must be sourced from B1, recursively.
# ---------------------------------------------------------------------
mkdir -p B2/.be
( cd B2 && "$BE" get "file://$SCRATCH/B1?/par" >../02.b2.out 2>../02.b2.err ) \
    || { cat 02.b2.err >&2; echo "FAIL: beagle re-clone of par" >&2; exit 1; }

# ---------------------------------------------------------------------
# 5. all three levels materialised, byte-exact with B1 — sourced
#    entirely from the parent beagle store (upstreams are gone).
# ---------------------------------------------------------------------
[ -f B2/p.txt ] \
    || { echo "FAIL: parent p.txt missing" >&2; find B2 ! -path '*/.be/*' >&2; exit 1; }
[ -f B2/chsub/c.txt ] \
    || { echo "FAIL: child chsub/c.txt missing (sub not sourced from parent)" >&2; exit 1; }
[ -f B2/chsub/gcsub/g.txt ] \
    || { echo "FAIL: grandchild chsub/gcsub/g.txt missing (deep recursion)" >&2; exit 1; }
match B1/p.txt             B2/p.txt
match B1/chsub/c.txt       B2/chsub/c.txt
match B1/chsub/gcsub/g.txt B2/chsub/gcsub/g.txt

echo "get/31-file-subs-parent-source: OK"
