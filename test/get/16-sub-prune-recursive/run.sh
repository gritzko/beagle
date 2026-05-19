#!/bin/sh
#  get/16-sub-prune-recursive — `--prune` propagates into submodule
#  recursion.  After clone, drop both an untracked file and a
#  gitignored file inside the sub.  `be get --prune --force ?master`
#  must drop the untracked file at every level, but spare the
#  gitignored one (mirrors `git clean -f`, not `-fx`).
#
#  Setup (from submodules.sh):
#      Clone parent at master.  Sub mounts at SUB_C2.
#
#  Test:
#      1. Clone parent into wt/.
#      2. Drop two files into vendor/sub/:
#           untracked.txt        — not in tree, not gitignored
#           build/cache.tmp      — covered by a sub .gitignore pattern
#      3. `be get --prune --force ?master` — --prune reaches the sub.
#      4. Assert vendor/sub/untracked.txt is gone (prune dropped it).
#      5. Assert vendor/sub/build/cache.tmp survives (gitignored).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt && cd wt

# --- 1. Clone parent (sub mounts at SUB_C2). -------------------------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "stage-1 be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f vendor/sub/core.c ] || fail "fixture: sub not mounted"

# --- 2. Drop a wt-only file + a gitignored file in the sub. ---------
#  The sub at SUB_C2 has only core.c + helper.c.  Anything else is
#  wt-only.  Add a .gitignore so `build/` is excluded.
mkdir -p vendor/sub/build
echo 'untracked content' > vendor/sub/untracked.txt
echo 'cached bytes'      > vendor/sub/build/cache.tmp
echo 'build/'            > vendor/sub/.gitignore

[ -f vendor/sub/untracked.txt ]      || fail "fixture: untracked seed failed"
[ -f vendor/sub/build/cache.tmp ]    || fail "fixture: gitignored seed failed"

# --- 3. --prune --force pass. ----------------------------------------
#  --force is needed because the new .gitignore counts as a dirty
#  edit at the sub level; --prune is the verb under test.
"$BE" get --prune --force "?master" >02.get.got.out 2>02.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get --prune --force exited $rc; stderr:
$(cat 02.get.got.err)"

# --- 4. Untracked file pruned. ---------------------------------------
if [ -e vendor/sub/untracked.txt ]; then
    echo "vendor/sub/untracked.txt survived --prune — flag did not reach the sub" >&2
    echo "stderr:" >&2
    cat 02.get.got.err >&2
    exit 1
fi

# --- 5. Gitignored file survives. ------------------------------------
[ -f vendor/sub/build/cache.tmp ] \
    || fail "vendor/sub/build/cache.tmp pruned despite gitignore — too aggressive"

note "get/16-sub-prune-recursive: --prune dropped untracked, spared gitignored"
