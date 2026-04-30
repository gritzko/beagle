# test/lib/case.sh — sourced at the top of every <verb>/<case>/run.sh.
#
# Resolves the BE binary, the per-case scratch dir, and exposes the
# match / match_re / must / mustnt helpers used by case drivers.
# Uses POSIX sh; no bash extensions.

set -eu

# 1. resolve the `be` binary -------------------------------------------
#   cmake passes the bin dir via ENVIRONMENT BIN=...; fall back to PATH
#   so the suite is also runnable by hand.
BE=${BE:-${BIN:+$BIN/be}}
BE=${BE:-$(command -v be || true)}
[ -n "$BE" ] && [ -x "$BE" ] || {
    echo "case.sh: cannot locate \`be\` (set BIN=... or BE=...)" >&2
    exit 2
}
export BE

# 2. resolve TMP --------------------------------------------------------
: "${TMP:=/tmp}"
export TMP

# 3. CASE / VERB / NAME -------------------------------------------------
#   $0 is the case driver; CASE is its dir, VERB its parent dir, NAME
#   its basename.  realpath is widely available; (cd ... && pwd) is
#   the pure-POSIX fallback.
CASE=$(cd "$(dirname "$0")" && pwd)
NAME=$(basename "$CASE")
VERB=$(basename "$(dirname "$CASE")")
export CASE NAME VERB

# 4. scratch dir --------------------------------------------------------
SCRATCH="$TMP/$$/$VERB/$NAME"
rm -rf "$SCRATCH"
mkdir -p "$SCRATCH"
cd "$SCRATCH"
export SCRATCH

# 5. helpers ------------------------------------------------------------

# match WANT GOT — byte-exact compare.  Diff to stderr on mismatch.
match() {
    _want=$1; _got=$2
    if [ ! -f "$_want" ]; then
        echo "match: WANT file missing: $_want" >&2; exit 1
    fi
    if [ ! -f "$_got" ]; then
        echo "match: GOT file missing: $_got" >&2; exit 1
    fi
    if ! diff -u "$_want" "$_got" >&2; then
        echo "match: $_got differs from $_want" >&2
        exit 1
    fi
}

# match_re WANT GOT — line-by-line regex compare.  Each line of WANT is
# a POSIX BRE pattern that must match the corresponding line of GOT.
# Empty WANT means GOT must also be empty.
match_re() {
    _want=$1; _got=$2
    if [ ! -f "$_want" ]; then
        echo "match_re: WANT file missing: $_want" >&2; exit 1
    fi
    if [ ! -f "$_got" ]; then
        echo "match_re: GOT file missing: $_got" >&2; exit 1
    fi
    if [ ! -s "$_want" ]; then
        if [ -s "$_got" ]; then
            echo "match_re: expected empty $_got, got:" >&2
            cat "$_got" >&2
            exit 1
        fi
        return 0
    fi
    awk -v want="$_want" -v got="$_got" '
        BEGIN {
            wn = 0
            while ((getline line < want) > 0) { wp[++wn] = line }
            close(want)
            gn = 0
            while ((getline line < got) > 0) { gl[++gn] = line }
            close(got)
            if (wn != gn) {
                printf("match_re: line count: want %d got %d\n", wn, gn) > "/dev/stderr"
                for (i = 1; i <= (wn > gn ? wn : gn); i++) {
                    printf("  W[%d]=%s\n", i, (i in wp ? wp[i] : "<eof>")) > "/dev/stderr"
                    printf("  G[%d]=%s\n", i, (i in gl ? gl[i] : "<eof>")) > "/dev/stderr"
                }
                exit 1
            }
            for (i = 1; i <= wn; i++) {
                if (gl[i] !~ wp[i]) {
                    printf("match_re: line %d:\n  want /%s/\n  got  %s\n",
                           i, wp[i], gl[i]) > "/dev/stderr"
                    exit 1
                }
            }
            exit 0
        }
    '
}

# empty GOT — assert file is zero-length.  On non-empty, dump first
# 200 bytes to stderr and exit 1.
empty() {
    _got=$1
    if [ ! -f "$_got" ]; then
        echo "empty: GOT file missing: $_got" >&2; exit 1
    fi
    if [ -s "$_got" ]; then
        echo "empty: expected $_got to be zero-length; first 200 bytes:" >&2
        dd if="$_got" bs=1 count=200 2>/dev/null >&2
        echo >&2
        exit 1
    fi
}

# must CMD ARGS... — run; nonzero exit → fail.  Stderr is left intact.
must() {
    if ! "$@"; then
        echo "must: command failed: $*" >&2
        exit 1
    fi
}

# mustnt CMD ARGS... — run; zero exit → fail.
mustnt() {
    if "$@" 2>/dev/null; then
        echo "mustnt: command unexpectedly succeeded: $*" >&2
        exit 1
    fi
}

# 6. trap: cleanup on success, keep scratch on failure -----------------
_case_cleanup() {
    _rc=$?
    if [ "$_rc" -eq 0 ]; then
        rm -rf "$SCRATCH"
        # best-effort prune of the per-pid parent
        rmdir "$TMP/$$/$VERB" 2>/dev/null || true
        rmdir "$TMP/$$"        2>/dev/null || true
    else
        echo "FAIL: scratch kept at $SCRATCH" >&2
    fi
    exit "$_rc"
}
trap _case_cleanup EXIT INT TERM
