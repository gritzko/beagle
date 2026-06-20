#!/bin/sh
#  head/25-sub-parent-source — HEAD-004.  `be head` against a BEAGLE
#  remote (`file://<store>?/<proj>`) must peek each submodule from the
#  SAME parent source (the in-flight `be head` source URI) — the PRIMARY
#  candidate — with the declared `.gitmodules` URL only the LAST fallback.
#
#  Pre-fix, HEAD's transport-mode sub recursion went STRAIGHT to the
#  declared `.gitmodules` URL (mirrors the real `be head be://localhost/
#  beagle` → `https://github.com/gritzko/libabc.git` over ssh →
#  github:22 refused → WIREUNRCH, aborting the whole peek).  See get/45
#  for the GET twin of this bug; HEAD shares the same parent-source rule.
#
#  Models the dogs/abc shape as a CROSS-STORE peek (source store B1 →
#  fresh dest B2):
#    * parent project `par` and sub project `ch` are SIBLING shards of
#      the SAME beagle source store B1 (sub objects in the `ch` shard,
#      named by the sub's PATH-basename — exactly as dogs' `abc` shard);
#    * the committed `.gitmodules` URL is UNREACHABLE and its basename
#      DIFFERS from the shard name (`dead-libch` != `ch`) — same shape as
#      dogs' declared `libabc.git` vs the `abc` shard.  A dead file://
#      path stands in for the firewalled github URL so the case fails
#      fast and stays fully offline.
#
#  PROOF: the only place `ch` exists is B1's sibling `ch` shard, reached
#  solely via the in-flight source `file://B1?/ch`.  The declared URL is
#  dead.  If `be head file://B1?/par` exits 0 and reports the sub, the
#  peek went via the parent — not the dead URL.  Fully local, no ssh.

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
#    (the parent mounts the sub from B1 via the in-flight source).
# ---------------------------------------------------------------------
mkdir -p B2/.be
( cd B2 && "$BE" get "file://$SCRATCH/B1?/par" >../05.b2.out 2>../05.b2.err ) \
    || { cat 05.b2.err >&2; echo "FAIL(setup): re-clone of par into B2" >&2; exit 1; }
[ -f B2/ch/.be ] \
    || { echo "FAIL(setup): B2 sub not mounted" >&2; exit 1; }

# ---------------------------------------------------------------------
# 6. THE TEST — `be head` against the BEAGLE source (transport mode).
#    The transport-mode sub recursion must peek `ch` via the parent
#    source `file://B1?/ch`, NOT the dead declared URL.  Pre-fix this
#    aborted with WIRENOTRP / BEDOGEXIT (the dead URL).
# ---------------------------------------------------------------------
rc=0
( cd B2 && "$BE" head "file://$SCRATCH/B1?/par" >../06.head.out 2>../06.head.err ) || rc=$?
[ "$rc" = 0 ] \
    || { echo "FAIL: be head exited $rc (sub peek hit the dead declared URL?)" >&2
         cat 06.head.err >&2; exit 1; }

#  The dead declared URL must NEVER appear — proof the peek went via the
#  parent source, not the `.gitmodules` URL.
if grep -q 'dead-libch' 06.head.err; then
    echo "FAIL: be head reached the dead declared URL 'dead-libch'" >&2
    cat 06.head.err >&2; exit 1
fi
if grep -qiE 'WIREUNRCH|WIRENOTRP|HOMENOPROJ|BEDOGEXIT' 06.head.err; then
    echo "FAIL: be head surfaced a wire/peek failure (declared-URL abort?)" >&2
    cat 06.head.err >&2; exit 1
fi

#  Recursion fired into the sub: the per-sub peek emits `head:ch?/ch`
#  (sourced from the parent shard) on stdout; the `be: head ch` marker is
#  ABC_TRACE-gated but present in this Debug build.  Accept either signal.
if ! grep -q '^be: head ch' 06.head.err && ! grep -q 'ch' 06.head.out; then
    echo "FAIL: no sign the sub 'ch' was peeked" >&2
    echo "--- stderr ---" >&2; cat 06.head.err >&2
    echo "--- stdout ---" >&2; cat 06.head.out >&2
    exit 1
fi

echo "head/25-sub-parent-source: OK (peek via parent source, dead URL avoided)"
