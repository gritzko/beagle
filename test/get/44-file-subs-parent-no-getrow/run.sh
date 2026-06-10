#!/bin/sh
#  get/44-file-subs-parent-no-getrow — GET-011.  A `be get` against a
#  beagle store whose parent shard's recorded `get` row is NOT
#  recoverable as a beagle remote (mirroring a real `~/.be/<proj>`
#  whose source is an unreachable upstream) must STILL source each
#  submodule from the store we are actually talking to (the live
#  `be get` source / the parent worktree's store), with the declared
#  `.gitmodules` URL only as a last-resort fallback.
#
#  This reproduces the exact ticket condition `~/.be/dogs` hits:
#    * the sub's `.gitmodules` URL is UNREACHABLE with a url-basename
#      that differs from the shard name (`…/dead-libch`, url-basename
#      `dead-libch`) — same shape as dogs' `libabc.git` declared vs the
#      `abc` shard.  (A dead file:// path stands in for the firewalled
#      github URL so the case fails fast and stays fully offline.)
#    * the sub's objects live in a SIBLING shard of the same store,
#      named by the BEAGLE project name `ch` (NOT the url-basename) —
#      so `subs_pin_present` against the url-basename shard misses and
#      a real fetch is required;
#    * the parent shard's recorded `get` row names a now-deleted
#      upstream, so `subs_recover_locator` cannot classify it as a
#      beagle remote.
#  Pre-fix, sniff builds NO parent-source candidate, tries only the
#  github URL, and the clone dies `WIRECLFL` / `BEDOGEXIT` even though
#  `ch` is sitting in the very store we are talking to.
#
#  PROOF: the declared sub URL is unreachable (offline) and every git
#  upstream is deleted; the only place `ch` exists is the sibling `ch`
#  shard of the store under `be get`.  If `ch/c.txt` materialises, the
#  sub was sourced from the parent store via the live source —
#  addressing the sibling shard by the `?/<project>` query (GET-004) —
#  not the declared URL.  Fully local (file://), no ssh.

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
#    file:// .gitmodules URL so the B0 bootstrap clone resolves the sub
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
# 2. clone git `par` into a beagle store B0 → sibling shards par/ch.
#    The sub objects land in the shard `ch` (the BEAGLE project name).
# ---------------------------------------------------------------------
mkdir -p B0/.be
( cd B0 && "$BE" get "file:$SCRATCH/par" >../02.b0.out 2>../02.b0.err ) \
    || { cat 02.b0.err >&2; echo "FAIL(setup): clone git par into B0" >&2; exit 1; }
[ -f B0/.be/ch/refs ] \
    || { echo "FAIL(setup): B0 missing sibling shard 'ch'" >&2
         ls -R B0/.be 2>/dev/null | head -40 >&2; exit 1; }
[ -f B0/ch/c.txt ] \
    || { echo "FAIL(setup): B0 sub not checked out" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. Rewrite the committed `.gitmodules` URL so its basename differs
#    from the beagle shard name (`dead-libch` != `ch`) and is
#    UNREACHABLE — exactly the dogs/abc shape (`libabc.git` declared,
#    shard `abc`).  A dead file:// path stands in for the firewalled
#    github URL so the case fails fast and stays offline.  Re-post in B0
#    so the committed parent tree carries the dead URL.  --nosub: only
#    the parent's `.gitmodules` edit is being committed.
# ---------------------------------------------------------------------
printf '[submodule "ch"]\n\tpath = ch\n\turl = file:///nonexistent/dead-libch\n' \
    > B0/.gitmodules
( cd B0 && "$BE" put .gitmodules >../03.put.out 2>../03.put.err \
        && "$BE" post --nosub -m "dead url" >../03.post.out 2>../03.post.err ) \
    || { cat 03.post.err >&2; echo "FAIL(setup): re-post dead .gitmodules" >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. Make B0 hostile to the declared URL and to REFS-based recovery:
#    delete every git upstream.  par's recorded `get` row now names a
#    deleted path (unrecoverable as a beagle remote), and the declared
#    `.gitmodules` URL is unreachable.  Drop the sub mount so
#    a re-get must re-mount + re-fetch the sub from scratch.  The `ch`
#    shard (objects) stays — the ONLY remaining source for the pin.
# ---------------------------------------------------------------------
rm -rf ch par
rm -rf B0/ch

# ---------------------------------------------------------------------
# 5. in-place re-get of `par` from the live source store B0
#    (file://B0?/par).  The sub must be sourced from B0's sibling `ch`
#    shard via the live source addressing `?/ch`, NOT from github.
# ---------------------------------------------------------------------
( cd B0 && "$BE" get "file://$SCRATCH/B0?/par" >../05.reget.out 2>../05.reget.err ) \
    || { cat 05.reget.err >&2
         echo "FAIL: re-get of par (sub not sourced from parent store)" >&2
         echo "--- SUBS.dbg trace ---" >&2
         grep -i 'SUBS.dbg\|fetch try' 05.reget.err >&2 || true
         exit 1; }

# ---------------------------------------------------------------------
# 6. the sub materialised — sourced from B0's sibling `ch` shard via
#    the live source (git upstream gone, github unreachable, REFS
#    get-row unrecoverable).
# ---------------------------------------------------------------------
[ -f B0/ch/c.txt ] \
    || { echo "FAIL: ch/c.txt missing (sub not sourced from parent store)" >&2
         echo "--- SUBS.dbg trace ---" >&2
         grep -i 'SUBS.dbg\|fetch try' 05.reget.err >&2 || true
         exit 1; }

#  the dead declared `.gitmodules` URL must NEVER be the source that
#  lands the pin.
if grep -q 'sub fetch try=file:///nonexistent/.* => OK' 05.reget.err; then
    echo "FAIL: sub was sourced from the dead declared URL, not the parent" >&2
    grep 'sub fetch try=' 05.reget.err >&2
    exit 1
fi

echo "get/44-file-subs-parent-no-getrow: OK"
