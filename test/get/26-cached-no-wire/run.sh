#!/bin/sh
#  get/26-cached-no-wire — `be get //host?<ref>` (cached form, no
#  transport scheme) must NOT open the wire.
#
#  Spec (https://replicated.wiki/html/wiki/URI.html §"Schemes — cached vs transport"):
#    "Cached form (no transport scheme): be ... //host reads only the
#     locally-cached remote-tracking refs in the per-project remote
#     shard ... No network, no registration step."
#
#  Bug site: keeper/KEEP.exe.c::keeper_get dispatches on
#  `!u8csEmpty(g->authority)` → KEEPGetRemote → WIREFetch, with no
#  check for a populated `g->scheme`.  Compare beagle/DISPATCH.c
#  BE_PLAN_PATCH (line 199) which correctly requires both
#  `URI_SCHEME|URI_AUTHORITY` before BEActKeeperGet — GET's plan
#  (line 168) is missing the scheme gate.
#
#  Repro: clone an ssh origin (populates the cache), move the
#  origin tree aside so any wire call would fail fast, then issue
#  the cached form.  Per spec it succeeds (cached refs serve it);
#  per current impl it tries the wire and dies on a missing path.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "get/26: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "get/26: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2; exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"

LOGS="$SCRATCH/logs"; mkdir -p "$LOGS"

# ----------------------------------------------------------------
# 1. seed origin (master @ commit A) via plain git.
# ----------------------------------------------------------------
git init --bare -b master "$ORIGIN" >/dev/null
git init -b master "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
echo "version A" > "$SEED/hello.c"
git -C "$SEED" add hello.c >/dev/null
git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# ----------------------------------------------------------------
# 2. clone into wt via the transport form (`ssh://...`).  Under the
#    flat layout this records the peer-URI remote-tracking ref in the
#    project's flat reflog `.be/<P>/refs` (no remotes/ dir) so later
#    `//localhost` resolves from cache.
# ----------------------------------------------------------------
mkdir wt wt/.be
cd wt
"$BE" get "ssh://localhost/$REL_ORIGIN?master" \
    > "$LOGS/01.clone.out" 2> "$LOGS/01.clone.err" || {
    echo "FAIL: initial transport clone failed" >&2
    cat "$LOGS/01.clone.err" >&2
    exit 1
}
[ "$(cat hello.c 2>/dev/null)" = "version A" ] || {
    echo "FAIL: post-clone hello.c is not 'version A'" >&2
    cat hello.c >&2 || true
    exit 1
}

# ----------------------------------------------------------------
# 3. Pull the rug: move origin aside.  Any wire access from now on
#    must fail (ssh upload-pack can't open the moved-away path).
# ----------------------------------------------------------------
mv "$ORIGIN" "$ORIGIN.moved"

# ----------------------------------------------------------------
# 4. `be get //localhost?master` — cached form.  Per spec this is
#    a local-only operation (reads cached remote-tracking refs).
#    Under the bug it dispatches to KEEPGetRemote → WIREFetch and
#    the wire call dies on the moved-away origin.
# ----------------------------------------------------------------
set +e
"$BE" get "//localhost?master" \
    > "$LOGS/02.cached.out" 2> "$LOGS/02.cached.err"
RC=$?
set -e

if [ "$RC" -ne 0 ]; then
    echo "FAIL: cached 'be get //localhost?master' failed (rc=$RC); spec says no wire — see stderr:" >&2
    cat "$LOGS/02.cached.err" >&2
    exit 1
fi

# The wire chatter is the smoking gun: any of these strings on
# stderr proves the wire opened despite the bare `//host` shape.
if grep -E -q 'upload-pack|pack-objects|fetch-pack|wcli|WIRE|No such file|Could not read|spawn' "$LOGS/02.cached.err"; then
    echo "FAIL: cached form opened the wire (stderr carries wire-side chatter):" >&2
    cat "$LOGS/02.cached.err" >&2
    exit 1
fi

# hello.c must still hold version A — no fetch happened.
[ "$(cat hello.c 2>/dev/null)" = "version A" ] || {
    echo "FAIL: hello.c changed across cached GET; expected 'version A', got '$(cat hello.c)'" >&2
    exit 1
}

rm -rf "$LOGS"
