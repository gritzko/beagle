#!/bin/sh
#  get/35-bare-authority-source — replicated.wiki todo/GET-002 (part 1).
#
#  A transport URI with an authority but an EMPTY path must complete
#  its repo path from the persisted clone source (the recorded `get`
#  row), so a bare-authority re-fetch resolves the repo it was cloned
#  from.  The ssh form `be get ssh://host` (no path) regressed to
#  WIRECLFL — the bare host went to the wire with an empty repo path.
#
#  Hermetic offline repro (ssh fixtures are awkward offline): use a
#  host-less `file://` keeper source.  A host-less `file://` clone
#  source has no host but a real path; the bug was keeper_remote_uri
#  gating the source-path completion on a non-empty resolved HOST,
#  which dropped the path of a host-less source and shipped an empty
#  repo to the wire (WIRECLFL).  This is the same completion the ssh
#  form relies on.  The fix gates on host OR path.
#
#  PROOF: clone a git source into beagle store B1, then keeper-clone B1
#  into B2 over `file://` transport (authority present).  From B2 the
#  bare-authority form `file://?master` (authority `//`, EMPTY path)
#  must complete to B1's recorded path and fetch — NOT fail WIRECLFL.

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
# 2. clone the git source into a beagle store B1 (git source → be dest).
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/B1/.be"
( cd "$SCRATCH/B1" && "$BE" get --nosub "file:$SCRATCH/src" \
        >"$SCRATCH/01.b1.out" 2>"$SCRATCH/01.b1.err" ) \
    || { cat "$SCRATCH/01.b1.err" >&2; echo "FAIL(setup): clone src into B1" >&2; exit 1; }
[ -f "$SCRATCH/B1/.be/src/refs" ] \
    || { echo "FAIL(setup): B1 missing project shard 'src'" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. keeper-clone the BEAGLE store B1 into B2 over `file://` transport
#    (authority present → keeper upload-pack).  B2's persisted source
#    is now `file://$SCRATCH/B1`.
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/B2/.be"
( cd "$SCRATCH/B2" && "$BE" get --nosub "file://$SCRATCH/B1?/src" \
        >"$SCRATCH/02.b2.out" 2>"$SCRATCH/02.b2.err" ) \
    || { cat "$SCRATCH/02.b2.err" >&2; echo "FAIL(setup): beagle re-clone B1 -> B2" >&2; exit 1; }
[ -f "$SCRATCH/B2/f.txt" ] \
    || { echo "FAIL(setup): B2 not checked out" >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. THE TEST — from B2, a bare-authority transport URI (authority
#    `//`, EMPTY path) must complete its path from B2's persisted
#    source and fetch.  Pre-fix this fails WIRECLFL / BEDOGEXIT.
# ---------------------------------------------------------------------
( cd "$SCRATCH/B2" && "$BE" head --nosub "file://?master" \
        >"$SCRATCH/03.bare.out" 2>"$SCRATCH/03.bare.err" )
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "FAIL: bare-authority re-fetch did not complete from source (rc=$rc)" >&2
    cat "$SCRATCH/03.bare.err" >&2
    exit 1
fi

echo "get/35-bare-authority-source: OK"
