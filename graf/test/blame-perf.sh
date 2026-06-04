#!/bin/sh
#  blame-perf.sh — BLAME-001 regression guard: `graf blame` must inflate
#  the file blob ~once per CHANGE, not once per ancestor commit.
#
#  Builds a history of N commits where the blamed file `sub/f.c` changes
#  in only a few of them (the rest bump an unrelated `other/g.c`, so the
#  root tree differs every commit but `sub/`'s subtree does not).  Runs
#  `graf blame:sub/f.c` with GRAF_BLAME_STATS=1 and asserts blobfetch is
#  bounded by the change count, not nord.  Pre-fix blobfetch ≈ nord.

set -eu
BIN=${BIN:-$(dirname "$(command -v be)")}
BE="$BIN/be"; GRAF="$BIN/graf"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
for t in "$BE" "$GRAF"; do [ -x "$t" ] || { echo "FAIL: $t not executable"; exit 1; }; done

TEST_ID=${TEST_ID:-GRAFblameperf}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base); mkdir -p "$TMP"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null||true; rmdir "${TMP%/*/*}" 2>/dev/null||true; }' EXIT INT TERM
fail(){ echo "FAIL: $*" >&2; exit 1; }

R="$TMP/repo"; rs_wt_at "$R"
git init --quiet -b main .; git config user.email t@t; git config user.name t
mkdir -p sub other
printf 'int f(int x){return x;}\n' > sub/f.c

N=16
i=1
while [ "$i" -le "$N" ]; do
    #  Every commit changes other/g.c (root tree bumps); sub/f.c changes
    #  only every 4th commit (≈4 real changes over 16 commits).
    printf 'int g%s(void){return %s;}\n' "$i" "$i" > other/g.c
    if [ $((i % 4)) -eq 0 ]; then
        printf 'int f(int x){return x+%s;}\n' "$i" > sub/f.c
    fi
    touch -d "2026-04-20 12:$(printf '%02d' "$i"):00" sub/f.c other/g.c
    "$BE" post -m "c$i" "?v0.0.$i" >/dev/null 2>&1
    i=$((i + 1))
done
"$GRAF" index >/dev/null 2>&1 || fail "graf index failed"

#  Blame sub/f.c; capture the perf counters.
STATS=$(GRAF_BLAME_STATS=1 "$GRAF" --plain 'blame:sub/f.c' 2>&1 >/dev/null \
        | grep '^BLAMESTATS' || true)
[ -n "$STATS" ] || fail "no BLAMESTATS line (blame produced no output?)"
echo "  $STATS"

nord=$(printf '%s\n' "$STATS" | sed -n 's/.*nord=\([0-9]*\).*/\1/p')
bf=$(printf '%s\n' "$STATS"   | sed -n 's/.*blobfetch=\([0-9]*\).*/\1/p')
[ "${nord:-0}" -ge 12 ] || fail "nord=$nord — history not built (want ≥12)"
#  ≈4 changes + first/last anchors ⇒ ≤8; pre-fix this is ≈nord (16).
[ "${bf:-999}" -le 8 ] || fail "blobfetch=$bf > 8 (should be ~changes, not ~nord=$nord)"
echo "PASS: blame inflated the file blob $bf times over $nord ancestors"
