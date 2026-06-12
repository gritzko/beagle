#!/bin/sh
#  blame-identical.sh — BLAME-002 correctness: the index-served path
#  descent (DAGChildStep over materialised (tree,name)→child edges) must
#  produce byte-for-byte identical blame to the keeper-inflate descent.
#  Hermetic: blames the SAME store + binary twice — once index-served
#  (default), once with GRAF_BLAME_NOINDEX=1 forcing the keeper-inflate
#  fallback — and asserts the two `--plain` outputs are identical.
set -eu
BIN=${BIN:-$(dirname "$(command -v be 2>/dev/null || echo /nonexistent/be)")}
BE="$BIN/be"; GRAF="$BIN/graf"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
for t in "$BE" "$GRAF"; do
    [ -x "$t" ] || { echo "FAIL: $t not executable" >&2; exit 1; }
done

TEST_ID=${TEST_ID:-GRAFblameident}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base); mkdir -p "$TMP"
trap '_rc=$?; rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null||true; rmdir "${TMP%/*/*}" 2>/dev/null||true; exit $_rc' EXIT INT TERM
fail(){ echo "FAIL: $*" >&2; exit 1; }

R="$TMP/repo"; rs_wt_at "$R"
git init --quiet -b main .; git config user.email t@t; git config user.name t
mkdir -p a/b/c/d other
#  A multi-line file so blame renders many attributed rows; each change
#  rewrites it with a different line count + content, exercising the
#  weave fold across versions.
printf 'line1\nline2\nline3\nline4\nline5\n' > a/b/c/d/deep.c

N=${N:-40}
i=1
while [ "$i" -le "$N" ]; do
    printf 'int noise%s(void){return %s;}\n' "$i" "$i" > other/noise.c
    if [ $((i % 7)) -eq 0 ]; then
        { printf 'line1\n'; printf 'changed-at-%s\n' "$i"; \
          printf 'line3\nline4\nextra%s\nline5\n' "$i" "$i"; } \
          > a/b/c/d/deep.c
    fi
    touch -d "2026-04-20 12:00:$(printf '%02d' $((i % 60)))" \
        other/noise.c a/b/c/d/deep.c 2>/dev/null || true
    "$BE" post -m "c$i" "?v0.0.$i" >/dev/null 2>&1
    i=$((i + 1))
done
"$GRAF" index >/dev/null 2>&1 || fail "graf index failed"

#  Index-served vs keeper-inflate descent — must be byte-identical.
IDX=$(                          "$GRAF" --plain 'blame:a/b/c/d/deep.c' 2>/dev/null) \
    || fail "index-served blame failed"
INFL=$(GRAF_BLAME_NOINDEX=1     "$GRAF" --plain 'blame:a/b/c/d/deep.c' 2>/dev/null) \
    || fail "keeper-inflate blame failed"

#  Sanity: index-served descent must actually inflate ZERO trees, while
#  the forced fallback inflates some — proving both paths really ran.
TI_IDX=$(GRAF_BLAME_STATS=1                  "$GRAF" --plain 'blame:a/b/c/d/deep.c' 2>&1 >/dev/null \
        | sed -n 's/.*treeinfl=\([0-9]*\).*/\1/p')
TI_INF=$(GRAF_BLAME_STATS=1 GRAF_BLAME_NOINDEX=1 "$GRAF" --plain 'blame:a/b/c/d/deep.c' 2>&1 >/dev/null \
        | sed -n 's/.*treeinfl=\([0-9]*\).*/\1/p')
[ "${TI_IDX:-1}" -eq 0 ] || fail "index-served treeinfl=$TI_IDX (expected 0 — descent not index-served)"
[ "${TI_INF:-0}" -gt 0 ] || fail "fallback treeinfl=$TI_INF (expected >0 — fallback path not exercised)"

if [ "$IDX" = "$INFL" ]; then
    echo "PASS: index-served blame byte-identical to keeper-inflate blame" \
         "($(printf '%s' "$IDX" | grep -c '^') lines; treeinfl idx=$TI_IDX infl=$TI_INF)"
else
    printf '%s\n' "$IDX"  > "$TMP/idx.txt"
    printf '%s\n' "$INFL" > "$TMP/infl.txt"
    echo "--- diff index-served vs keeper-inflate ---" >&2
    diff "$TMP/infl.txt" "$TMP/idx.txt" >&2 || true
    fail "blame output DIFFERS between index-served and keeper-inflate descent"
fi
