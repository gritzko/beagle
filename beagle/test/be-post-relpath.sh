#!/bin/sh
#  be-post-relpath.sh — SUBS-022.  Sub-recursion must work regardless of
#  how `be` is invoked: a RELATIVE argv[0] (`../path/be`), an ABSOLUTE
#  one (`/full/path/be`), or a bare name resolved via $PATH.
#
#  THE BUG (SUBS-022): the recursion fork's, chdir's into
#  `<wt>/<subpath>`, then execvp's itself using argv[0] (and the bin dir
#  HOMEResolveSibling derives via dirname).  A RELATIVE argv[0] is still
#  relative to the ORIGINAL cwd, so post-chdir it no longer resolves —
#  `be: post: execvp ../…/be: No such file or directory` → BEDOGEXIT,
#  the whole recursing verb aborts.  Bare-name (PATH-resolved by execvp)
#  and absolute argv[0] always worked; only a relative one broke.  This
#  is exactly how the local `build-*/bin/be` worker trees invoke `be`.
#
#  FIX: HOMEResolveSibling (dog/HOME.c) absolutizes a relative argv[0]
#  (has a `/`, not leading `/`) via realpath(3) against the LAUNCH cwd
#  BEFORE any chdir, so the derived bin dir survives the recursion chdir.
#
#  This test is table-driven over the three invocation FORMS.  For each
#  form it freshly seeds a parent+vendor/sub mount, dirties + stages the
#  PARENT, and runs `be post -m <msg>` via that form, asserting:
#    * the sub-recursion marker (`be: post vendor/sub`) IS emitted,
#    * NO `execvp … No such file or directory` and NO BEDOGEXIT abort,
#    * the post exits 0 and the parent tip advanced.
#  Pre-fix, the `rel` row fails (execvp ENOENT → BEDOGEXIT); `abs` and
#  `bare` pass on both sides (no-regression guard).
#
#  Modelled on be-post-nosub.sh; ssh-localhost gates the ssh:// clone.
#
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
BE="$BIN/be"

TEST_ID=${TEST_ID:-be-post-relpath}
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

#  $HOME-relative ssh URL ($HOME-rooted; git strips leading /).
home_uri() {
    case "$1" in
        "$HOME"/*) printf 'ssh://localhost%s\n' "${1#$HOME}" ;;
        *) skip "TMP=$TMP not under \$HOME — adjust TMP to use ssh path" ;;
    esac
}

#  rel_to <target> <fromdir> — POSIX-sh relative path from <fromdir> to
#  <target> (both absolute).  Pure shell; no python dependency.
rel_to() {
    _t=$1; _f=$2; _up=""
    while [ "${_t#"$_f"/}" = "$_t" ] && [ "$_f" != "/" ]; do
        _up="../$_up"; _f=$(dirname "$_f")
    done
    if [ "$_f" = "/" ]; then printf '%s%s\n' "$_up" "${_t#/}"
    else printf '%s%s\n' "$_up" "${_t#"$_f"/}"; fi
}

tip_of() {  # last get/post/patch wtlog hash, fragment-stripped
    awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                END { h=last; sub(/^[^#]*#/, "", h); print h }' "$1/.be/wtlog"
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
git -C "$PARENT" update-index --add --cacheinfo \
    160000,"$SUB_C1",vendor/sub
git -C "$PARENT" commit --quiet -m "parent: vendor sub"
PARENT_URI=$(home_uri "$PARENT")
note "parent uri=$PARENT_URI"

#  Bare-name form: a $PATH dir holding ONLY a `be` -> $BE symlink, so
#  execvp resolves `be` to our build (not any dev-box `be`).
BAREDIR="$TMP/barebin"; mkdir -p "$BAREDIR"
ln -sf "$BE" "$BAREDIR/be"

# --- run one invocation form -----------------------------------------
#  $1 = label, $2 = the argv[0] string to invoke `be` with (already
#  correct for the per-form cwd: WT for rel/abs, any for bare).
run_form() {
    label=$1; n=$2

    WT="$TMP/wt-$label"; rs_wt_at "$WT"
    GETLOG="$TMP/get-$label.log"
    #  Mount the sub with the ABSOLUTE be (the get-side recursion is not
    #  under test here — the point is the POST recursion invocation form).
    if ! timeout 30 "$BE" get "$PARENT_URI?master" >"$GETLOG" 2>&1; then
        cat "$GETLOG" >&2; fail "[$label] be get (recursive) failed"
    fi
    [ -f vendor/sub/.be ]    || { cat "$GETLOG" >&2; fail "[$label] vendor/sub not mounted"; }
    [ -f vendor/sub/core.c ] || { cat "$GETLOG" >&2; fail "[$label] sub core.c missing"; }

    base=$(tip_of "$WT")

    #  Build the actual argv[0] for this form, relative to THIS wt.
    case "$label" in
        rel)  inv=$(rel_to "$BE" "$WT") ;;
        abs)  inv="$BE" ;;
        bare) inv="be"; PATH="$BAREDIR:$PATH"; export PATH ;;
    esac
    note "[$label] invoke argv0 = $inv"

    sleep 0.02
    printf '\nint helper_%s(void){return 2;}\n' "$label" >> main.c
    "$inv" put main.c >"$TMP/put-$label.log" 2>&1 \
        || { cat "$TMP/put-$label.log" >&2; fail "[$label] be put failed"; }

    POSTLOG="$TMP/post-$label.log"
    rc=0
    timeout 30 "$inv" post -m "#$label-change" >"$POSTLOG" 2>&1 || rc=$?

    #  POST-019: the recursion descends into the mounted (clean) sub; it
    #  no-ops, so there is no relayed commit banner — the descent surfaces
    #  as a ULOG `post vendor/sub` marker row on stdout (replacing the old
    #  stderr echo).  MUST fire for all argv0 forms (recursion default-on).
    grep -qE 'post vendor/sub$' "$POSTLOG" \
        || { cat "$POSTLOG" >&2; fail "[$label] post did NOT recurse into vendor/sub"; }
    #  SUBS-022 symptom: a relative argv[0] makes the child execvp fail.
    if grep -qiE 'execvp .*No such file or directory' "$POSTLOG"; then
        cat "$POSTLOG" >&2
        fail "[$label] sub-recursion execvp failed (SUBS-022 relative argv0)"
    fi
    if grep -q 'aborting parent commit' "$POSTLOG"; then
        cat "$POSTLOG" >&2
        fail "[$label] post aborted on sub recursion (SUBS-022)"
    fi
    [ "$rc" -eq 0 ] || { cat "$POSTLOG" >&2; fail "[$label] post exit=$rc (expected 0)"; }

    after=$(tip_of "$WT")
    [ "$after" != "$base" ] \
        || { cat "$POSTLOG" >&2; fail "[$label] parent tip did not advance"; }
    note "[$label] OK — recursed into vendor/sub, tip $base -> $after"
}

# --- 3. table: the three invocation forms ----------------------------
echo "=== 3. post-recursion via rel / abs / bare argv[0] ==="
for form in rel abs bare; do
    run_form "$form" "$form"
done

echo "=== be-post-relpath: OK ==="
