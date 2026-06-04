#!/bin/sh
#  be-post-nosub.sh — `be post --nosub <msg>` commits the PARENT only
#  and does NOT recurse into mounted submodules.
#
#  POST-001 regression.  `BEActSubsPost` (beagle/BE.cli.c) lacked the
#  `--nosub` early-return guard that BEActSubsGet / BEActSubsPatch /
#  BEActSubsRelay all carry, so every `be post` recursed into mounted
#  subs regardless of the flag.  This test mounts a parent with a real
#  `vendor/sub` submodule, dirties + stages the PARENT only, then runs
#  `be post --nosub '<msg>'` and asserts:
#    * the parent commit succeeded (tip advanced);
#    * NO sub-recursion marker (`be: post vendor/sub`) was emitted;
#    * the post did not abort on a sub-driven failure.
#  Without the guard the recursion marker fires (the bug); with it the
#  post is parent-local.
#
#  Modelled on be-get-nosub.sh: seeds two git repos over an
#  ssh-reachable path (sub + parent-with-gitlink), be-get clones the
#  parent recursively (mounting the sub), then exercises post.
#
#  Skips cleanly when sshd-to-localhost is not configured.
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
BE="$BIN/be"

TEST_ID=${TEST_ID:-be-post-nosub}
#  Shared isolated repo-setup.  ssh-clones a SOURCE repo whose basename
#  names the remote-side keeper shard ($HOME/.be/<basename>), so the
#  source dirs MUST be uniquely named (not generic) or they corrupt the
#  developer's REAL $HOME/.be store.  We name them under $TEST_ID-*.
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
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

export GIT_CONFIG_GLOBAL=/dev/null

#  Helper: $HOME-relative ssh URL ($HOME-rooted; git strips leading /).
home_uri() {
    case "$1" in
        "$HOME"/*) printf 'ssh://localhost%s\n' "${1#$HOME}" ;;
        *) skip "TMP=$TMP not under \$HOME — adjust TMP to use ssh path" ;;
    esac
}

# --- 1. seed the sub upstream ----------------------------------------
echo "=== 1. seed sub git repo ==="
SUB="$TMP/$TEST_ID-sub"; mkdir -p "$SUB"
git init --quiet -b master "$SUB"
git -C "$SUB" config user.email t@t
git -C "$SUB" config user.name  t
echo "int core(void){return 1;}" > "$SUB/core.c"
git -C "$SUB" add core.c
git -C "$SUB" commit --quiet -m "sub: core"
SUB_C1=$(git -C "$SUB" rev-parse HEAD)
SUB_URI=$(home_uri "$SUB")
note "sub sha=$SUB_C1  uri=$SUB_URI"

# --- 2. seed the parent upstream with a gitlink at vendor/sub --------
echo "=== 2. seed parent git repo with vendor/sub gitlink ==="
PARENT="$TMP/$TEST_ID-parent"; mkdir -p "$PARENT"
git init --quiet -b master "$PARENT"
git -C "$PARENT" config user.email t@t
git -C "$PARENT" config user.name  t
echo "int main(void){return 0;}" > "$PARENT/main.c"
cat > "$PARENT/.gitmodules" <<EOF
[submodule "vendor/sub"]
	path = vendor/sub
	url = $SUB_URI
EOF
git -C "$PARENT" add main.c .gitmodules
#  cacheinfo gitlink — pins vendor/sub at SUB_C1 without an eager clone.
git -C "$PARENT" update-index --add --cacheinfo \
    160000,"$SUB_C1",vendor/sub
git -C "$PARENT" commit --quiet -m "parent: vendor sub"
PARENT_C1=$(git -C "$PARENT" rev-parse HEAD)
PARENT_URI=$(home_uri "$PARENT")
note "parent sha=$PARENT_C1  uri=$PARENT_URI"

# --- 3. be get (recursive): parent + mounted vendor/sub --------------
echo "=== 3. be get --sub parent?master (mounts vendor/sub) ==="
#  POST-001 phase 2: an ssh (git) parent does NOT recurse into subs by
#  default; `--sub` is the explicit opt-in that forces the mount so the
#  rest of this --nosub-post regression has a sub to (not) recurse into.
WT="$TMP/wt"; rs_wt_at "$WT"
GETLOG="$TMP/be-get.log"
if ! timeout 30 "$BE" get --sub "$PARENT_URI?master" >"$GETLOG" 2>&1; then
    cat "$GETLOG" >&2
    fail "be get (recursive) failed"
fi
[ -f main.c ]            || { cat "$GETLOG" >&2; fail "main.c missing after clone"; }
[ -f vendor/sub/.be ]    || { cat "$GETLOG" >&2; fail "vendor/sub not mounted"; }
[ -f vendor/sub/core.c ] || { cat "$GETLOG" >&2; fail "vendor/sub/core.c missing"; }
note "parent + vendor/sub mounted"

baseline=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                       END { h=last; sub(/^[^#]*#/, "", h); print h }' .be/wtlog)

# --- 4. dirty + stage the PARENT only --------------------------------
echo "=== 4. edit + stage parent main.c (sub stays clean) ==="
sleep 0.02
printf '\nint helper(void){return 2;}\n' >> main.c
"$BE" put main.c >"$TMP/be-put.log" 2>&1 || { cat "$TMP/be-put.log" >&2; fail "be put main.c failed"; }

# --- 5. be post --nosub <msg>: PARENT-only, no sub recursion ---------
echo "=== 5. be post --nosub '<msg>' ==="
POSTLOG="$TMP/be-post.log"
if ! timeout 30 "$BE" post --nosub '#parent-only' >"$POSTLOG" 2>&1; then
    cat "$POSTLOG" >&2
    fail "be post --nosub failed (likely sub recursion reaching the sub)"
fi

#  The recursion callback emits `be: post vendor/sub` for each mounted
#  sub it descends into.  With --nosub honoured, that line must be
#  absent.  Its presence is the POST-001 bug.
if grep -q '^be: post vendor/sub' "$POSTLOG"; then
    cat "$POSTLOG" >&2
    fail "post recursed into vendor/sub despite --nosub (POST-001)"
fi
#  A sub-driven abort is the downstream symptom of the same bug.
if grep -q 'aborting parent commit' "$POSTLOG"; then
    cat "$POSTLOG" >&2
    fail "post aborted on sub recursion despite --nosub (POST-001)"
fi

# --- 6. parent tip actually advanced ---------------------------------
after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                    END { h=last; sub(/^[^#]*#/, "", h); print h }' .be/wtlog)
[ "$after" != "$baseline" ] \
    || { cat "$POSTLOG" >&2; fail "parent tip did not advance ($baseline)"; }
note "parent committed ($baseline -> $after), no sub recursion"

echo "=== be-post-nosub: OK ==="
