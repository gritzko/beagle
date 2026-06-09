#!/bin/sh
#  GET-005 load regression: clone a LARGE repo over the keeper wire and
#  assert the ingest completes instead of dying SNOROOM.
#
#  Background.  A `be://` / keeper-protocol fetch of a large repo wrote
#  the whole pack stream into the keeper log (FILEBook'd at 16 GB, not
#  the limit) but then over-emitted index entries during UNPKIndex:
#  the forked phase-B workers carry COW-private `resolved[]`, so a delta
#  object reachable from roots owned by different workers is emitted
#  once per worker.  Those duplicates collapse in the post-index
#  sort+dedup, but the `entries` buffer was a FIXED `ph.count + 16`
#  heap allocation, so the pre-dedup overrun first dropped entries and
#  then tripped SNOROOM on the trailing bookmark push
#  (KEEPIngestStream KEEP.c:2599 / KEEPIngestFile KEEP.c:2317).  Beagle
#  itself (289862 objects) emitted 290616 entries → clone died SNOROOM;
#  smaller repos cloned fine.  Fix: UNPKIndex now grows `entries` on
#  demand (unpk_push → wh128bReserve) and the bookmark push reserves
#  first.
#
#  This is a LOAD test: it needs a large source store and passwordless
#  ssh localhost (the documented repro), so it is gated behind
#  WITH_LOAD and skips (exit 0) when neither is available.  It writes
#  ONLY under $HOME/tmp with an anchored `.be/` so store discovery can
#  never escape to the user's real `~/.be`.
set -e

: "${BIN:?BIN (bin dir with be/keeper) must be set}"
: "${TMP:=$HOME/tmp}"

#  Source: a large keeper project shard under $HOME/.be.  Default to
#  `beagle` (the repo this bug was found cloning); override with
#  GET005_PROJ.  Pick the largest available if the default is absent.
PROJ="${GET005_PROJ:-beagle}"
SRC_STORE="$HOME/.be/$PROJ"

skip() { echo "GET-005 load clone SKIP: $1"; exit 0; }

#  Need a sizable source shard — a tiny one won't trip the fixed cap.
#  (busybox `find -size` is unreliable, so size via `wc -c`.)
[ -d "$SRC_STORE" ] || skip "no source store $SRC_STORE"
big=""
for f in "$SRC_STORE"/*.keeper; do
    [ -f "$f" ] || continue
    sz=$(wc -c < "$f" 2>/dev/null || echo 0)
    if [ "$sz" -gt 52428800 ]; then big="$f"; break; fi
done
[ -n "$big" ] || skip "source store $SRC_STORE has no large pack (>50M)"

#  Need passwordless ssh localhost with keeper on PATH (the be:// path).
if ! ssh -o BatchMode=yes -o ConnectTimeout=5 localhost \
        'command -v keeper >/dev/null 2>&1' >/dev/null 2>&1; then
    skip "no passwordless ssh localhost with keeper on PATH"
fi

WORK="$TMP/get005-load-$$"
DEST="$WORK/wt"
rm -rf "$WORK"
mkdir -p "$DEST/.be"
#  Anchor discovery: a wtlog file in DEST/.be stops the HOME walk-up so
#  nothing escapes to the real ~/.be.
: > "$DEST/.be/wtlog"
trap 'rm -rf "$WORK"' EXIT

echo "GET-005 load clone: be://localhost?/$PROJ -> $DEST"
cd "$DEST"
if ! "$BIN/be" get --nosub "be://localhost?/$PROJ" \
        >"$WORK/get.out" 2>"$WORK/get.err"; then
    echo "FAIL: be get exited non-zero"
    grep -i 'snoroom\|noroom\|failed\|error' "$WORK/get.err" | head || true
    exit 1
fi

if grep -qi 'SNOROOM' "$WORK/get.err"; then
    echo "FAIL: KEEPIngestStream returned SNOROOM (GET-005 regressed)"
    exit 1
fi

#  Objects must have landed: a non-trivial pack log in the dest shard.
landed=""
for f in "$DEST/.be/$PROJ"/*.keeper; do
    [ -f "$f" ] || continue
    sz=$(wc -c < "$f" 2>/dev/null || echo 0)
    if [ "$sz" -gt 1048576 ]; then landed="$f"; break; fi
done
[ -n "$landed" ] || { echo "FAIL: no pack landed in $DEST/.be/$PROJ"; exit 1; }

#  And the worktree must have checked out at least one file.
nfiles=$(ls -1 "$DEST" | grep -v '^\.be$' | wc -l)
[ "$nfiles" -gt 0 ] || { echo "FAIL: worktree empty after clone"; exit 1; }

echo "GET-005 load clone PASS: pack $(basename "$landed") landed, $nfiles top-level entries checked out"
exit 0
