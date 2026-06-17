#!/bin/sh
#  shareset.sh — GET-027 regression: `be put '?#<sha>'` (trunk sha-reset)
#  must record a SINGLE-sigil `?#<sha>` REFS row, never the corrupt
#  double-sigil `?#?<sha>`.  PUTSetBranch used to prepend a stray `?` to
#  the ref value, so URIutf8Feed emitted `#` + `?<sha>` = `?#?<sha>`,
#  shifting the recorded tip by one char (the "extra char in a sha"
#  shape the ticket flags alongside the wire-clone bug).
#
#  Run: BIN=build-debug/bin sh sniff/test/shareset.sh
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TEST_ID=${TEST_ID:-SNIFFshareset}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

WT="$TMP/wt"
rs_wt_at "$WT"

# --- step 1: two commits so we have a non-tip sha to reset to ---------
echo "=== 1. base commits ==="
echo "a" > a.txt
sniff put a.txt >/dev/null
sniff post -m "base" >/dev/null
SHA0=$(grep -oE '[0-9a-f]{40}' .be/*/refs | head -1)
[ -n "$SHA0" ] || fail "no commit sha recorded"
echo "b" >> a.txt
sniff put a.txt >/dev/null
sniff post -m "second" >/dev/null
note "two commits; first tip = $SHA0"

# --- step 2: reset trunk to the earlier sha via `put ?#<sha>` ---------
echo "=== 2. sniff put '?#<sha>' (trunk reset) ==="
sniff put "?#$SHA0" >/dev/null

# --- step 3: NO double-sigil row may exist ----------------------------
echo "=== 3. assert single-sigil REFS rows ==="
REFS=$(ls .be/*/refs | head -1)
if grep -q '#?[0-9a-f]' "$REFS"; then
    echo "--- offending refs ---" >&2
    grep '#?[0-9a-f]' "$REFS" >&2
    fail "double-sigil '?#?<sha>' row recorded (GET-027 regression)"
fi
note "no '?#?<sha>' rows present"

# --- step 4: the reset value resolves back to exactly SHA0 ------------
echo "=== 4. trunk tip resolves to the reset sha ==="
TIP=$(grep -oE '\?#[0-9a-f]{40}' "$REFS" | tail -1 | sed 's/^?#//')
[ "$TIP" = "$SHA0" ] || fail "trunk reset row = $TIP, expected $SHA0"
note "trunk tip correctly = $SHA0"

echo "PASS: shareset"
