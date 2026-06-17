#!/bin/sh
#  get/45-file-subs-inflight-source — GET-011.  When `be get` clones from
#  a BEAGLE remote (`file://<store>?/<proj>`), each submodule must be
#  fetched from THE EXACT SOURCE WE ARE TALKING TO — the in-flight `be
#  get` source URI on the command line — as the PRIMARY candidate, with
#  the declared `.gitmodules` URL only the LAST fallback.
#
#  This models the real `~/.be/dogs` shape, but as a CROSS-STORE clone
#  (source store B1 → fresh destination B2) so the sub fetch is a genuine
#  wire round-trip against the in-flight source, not a same-store local
#  sibling-shard swap:
#    * the parent project `par` and the sub project `ch` are SIBLING
#      shards of the SAME beagle source store B1 (sub objects live in the
#      `ch` shard, named by the sub's PATH-basename — exactly as dogs'
#      `abc` shard);
#    * the committed `.gitmodules` URL is UNREACHABLE and its basename
#      DIFFERS from the shard name (`dead-libch` != `ch`) — same shape as
#      dogs' declared `libabc.git` vs the `abc` shard.  A dead file://
#      path stands in for the firewalled github URL so the case fails
#      fast and stays fully offline;
#    * we clone `file://B1?/par` into a FRESH destination B2.  The sub
#      `ch` is NOT yet present in B2's store, so a real fetch is required
#      — and it must go to the in-flight source `file://B1?/ch`, NOT the
#      dead `.gitmodules` URL.
#
#  Pre-fix, sniff sent the sub fetch to the declared (offline) URL and
#  the clone died `WIRECLFL`/`BEDOGEXIT`.  Fix: beagle threads the
#  in-flight `be get` source URI down to `sniff sub-mount --source`, and
#  SNIFFSubMount builds the PRIMARY candidate from it
#  (`<src-scheme+auth+store-path>?/<sub-path-basename>` →
#  `file://B1?/ch`).
#
#  PROOF: the only place `ch` exists is B1's sibling shard; the declared
#  URL is dead.  If `B2/ch/c.txt` materialises, the sub was sourced from
#  the in-flight source store.  Fully local (file://), no ssh.

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
# 1. git chain: ch ← par (par gitlinks ch at PATH `ch`, via a LOCAL
#    file:// .gitmodules URL so the B1 bootstrap clone resolves the sub
#    offline).
# ---------------------------------------------------------------------
mkg ch  || { echo "FAIL(setup): git init ch";  exit 1; }
printf 'ch\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm ch

mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/ch" ch >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add ch → par"; exit 1; }
git -C par add -A; git -C par commit -qm par

# ---------------------------------------------------------------------
# 2. clone git `par` into a beagle SOURCE store B1 → sibling shards
#    par/ch.  The sub objects land in the shard `ch` (the BEAGLE project
#    name = the sub's PATH-basename).
# ---------------------------------------------------------------------
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >../02.b1.out 2>../02.b1.err ) \
    || { cat 02.b1.err >&2; echo "FAIL(setup): clone git par into B1" >&2; exit 1; }
[ -f B1/.be/ch/refs ] \
    || { echo "FAIL(setup): B1 missing sibling shard 'ch'" >&2
         ls -R B1/.be 2>/dev/null | head -40 >&2; exit 1; }
[ -f B1/ch/c.txt ] \
    || { echo "FAIL(setup): B1 sub not checked out" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. Rewrite the committed `.gitmodules` URL so its basename differs
#    from the beagle shard name (`dead-libch` != `ch`) and is
#    UNREACHABLE — exactly the dogs/abc shape (`libabc.git` declared,
#    shard `abc`).  Re-post in B1 so the committed parent tree carries
#    the dead URL.  --nosub: only the parent's `.gitmodules` edit.
# ---------------------------------------------------------------------
printf '[submodule "ch"]\n\tpath = ch\n\turl = file:///nonexistent/dead-libch\n' \
    > B1/.gitmodules
( cd B1 && "$BE" put .gitmodules >../03.put.out 2>../03.put.err \
        && "$BE" post --nosub -m "dead url" >../03.post.out 2>../03.post.err ) \
    || { cat 03.post.err >&2; echo "FAIL(setup): re-post dead .gitmodules" >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. Make the world hostile to the declared URL: delete the original git
#    upstreams.  The ONLY remaining source for the `ch` pin is B1's
#    sibling `ch` shard, reachable solely via the in-flight source.
# ---------------------------------------------------------------------
rm -rf ch par

# ---------------------------------------------------------------------
# 5. clone the BEAGLE source store's `par` project into a FRESH dest B2
#    (transport form, triple-slash → keeper upload-pack).  The sub must
#    be sourced from the in-flight source `file://B1?/ch`, NOT the dead
#    declared URL.
# ---------------------------------------------------------------------
mkdir -p B2/.be
( cd B2 && "$BE" get "file://$SCRATCH/B1?/par" >../05.b2.out 2>../05.b2.err ) \
    || { cat 05.b2.err >&2
         echo "FAIL: re-clone of par (sub not sourced from in-flight source)" >&2
         exit 1; }

# ---------------------------------------------------------------------
# 6. the sub materialised in B2 — sourced from B1's sibling `ch` shard
#    via the in-flight source.  PROOF (per the case header): the git
#    upstreams are gone AND the declared `.gitmodules` URL is dead, so
#    the ONLY remaining source for the `ch` pin is B1's sibling shard,
#    reachable solely via the in-flight `file://B1?/ch` source.  A
#    matching `B2/ch/c.txt` therefore proves the in-flight-source fetch.
# ---------------------------------------------------------------------
[ -f B2/ch/c.txt ] \
    || { echo "FAIL: B2/ch/c.txt missing (sub not sourced from in-flight source)" >&2
         exit 1; }
match B1/ch/c.txt B2/ch/c.txt

echo "get/45-file-subs-inflight-source: OK"
