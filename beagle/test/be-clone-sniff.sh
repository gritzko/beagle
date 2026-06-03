#!/bin/sh
#  be-clone-sniff.sh — verify `.be/wtlog` after `be get ssh://...` clone.
#
#  After a clone the secondary `.be/wtlog` must hold both:
#    row 0: `repo file:<wt>/.be/`
#    row 1: `get   ?<branch>#<sha>`
#
#  The `repo` row is bootstrapped by SNIFFOpen; the `get` row is
#  appended by sniff/GET.c at the end of the checkout.  Regression
#  observed in field reports: only the `repo` row survives on disk
#  even though `sniff: checkout done` printed; the SSH clone path
#  trips it but the local-dir-clone path does not.
#
#  Skips cleanly when sshd-to-localhost is not configured (CI envs
#  without ssh keys).
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
BE="$BIN/be"

TEST_ID=${TEST_ID:-be-clone-sniff}
#  Shared isolated repo-setup.  This ssh-clones a SOURCE repo whose
#  basename names the remote-side keeper shard ($HOME/.be/<basename>),
#  so the source dir MUST be uniquely named (not a generic `src`) or it
#  corrupts the developer's REAL `$HOME/.be/src`.  Use `$TEST_ID-src`.
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
rs_shield "$TMP"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }
skip() { echo "SKIP: $*" >&2; exit 0; }

#  Skip cleanly when localhost ssh isn't reachable in batch mode
#  (no key, no sshd, etc.) — the clone path needs a working ssh.
if ! ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new \
        -o ConnectTimeout=5 localhost true 2>/dev/null; then
    skip "ssh localhost not reachable in batch mode"
fi

# --- 1. seed a tiny git repo at the ssh-resolvable path ------------
#
#  ssh transport on git strips the leading slash, so the URL path
#  resolves relative to the ssh login's cwd ($HOME).  Place the seed
#  repo under $TMP (which lives under $HOME/tmp by default) and pass
#  the leading-slash form `/path-relative-to-home` in the URI.
echo "=== 1. seed git repo over ssh-reachable path ==="
SRC="$TMP/$TEST_ID-src"; rs_wt_at "$SRC"
git init --quiet -b main
git config user.email t@t
git config user.name  t
echo hello > README
git add README
git commit --quiet -m seed
SEED=$(git rev-parse HEAD)
[ ${#SEED} -eq 40 ] || fail "seed sha not 40 hex"

#  Build the ssh URL relative to $HOME (git strips the leading slash
#  on the remote so `/dir-under-home` resolves correctly via $HOME).
case "$SRC" in
    "$HOME"/*) REL="${SRC#$HOME}" ;;
    *) skip "TMP=$TMP not under \$HOME — adjust TMP to use ssh path" ;;
esac
URI="ssh://localhost${REL}"
note "seed sha=$SEED, ssh URI=$URI"

# --- 2. clone via be get ssh:// ------------------------------------
echo "=== 2. be get ssh://... in fresh wt ==="
WT="$TMP/wt"; rs_wt_at "$WT"
"$BE" get "$URI" >/dev/null 2>&1 || fail "wt: be get failed"

[ -f .be/wtlog ] || fail "wt: .be/wtlog missing"

# --- 3. .be/wtlog must carry both rows --------------------------------
echo "=== 3. .be/wtlog has anchor + checkout get rows ==="
#  Row 0 is the wt->store anchor: verb `get` (the get-unification;
#  formerly `repo`), $3 = `file:<wt>/.be/`.  The clone checkout is a
#  later `get ?...#<sha>` row whose $3 is NOT a `file:` store path.
NANCHOR=$(awk -F'\t' 'NR==1 && $2=="get" && $3 ~ /^file:/' .be/wtlog | wc -l)
NCHK=$(awk -F'\t' '$2=="get" && $3 !~ /^file:/'  .be/wtlog | wc -l)

[ "$NANCHOR" -eq 1 ] || fail ".be/wtlog: row-0 get anchor missing"
[ "$NCHK"  -eq 1 ] || {
    echo "--- .be/wtlog hex dump ---" >&2
    xxd .be/wtlog >&2
    echo "-----------------------" >&2
    echo "--- .be/refs ---" >&2
    cat .be/refs 2>/dev/null >&2
    echo "------------------" >&2
    fail ".be/wtlog: expected 1 checkout get row after clone, got $NCHK"
}

GROW=$(awk -F'\t' '$2=="get" && $3 !~ /^file:/ {print $3}' .be/wtlog)
case "$GROW" in
    *"#$SEED") note "get row OK: $GROW" ;;
    *)        fail "get row sha mismatch: row=$GROW, expected #$SEED" ;;
esac

echo "=== be-clone-sniff: OK ==="
