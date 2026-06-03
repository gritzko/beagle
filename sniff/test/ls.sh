#!/bin/sh
#  ls.sh — `sniff ls:` / `sniff lsr:` / `be ls:` smoke test (verbless
#  per VERBS.md §"View projectors").  `ls:` lists one level (immediate
#  files + one `dir` row per subdir); `lsr:` is recursive — every
#  descendant gets its own status row (put/new/mov/mod/del/mis/unk/eq).
#  Confirms `be ls:` is non-destructive (does not fall through to
#  `sniff get`).
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TEST_ID=${TEST_ID:-SNIFFls}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

WT="$TMP/wt"
rs_wt_at "$WT"
mkdir -p sub
echo "a line"     > a.txt
echo "sub file 1" > sub/s1.txt
echo "sub file 2" > sub/s2.txt
sniff post -m "base" >/dev/null
note "base commit staged"

# --- one-level: clean wt — top-level file + collapsed subdir row -----
echo "=== 1. sniff ls: (one level: a.txt eq + sub/ dir row) ==="
out=$(sniff 'ls:' 2>/dev/null)
echo "$out" | grep -qE '^ *[^ ]+ +eq +a\.txt$'      || fail "missing eq row for a.txt"
echo "$out" | grep -qE '^ *[^ ]* +dir +sub/$'         || fail "missing dir row for sub/"
echo "$out" | grep -vqE '(^| )sub/s1.txt$'             || fail "sub/s1.txt leaked into one-level ls"
note "one-level: a.txt (eq) + sub/ (dir)"

# --- recursive: every descendant gets its own row --------------------
echo "=== 2. sniff lsr: (recursive — every path is eq) ==="
out=$(sniff 'lsr:' 2>/dev/null)
echo "$out" | grep -qE '^ *[^ ]+ +eq +a\.txt$'      || fail "missing eq row for a.txt"
echo "$out" | grep -qE '^ *[^ ]+ +eq +sub/s1\.txt$' || fail "missing eq row for sub/s1.txt"
echo "$out" | grep -qE '^ *[^ ]+ +eq +sub/s2\.txt$' || fail "missing eq row for sub/s2.txt"
echo "$out" | grep -vqE '^ *[^ ]+ +dir +sub/$'        || fail "lsr: should not collapse to dir row"
note "recursive: 3 eq rows present, no dir collapse"

# --- dirty wt: edited path flips to `mod`, others stay `eq` -----------
echo "=== 3. sniff lsr: after editing a.txt (mod + eq mix) ==="
echo "edited" >> a.txt
out=$(sniff 'lsr:' 2>/dev/null)
echo "$out" | grep -qE '^ *[^ ]+ +mod +a\.txt$'     || fail "a.txt should be mod"
echo "$out" | grep -qE '^ *[^ ]+ +eq +sub/s1\.txt$' || fail "sub/s1.txt should stay eq"
note "edited a.txt → mod row; siblings stay eq"

# --- untracked: new file shows as `unk` -------------------------------
echo "=== 4. sniff ls: with an untracked file (unk row at top level) ==="
echo "fresh" > new.txt
out=$(sniff 'ls:' 2>/dev/null)
echo "$out" | grep -qE '^ *[^ ]+ +unk +new\.txt$'    || fail "new.txt should be unk"
rm new.txt
note "untracked new.txt → unk row"

# --- prefix filter: `ls:sub/` lists subdir's immediate children ------
echo "=== 5. sniff ls:sub/ (subdir scope, one level) ==="
out=$(sniff 'ls:sub/' 2>/dev/null)
echo "$out" | grep -qE '(^| )sub/s1.txt$'             || fail "missing sub/s1.txt"
echo "$out" | grep -qE '(^| )sub/s2.txt$'             || fail "missing sub/s2.txt"
echo "$out" | grep -vqE '(^| )a.txt$'                  || fail "a.txt leaked into sub/ scope"
note "subdir scope ok"

# --- TLV mode ---------------------------------------------------------
echo "=== 6. sniff --tlv ls: produces HUNK-framed output ==="
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
echo "=== 7. be ls: (verbless) is non-destructive ==="
mtime_before=$(stat -c %Y a.txt 2>/dev/null || stat -f %m a.txt)
out=$(be 'ls:' 2>/dev/null)
mtime_after=$(stat -c %Y a.txt 2>/dev/null || stat -f %m a.txt)
[ "$mtime_before" = "$mtime_after" ] \
    || fail "be ls: touched a.txt (mtime changed)"
echo "$out" | grep -qE '^ *[^ ]+ +mod +a\.txt$' || fail "be ls: missing mod row for a.txt"
note "be ls: non-destructive and matches sniff ls:"

echo
echo "=== sniff ls: / lsr: OK ==="
