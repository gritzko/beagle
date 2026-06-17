#!/bin/sh
#  put/13-slashless-dir — DIS-034 + SUBS-014 repro: `be put <dir>` (NO
#  trailing slash) must recurse a directory exactly like `be put <dir>/`,
#  and an untracked dir that yields nothing to stage must give a clear
#  "no files to stage (did you mean `<dir>/`?)" hint, never a misleading
#  "is unchanged".
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
#    (e) untracked empty dir     → "no files to stage" + "did you mean
#                                  `<dir>/`?" hint, never "is unchanged".
#    (f) tracked clean dir       → "is unchanged", no hint.
#    (g) explicit `<dir>/` form  → "no files to stage", no redundant hint.

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

#  POST-018: dir put now reports one ROWS `put <path>` row per staged
#  file (no count summary); assert both per-file rows.
"$BE" put sub >"$OUT/a.out" 2>&1
if ! grep -qE '^[[:space:]]*put[[:space:]]+sub/a\.txt' "$OUT/a.out" \
   || ! grep -qE '^[[:space:]]*put[[:space:]]+sub/b\.txt' "$OUT/a.out"; then
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
if ! grep -qE '^[[:space:]]*put[[:space:]]+sub/a\.txt' "$OUT/a_slash.out" \
   || ! grep -qE '^[[:space:]]*put[[:space:]]+sub/b\.txt' "$OUT/a_slash.out"; then
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
if ! grep -qE '^[[:space:]]*put[[:space:]]+fresh/x\.txt' "$OUT/b.out" \
   || ! grep -qE '^[[:space:]]*put[[:space:]]+fresh/y\.txt' "$OUT/b.out"; then
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

# === (e) UNTRACKED dir with NO stageable file, no slash (SUBS-014) =====
#  An untracked dir named without a trailing slash that holds nothing
#  stageable (empty here) must NOT claim "is unchanged" (it was never
#  tracked) — it gets a clear "no files to stage" plus a "did you mean
#  `void/`?" hint that names the explicit dir form.
mkdir -p void
"$BE" put void >"$OUT/e.out" 2>"$OUT/e.err" || true
if ! grep -qE 'void has no files to stage' "$OUT/e.out" "$OUT/e.err"; then
    echo "FAIL (e): untracked empty dir should report 'no files to stage'" >&2
    cat "$OUT/e.out" "$OUT/e.err" >&2
    exit 1
fi
if ! grep -qE 'did you mean .void/.' "$OUT/e.out" "$OUT/e.err"; then
    echo "FAIL (e): missing 'did you mean \`void/\`?' hint" >&2
    cat "$OUT/e.out" "$OUT/e.err" >&2
    exit 1
fi
if grep -qE 'void.* is unchanged' "$OUT/e.out" "$OUT/e.err"; then
    echo "FAIL (e): untracked empty dir must NOT claim 'is unchanged'" >&2
    cat "$OUT/e.out" "$OUT/e.err" >&2
    exit 1
fi

# === (f) TRACKED clean dir, no slash — still 'is unchanged' ============
#  A directory that IS tracked and has no dirty file under it is
#  genuinely unchanged; the SUBS-014 hint must NOT fire here.  `sub/`
#  was committed clean above (commit a slash), so `be put sub` on it
#  now reports "is unchanged".
"$BE" put sub >"$OUT/f.out" 2>"$OUT/f.err" || true
if ! grep -qE 'sub/ is unchanged' "$OUT/f.out" "$OUT/f.err"; then
    echo "FAIL (f): tracked clean dir should report 'is unchanged'" >&2
    cat "$OUT/f.out" "$OUT/f.err" >&2
    exit 1
fi
if grep -qE 'did you mean' "$OUT/f.out" "$OUT/f.err"; then
    echo "FAIL (f): tracked clean dir must NOT carry the 'did you mean' hint" >&2
    cat "$OUT/f.out" "$OUT/f.err" >&2
    exit 1
fi

# === (g) UNTRACKED empty dir WITH a trailing slash — no hint ==========
#  When the user already typed the explicit `<dir>/` form, the
#  "did you mean `<dir>/`?" hint is redundant and must be suppressed;
#  the message is just "no files to stage".
mkdir -p void2
"$BE" put void2/ >"$OUT/g.out" 2>"$OUT/g.err" || true
if ! grep -qE 'void2/ has no files to stage' "$OUT/g.out" "$OUT/g.err"; then
    echo "FAIL (g): trailing-slash empty dir should report 'no files to stage'" >&2
    cat "$OUT/g.out" "$OUT/g.err" >&2
    exit 1
fi
if grep -qE 'did you mean' "$OUT/g.out" "$OUT/g.err"; then
    echo "FAIL (g): explicit trailing-slash form must NOT carry a 'did you mean' hint" >&2
    cat "$OUT/g.out" "$OUT/g.err" >&2
    exit 1
fi
