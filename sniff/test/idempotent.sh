#!/bin/sh
#  idempotent.sh — `be`/`sniff status` is idempotent and never hides a
#  real modification (DIS-023).
#
#  Two consecutive read-only status runs with no edit and no commit in
#  between must report the SAME result; a genuinely-modified tracked
#  file must stay `mod` on every run.  The historical bug: the dirty
#  gate was mtime-only, so a file whose mtime coincides with a known
#  stamp ts (a baseline get/post row) was reported clean even though
#  its on-disk bytes differ from the baseline blob.  The fix: the
#  CLASS_BOTH fast-path confirms the on-disk content hashes to the
#  baseline blob sha before declaring a file clean.
#
#  Run: BIN=build-debug/bin sh sniff/test/idempotent.sh
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TEST_ID=${TEST_ID:-SNIFFidempotent}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

#  Pull the trailing summary line ("?\t<n> ok, <m> mod") out of a
#  status capture and echo the ok/mod counts as "<ok> <mod>".
counts() {
    awk -F'\t' '
        /^\?/ {
            line = $0
            ok = 0; mod = 0
            n = split(line, parts, ",")
            for (i = 1; i <= n; i++) {
                if (parts[i] ~ /ok/)  { gsub(/[^0-9]/, "", parts[i]); ok  = parts[i] }
                if (parts[i] ~ /mod/) { gsub(/[^0-9]/, "", parts[i]); mod = parts[i] }
            }
        }
        END { printf("%d %d\n", ok, mod) }
    ' "$1"
}

# ====================================================================
# Scenario 1 — plain edit stays `mod` across two status runs.
# ====================================================================
echo "=== 1. edited file stays mod across re-runs ==="
WT="$TMP/wt1"
rs_wt_at "$WT"
for i in 1 2 3 4 5; do echo "f$i v1" > f$i.txt; done
sniff post -m "base" >/dev/null
note "committed 5 files"
sleep 0.1
for i in 1 2 3 4; do echo "f$i EDITED" > f$i.txt; done

sniff status >"$TMP/s1.a" 2>&1 || fail "status run 1 failed"
sniff status >"$TMP/s1.b" 2>&1 || fail "status run 2 failed"
c1=$(counts "$TMP/s1.a"); c2=$(counts "$TMP/s1.b")
note "run 1: $c1 ok/mod ; run 2: $c2 ok/mod"
[ "$c1" = "1 4" ] || fail "run 1 expected '1 4' (ok mod); got '$c1'"
[ "$c2" = "1 4" ] || fail "run 2 expected '1 4' (ok mod); got '$c2'"
[ "$c1" = "$c2" ] || fail "status not idempotent: '$c1' then '$c2'"
note "plain edit idempotent (no flip to ok)"

# ====================================================================
# Scenario 2 — a modified file whose mtime coincides with a known
#              stamp ts must still report `mod` (mtime-only gate trap).
#
#  ref.txt keeps its post-stamp mtime; a.txt is genuinely edited and
#  then has its mtime restored to that exact stamp ts via `touch -r`.
#  The mtime-only gate would call a.txt clean; the content double-check
#  must keep it `mod` on BOTH runs.
# ====================================================================
echo "=== 2. mtime == known stamp ts but content differs → mod ==="
WT="$TMP/wt2"
rs_wt_at "$WT"
echo "AAAA"     > a.txt
echo "ref-keep" > ref.txt
sniff post -m "base" >/dev/null
note "a.txt + ref.txt committed (share the post stamp ts)"
sleep 0.2
echo "EDITED-DIFFERENT" > a.txt    # genuine content change
#  Restore a.txt's mtime to ref.txt's (a known stamp ts).
touch -r ref.txt a.txt
note "a.txt mtime restored to a known stamp ts; content still edited"

sniff status >"$TMP/s2.a" 2>&1 || fail "status run 1 failed"
sniff status >"$TMP/s2.b" 2>&1 || fail "status run 2 failed"
c1=$(counts "$TMP/s2.a"); c2=$(counts "$TMP/s2.b")
note "run 1: $c1 ok/mod ; run 2: $c2 ok/mod"
grep -E "[[:space:]]mod[[:space:]]+a\.txt" "$TMP/s2.a" >/dev/null \
    || fail "run 1: a.txt should be mod; status: $(cat "$TMP/s2.a")"
grep -E "[[:space:]]mod[[:space:]]+a\.txt" "$TMP/s2.b" >/dev/null \
    || fail "run 2: a.txt flipped to clean (non-idempotent); status: $(cat "$TMP/s2.b")"
[ "$c1" = "$c2" ] || fail "status not idempotent: '$c1' then '$c2'"
note "content-edited file stays mod despite known-stamp mtime"

# ====================================================================
# Scenario 3 — clean baseline stays `ok` and idempotent (no false mod).
# ====================================================================
echo "=== 3. clean tree stays ok and idempotent ==="
WT="$TMP/wt3"
rs_wt_at "$WT"
echo "x" > x.txt
echo "y" > y.txt
sniff post -m "base" >/dev/null
sniff status >"$TMP/s3.a" 2>&1 || fail "status run 1 failed"
sniff status >"$TMP/s3.b" 2>&1 || fail "status run 2 failed"
c1=$(counts "$TMP/s3.a"); c2=$(counts "$TMP/s3.b")
[ "$c1" = "2 0" ] || fail "clean tree expected '2 0'; got '$c1'"
[ "$c1" = "$c2" ] || fail "clean status not idempotent: '$c1' then '$c2'"
note "clean tree idempotent ($c1 ok/mod)"

echo "=== all idempotent-status scenarios passed ==="
