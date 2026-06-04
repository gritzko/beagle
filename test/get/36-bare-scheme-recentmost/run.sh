#!/bin/sh
#  get/36-bare-scheme-recentmost — replicated.wiki todo/GET-002 (part 2).
#
#  A transport URI with a SCHEME ALONE — no authority, no path, no
#  query (`ssh:`, `file:`) — must complete to the RECENTMOST get/post
#  row recorded with that transport scheme: authority + path + query
#  are adopted from that row's full URI.  E.g. after a
#  `ssh://localhost/src/dogs` clone, `be get ssh:` resolves to
#  `ssh://localhost/src/dogs`.
#
#  Pre-fix, a bare scheme `file:` is mangled by DOGParseURI's host:path
#  promotion into a phantom host `//file`, which has no recorded row,
#  so resolution dies KEEPNONE / NONE.  The fix (a) stops promoting a
#  pathless bare transport scheme to a host, and (b) makes
#  keeper_remote_uri scan the project refs backward for the recentmost
#  row whose scheme matches, adopting its full transport URI.
#
#  Hermetic offline repro: clone a git source into beagle store B1,
#  then keeper-clone B1 into B2 over `file://` transport.  B2's refs
#  now carry a `file://$SCRATCH/B1?...` get row.  From B2 the bare
#  scheme `file:` must recover that row's authority+path and resolve.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

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
# 2. clone the git source into a beagle store B1.
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/B1/.be"
( cd "$SCRATCH/B1" && "$BE" get --nosub "file:$SCRATCH/src" \
        >"$SCRATCH/01.b1.out" 2>"$SCRATCH/01.b1.err" ) \
    || { cat "$SCRATCH/01.b1.err" >&2; echo "FAIL(setup): clone src into B1" >&2; exit 1; }
[ -f "$SCRATCH/B1/.be/src/refs" ] \
    || { echo "FAIL(setup): B1 missing project shard 'src'" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. keeper-clone B1 into B2 over `file://` transport.  B2's persisted
#    source row is `file://$SCRATCH/B1?...` (scheme `file`, full path).
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/B2/.be"
( cd "$SCRATCH/B2" && "$BE" get --nosub "file://$SCRATCH/B1?/src" \
        >"$SCRATCH/02.b2.out" 2>"$SCRATCH/02.b2.err" ) \
    || { cat "$SCRATCH/02.b2.err" >&2; echo "FAIL(setup): beagle re-clone B1 -> B2" >&2; exit 1; }
[ -f "$SCRATCH/B2/f.txt" ] \
    || { echo "FAIL(setup): B2 not checked out" >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. THE TEST — from B2, the bare SCHEME `file:` (no authority, no
#    path) must recover the recentmost `file://...` row and resolve.
#    Pre-fix this dies KEEPNONE / NONE (phantom `//file` host).
# ---------------------------------------------------------------------
( cd "$SCRATCH/B2" && "$BE" head --nosub "file:?master" \
        >"$SCRATCH/03.scheme.out" 2>"$SCRATCH/03.scheme.err" )
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "FAIL: bare-scheme re-fetch did not complete from recentmost row (rc=$rc)" >&2
    cat "$SCRATCH/03.scheme.err" >&2
    exit 1
fi

echo "get/36-bare-scheme-recentmost: OK"
