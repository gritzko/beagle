#!/bin/sh
#  ls.sh — `sniff ls:` / `be ls:` smoke test (verbless per VERBS.md
#  §"View projectors").  Asserts the bare-`be`-with-statuses shape:
#  one status hunk per wt path, classified the same way bare-`be`
#  classifies (put/new/mov/mod/del/mis/unk/eq).  Confirms `be ls:`
#  is non-destructive (does not fall through to `sniff get`).
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TMP=${TMP:-$HOME/tmp/run-$(date +%Y%m%d-%H%M%S)}
TEST_ID=${TEST_ID:-SNIFFls}
TMP=$TMP/$TEST_ID/$$
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

WT="$TMP/wt"
mkdir -p "$WT/sub" "$WT/.be"
cd "$WT"
echo "a line"     > a.txt
echo "sub file 1" > sub/s1.txt
echo "sub file 2" > sub/s2.txt
sniff post -m "base" >/dev/null
note "base commit staged"

# --- clean wt: every path classified as `eq` --------------------------
echo "=== 1. sniff ls: (clean wt, every path is eq) ==="
out=$(sniff 'ls:' 2>/dev/null)
echo "$out" | grep -qE '^[^	]*	eq	a\.txt$'      || fail "missing eq row for a.txt"
echo "$out" | grep -qE '^[^	]*	eq	sub/s1\.txt$' || fail "missing eq row for sub/s1.txt"
echo "$out" | grep -qE '^[^	]*	eq	sub/s2\.txt$' || fail "missing eq row for sub/s2.txt"
note "clean wt: 3 eq rows present"

# --- dirty wt: edited path flips to `mod`, others stay `eq` -----------
echo "=== 2. sniff ls: after editing a.txt (mod + eq mix) ==="
echo "edited" >> a.txt
out=$(sniff 'ls:' 2>/dev/null)
echo "$out" | grep -qE '^[^	]*	mod	a\.txt$'     || fail "a.txt should be mod"
echo "$out" | grep -qE '^[^	]*	eq	sub/s1\.txt$' || fail "sub/s1.txt should stay eq"
note "edited a.txt → mod row; siblings stay eq"

# --- untracked: new file shows as `unk` -------------------------------
echo "=== 3. sniff ls: with an untracked file (unk row) ==="
echo "fresh" > new.txt
out=$(sniff 'ls:' 2>/dev/null)
echo "$out" | grep -qE '^[^	]*	unk	new\.txt$'    || fail "new.txt should be unk"
rm new.txt
note "untracked new.txt → unk row"

# --- prefix filter: `ls:sub/` scopes to subdir ------------------------
echo "=== 4. sniff ls:sub/ (subdir scope) ==="
out=$(sniff 'ls:sub/' 2>/dev/null)
echo "$out" | grep -qE '	sub/s1\.txt$'             || fail "missing sub/s1.txt"
echo "$out" | grep -vqE '	a\.txt$'                  || fail "a.txt leaked into sub/ scope"
note "subdir scope ok"

# --- TLV mode ---------------------------------------------------------
echo "=== 5. sniff --tlv ls: produces HUNK-framed output ==="
#  First byte of a HUNK record is the tag letter 'H' (long form, 0x48)
#  or 'h' (compact, 0x68).  Both are valid TLV; HUNKu8sDrain accepts
#  either.
head_byte=$(sniff --tlv 'ls:' 2>/dev/null | head -c1 | od -An -v -tx1 \
            | tr -d ' ')
case "$head_byte" in
    48|68) ;;
    *) fail "--tlv: bad first byte '$head_byte' (want 48 'H' or 68 'h')" ;;
esac
tlv_bytes=$(sniff --tlv 'ls:' 2>/dev/null | wc -c)
[ "$tlv_bytes" -gt 20 ] || fail "--tlv: output too small ($tlv_bytes bytes)"
note "--tlv frame ok (first byte = H/h, $tlv_bytes bytes total)"

# --- be wiring --------------------------------------------------------
echo "=== 6. be ls: (verbless) is non-destructive ==="
mtime_before=$(stat -c %Y a.txt 2>/dev/null || stat -f %m a.txt)
out=$(be 'ls:' 2>/dev/null)
mtime_after=$(stat -c %Y a.txt 2>/dev/null || stat -f %m a.txt)
[ "$mtime_before" = "$mtime_after" ] \
    || fail "be ls: touched a.txt (mtime changed)"
echo "$out" | grep -qE '	mod	a\.txt$' || fail "be ls: missing mod row for a.txt"
note "be ls: non-destructive and matches sniff ls:"

echo
echo "=== sniff ls: OK ==="
