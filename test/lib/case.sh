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

# 1b. wire-transport env for ssh/keeper cases --------------------------
#   The be:// / keeper:// wire edge spawns `ssh <host> keeper upload-pack`
#   on the remote side; keeper/WIRECLI.c prepends $DOG_REMOTE_PATH to the
#   remote shell PATH and honours $KEEPER_BIN for local transport.  Point
#   both at the dir holding `be` (keeper sits beside it in the build's
#   bin/) so wire / submodule / triangle cases pass under a plain `ctest`
#   without the caller exporting anything.  Pre-set values win.
_BE_BIN=$(dirname "$BE")
: "${KEEPER_BIN:=$_BE_BIN/keeper}"
: "${DOG_REMOTE_PATH:=$_BE_BIN}"
export KEEPER_BIN DOG_REMOTE_PATH
case ":$PATH:" in
    *":$_BE_BIN:"*) ;;
    *) PATH="$_BE_BIN:$PATH"; export PATH ;;
esac

# 1c. portable `timeout` ------------------------------------------------
#   macOS / BSD ship no `timeout(1)` (it's GNU coreutils), so tests that
#   guard a possibly-hanging `be` with `timeout <secs> ...` would fail
#   with "timeout: command not found" — the guarded command never runs.
#   Provide a shim only when the real tool is absent: prefer `gtimeout`
#   (coreutils via brew), else a pure-sh background-kill fallback that
#   returns the command's status (or 124-ish on expiry, like timeout).
if ! command -v timeout >/dev/null 2>&1; then
    if command -v gtimeout >/dev/null 2>&1; then
        timeout() { gtimeout "$@"; }
    else
        timeout() {
            _to_secs=$1; shift
            "$@" &
            _to_cmd=$!
            ( sleep "$_to_secs"; kill -TERM "$_to_cmd" 2>/dev/null ) &
            _to_killer=$!
            if wait "$_to_cmd" 2>/dev/null; then _to_rc=0; else _to_rc=$?; fi
            kill -KILL "$_to_killer" 2>/dev/null || true
            wait "$_to_killer" 2>/dev/null || true
            return "$_to_rc"
        }
    fi
fi

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
#   Route the `.be` bootstrap through the ONE shared repo-setup
#   procedure (rs_fresh_wt) so case scratch isolation is identical to
#   every other test family.  $SCRATCH keeps its historical layout
#   ($TMP/$$/$VERB/$NAME) for callers that build sibling fixtures off
#   it; rs_fresh_wt seeds the empty-`.be/` shield that stops `be`'s
#   walk-up from escaping to a real `$HOME/.be`.
. "$(dirname "$0")/../../lib/repo-setup.sh"
RS_ROOT="$TMP/$$"
rs_fresh_wt "$VERB/$NAME"
SCRATCH="$RS_WT"
export SCRATCH

# 5. project shard name ------------------------------------------------
#   `be_ensure_project_repo` derives the project name from PWD basename
#   when no URL is in play (the common test case).  Expose it as $P so
#   tests can assert on `.be/$P/<branch>` paths without hard-coding
#   the case name in every glob.
P="$NAME"
export P

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
