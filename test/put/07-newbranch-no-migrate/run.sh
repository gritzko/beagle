#!/bin/sh
#  put/07-newbranch-no-migrate — under the FLAT store layout, creating a
#  new branch at the parent tip is a pure REF op: no per-branch dir, no
#  object migration.  The child resolves the parent tip from the one
#  shared project pool, and the operation must not hang on a long chain.
#
#  Spec / rationale (keeper/INDEX.md §"Storage layout"):
#    All objects live in the one project shard `.be/$P/`; branches are
#    REFS rows.  A new branch pointed at an existing tip copies nothing.
#    (The originating report: `be put ?recover/#<sha>` hung on a long
#    trunk chain because it tried to migrate the whole FP chain.)
#
#  Repro shape:
#    1. Build N>5 commits on trunk (long-ish FP chain).
#    2. `be put ?recover/#<trunk-tip>` mints a new child branch
#       label pointing at trunk's current tip (must not hang).
#    3. Assert: NO per-branch dir `.be/$P/recover/` is created.
#    4. Switch into ?recover and read x.txt — content resolves from
#       the shared pool, no migration required.

. "$(dirname "$0")/../../lib/case.sh"

# Anchor project shard at .be/$P/ so subsequent be invocations
# don't derive the project name from the first URI's basename.
"$BE" put "?/$P/" 2>/dev/null || true

LOGS=$(cd .. && pwd)/logs-$NAME
rm -rf "$LOGS"; mkdir -p "$LOGS"

ref_tip() {
    "$BIN/keeper" refs 2>/dev/null | awk -v k="$1" '
        { sub(/^[[:space:]]+/, "")
          tab = index($0, "\t"); if (tab == 0) next
          kf = substr($0, 1, tab - 1); if (kf != k) next
          n = split($0, toks, /[[:space:]]+/)
          v = toks[n]; sub(/^\?/, "", v); print v; exit
        }'
}

# ------------------------------------------------------------------
# 1. Build 8 commits on trunk so the FP chain is non-trivial.
# ------------------------------------------------------------------
N=8
i=1
while [ "$i" -le "$N" ]; do
    sleep 0.02
    echo "x v$i" > x.txt
    "$BE" put x.txt > "$LOGS/$(printf '%02d' $i).put.out" 2>&1
    "$BE" post "t$i" > "$LOGS/$(printf '%02d' $i).post.out" 2>&1
    i=$((i + 1))
done

TRUNK_TIP=$(ref_tip '?')
[ -n "$TRUNK_TIP" ] || {
    echo "FAIL: no trunk tip after $N commits" >&2
    ls -la "$LOGS" >&2
    exit 1
}

# ------------------------------------------------------------------
# 2. `be put ?recover/#<trunk-tip>` — new ref, child of trunk.
#    Per the bug report this hangs / migrates the entire chain.
#    Cap the run so a hang surfaces as a deterministic failure
#    instead of a CI timeout.
# ------------------------------------------------------------------
PUT_OUT="$LOGS/put-recover.out"
PUT_ERR="$LOGS/put-recover.err"
if ! timeout 30 "$BE" put "?recover/#$TRUNK_TIP" \
        > "$PUT_OUT" 2> "$PUT_ERR"; then
    rc=$?
    echo "FAIL: be put ?recover/#<tip> failed or hung (rc=$rc)" >&2
    cat "$PUT_ERR" >&2
    exit 1
fi

[ ! -d ".be/$P/recover" ] || {
    echo "FAIL: per-branch shard dir .be/$P/recover must NOT exist (flat layout)" >&2
    ls -la ".be/$P" >&2
    exit 1
}

[ "$(ref_tip '?recover/')" = "$TRUNK_TIP" ] || {
    echo "FAIL: ?recover/ tip didn't land at trunk tip $TRUNK_TIP (got '$(ref_tip '?recover/')')" >&2
    exit 1
}

# ------------------------------------------------------------------
# 3. Flat layout: NO per-branch shard dir exists.  All commits live in
#    the one project pool; the new branch is a REFS row only.
# ------------------------------------------------------------------
[ ! -d ".be/$P/recover" ] || {
    echo "FAIL: per-branch shard dir .be/$P/recover must NOT exist (flat layout)" >&2
    ls -la ".be/$P/recover" >&2
    exit 1
}

# ------------------------------------------------------------------
# 4. Switch into ?recover and check the wt reads the last commit's
#    content (resolved from the shared pool, no migration).
# ------------------------------------------------------------------
"$BE" get '?recover/' > "$LOGS/get-recover.out" 2> "$LOGS/get-recover.err" || {
    echo "FAIL: be get ?recover/ failed after non-migrating PUT" >&2
    cat "$LOGS/get-recover.err" >&2
    exit 1
}
[ "$(cat x.txt 2>/dev/null)" = "x v$N" ] || {
    echo "FAIL: x.txt on ?recover/: expected 'x v$N', got '$(cat x.txt 2>/dev/null)'" >&2
    exit 1
}

rm -rf "$LOGS"
