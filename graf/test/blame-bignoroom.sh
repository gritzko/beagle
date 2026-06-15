#!/bin/sh
#  blame-bignoroom.sh — BLAME-004 regression: blaming a large, long-history
#  file must finish OK, not BNOROOM.
#
#  GRAFFileWeave folds every kept blob version into a single accumulating
#  weave.  Each fold's WEAVEFromBlob/WEAVEApply/WEAVEDiff carve transient
#  scratch off ABC_BASS sized to the weave-so-far.  Pre-fix those folds were
#  invoked BARE (a plain `ret = WEAVE...()`), so their per-step BASS scratch
#  was never rewound: it piled up across every version until a u32bAcquire in
#  weave_diff_core hit the 1 GB arena cap and returned BNOROOM mid-walk —
#  exit 22, zero output.  A small file blames fine; a large file with enough
#  history is un-blameable.  The fix wraps each fold in try() (PRO.h), which
#  rewinds BASS per iteration; the woven result survives in each weave's own
#  mmap'd TLV.
#
#  This builds a ~4000-line file changed across N commits (heavy per-commit
#  churn so the weave grows fast) and asserts `graf blame` exits 0 and emits
#  one annotated line per source line.  Pre-fix: BNOROOM, exit != 0, 0 lines.

set -eu
BIN=${BIN:-$(dirname "$(command -v be)")}
BE="$BIN/be"; GRAF="$BIN/graf"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
for t in "$BE" "$GRAF"; do [ -x "$t" ] || { echo "FAIL: $t not executable"; exit 1; }; done

TEST_ID=${TEST_ID:-GRAFblamebignoroom}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base); mkdir -p "$TMP"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null||true; rmdir "${TMP%/*/*}" 2>/dev/null||true; }' EXIT INT TERM
fail(){ echo "FAIL: $*" >&2; exit 1; }

R="$TMP/repo"; rs_wt_at "$R"
git init --quiet -b main .; git config user.email t@t; git config user.name t

NLINES=4000
NCOMMITS=80

#  Seed a wide file; one distinct function per line keeps tokens unique.
seq 1 "$NLINES" | awk '{printf "int line_%04d(int x) { return x + %d; }\n", $1, $1}' > big.c

i=1
while [ "$i" -le "$NCOMMITS" ]; do
    #  Rewrite ~1/5 of the lines each commit (a different fifth each time)
    #  so the accumulating weave gains many new tokens per fold — that is
    #  what drives the pre-fix BASS exhaustion within a modest commit count.
    awk -v v="$i" 'NR%5==(v%5){printf "int line_%04d_v%d(int x) { return x * %d + %d; }\n", NR, v, NR, v; next}{print}' big.c > big.c.new
    mv big.c.new big.c
    hh=$((6 + i / 60)); mm=$((i % 60))
    touch -d "2026-04-20 $(printf '%02d:%02d' "$hh" "$mm"):00" big.c
    "$BE" post -m "v$i" "?v0.0.$i" >/dev/null 2>&1
    i=$((i + 1))
done
"$GRAF" index >/dev/null 2>&1 || fail "graf index failed"

#  Blame the big file.  Pre-fix this BNOROOMs (exit != 0, no output).
set +e
BOUT=$("$GRAF" --plain blame:big.c 2>/tmp/blame_bignoroom.err)
RC=$?
set -e
ERR=$(cat /tmp/blame_bignoroom.err 2>/dev/null || true)

[ "$RC" -eq 0 ] || fail "blame exited $RC (err: ${ERR:-none}) — BNOROOM regression?"
printf '%s\n' "$ERR" | grep -q 'BNOROOM' && fail "blame reported BNOROOM"

#  One annotated line per source line.  Allow a small slack for the
#  renderer's trailing-newline / framing rows.
LINES=$(printf '%s\n' "$BOUT" | wc -l)
[ "$LINES" -ge "$NLINES" ] || fail "blame emitted $LINES lines, want >= $NLINES"

#  Tip content must survive: the last commit's rewritten lines carry _v$NCOMMITS.
printf '%s\n' "$BOUT" | grep -q "_v${NCOMMITS}(" \
    || fail "blame missing tip content (_v${NCOMMITS})"

echo "PASS: blamed a ${NLINES}-line file over ${NCOMMITS} commits ($LINES lines, exit 0)"
