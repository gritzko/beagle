#!/bin/sh
#  be-diff-projector.sh — VERBS.md `diff:` projector under GET.
#
#  Wires `be get diff:<path>?<ref>` and `be get diff:?<ref>` (plus the
#  verb-less projector form `be diff:<path>?<ref>`) through to graf's
#  diff machinery and checks that unified-diff output appears.  Does
#  not assert exact bytes — graf's diff is token-level and its output
#  shape evolves with the renderer.  Also smoke-checks the file vs
#  ref-to-ref form.
#
#  Run: BIN=build-asan/bin sh beagle/test/be-diff-projector.sh
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
export PATH="$BIN:$PATH"

TMP=${TMP:-$HOME/tmp/run-$(date +%Y%m%d-%H%M%S)}
TEST_ID=${TEST_ID:-be-diff-projector}
T=$TMP/$TEST_ID
mkdir -p "$T"
trap 'rm -rf "$T"; rmdir "$TMP" 2>/dev/null || true' EXIT INT TERM

FAIL=0
CASE=0
fail() { echo "FAIL [$CASE]: $*" >&2; FAIL=$((FAIL + 1)); }
pass() { echo "PASS [$CASE]: $*"; }

# Assert the file contains every PATTERN.
want_all() {
    out=$1; shift
    for p in "$@"; do
        grep -qE "$p" "$out" || { fail "missing /$p/ in $out"; cat "$out"; return; }
    done
    pass "all patterns matched"
}

# --- Build a tiny 2-tag repo + a wt edit -----------------------------
R=$T/repo; mkdir -p "$R"; cd "$R"
sniff init >/dev/null

cat > a.txt <<'EOF'
hello world
goodnight moon
EOF
be post -m v1 '?tags/v1' >/dev/null

cat > a.txt <<'EOF'
hello world
goodbye moon
EOF
be post -m v2 '?tags/v2' >/dev/null

# wt drift on top of v2
cat > a.txt <<'EOF'
hello universe
goodbye moon
EOF

# --- Case A: file ref-vs-wt via `be get diff:<path>?<ref>` ---------
CASE=A
be get 'diff:a.txt?tags/v1' > "$T/A.out" 2>&1 || true
want_all "$T/A.out" 'a.txt' 'universe'

# --- Case B: tree ref-vs-wt via `be get diff:?<ref>` ---------------
CASE=B
be get 'diff:?tags/v1' > "$T/B.out" 2>&1 || true
want_all "$T/B.out" 'a.txt' 'universe'

# --- Case C: verb-less `be diff:<path>?<ref>` ---------------------
CASE=C
be 'diff:a.txt?tags/v1' > "$T/C.out" 2>&1 || true
want_all "$T/C.out" 'a.txt' 'universe'

# --- Case D: file ref-to-ref via `be get diff:<path>?<from>..<to>` -
CASE=D
be get 'diff:a.txt?tags/v1..tags/v2' > "$T/D.out" 2>&1 || true
want_all "$T/D.out" 'a.txt' 'goodnight|goodbye'

# --- Summary -----------------------------------------------------
echo ""
if [ "$FAIL" = "0" ]; then
    echo "=== be-diff-projector OK (4 cases) ==="
else
    echo "=== be-diff-projector FAIL ($FAIL case(s)) ==="
    exit 1
fi
