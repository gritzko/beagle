#!/bin/sh
#  blame.sh — BLAME perf benchmark (BLAME-001/002).
#
#  `graf blame` is inflate-bound: it topo-walks the file's ancestor
#  closure and, per commit, descends the path to the blamed file.  Two
#  cost parameters dominate and this bench makes both brutally obvious:
#
#    blobfetch — file-blob inflates (BLAME-001 target: ~changes, not nord).
#    treeinfl  — tree-object inflates done while descending the path
#                (BLAME-002 target: ~0 once the DAG index carries
#                (tree,name)->child edges and descent is index-only).
#
#  The fixture is built to maximise the inflate-bound work: a LONG
#  history (N commits) where the blamed file `a/b/c/d/deep.c` sits at
#  depth 4, but its blob changes in only a FEW commits — every other
#  commit bumps an unrelated top-level `noise.c`, so the root tree (and
#  every commit) differs while `a/b/c/d/`'s subtree mostly does not.
#
#  Pre-BLAME-002, descending the path costs up to depth tree inflates on
#  EVERY commit whose root tree differs from the last folded one (the
#  index-side root-tree skip rarely fires — the root always bumps).  So
#  treeinfl ~ nord*depth.  Post-BLAME-002 the per-segment child OID comes
#  from an in-memory index lookup; treeinfl collapses toward 0.
#
#  Output: one machine-readable line
#     BLAMEBENCH nord=<N> blobfetch=<B> treeinfl=<T> ms=<wallclock>
#  plus the raw BLAMESTATS line.  Run BIN=<build>/bin sh graf/bench/blame.sh.

set -eu
BIN=${BIN:-$(dirname "$(command -v be 2>/dev/null || echo /nonexistent/be)")}
BE="$BIN/be"; GRAF="$BIN/graf"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
for t in "$BE" "$GRAF"; do
    [ -x "$t" ] || { echo "FAIL: $t not executable" >&2; exit 1; }
done

#  History length and how often the deep file actually changes.  N large
#  enough that nord >> changes so the inflate-bound cost is visible.
N=${N:-120}
CHANGE_EVERY=${CHANGE_EVERY:-30}     # deep.c changes ~N/CHANGE_EVERY times

TEST_ID=${TEST_ID:-GRAFblamebench}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base); mkdir -p "$TMP"
trap '_rc=$?; rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null||true; rmdir "${TMP%/*/*}" 2>/dev/null||true; exit $_rc' EXIT INT TERM
fail(){ echo "FAIL: $*" >&2; exit 1; }

R="$TMP/repo"; rs_wt_at "$R"
git init --quiet -b main .; git config user.email t@t; git config user.name t
mkdir -p a/b/c/d
printf 'int deep(int x){return x;}\n' > a/b/c/d/deep.c

i=1
while [ "$i" -le "$N" ]; do
    printf 'int noise%s(void){return %s;}\n' "$i" "$i" > noise.c
    if [ $((i % CHANGE_EVERY)) -eq 0 ]; then
        printf 'int deep(int x){return x+%s;}\n' "$i" > a/b/c/d/deep.c
    fi
    touch -d "2026-04-20 12:00:$(printf '%02d' $((i % 60)))" noise.c a/b/c/d/deep.c 2>/dev/null || true
    "$BE" post -m "c$i" "?v0.0.$i" >/dev/null 2>&1
    i=$((i + 1))
done
"$GRAF" index >/dev/null 2>&1 || fail "graf index failed"

#  Warm the page cache once (mmap of packs + idx) so the timed run
#  measures CPU (inflate) not first-touch I/O.
GRAF_BLAME_STATS=1 "$GRAF" --plain 'blame:a/b/c/d/deep.c' >/dev/null 2>&1 || true

#  Timed run.  `date +%s%N` → nanoseconds; portable enough on Linux.
t0=$(date +%s%N 2>/dev/null || echo 0)
STATS=$(GRAF_BLAME_STATS=1 "$GRAF" --plain 'blame:a/b/c/d/deep.c' 2>&1 >/dev/null \
        | grep '^BLAMESTATS' || true)
t1=$(date +%s%N 2>/dev/null || echo 0)
ms=$(( (t1 - t0) / 1000000 ))

[ -n "$STATS" ] || fail "no BLAMESTATS line (blame produced no output?)"
nord=$(printf '%s\n' "$STATS"     | sed -n 's/.*nord=\([0-9]*\).*/\1/p')
bf=$(printf '%s\n' "$STATS"       | sed -n 's/.*blobfetch=\([0-9]*\).*/\1/p')
ti=$(printf '%s\n' "$STATS"       | sed -n 's/.*treeinfl=\([0-9]*\).*/\1/p')

echo "  $STATS"
echo "BLAMEBENCH nord=${nord:-?} blobfetch=${bf:-?} treeinfl=${ti:-?} ms=${ms}"
