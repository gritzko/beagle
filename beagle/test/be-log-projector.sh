#!/bin/sh
#  be-log-projector.sh — VERBS.md `log:` projector.
#
#  Wires `be log:?<ref>[#N]` (branch history) and
#  `be log:./<path>?<ref>[#N]` (file history) through to graf's LOG
#  routine, which walks graf's DAG index (COMMIT_PARENT for branch
#  history, PATH_VER for file history) and prints one commit per line.
#
#  Run: BIN=build-asan/bin sh beagle/test/be-log-projector.sh
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
export PATH="$BIN:$PATH"

TMP=${TMP:-$HOME/tmp/run-$(date +%Y%m%d-%H%M%S)}
TEST_ID=${TEST_ID:-be-log-projector}
T=$TMP/$TEST_ID/$$
mkdir -p "$T/.be"
trap 'rm -rf "$T"; rmdir "${T%/*}" 2>/dev/null || true; rmdir "$TMP" 2>/dev/null || true' EXIT INT TERM

FAIL=0
CASE=0
fail() { echo "FAIL [$CASE]: $*" >&2; FAIL=$((FAIL + 1)); }
pass() { echo "PASS [$CASE]: $*"; }

want_lines() {
    out=$1; n=$2
    got=$(grep -cE '^[0-9a-f]{7} ' "$out" || true)
    [ "$got" = "$n" ] || { fail "want $n commit-rows, got $got in $out"; cat "$out"; return; }
    pass "$n commit-rows"
}

want_grep() {
    out=$1; pat=$2
    grep -qE "$pat" "$out" || { fail "missing /$pat/ in $out"; cat "$out"; return; }
    pass "matched /$pat/"
}

# --- Build a 4-commit repo: a.txt touched in c1/c3, b.txt only in c2/c4
R=$T/repo; mkdir -p "$R/.be"; cd "$R"
sniff init >/dev/null

echo a1 > a.txt
be post -m c1 '?v1' >/dev/null

echo b2 > b.txt
be put b.txt >/dev/null
be post -m c2 >/dev/null

echo a3 >> a.txt
be put a.txt >/dev/null
be post -m c3 >/dev/null

echo b4 >> b.txt
be put b.txt >/dev/null
be post -m c4 '?v4' >/dev/null

# --- Case A: full branch log via tip tag (tags/v4 = c4) ----------
CASE=A
be 'log:?v4#10' > "$T/A.out" 2>&1 || true
want_lines "$T/A.out" 4
want_grep  "$T/A.out" 'c4'
want_grep  "$T/A.out" 'c1'

# --- Case B: branch log truncated by #N --------------------------
CASE=B
be 'log:?v4#2' > "$T/B.out" 2>&1 || true
want_lines "$T/B.out" 2
want_grep  "$T/B.out" 'c4'
want_grep  "$T/B.out" 'c3'

# --- Case C: file log of a.txt — only c1 + c3 touched it ---------
CASE=C
be 'log:./a.txt?v4#10' > "$T/C.out" 2>&1 || true
want_lines "$T/C.out" 2
want_grep  "$T/C.out" 'c3'
want_grep  "$T/C.out" 'c1'

# --- Case D: file log of b.txt — only c2 + c4 touched it ---------
CASE=D
be 'log:./b.txt?v4#10' > "$T/D.out" 2>&1 || true
want_lines "$T/D.out" 2
want_grep  "$T/D.out" 'c4'
want_grep  "$T/D.out" 'c2'

# --- Case E: file log truncated by #N ----------------------------
CASE=E
be 'log:./a.txt?v4#1' > "$T/E.out" 2>&1 || true
want_lines "$T/E.out" 1
want_grep  "$T/E.out" 'c3'

# --- Case F: bare `log:` resolves cur via wtlog, ignoring intervening
#   put-with-40hex-fragment rows (submodule/sub-shard pointers).
#   Regression: SNIFFAtTailOf used to walk every row with a 40-hex
#   fragment and adopt it as cur_sha — a `put abc#<sub-project-sha>`
#   row appearing after the latest get/post made bare `log:` resolve
#   to the sub-project's sha (or REFSNONE when the sha was absent
#   from the active project's keeper), so bare `log:` printed an
#   empty hunk in any wt that touched a sibling project shard.
CASE=F
WTLOG="$R/.be/wtlog"
[ -f "$WTLOG" ] || WTLOG="$R/.be/dogs/wtlog"
#  Pick a ts strictly ahead of any legitimate row by stamping the
#  prefix of the wtlog's tail row and trailing it with `~~~~`.
#  `~` is RON64's max digit (value 63), so the resulting ts is the
#  largest value sharing the tail row's high-order prefix — guaranteed
#  to satisfy ulog_scan_log's strict-monotonicity gate regardless of
#  wall-clock drift between the legitimate rows and the appended row.
TAIL_TS=$(awk -F'\t' 'NF { ts = $1 } END { print ts }' "$WTLOG")
PREFIX=$(printf '%s' "$TAIL_TS" | cut -c1-6)
echo "${PREFIX}~~~~	put	abc#0000000000000000000000000000000000000123" \
    >> "$WTLOG"
rm -f "$R/.be/.wtlog.idx" "$R/.be/dogs/.wtlog.idx"
be 'log:' > "$T/F.out" 2>&1 || true
want_grep "$T/F.out" 'c4'
want_grep "$T/F.out" 'c1'
want_lines "$T/F.out" 4

# --- Summary -----------------------------------------------------
echo ""
if [ "$FAIL" = "0" ]; then
    echo "=== be-log-projector OK (6 cases) ==="
else
    echo "=== be-log-projector FAIL ($FAIL case(s)) ==="
    exit 1
fi
