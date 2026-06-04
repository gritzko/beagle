#!/bin/sh
#  head/20-cached-no-wire — replicated.wiki todo/DIS-016.
#
#  Per [URI]/[HEAD]: the transport SCHEME alone flips cached vs wire.
#  `be head //origin` (authority, NO scheme) prints the cached
#  cur-vs-remote diff with NO network; only `be head ssh://origin`
#  (scheme present) opens the wire.  GET already honours this (its
#  network arm gates on URI_SCHEME|URI_AUTHORITY); HEAD's arm fired on
#  URI_AUTHORITY alone, so a bare `//origin` was fetched over the wire,
#  contradicting HEAD's "cached by default" contract.
#
#  Bug site: beagle/DISPATCH.c::BE_PLAN_HEAD — BEActKeeperGet (and the
#  spot/graf re-walk rows) gated on URI_AUTHORITY; GET gates the same
#  arm on URI_SCHEME|URI_AUTHORITY.  Fix: align HEAD's wire arm to
#  GET's, and route authority-WITHOUT-scheme to BEActGrafHead (the
#  cached cur-vs-remote diff) by excluding only URI_SCHEME there.
#
#  Hermetic offline repro (ssh fixtures are awkward offline): use a
#  host-less `file://` keeper source, mirroring get/35.  Clone a git
#  source into beagle store B1, then keeper-clone B1 into B2 over
#  `file://` transport (records the peer-tracking ref in B2).  Move B1
#  aside so ANY wire access fails fast, then:
#
#    A. `be head //<B1-path>?master` (NO scheme) must SUCCEED from the
#       cache — no wire chatter, no fetch.  Pre-fix it dispatches to
#       KEEPGetRemote -> the wire and dies (rc != 0).
#    B. `be head file://<B1-path>?master` (transport scheme) must
#       ATTEMPT the wire and fail (B1 is gone) — proving the scheme,
#       not the authority, is what opens the wire.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap
#  as their own fresh stores.
rm -rf "$SCRATCH/.be"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

# ---------------------------------------------------------------------
# 1. seed a git source repo.
# ---------------------------------------------------------------------
git init -q -b master "$SCRATCH/src" >/dev/null 2>&1 \
    || { echo "FAIL(setup): git init src" >&2; exit 1; }
git -C "$SCRATCH/src" config user.email t@t
git -C "$SCRATCH/src" config user.name  T
printf 'src\n' > "$SCRATCH/src/f.txt"
git -C "$SCRATCH/src" add -A
git -C "$SCRATCH/src" commit -qm src >/dev/null 2>&1 \
    || { echo "FAIL(setup): commit src" >&2; exit 1; }

# ---------------------------------------------------------------------
# 2. clone the git source into a beagle store B1 (git source -> be dest).
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/B1/.be"
( cd "$SCRATCH/B1" && "$BE" get --nosub "file:$SCRATCH/src" \
        >"$SCRATCH/01.b1.out" 2>"$SCRATCH/01.b1.err" ) \
    || { cat "$SCRATCH/01.b1.err" >&2; echo "FAIL(setup): clone src into B1" >&2; exit 1; }
[ -f "$SCRATCH/B1/.be/src/refs" ] \
    || { echo "FAIL(setup): B1 missing project shard 'src'" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. keeper-clone the BEAGLE store B1 into B2 over `file://` transport
#    (authority present -> keeper upload-pack).  B2 records the peer
#    source URI in its remote-tracking refs.
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/B2/.be"
( cd "$SCRATCH/B2" && "$BE" get --nosub "file://$SCRATCH/B1?/src" \
        >"$SCRATCH/02.b2.out" 2>"$SCRATCH/02.b2.err" ) \
    || { cat "$SCRATCH/02.b2.err" >&2; echo "FAIL(setup): beagle re-clone B1 -> B2" >&2; exit 1; }
[ -f "$SCRATCH/B2/f.txt" ] \
    || { echo "FAIL(setup): B2 not checked out" >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. Pull the rug: move B1 aside.  Any wire access from now on must
#    fail fast (the keeper upload-pack source path is gone).
# ---------------------------------------------------------------------
mv "$SCRATCH/B1" "$SCRATCH/B1.moved"

# ---------------------------------------------------------------------
# A. cached form: `be head //<B1-path>?master` (authority, NO scheme).
#    Per spec this is cache-only.  Pre-fix it opens the wire and dies.
# ---------------------------------------------------------------------
#   `$SCRATCH` is absolute (leading `/`), so prefixing one more `/`
#   yields a `//<abs-path>` authority URI with NO transport scheme.
#   keeper resolves it against the cached peer-tracking row.
set +e
( cd "$SCRATCH/B2" && "$BE" head --nosub "/$SCRATCH/B1?master" \
        >"$SCRATCH/A.cached.out" 2>"$SCRATCH/A.cached.err" )
RC_A=$?
set -e

if [ "$RC_A" -ne 0 ]; then
    echo "FAIL(A): cached 'be head //...?master' failed (rc=$RC_A); spec says no wire:" >&2
    cat "$SCRATCH/A.cached.err" >&2
    exit 1
fi

#  The wire chatter is the smoking gun: any of these strings on stderr
#  proves the wire opened despite the scheme-less `//` shape.
if grep -E -q 'upload-pack|pack-objects|fetch-pack|WIRE|Could not resolve|Could not read|No such file|ssh:|Total |Enumerating' \
        "$SCRATCH/A.cached.err"; then
    echo "FAIL(A): cached form opened the wire (stderr carries wire-side chatter):" >&2
    cat "$SCRATCH/A.cached.err" >&2
    exit 1
fi

# ---------------------------------------------------------------------
# B. transport form: `be head file://<B1-path>?master` (scheme present)
#    MUST attempt the wire — and fail, because B1 was moved aside.
#    This proves the scheme (not the authority) is the wire trigger.
# ---------------------------------------------------------------------
set +e
( cd "$SCRATCH/B2" && "$BE" head --nosub "file://$SCRATCH/B1?master" \
        >"$SCRATCH/B.wire.out" 2>"$SCRATCH/B.wire.err" )
RC_B=$?
set -e

if [ "$RC_B" -eq 0 ]; then
    echo "FAIL(B): transport 'be head file://...?master' unexpectedly succeeded" >&2
    echo "         (the moved-away source should have failed the wire fetch)" >&2
    cat "$SCRATCH/B.wire.err" >&2
    exit 1
fi

echo "head/20-cached-no-wire: OK"
