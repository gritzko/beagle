#!/bin/sh
#  be-status-ahead-behind.sh — GET-021.
#
#  A worktree silently diffs against an advanced cur it was never
#  materialized to.  `be status` must WARN when the wt's cur ≠ the
#  branch tip: PREPEND a commit-difference block above the per-file
#  rows — one `miss <sha> …` row per BEHIND commit (in the branch tip,
#  not local) and one `post <sha> …` row per AHEAD commit (local, not
#  in the branch tip).  Rows are clickable `commit:?<sha>`.
#
#  Two worktrees share one `.be` store:
#    * wt-B lands a commit → the shared branch tip (REFS) advances.
#    * wt-A is left behind (its wtlog cur < REFS tip).
#  `be status` in wt-A must show a `miss <sha>` row; after `be get ?`
#  brings wt-A current the row clears.  Symmetric AHEAD (`post` row,
#  cur > REFS tip) and DIVERGED (both) cases round out the table.
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
BE="$BIN/be"

TEST_ID=${TEST_ID:-BEstatusahead}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
mkdir -p "$TMP"; echo "Running in $PWD"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

# Tail sha of a worktree's wtlog (`.be` file or `.be/wtlog` dir).
cur_of() {
    f="$1/.be"; [ -d "$f" ] && f="$f/wtlog"
    awk -F'\t' '$2=="post" || $2=="get" { last=$3 } END {
        h=last; sub(/^[^#]*#/, "", h)
        if (length(h)==40 && h ~ /^[0-9a-f]+$/) print h }' "$f"
}

# `be status` of a worktree, ANSI-free.
status_of() { ( cd "$1" && "$BE" --plain 2>/dev/null ); }

# --- 1. primary: seed commit ----------------------------------------
echo "=== 1. primary: seed ==="
PRIM="$TMP/prim"; rs_wt_at "$PRIM"
echo hello > README
"$BE" post --seq -m "seed" >/dev/null
SEED=$(cur_of "$PRIM")
[ -n "$SEED" ] || fail "primary cur empty after seed post"
note "primary SEED=$SEED"

# --- 2. secondary worktree sharing the store ------------------------
echo "=== 2. secondary wt (shared store) ==="
WTB="$TMP/wtb"; mkdir -p "$WTB"; cd "$WTB"
"$BE" get --seq "$PRIM" >/dev/null 2>/dev/null || true
[ "$(cur_of "$WTB")" = "$SEED" ] || fail "secondary wt not at SEED"
note "secondary wt at SEED"

# --- 3. wt-B lands TWO commits → shared branch tip advances by 2 -----
echo "=== 3. wt-B advances the shared branch tip (2 commits) ==="
echo world > NEWFILE
"$BE" put --seq NEWFILE >/dev/null
"$BE" post --seq -m "behind-advance-1: NEWFILE" >/dev/null
MID=$(cur_of "$WTB")
echo more >> NEWFILE
"$BE" put --seq NEWFILE >/dev/null
"$BE" post --seq -m "behind-advance-2: NEWFILE" >/dev/null
TIPB=$(cur_of "$WTB")
[ "$TIPB" != "$SEED" ] && [ "$TIPB" != "$MID" ] || fail "wt-B cur did not advance twice"
note "shared branch tip now $TIPB; primary cur still $(cur_of "$PRIM")"
[ "$(cur_of "$PRIM")" = "$SEED" ] || fail "primary cur moved unexpectedly"

# --- 4. BEHIND: `be status` in the primary shows TWO `miss` rows -----
echo "=== 4. behind: primary status shows miss rows (newest-first) ==="
OUT=$(status_of "$PRIM")
printf '%s\n' "$OUT" | sed 's/^/    | /'
SHORT_TIPB=$(printf '%s' "$TIPB" | cut -c1-8)
SHORT_MID=$(printf '%s' "$MID" | cut -c1-8)
#  COMMIT-001 row grammar: `<date>  miss ?<hashlet>#<subject>` with a
#  hidden `commit:?<sha>` nav.  Match the visible `miss ?<hashlet>` cell.
printf '%s\n' "$OUT" | grep -Eq "miss +\??${SHORT_TIPB}" \
    || fail "behind: no 'miss ${SHORT_TIPB}' (tip) row in primary status"
printf '%s\n' "$OUT" | grep -Eq "miss +\??${SHORT_MID}" \
    || fail "behind: no 'miss ${SHORT_MID}' (mid) row in primary status"
MISS_N=$(printf '%s\n' "$OUT" | grep -Ec "miss +\?")
[ "$MISS_N" -eq 2 ] || fail "behind: expected 2 miss rows, got $MISS_N"
note "behind: 2 miss rows ($SHORT_TIPB, $SHORT_MID)"
#  Newest-first: the tip row precedes the mid row.
printf '%s\n' "$OUT" | grep -E "miss +\?" | head -1 | grep -q "$SHORT_TIPB" \
    || fail "behind: tip miss row not first (newest-first order)"
note "behind: miss rows newest-first"
#  Row is clickable: the hidden nav carries `commit:?<hashlet>` (the
#  same 8-hex handle `be log:` rows use).  The --plain renderer drops
#  the U-tagged nav; assert the clickable form in the raw --tlv stream.
( cd "$PRIM" && "$BE" --tlv 2>/dev/null ) | strings | grep -q "commit:?${SHORT_TIPB}" \
    || fail "behind: miss row missing clickable commit:?${SHORT_TIPB} nav"
note "behind: rows clickable (commit:?<hashlet>)"
#  Summary carries the behind count.
printf '%s\n' "$OUT" | grep -q "behind 2" \
    || fail "behind: summary missing 'behind 2'"
note "summary reports behind 2"

# --- 5. rows clear after `be get ?` brings the primary current ------
echo "=== 5. be get ? clears the miss row ==="
( cd "$PRIM" && "$BE" get --seq '?' >/dev/null 2>/dev/null )
[ "$(cur_of "$PRIM")" = "$TIPB" ] || fail "primary not current after be get ?"
OUT=$(status_of "$PRIM")
printf '%s\n' "$OUT" | sed 's/^/    | /'
printf '%s\n' "$OUT" | grep -Eq "^ *miss " \
    && fail "miss row did not clear after be get ?"
printf '%s\n' "$OUT" | grep -Eq "behind|ahead" \
    && fail "behind/ahead note lingered for an up-to-date wt"
note "current wt: no commit block, no behind/ahead note"

# --- 6. AHEAD: cur > branch tip → `post` row ------------------------
#  Simulate a peer-stale branch tip (the cur-advanced-past-tip class):
#  rewind the shared REFS by one row so the wt's cur is a descendant
#  the REFS tip does not name.  Drop the stale ref idx so the next
#  resolve rebuilds it.
echo "=== 6. ahead: cur descendant of a stale branch tip ==="
REFS=$(find "$PRIM/.be" -name refs | head -1)
[ -n "$REFS" ] || fail "store refs file not found"
LINES=$(wc -l < "$REFS")
[ "$LINES" -ge 2 ] || fail "refs has too few rows to rewind"
head -n $((LINES - 1)) "$REFS" > "$REFS.tmp" && mv "$REFS.tmp" "$REFS"
rm -f "$(dirname "$REFS")/.refs.idx"
OUT=$(status_of "$PRIM")
printf '%s\n' "$OUT" | sed 's/^/    | /'
printf '%s\n' "$OUT" | grep -Eq "post +\??${SHORT_TIPB}" \
    || fail "ahead: no 'post ${SHORT_TIPB}' row in primary status"
printf '%s\n' "$OUT" | grep -q "ahead 1" \
    || fail "ahead: summary missing 'ahead 1'"
note "ahead: post ${SHORT_TIPB} row + 'ahead 1'"

echo "=== be-status-ahead-behind OK ==="
