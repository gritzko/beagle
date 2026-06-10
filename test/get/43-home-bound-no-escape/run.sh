#!/bin/sh
#  get/43-home-bound-no-escape — GET-010 Defect-2 (the HOME-escape).
#
#  `dog/HOME.c::home_walk_up` is the cwd→`/` ascent that locates the
#  enclosing `.be` anchor.  Before the fix it NEVER consulted `$HOME`:
#  a process running UNDER `$HOME` with no local `.be` shield kept
#  climbing PAST `$HOME` and grabbed/created a store sitting above it —
#  on a dev/CI box, the REAL `~/.be`.  That broke test hermeticity:
#  a test setting `HOME=<scratch>` would still reach `/home/<dev>/.be`.
#
#  FIX (dog/HOME.c::home_walk_up + home_walk_ceiling): if the walk
#  STARTS under `$HOME` (canonical path-prefix), `$HOME` is the ceiling
#  and the ascent stops there (so a scratch `$HOME` can never escape to
#  a store above it / the real `~/.be`).  If the start dir is NOT under
#  `$HOME`, the walk stays UNBOUNDED — single-project `~` anchors and
#  stores located outside `$HOME` keep working unchanged.
#
#  HERMETIC: everything lives inside $SCRATCH.  A FAKE "store above
#  HOME" stands in for the real `~/.be`; with the bound in place the
#  walk must NEVER stat it.  strace proves zero access to the FAKE
#  store AND to the dev box's real `/home/.../.be`; a before/after
#  find-snapshot of the real `~/.be` confirms it is byte-identical.

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh shielded $SCRATCH/.be; drop it — this case builds its own
#  bespoke tree and manages $HOME directly.
rm -rf "$SCRATCH/.be"

command -v strace >/dev/null 2>&1 || {
    echo "get/43-home-bound-no-escape: SKIP (no strace)" ; exit 0
}

#  Disable LeakSanitizer: it cannot run under ptrace (strace).
export ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0"

REAL_HOME=$HOME                      # the dev box's real $HOME (~/.be lives here)
REAL_BE="$REAL_HOME/.be"

#  Snapshot the real ~/.be (sizes + mtimes) — must be byte-identical
#  after the run, and never even stat'd by the bounded walk.
snap() { find "$REAL_BE" -printf '%s %T@ %p\n' 2>/dev/null | sort; }
snap > "$SCRATCH/realbe.before"

# ---------------------------------------------------------------------
#  Build the tree: a FAKE single-project store ONE LEVEL ABOVE a
#  scratch-HOME dir; the scratch-HOME dir has NO `.be` shield of its
#  own, and a deep cwd sits under it.
#
#     $SCRATCH/store/.be/proj          <- store above HOME (the bait)
#     $SCRATCH/store/scratchhome/      <- $HOME (NO .be shield)
#     $SCRATCH/store/scratchhome/work/deep   <- cwd
#     $SCRATCH/elsewhere/              <- an unrelated HOME for case B
# ---------------------------------------------------------------------
mkdir -p "$SCRATCH/store/.be/proj"
mkdir -p "$SCRATCH/store/scratchhome/work/deep"
mkdir -p "$SCRATCH/elsewhere"

# =====================================================================
# CASE A — started UNDER $HOME, no shield: must BIND at $HOME (NOHOME),
#          never escaping to the store above it.
# =====================================================================
(
    cd "$SCRATCH/store/scratchhome/work/deep"
    env HOME="$SCRATCH/store/scratchhome" \
        strace -f -e trace=stat,lstat,newfstatat,openat \
        -o "$SCRATCH/A.strace" \
        "$BE" status > "$SCRATCH/A.out" 2> "$SCRATCH/A.err"
) || true     # expected to fail (NOHOME) — we assert on the WALK, not exit

#  (A1) The walk must NOT have stat'd the store ABOVE $HOME.
if grep -qF "$SCRATCH/store/.be" "$SCRATCH/A.strace"; then
    echo "FAIL(A): walk escaped above \$HOME — stat'd the store above it" >&2
    grep -F "$SCRATCH/store/.be" "$SCRATCH/A.strace" >&2
    exit 1
fi

#  (A2) The walk must NOT have touched the dev box's REAL ~/.be.
if grep -qF "$REAL_BE" "$SCRATCH/A.strace"; then
    echo "FAIL(A): walk reached the real \$HOME/.be" >&2
    grep -F "$REAL_BE" "$SCRATCH/A.strace" >&2
    exit 1
fi

#  (A3) No stray `.be` created above $HOME (no doubled shard / refs).
if [ -e "$SCRATCH/store/.be/.be" ]; then
    echo "FAIL(A): stray nested $SCRATCH/store/.be/.be created" >&2
    exit 1
fi

# =====================================================================
# CASE B — started OUTSIDE $HOME: walk stays UNBOUNDED and DOES find
#          the store above cwd (semantics preserved).
# =====================================================================
(
    cd "$SCRATCH/store/scratchhome/work/deep"
    env HOME="$SCRATCH/elsewhere" \
        strace -f -e trace=stat,lstat,newfstatat,openat \
        -o "$SCRATCH/B.strace" \
        "$BE" status > "$SCRATCH/B.out" 2> "$SCRATCH/B.err"
) || true

#  (B1) The unbounded walk MUST have reached the store above cwd
#       (the project shard probe proves discovery climbed to it).
grep -qF "$SCRATCH/store/.be/proj" "$SCRATCH/B.strace" \
    || grep -qF "$SCRATCH/store/.be" "$SCRATCH/B.strace" \
    || { echo "FAIL(B): unbounded walk did not reach the store above cwd" >&2
         exit 1; }

# ---------------------------------------------------------------------
#  The real ~/.be must be byte-identical (sizes + mtimes) after both runs.
# ---------------------------------------------------------------------
snap > "$SCRATCH/realbe.after"
if ! diff -u "$SCRATCH/realbe.before" "$SCRATCH/realbe.after" >&2; then
    echo "FAIL: real \$HOME/.be was modified by the run" >&2
    exit 1
fi

echo "get/43-home-bound-no-escape: OK"
