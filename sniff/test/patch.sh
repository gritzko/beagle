#!/bin/sh
#  patch.sh — `sniff patch` end-to-end on toy two-branch repos.
#
#  Drives:
#    1. Disjoint-edit merge — feat-a prepends, feat-b appends.  Both
#       additions must survive in the worktree file, exit 0.
#    2. Conflict merge — same-line edit on both sides — JOIN emits
#       `<<<<` markers.  DIS-018: reports `conf`, exit 0, markers
#       stay in the file (POST's POSTCFLCT scan is the safety net).
#    3. Target-side add — theirs adds a new file, ours unchanged.
#       File appears in wt, exit 0.
#    4. Modify/delete divergence — theirs deletes a path ours
#       modified.  DIS-018: reports `modl`, exit 0, ours kept.
#    5. POST safety net — after the conflicting PATCH (scenario 2)
#       `be post` still REFUSES on the conflict markers (POSTCFLCT).
#
#  Each scenario builds its own bare git source, keeper-fetches both
#  branches via ssh localhost (same transport as
#  beagle/test/blobs-from-git.sh), `sniff get`s master into a fresh
#  wt, then `sniff patch <target>` merges.
#
#  Run: BIN=build-debug/bin sh sniff/test/patch.sh

set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export DOG_REMOTE_PATH="$BIN"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

HOST=${HOST:-localhost}

#  Per-run scratch dir: $TMP/<test-id>/.  Base TMP and TEST_ID come
#  from ctest (see sniff/test/CMakeLists.txt); standalone runs fall
#  back to $HOME/tmp/run-<timestamp> and the script's basename.
TEST_ID=${TEST_ID:-SNIFFpatch}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
trap 'rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true' EXIT
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

# --- Scenario 1: disjoint edits --------------------------------------

