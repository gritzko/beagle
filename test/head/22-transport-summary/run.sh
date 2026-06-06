#!/bin/sh
#  head/22-transport-summary — replicated.wiki todo/HEAD-001.
#
#  `be head <scheme>://host?ref` is the read-only "what changed and
#  what would change" verb.  It fetches the remote tip into the local
#  cache (keeper get) and re-indexes (spot/graf get) — but pre-fix it
#  then EXITS 0 with EMPTY stdout: no `ahead N, behind M` counts, no
#  differing-file rows.  A behind/diverged branch reads identical to an
#  up-to-date one (the gap masked a real divergence that only surfaced
#  later as a cryptic WIRECLNFF on POST).
#
#  The cached form (`//host?ref`, NO scheme) already prints the summary
#  via BEActGrafHead (see head/20-cached-no-wire); the transport form
#  (scheme present) skipped it because BEActGrafHead's row excludes
#  URI_SCHEME.  Fix: after the transport fetch+reindex, run the same
#  cached cur-vs-remote diff so the summary prints.
#
#  Hermetic offline repro (no ssh): a host-less `file://` keeper source,
#  mirroring head/20.  Clone a git source into beagle store B1, then
#  keeper-clone B1 into B2 over `file://` transport (records the peer-
#  tracking ref in B2).  Advance B1 by one commit, then from B2:
#
#    `be head file://<B1-path>?master`  (transport scheme) MUST
#    fetch the new tip AND print the ahead/behind summary, with B2 one
#    commit behind B1.

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
# 2. clone the git source into a beagle store B1.
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/B1/.be"
( cd "$SCRATCH/B1" && "$BE" get --nosub "file:$SCRATCH/src" \
        >"$SCRATCH/01.b1.out" 2>"$SCRATCH/01.b1.err" ) \
    || { cat "$SCRATCH/01.b1.err" >&2; echo "FAIL(setup): clone src into B1" >&2; exit 1; }
[ -f "$SCRATCH/B1/.be/src/refs" ] \
    || { echo "FAIL(setup): B1 missing project shard 'src'" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. keeper-clone the BEAGLE store B1 into B2 over `file://` transport.
#    B2 records the peer source URI in its remote-tracking refs.
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/B2/.be"
( cd "$SCRATCH/B2" && "$BE" get --nosub "file://$SCRATCH/B1?/src" \
        >"$SCRATCH/02.b2.out" 2>"$SCRATCH/02.b2.err" ) \
    || { cat "$SCRATCH/02.b2.err" >&2; echo "FAIL(setup): beagle re-clone B1 -> B2" >&2; exit 1; }
[ -f "$SCRATCH/B2/f.txt" ] \
    || { echo "FAIL(setup): B2 not checked out" >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. advance B1 by one commit on trunk (a new file).  B2 is now one
#    commit BEHIND B1, with one differing path.
# ---------------------------------------------------------------------
( cd "$SCRATCH/B1" \
    && printf 'advance\n' > g.txt \
    && "$BE" put g.txt >"$SCRATCH/03.put.out" 2>"$SCRATCH/03.put.err" \
    && "$BE" post '#advance' >"$SCRATCH/04.post.out" 2>"$SCRATCH/04.post.err" ) \
    || { cat "$SCRATCH/04.post.err" >&2; echo "FAIL(setup): advance B1" >&2; exit 1; }

# ---------------------------------------------------------------------
# 5. transport form: `be head file://<B1-path>?master` (scheme present).
#    MUST fetch the new tip AND print the ahead/behind summary.
# ---------------------------------------------------------------------
set +e
( cd "$SCRATCH/B2" && "$BE" head --nosub "file://$SCRATCH/B1?master" \
        >"$SCRATCH/T.transport.out" 2>"$SCRATCH/T.transport.err" )
RC=$?
set -e

if [ "$RC" -ne 0 ]; then
    echo "FAIL: transport 'be head file://...?master' failed (rc=$RC):" >&2
    cat "$SCRATCH/T.transport.err" >&2
    exit 1
fi

#  HEAD-001: stdout MUST carry the summary, not be empty.
if [ ! -s "$SCRATCH/T.transport.out" ]; then
    echo "FAIL(HEAD-001): transport 'be head' stdout is EMPTY;" >&2
    echo "                expected ahead/behind summary + differing rows." >&2
    exit 1
fi

#  Summary line: B2 is 0 ahead, 1 behind, 1 changed (the new g.txt).
if ! grep -E -q '^head: \?[a-zA-Z0-9_/-]*: 0 ahead, 1 behind, 1 changed$' \
        "$SCRATCH/T.transport.out"; then
    echo "FAIL(HEAD-001): summary line missing or wrong; got:" >&2
    cat "$SCRATCH/T.transport.out" >&2
    exit 1
fi

#  The behind commit (`advance`) must be listed with a `-` prefix.
if ! grep -E -q '^- [0-9a-f]+  [0-9]{2}:[0-9]{2}  advance \(.+\)$' \
        "$SCRATCH/T.transport.out"; then
    echo "FAIL(HEAD-001): behind-commit row missing; got:" >&2
    cat "$SCRATCH/T.transport.out" >&2
    exit 1
fi

#  The differing path (`g.txt`) must be listed with a `-` prefix.
if ! grep -E -q '^- g\.txt$' "$SCRATCH/T.transport.out"; then
    echo "FAIL(HEAD-001): differing-path row missing; got:" >&2
    cat "$SCRATCH/T.transport.out" >&2
    exit 1
fi

echo "head/22-transport-summary: OK"
