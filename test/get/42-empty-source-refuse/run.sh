#!/bin/sh
#  get/42-empty-source-refuse — GET-010.
#
#  `be get "file://<abs-store>?/<proj>"` against a project store whose
#  `refs` is empty/elided must NOT manufacture store damage on the
#  SOURCE side.  Before the fix, the keeper upload-pack SERVER, opened
#  read-only to serve the clone, resolved the `.be`-suffixed source path
#  as a worktree ROOT and re-appended `.be` (→ `<store>/.be/.be/<proj>`),
#  then `KEEPOpenBranch` `FILEMakeDirP`'d that doubled trunk dir even on
#  a READ.  Result: a stray nested `<store>/.be/.be/` shard (with a fresh
#  0-byte `refs` + `.refs.idx`) was written into the source store — the
#  exact empty-refs shape that looks like store damage — and the clone
#  STILL failed (the source had no commits to advertise).
#
#  FIX (keeper/KEEP.c::KEEPOpenBranch): a read-only keeper open never
#  creates directories.  The trunk-dir `FILEMakeDirP` is gated on `rw`;
#  the pack/idx scan (DOGPupOpenAll) already tolerates an absent dir, so
#  an RO serve of an empty/missing shard reads zero refs and the clone
#  fails CLEANLY — no stray shard, no placeholder files in the source.
#
#  HERMETIC: source + dest live entirely inside $SCRATCH; case.sh's
#  shield + firewall keep store discovery from ever reaching the dev
#  box's real $HOME/.be.

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap
#  as their own fresh stores.
rm -rf "$SCRATCH/.be"

# ---------------------------------------------------------------------
# 1. A SOURCE store with a seed commit, then its `refs` emptied — the
#    elided/empty-refs shape a clone must refuse instead of "repairing".
# ---------------------------------------------------------------------
mkdir -p src/.be
(
    cd src
    printf 'hello\n' > seed.txt
    "$BE" put seed.txt >/dev/null 2>&1
    "$BE" post '#seed'  >/dev/null 2>&1
) || { echo "FAIL(setup): source store seed failed" >&2; exit 1; }

SRC_REFS="$SCRATCH/src/.be/src/refs"
[ -f "$SRC_REFS" ] || { echo "FAIL(setup): $SRC_REFS missing" >&2; ls -laR src/.be >&2; exit 1; }
: > "$SRC_REFS"                       # empty/elided refs

#  Snapshot the source `.be` tree (sorted entry list) — must be
#  byte-identical after the failed clone (no stray shard, no new files).
( cd src/.be && find . | sort ) > "$SCRATCH/src.be.before"

# ---------------------------------------------------------------------
# 2. Clone from the empty source via the `.be`-dir store form.  Must
#    FAIL (nothing to clone) — never succeed.
# ---------------------------------------------------------------------
mkdir -p dest
GETRC=0
(
    cd dest
    "$BE" get "file://$SCRATCH/src/.be?/src" > get.out 2> get.err
) || GETRC=$?
[ "$GETRC" -ne 0 ] || {
    echo "FAIL: be get from an empty source must fail, exit was 0" >&2
    cat dest/get.out dest/get.err >&2
    exit 1
}

# ---------------------------------------------------------------------
# 3. ASSERT — the SOURCE store was not mutated.
# ---------------------------------------------------------------------
#  (a) No stray nested `<store>/.be/.be/` shard.
if [ -e "$SCRATCH/src/.be/.be" ]; then
    echo "FAIL: stray nested shard src/.be/.be was created in the source" >&2
    ls -laR "$SCRATCH/src/.be/.be" >&2
    exit 1
fi

#  (b) The source `.be` tree is byte-identical (no new files at all).
( cd src/.be && find . | sort ) > "$SCRATCH/src.be.after"
match "$SCRATCH/src.be.before" "$SCRATCH/src.be.after"

#  (c) The source's own `refs` is still the 0-byte file we left — not
#      re-created elsewhere, not repopulated.
[ -f "$SRC_REFS" ] && [ ! -s "$SRC_REFS" ] || {
    echo "FAIL: source refs changed (expected the empty file we left)" >&2
    ls -la "$SRC_REFS" >&2
    exit 1
}

echo "get/42-empty-source-refuse: OK"
