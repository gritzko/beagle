#!/bin/sh
#  put/02-put-dir — `be put <dir>/` on a tracked subtree.
#
#  Per VERBS.md §PUT line 278: "If `src/` is **tracked** (any baseline
#  entry under it): tracked-dirty files only."  This is the contract.
#  Three sub-properties verified here:
#
#    (a) status undercount — a tracked file modified since the last
#        post must surface in `be` (bare status) as `mod`, not be
#        silently rolled into the `ok` count.
#    (b) dir-prefix expansion — `be put dir/` must stage every
#        tracked-dirty file under the prefix as one `put` row each
#        (matching VERBS.md's "one put row per path" model), so a
#        subsequent `be` reports them as `put`.
#    (c) idempotence — re-running `be put dir/` once nothing is
#        dirty under the prefix must refuse with `PUTNONE`, not
#        silently append a duplicate row.
#
#  Repro originates from the user trace where `be put graf/fuzz/`
#  reported `staged 1 put row(s)`, status kept reporting `0 mod /
#  0 put`, and the next `be post` surprised the user by committing
#  two `M` files invisible to status.

. "$(dirname "$0")/../../lib/case.sh"

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

# --- 1. baseline: tracked dir/a.txt + dir/b.txt --------------------
mkdir -p dir
cp "$CASE/01.a.txt" dir/a.txt
cp "$CASE/02.b.txt" dir/b.txt
"$BE" put dir/a.txt dir/b.txt >/dev/null 2>&1
"$BE" post baseline            >/dev/null 2>&1

# --- 2. modify both tracked files ---------------------------------
cp "$CASE/03.a-mod.txt" dir/a.txt
cp "$CASE/04.b-mod.txt" dir/b.txt

# --- 3. status before any put — both must show as `mod` -----------
"$BE" >"$OUT/before.out" 2>&1
if ! grep -qE '^[[:space:]]*[^[:space:]]+[[:space:]]+mod[[:space:]]+dir/a\.txt' "$OUT/before.out"; then
    echo "FAIL (a): dir/a.txt not reported as mod" >&2
    cat "$OUT/before.out" >&2
    exit 1
fi
if ! grep -qE '^[[:space:]]*[^[:space:]]+[[:space:]]+mod[[:space:]]+dir/b\.txt' "$OUT/before.out"; then
    echo "FAIL (a): dir/b.txt not reported as mod" >&2
    cat "$OUT/before.out" >&2
    exit 1
fi
if ! grep -qE 'sniff:.*2 mod' "$OUT/before.out"; then
    echo "FAIL (a): summary not '2 mod'" >&2
    cat "$OUT/before.out" >&2
    exit 1
fi

# --- 4. dir-prefix put — expand to per-file put rows --------------
"$BE" put dir/ >"$OUT/put1.out" 2>&1
if ! grep -qE 'staged 2 put row' "$OUT/put1.out"; then
    echo "FAIL (b): expected 'staged 2 put row(s)' from dir-prefix put" >&2
    cat "$OUT/put1.out" >&2
    exit 1
fi

# --- 5. status after put — both must show as `put` ----------------
"$BE" >"$OUT/after.out" 2>&1
if ! grep -qE '^[[:space:]]*[^[:space:]]+[[:space:]]+put[[:space:]]+dir/a\.txt' "$OUT/after.out"; then
    echo "FAIL (b): dir/a.txt not reported as put after dir-prefix put" >&2
    cat "$OUT/after.out" >&2
    exit 1
fi
if ! grep -qE '^[[:space:]]*[^[:space:]]+[[:space:]]+put[[:space:]]+dir/b\.txt' "$OUT/after.out"; then
    echo "FAIL (b): dir/b.txt not reported as put after dir-prefix put" >&2
    cat "$OUT/after.out" >&2
    exit 1
fi
if ! grep -qE 'sniff:.*2 put' "$OUT/after.out"; then
    echo "FAIL (b): summary not '2 put'" >&2
    cat "$OUT/after.out" >&2
    exit 1
fi

# --- 6. re-run dir-prefix put with nothing dirty — must refuse ----
"$BE" put dir/ >"$OUT/put2.out" 2>"$OUT/put2.err" || true
if ! grep -qE 'PUTNONE|nothing|no dirty' "$OUT/put2.out" "$OUT/put2.err"; then
    echo "FAIL (c): re-run on clean dir should refuse with PUTNONE" >&2
    echo "stdout:" >&2; cat "$OUT/put2.out" >&2
    echo "stderr:" >&2; cat "$OUT/put2.err" >&2
    exit 1
fi

# --- 7. post commits both tracked-dirty files ---------------------
"$BE" post 'mod a + b' >"$OUT/post.out" 2>"$OUT/post.err"
grep -q 'M dir/a\.txt' "$OUT/post.out" "$OUT/post.err" || {
    echo "FAIL: post didn't commit M dir/a.txt" >&2
    echo "stdout:" >&2; cat "$OUT/post.out" >&2
    echo "stderr:" >&2; cat "$OUT/post.err" >&2
    exit 1
}
grep -q 'M dir/b\.txt' "$OUT/post.out" "$OUT/post.err" || {
    echo "FAIL: post didn't commit M dir/b.txt" >&2
    echo "stdout:" >&2; cat "$OUT/post.out" >&2
    echo "stderr:" >&2; cat "$OUT/post.err" >&2
    exit 1
}