scenario1() {
    echo "=== 1. disjoint-edit merge ==="
    SRC=$TMP/s1/src
    CLI=$TMP/s1/client
    mkdir -p "$SRC"; rs_shield "$CLI"
    git init --quiet --bare "$SRC"

    W=$(mktemp -d)
    git -c init.defaultBranch=master init --quiet "$W"
    git -C "$W" config user.email t@t
    git -C "$W" config user.name  t
    git -C "$W" remote add origin "$SRC"

    cat >"$W/f.c" <<'EOF'
int f(int x) {
    return x + 1;
}
EOF
    git -C "$W" add f.c
    git -C "$W" commit --quiet -m "base"
    git -C "$W" push --quiet origin master

    git -C "$W" checkout --quiet -b feat-a master
    cat >"$W/f.c" <<'EOF'
int foo(int a) {
    return a - 7;
}
int f(int x) {
    return x + 1;
}
EOF
    git -C "$W" commit --quiet -am "feat-a prepend"
    git -C "$W" push --quiet origin feat-a
    rm -rf "$W"

    SRC_REL=${SRC#$HOME/}
    cd "$CLI"
    keeper get "//$HOST/$SRC_REL?master" >/dev/null
    keeper get "//$HOST/$SRC_REL?feat-a" >/dev/null

    sniff get "?master" >/dev/null 2>&1 \
        || fail "sniff get master failed"
    grep -qF 'return x + 1' f.c || fail "wt missing base line"

    sniff patch "?feat-a" 2>&1 | sed 's/^/  | /'
    grep -qF 'int foo(' f.c || fail "patch lost feat-a prepend"
    grep -qF 'return x + 1' f.c || fail "patch dropped base function"
    ! grep -qF '<<<<' f.c \
        || fail "unexpected conflict markers (disjoint edits)"

    note "f.c carries both feat-a's foo() and master's f()"
}

# --- Scenario 2: same-line conflict -----------------------------------

scenario2() {
    echo "=== 2. conflict merge ==="
    SRC=$TMP/s2/src
    CLI=$TMP/s2/client
    mkdir -p "$SRC"; rs_shield "$CLI"
    git init --quiet --bare "$SRC"

    W=$(mktemp -d)
    git -c init.defaultBranch=master init --quiet "$W"
    git -C "$W" config user.email t@t
    git -C "$W" config user.name  t
    git -C "$W" remote add origin "$SRC"

    cat >"$W/g.c" <<'EOF'
int g(int y) {
    return y + 0;
}
EOF
    git -C "$W" add g.c
    git -C "$W" commit --quiet -m "base"
    git -C "$W" push --quiet origin master

    git -C "$W" checkout --quiet -b feat-x master
    cat >"$W/g.c" <<'EOF'
int g(int y) {
    return y + 42;
}
EOF
    git -C "$W" commit --quiet -am "feat-x 42"
    git -C "$W" push --quiet origin feat-x

    git -C "$W" checkout --quiet master
    cat >"$W/g.c" <<'EOF'
int g(int y) {
    return y - 100;
}
EOF
    git -C "$W" commit --quiet -am "master -100"
    git -C "$W" push --quiet origin master
    rm -rf "$W"

    SRC_REL=${SRC#$HOME/}
    cd "$CLI"
    keeper get "//$HOST/$SRC_REL?master" >/dev/null
    keeper get "//$HOST/$SRC_REL?feat-x" >/dev/null

    sniff get "?master" >/dev/null 2>&1 \
        || fail "sniff get master failed"

    set +e
    sniff patch "?feat-x" >"$TMP/s2.out" 2>&1
    PATCH_RC=$?
    set -e
    sed 's/^/  | /' "$TMP/s2.out"
    #  DIS-018: a genuine WEAVE conflict reports `conf` and returns OK
    #  (exit 0) — a non-zero exit broke parent recursion on a
    #  conflicting submodule.  The markers stay in the file so POST's
    #  POSTCFLCT scan is the patch→test→post safety net (scenario 5).
    [ "$PATCH_RC" = "0" ] \
        || fail "conflict merge should exit 0 now (DIS-018), got $PATCH_RC"
    grep -qE "$(printf '\t')conf$(printf '\t')" "$TMP/s2.out" \
        || fail "expected 'conf' status row in report"
    grep -qF '<<<<' g.c \
        || fail "expected conflict markers in g.c"
    #  Stash the conflicted client dir for scenario 5's POST check.
    S2_CLI=$CLI
    note "g.c carries conflict markers, report=conf, exit=$PATCH_RC"
}

# --- Scenario 3: target adds a new file -------------------------------

scenario3() {
    echo "=== 3. target-side add ==="
    SRC=$TMP/s3/src
    CLI=$TMP/s3/client
    mkdir -p "$SRC"; rs_shield "$CLI"
    git init --quiet --bare "$SRC"

    W=$(mktemp -d)
    git -c init.defaultBranch=master init --quiet "$W"
    git -C "$W" config user.email t@t
    git -C "$W" config user.name  t
    git -C "$W" remote add origin "$SRC"

    cat >"$W/a.c" <<'EOF'
int a(void) { return 1; }
EOF
    git -C "$W" add a.c
    git -C "$W" commit --quiet -m "base"
    git -C "$W" push --quiet origin master

    git -C "$W" checkout --quiet -b feat-add master
    cat >"$W/b.c" <<'EOF'
int b(void) { return 2; }
EOF
    git -C "$W" add b.c
    git -C "$W" commit --quiet -am "add b.c"
    git -C "$W" push --quiet origin feat-add
    rm -rf "$W"

    SRC_REL=${SRC#$HOME/}
    cd "$CLI"
    keeper get "//$HOST/$SRC_REL?master" >/dev/null
    keeper get "//$HOST/$SRC_REL?feat-add" >/dev/null

    sniff get "?master" >/dev/null 2>&1 \
        || fail "sniff get master failed"
    [ ! -f b.c ] || fail "b.c should be absent before patch"

    sniff patch "?feat-add" 2>&1 | sed 's/^/  | /'
    [ -f b.c ] || fail "patch did not add b.c"
    grep -qF 'int b(void)' b.c || fail "b.c content missing"

    note "b.c appeared with target's bytes"
}

# --- Scenario 4: modify/delete divergence → modl ----------------------

scenario4() {
    echo "=== 4. modify/delete divergence ==="
    SRC=$TMP/s4/src
    CLI=$TMP/s4/client
    mkdir -p "$SRC"; rs_shield "$CLI"
    git init --quiet --bare "$SRC"

    W=$(mktemp -d)
    git -c init.defaultBranch=master init --quiet "$W"
    git -C "$W" config user.email t@t
    git -C "$W" config user.name  t
    git -C "$W" remote add origin "$SRC"

    cat >"$W/d.c" <<'EOF'
int d(int z) {
    return z + 0;
}
EOF
    git -C "$W" add d.c
    git -C "$W" commit --quiet -m "base"
    git -C "$W" push --quiet origin master

    #  feat-del deletes d.c.
    git -C "$W" checkout --quiet -b feat-del master
    git -C "$W" rm --quiet d.c
    git -C "$W" commit --quiet -m "delete d.c"
    git -C "$W" push --quiet origin feat-del

    #  master modifies d.c (so ours diverges from the merge base).
    git -C "$W" checkout --quiet master
    cat >"$W/d.c" <<'EOF'
int d(int z) {
    return z + 99;
}
EOF
    git -C "$W" commit --quiet -am "master modify d.c"
    git -C "$W" push --quiet origin master
    rm -rf "$W"

    SRC_REL=${SRC#$HOME/}
    cd "$CLI"
    keeper get "//$HOST/$SRC_REL?master"   >/dev/null
    keeper get "//$HOST/$SRC_REL?feat-del" >/dev/null

    sniff get "?master" >/dev/null 2>&1 \
        || fail "sniff get master failed"
    grep -qF 'return z + 99' d.c || fail "wt missing master's edit"

    set +e
    sniff patch "?feat-del" >"$TMP/s4.out" 2>&1
    PATCH_RC=$?
    set -e
    sed 's/^/  | /' "$TMP/s4.out"
    #  DIS-018: theirs deleted, ours modified → keep ours, report
    #  `modl` (distinct from the old undocumented `kept`), exit 0.
    [ "$PATCH_RC" = "0" ] \
        || fail "modify/delete should exit 0, got $PATCH_RC"
    #  Status rows are tab-delimited (`<date>\t<verb>\t<path>`); match
    #  the verb column so the loud "kept ours" warning prose doesn't
    #  produce a false hit.
    grep -qE "$(printf '\t')modl$(printf '\t')" "$TMP/s4.out" \
        || fail "expected 'modl' status row in report"
    ! grep -qE "$(printf '\t')kept$(printf '\t')" "$TMP/s4.out" \
        || fail "old 'kept' status row should be gone (now 'modl')"
    [ -f d.c ] || fail "modify/delete dropped ours (d.c missing)"
    grep -qF 'return z + 99' d.c || fail "ours bytes not kept"

    note "d.c kept (ours modified), report=modl, exit=$PATCH_RC"
}

# --- Scenario 5: POST still refuses the conflict markers ---------------

scenario5() {
    echo "=== 5. POST refuses conflict markers (safety net) ==="
    [ -n "${S2_CLI:-}" ] || fail "scenario 2 must run first"
    cd "$S2_CLI"
    grep -qF '<<<<' g.c || fail "scenario 2 left no markers to test"

    set +e
    #  Explicit message → POST takes the commit path (POSTCommit),
    #  where the conflict-marker scan lives; bare `be post` may fall
    #  to a dry-run status preview that never reaches the scan.
    be post "merge feat-x" >"$TMP/s5.out" 2>&1
    POST_RC=$?
    set -e
    sed 's/^/  | /' "$TMP/s5.out"
    #  CRITICAL: the conflict safety net moved from PATCH to POST.
    #  POST must REFUSE (POSTCFLCT, non-zero) on the markered file,
    #  proving patch→test→post still blocks a bad commit.
    [ "$POST_RC" != "0" ] \
        || fail "POST must refuse on conflict markers (POSTCFLCT)"
    grep -qiE 'conflict|POSTCFLCT' "$TMP/s5.out" \
        || fail "POST refusal should mention the conflict"

    note "POST refused the markered g.c, exit=$POST_RC"
}

scenario1
scenario2
scenario3
scenario4
scenario5

echo
echo "=== sniff patch toys: OK ==="
