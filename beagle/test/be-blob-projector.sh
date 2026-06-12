#!/bin/sh
#  be-blob-projector.sh — https://replicated.wiki/html/wiki/Projector.html `blob:` projector.
#
#  SUBS-015 repro.  Per the Projector page `blob:<path>?<ref|sha>` emits
#  a blob's raw bytes (`git cat-file -p <sha>:<path>`).  The bug: the
#  no-`?ref` and empty-`?ref` forms returned KEEPNONE/KEEPFAIL instead of
#  resolving against the current branch tip — even on a plain parent
#  file, not submodule-related.  The sibling `sha1:<path>` projector
#  already resolved both forms (via KEEPResolveTree's cur-tip fallback);
#  `blob:` short-circuited in KEEPGetByURI before reaching that path.
#
#  Forms exercised:
#    blob:<path>?<tag>   → bytes of the named ref's tree entry (control)
#    blob:<path>?<sha>   → bytes at an explicit commit (control)
#    blob:<path>?        → bytes of the cur-branch tracked blob (was KEEPFAIL)
#    blob:<path>         → bytes of the cur-branch tracked blob (was KEEPNONE)
#
#  Run: BIN=build-debug/bin sh beagle/test/be-blob-projector.sh
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
export PATH="$BIN:$PATH"

TEST_ID=${TEST_ID:-be-blob-projector}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
T=$(rs_repo_base)
rs_shield "$T"
trap 'rm -rf "$T"; rmdir "${T%/*}" 2>/dev/null || true; rmdir "$TMP" 2>/dev/null || true' EXIT INT TERM

FAIL=0
CASE=0
fail() { echo "FAIL [$CASE]: $*" >&2; FAIL=$((FAIL + 1)); }
pass() { echo "PASS [$CASE]: $*"; }

# Assert the file's content is EXACTLY $1 (byte-faithful blob bytes).
want_exact() {
    out=$1; shift; expect=$1
    got=$(cat "$out")
    if [ "$got" = "$expect" ]; then
        pass "blob bytes match"
    else
        fail "blob mismatch: got [$got] want [$expect]"
        cat "$out"
    fi
}

# --- Build a tiny 2-tag repo --------------------------------------------
#   v1: main.c = "alpha\n"
#   v2: main.c = "beta\n"   (= cur/base)
R=$T/repo; rs_wt_at "$R"
sniff init >/dev/null

printf 'alpha\n' > main.c
be post -m v1 '?v1' >/dev/null

printf 'beta\n' > main.c
be post -m v2 '?v2' >/dev/null

# --- Case A: explicit tag ref — `blob:<path>?<tag>` (control) ----------
CASE=A
be 'blob:main.c?v1' > "$T/A.out" 2>&1 || true
want_exact "$T/A.out" 'alpha'

# --- Case B: cur tag ref — `blob:<path>?<tag>` (control) ---------------
CASE=B
be 'blob:main.c?v2' > "$T/B.out" 2>&1 || true
want_exact "$T/B.out" 'beta'

# --- Case C: EMPTY ref — `blob:<path>?` (SUBS-015) ---------------------
#  Empty ?ref → cur-branch tracked blob (= v2 cur tip).  Was KEEPFAIL.
CASE=C
be 'blob:main.c?' > "$T/C.out" 2>&1 || true
want_exact "$T/C.out" 'beta'

# --- Case D: NO ref — `blob:<path>` (SUBS-015) ------------------------
#  No ?ref slot at all → cur-branch tracked blob (= v2 cur tip).  Was
#  KEEPNONE.
CASE=D
be 'blob:main.c' > "$T/D.out" 2>&1 || true
want_exact "$T/D.out" 'beta'

# --- Summary -----------------------------------------------------
echo ""
if [ "$FAIL" = "0" ]; then
    echo "=== be-blob-projector OK (4 cases) ==="
else
    echo "=== be-blob-projector FAIL ($FAIL case(s)) ==="
    exit 1
fi
