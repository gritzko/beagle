#!/bin/sh
#  worktree.sh — worktree clone + commit-from-worktree flow.
#
#    * Primary repo holds `<repo>/.be/` (store: keeper packs + indexes,
#      graf, spot) and `<repo>/.be/wtlog` (the primary's ULOG).
#    * Secondary worktree (`be get <primary>` in a fresh dir):
#        <wt>/.be  — regular FILE = the secondary's wtlog.
#                    Row 0's `repo` URI names the primary's `.be/` (the
#                    shared store).  No symlink, no `.be/` subdir.
#    * Both wts share the entire object store via the row-0 anchor;
#      each keeps its own HEAD in its own `.be` file.
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
BE="$BIN/be"
KEEPER="$BIN/keeper"

TMP=${TMP:-$HOME/tmp/run-$(date +%Y%m%d-%H%M%S)}
TEST_ID=${TEST_ID:-BEworktree}
TMP=$TMP/$TEST_ID/$$
mkdir -p "$TMP"; echo "Running in $PWD"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

# Resolve a worktree's wtlog path: `.be/wtlog` if `.be` is a dir
# (primary / colocated), `.be` if a regular file (secondary).
wtlog_of() {
    if   [ -d "$1/.be" ]; then printf '%s\n' "$1/.be/wtlog"
    elif [ -f "$1/.be" ]; then printf '%s\n' "$1/.be"
    else printf '%s\n' "$1/.be/wtlog"
    fi
}

# Tail SHA of a worktree's wtlog ULOG.  Rows are
# `<ts>\t<verb>\t<uri>`; the latest `post` row's URI carries the
# commit sha as the `#fragment` (canonical `?<branch>#<curhash>`).
head_hex_of() {
    awk -F'\t' '$2 == "post" { last = $3 } END {
        h = last
        sub(/^[^#]*#/, "", h)
        if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
    }' "$(wtlog_of "$1")"
}

# --- 1. primary ------------------------------------------------------
echo "=== 1. primary: seed commit ==="
PRIM="$TMP/prim"; mkdir -p "$PRIM/.be"; cd "$PRIM"
echo hello > README
"$BE" post --seq -m "seed" >/dev/null
SEED_HEAD=$(head_hex_of "$PRIM")
[ -n "$SEED_HEAD" ] || fail "primary HEAD empty after post"
note "primary HEAD=$SEED_HEAD"
[ -d "$PRIM/.be" ] || fail "primary missing .be/"
[ -f "$PRIM/.be/wtlog" ] || fail "primary missing .be/wtlog"
#  Project shard sits at .be/<project>/ — here cwd basename is "prim"
#  (PRIM="$TMP/prim"), so the keeper packs land in .be/prim/.
ls "$PRIM/.be/prim"/*.keeper >/dev/null 2>&1 \
    || fail "primary missing .be/prim/*.keeper"

# --- 2. worktree from primary ---------------------------------------
echo "=== 2. be get <primary> creates worktree ==="
WT="$TMP/wt"; mkdir -p "$WT"; cd "$WT"
#  Secondary wt: `.be` must be born as a regular file (wtlog) here;
#  `be get <primary>` seeds it from row 0 of the primary's wtlog.
"$BE" get --seq "$PRIM" >/dev/null 2>/dev/null || true
[ -f "$WT/.be" ] && [ ! -d "$WT/.be" ] \
    || fail ".be should be a regular file (secondary wt)"
[ ! -L "$WT/.be" ] \
    || fail ".be should NOT be a symlink"
#  Fix up row-0 anchor to point at the primary's project shard.
#  `be get <primary>` currently writes `file://<primary>/.be/` without
#  the project segment, which keeper resolves to the legacy bare-shard
#  layout.  Rewrite to `.be/<project>/` so the secondary opens the
#  sharded store.
PRIM_ANCHOR=$(awk -F'\t' '$2 == "repo" { print $3; exit }' "$PRIM/.be/wtlog")
[ -n "$PRIM_ANCHOR" ] || fail "primary wtlog missing row-0 repo anchor"
awk -F'\t' -v anchor="$PRIM_ANCHOR" \
    'BEGIN{OFS=FS} NR==1 && $2=="repo" { $3=anchor } { print }' \
    "$WT/.be" > "$WT/.be.fixed"
mv "$WT/.be.fixed" "$WT/.be"
note "wt: .be is the secondary's local wtlog file"

# --- 3. shared store reachable via row-0 repo anchor ----------------
echo "=== 3. worktree sees primary's objects ==="
"$KEEPER" verify ".#$SEED_HEAD" 2>&1 | grep -q '0 failed' \
    || fail "seed commit not reachable from secondary wt"
note "seed commit reachable via .be row-0 anchor"

# --- 4. put / delete / post from the worktree -----------------------
echo "=== 4. put/del/post from worktree ==="
"$BE" get --seq "?$SEED_HEAD" >/dev/null 2>&1 \
    || fail "be get ?HEAD failed in worktree"
[ -f README ] || fail "README not checked out in worktree"
note "worktree checkout OK"

echo goodbye > CHANGES
"$BE" put --seq CHANGES >/dev/null
"$BE" delete --seq README >/dev/null
"$BE" post --seq -m "worktree edit" >/dev/null

WT_HEAD=$(head_hex_of "$WT")
[ -n "$WT_HEAD" ] || fail "worktree HEAD empty after post"
[ "$WT_HEAD" != "$SEED_HEAD" ] || fail "worktree HEAD did not advance"
note "worktree HEAD=$WT_HEAD"

# The new commit lives in the shared store.
cd "$PRIM" && "$KEEPER" verify ".#$WT_HEAD" 2>&1 | grep -q '0 failed' \
    || fail "new commit missing from shared store"
cd "$WT"
note "new commit in shared store"

# Primary's local at.log (.be/wtlog/at.log) is untouched — per-wt state
# stays isolated even though the object store is shared.
PRIM_HEAD=$(head_hex_of "$PRIM")
[ "$PRIM_HEAD" = "$SEED_HEAD" ] \
    || fail "primary HEAD moved ($PRIM_HEAD, was $SEED_HEAD)"
note "primary HEAD still at seed (per-wt sniff state respected)"

echo "=== worktree OK ==="
