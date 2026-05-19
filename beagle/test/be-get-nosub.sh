#!/bin/sh
#  be-get-nosub.sh — `be get --nosub` skips the submodule-mount loop.
#
#  Seeds a tiny git repo whose tree contains:
#    * a regular file (proves the wt was materialised)
#    * `.gitmodules` referencing a bogus URL (port-1 → refused fast,
#      but unreachable in all sensible network configs)
#    * a gitlink (mode 160000) at the `.gitmodules`-named path —
#      the entry that triggers sniff/GET.c's submodule-mount loop
#
#  With `--nosub` the loop must be short-circuited: clone completes
#  promptly and stderr carries the `submodule(s) skipped (--nosub)`
#  line.  Regression spec: the loop ran unconditionally before
#  `--nosub` was wired through, so even repos with unreachable subs
#  added a network-timeout-shaped delay to every clone.
#
#  Skips cleanly when sshd-to-localhost is not configured (CI envs
#  without ssh keys).
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
BE="$BIN/be"

TMP=${TMP:-$HOME/tmp/run-$(date +%Y%m%d-%H%M%S)}
TEST_ID=${TEST_ID:-be-get-nosub}
TMP=$TMP/$TEST_ID/$$
mkdir -p "$TMP"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }
skip() { echo "SKIP: $*" >&2; exit 0; }

#  ssh-localhost required for the ssh:// clone path; skip when absent.
if ! ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new \
        -o ConnectTimeout=5 localhost true 2>/dev/null; then
    skip "ssh localhost not reachable in batch mode"
fi

# --- 1. seed: file + .gitmodules + gitlink ---------------------------
echo "=== 1. seed git repo with a bogus submodule ==="
SRC="$TMP/src"; mkdir -p "$SRC/.be"; cd "$SRC"
git init --quiet -b main
git config user.email t@t
git config user.name  t

echo "hello" > README
cat > .gitmodules <<'EOF'
[submodule "bogus"]
	path = bogus
	url = https://localhost:1/bogus.git
EOF
git add README .gitmodules

#  Synthetic gitlink at `bogus/` — points at an arbitrary 40-hex sha.
#  Git accepts the cacheinfo entry without verifying the target sha,
#  so we can fabricate a submodule reference without actually cloning
#  one.  The sha here is just `0...001` — sniff would try to fetch
#  it from the bogus URL above if `--nosub` were ignored.
git update-index --add --cacheinfo \
    160000,0000000000000000000000000000000000000001,bogus
git commit --quiet -m "seed with bogus sub"
SEED=$(git rev-parse HEAD)
note "seed sha=$SEED"

#  ssh URL relative to $HOME (git strips the leading slash so
#  `/path-relative-to-home` resolves correctly on the remote side).
case "$SRC" in
    "$HOME"/*) REL="${SRC#$HOME}" ;;
    *) skip "TMP=$TMP not under \$HOME — adjust TMP to use ssh path" ;;
esac
URI="ssh://localhost${REL}"

# --- 2. be get --nosub -----------------------------------------------
echo "=== 2. be get --nosub <uri> ==="
WT="$TMP/wt"; mkdir -p "$WT"; cd "$WT"

#  Cap wall time at 30s — the bogus URL would take far longer than
#  that on a misconfigured network (DNS retries, TCP backoff).  A
#  working `--nosub` finishes in well under a second.
LOG="$TMP/be-get.log"
START=$(date +%s)
if ! timeout 30 "$BE" get --nosub "$URI" >"$LOG" 2>&1; then
    cat "$LOG" >&2
    fail "be get --nosub timed out or failed"
fi
END=$(date +%s)
ELAPSED=$((END - START))
note "elapsed: ${ELAPSED}s"

# --- 3. assert: clone landed + skip line printed ---------------------
[ -f README ] || { cat "$LOG" >&2; fail "README missing after clone"; }
grep -q "skipped (--nosub)" "$LOG" || {
    cat "$LOG" >&2
    fail "expected 'skipped (--nosub)' marker on stderr"
}
#  The bogus URL must never have been contacted — sniff's own
#  per-sub fetch line ("sniff: submodule fetch failed for ...") is
#  proof of the loop running.  Its absence is what we want.
if grep -q "submodule fetch failed" "$LOG"; then
    cat "$LOG" >&2
    fail "submodule fetch attempted despite --nosub"
fi

echo "=== be-get-nosub: OK ==="
