#!/bin/sh
#  put/13-slashless-dir — DIS-034 repro: `be put <dir>` (NO trailing
#  slash) must recurse a directory exactly like `be put <dir>/`.
#
#  The DIS-030 changeset regressed the put-arg classifier so a
#  directory argument was only recognised WITH a trailing slash; a
#  slashless dir fell through to the file-form path, never matched the
#  per-file merge, and was reported "<dir> does not exist — skipped"
#  (→ PUTNONE when it was the only arg).  `be put <dir>/` still worked.
#  This affects BOTH tracked and untracked dirs — it's the missing
#  slash, not untracked-ness.
#
#  Properties verified:
#    (a) tracked dir, no slash   → stages every tracked-dirty file
#                                  under it, identical to `<dir>/`.
#    (b) untracked dir, no slash → stages every file under it.
#    (c) sibling no-over-match   → `be put sub` must NOT stage a
#                                  sibling FILE `sub.txt` whose name
#                                  shares the dir's prefix (the
#                                  trailing-slash boundary guards this).
#    (d) bareword non-path       → a slashless arg that is NOT a path
#                                  (a typo / branch word) is still
#                                  "does not exist", not mis-recursed.

. "$(dirname "$0")/../../lib/case.sh"

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

# === (a) TRACKED dir, no trailing slash ===============================
mkdir -p sub
sleep 0.02; printf 'aaa\n' > sub/a.txt
sleep 0.02; printf 'bbb\n' > sub/b.txt
# A sibling FILE sharing the dir's name-prefix; tracked so its dirty
# state can be observed independently (property c).
sleep 0.02; printf 'side\n' > sub.txt
"$BE" put sub/a.txt sub/b.txt sub.txt >/dev/null
"$BE" post 'baseline' >/dev/null

sleep 0.02; printf 'aaaX\n' > sub/a.txt
sleep 0.02; printf 'bbbY\n' > sub/b.txt
sleep 0.02; printf 'sideX\n' > sub.txt

"$BE" put sub >"$OUT/a.out" 2>&1
if ! grep -qE 'staged 2 put row' "$OUT/a.out"; then
    echo "FAIL (a): 'be put sub' (no slash) did not stage 2 tracked-dirty files" >&2
    cat "$OUT/a.out" >&2
    exit 1
fi

# === (c) sibling no-over-match ========================================
#  Only sub/a.txt + sub/b.txt staged; sub.txt must remain `mod`, never
#  rolled into the dir expansion via a prefix match.
"$BE" >"$OUT/status.out" 2>&1
if ! grep -qE '[[:space:]]put[[:space:]]+sub/a\.txt' "$OUT/status.out" ||
   ! grep -qE '[[:space:]]put[[:space:]]+sub/b\.txt' "$OUT/status.out"; then
    echo "FAIL (a): sub/a.txt and sub/b.txt not reported as put" >&2
    cat "$OUT/status.out" >&2
    exit 1
fi
if ! grep -qE '[[:space:]]mod[[:space:]]+sub\.txt' "$OUT/status.out"; then
    echo "FAIL (c): sibling sub.txt should stay 'mod' (over-matched by 'be put sub')" >&2
    cat "$OUT/status.out" >&2
    exit 1
fi

# Equivalence: a fresh worktree where the SAME dirty set is staged via
# the trailing-slash form yields the identical staged file set.
"$BE" post 'commit a' >/dev/null
sleep 0.02; printf 'aaaZ\n' > sub/a.txt
sleep 0.02; printf 'bbbZ\n' > sub/b.txt
"$BE" put sub/ >"$OUT/a_slash.out" 2>&1
if ! grep -qE 'staged 2 put row' "$OUT/a_slash.out"; then
    echo "FAIL (a): 'be put sub/' (control) did not stage 2 files" >&2
    cat "$OUT/a_slash.out" >&2
    exit 1
fi
"$BE" post 'commit a slash' >/dev/null

# === (b) UNTRACKED dir, no trailing slash =============================
mkdir -p fresh
sleep 0.02; printf '1\n' > fresh/x.txt
sleep 0.02; printf '2\n' > fresh/y.txt
"$BE" put fresh >"$OUT/b.out" 2>&1
if ! grep -qE 'staged 2 put row' "$OUT/b.out"; then
    echo "FAIL (b): 'be put fresh' (untracked dir, no slash) did not stage 2 files" >&2
    cat "$OUT/b.out" >&2
    exit 1
fi
"$BE" post 'commit fresh' >/dev/null

# === (d) bareword non-path must NOT be mis-recursed ===================
#  `nope` is not a path on disk; it stays file-form and is correctly
#  "does not exist", not silently treated as a directory.
"$BE" put nope >"$OUT/d.out" 2>"$OUT/d.err" || true
if ! grep -qE 'nope does not exist' "$OUT/d.out" "$OUT/d.err"; then
    echo "FAIL (d): a non-path bareword should still be 'does not exist'" >&2
    echo "stdout:" >&2; cat "$OUT/d.out" >&2
    echo "stderr:" >&2; cat "$OUT/d.err" >&2
    exit 1
fi
