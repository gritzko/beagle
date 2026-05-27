#!/bin/sh
#  tags-from-git.sh — keeper and git each iterate v1 → v2 → master from
#  a toy git repo, comparing worktrees at every step.
#
#  Builds the same toy repo as clone-from-git.sh.  At each step:
#    1. `be get //localhost/SRC?refs/tags/X`   (or ?heads/master for tip)
#    2. `git fetch + checkout` of the same ref
#    3. rsync --dry-run --delete compare
#
#  Run: BIN=build-debug/bin sh beagle/test/tags-from-git.sh

set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
BIN=$(cd "$BIN" && pwd)
export PATH="$BIN:$PATH"
export DOG_REMOTE_PATH="$BIN"

TMP=${TMP:-$HOME/tmp/run-$(date +%Y%m%d-%H%M%S)}
TEST_ID=${TEST_ID:-tags-from-git}
TMP=$TMP/$TEST_ID/$$
TMP_REL=${TMP#$HOME/}
mkdir -p "$TMP"; echo "Running in $PWD"
trap 'rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true' EXIT

# --- 1. toy git source: 3 revs, v1, v2 tags, master tip ---
SRC=$TMP/src
git init --quiet --bare "$SRC"

W=$(mktemp -d)
git -c init.defaultBranch=master init --quiet "$W"
git -C "$W" config user.email t@t
git -C "$W" config user.name t
git -C "$W" remote add origin "$SRC"

echo rev1 >"$W/f.txt"
git -C "$W" add f.txt
git -C "$W" commit --quiet -m "rev 1"
git -C "$W" tag v1
echo rev2 >"$W/f.txt"
git -C "$W" commit --quiet -am "rev 2"
git -C "$W" tag v2
echo rev3 >"$W/f.txt"
git -C "$W" commit --quiet -am "rev 3"
git -C "$W" push --quiet origin master --tags
rm -rf "$W"

SRC_REL=${SRC#$HOME/}

# --- 2. init both consumer roots ---
mkdir -p "$TMP/be-clone"
cd "$TMP/be-clone"
git init --quiet .
mkdir -p .be

git clone --quiet --no-checkout "ssh://localhost/$SRC" "$TMP/git-clone"

# --- 3. iterate v1 → v2 → master ---
FAIL=0
# Be-side ref form: bare branch / `tags/<name>` (per VERBS.md
# §"Ref resolution").  Wire form `refs/heads/<X>` / `refs/tags/<X>`
# isn't accepted by be's URI parser.
for STEP in "v1 tags/v1 refs/tags/v1" \
            "v2 tags/v2 refs/tags/v2" \
            "master master refs/heads/master"; do
    NAME=${STEP%% *}
    REST=${STEP#* }
    BE_REF=${REST%% *}
    GIT_REF=${REST##* }

    cd "$TMP/be-clone"
    be get "//localhost/$SRC_REL?$BE_REF" >/dev/null

    git -C "$TMP/git-clone" fetch --quiet --no-tags origin \
        "$GIT_REF:refs/keep/$NAME"
    git -C "$TMP/git-clone" checkout --quiet "refs/keep/$NAME"

    RDIFF=$(rsync -rlcni --delete \
        --exclude='/.git/' --exclude='/.be/' --exclude='/.be/wtlog' \
        "$TMP/git-clone/" "$TMP/be-clone/" 2>&1)
    if [ -n "$RDIFF" ]; then
        echo "FAIL: tags-from-git $NAME"
        echo "$RDIFF" | head -10
        FAIL=$((FAIL + 1))
    else
        echo "PASS: tags-from-git $NAME"
    fi
done

exit $FAIL
