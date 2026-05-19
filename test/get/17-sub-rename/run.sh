#!/bin/sh
#  get/17-sub-rename — target tree renames a sub mount path.
#
#  Inline-extends the 2-level fixture with a new parent commit (on
#  branch `rename`) that moves the gitlink from vendor/sub →
#  libs/sub (same pin sha).  Test sequence:
#    1. `be get $PARENT_URL?master` — mounts vendor/sub.
#    2. `be get $PARENT_URL?rename` — switches to the rename commit.
#       Beagle's baseline-vs-target diff unmounts vendor/sub and
#       mounts libs/sub at the same pin.

. "$(dirname "$0")/../../lib/submodules.sh"

#  --- Push a `rename` branch on parent.git with vendor/sub → libs/sub.
RENAME_SEED="$ETMP/parent-rename-seed"
rm -rf "$RENAME_SEED"
git clone -q "$PARENT_BARE" "$RENAME_SEED"
git -C "$RENAME_SEED" config user.email t@t
git -C "$RENAME_SEED" config user.name  T
git -C "$RENAME_SEED" checkout -q master

#  Strip vendor/sub from the index and re-add as libs/sub (same pin).
git -C "$RENAME_SEED" rm -q --cached vendor/sub
git -C "$RENAME_SEED" update-index --add --cacheinfo \
    160000,"$PARENT_PINNED",libs/sub

#  Rewrite .gitmodules with the new path.
cat > "$RENAME_SEED/.gitmodules" <<EOF
[submodule "libs/sub"]
	path = libs/sub
	url = $SUB_URL
EOF
git -C "$RENAME_SEED" add .gitmodules
git -C "$RENAME_SEED" commit -qm 'parent: rename vendor/sub -> libs/sub'
git -C "$RENAME_SEED" push -q "$PARENT_BARE" master:refs/heads/rename

mkdir wt && cd wt

# --- Step 1: clone at master (vendor/sub mounted). -------------------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
[ -f vendor/sub/.be ]    || fail "step-1: vendor/sub not mounted"
[ -f vendor/sub/core.c ] || fail "step-1: sub content missing"
[ ! -e libs/sub ]        || fail "step-1: libs/sub unexpectedly present"

# --- Step 2: switch to rename — vendor/sub unmounts, libs/sub mounts.
"$BE" get "$PARENT_URL?rename" >02.get.got.out 2>02.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "step-2 be get exited $rc; stderr:
$(cat 02.get.got.err)"

# Old anchor gone.
[ ! -f vendor/sub/.be ] || fail "vendor/sub anchor unexpectedly present"
grep -q '^be: get vendor/sub: unmounted' 02.get.got.err \
    || fail "unmount marker for vendor/sub missing; stderr:
$(cat 02.get.got.err)"

# New mount present at libs/sub.
[ -d libs/sub ]          || fail "libs/sub dir missing"
[ -f libs/sub/.be ]      || fail "libs/sub anchor missing"
[ ! -d libs/sub/.be ]    || fail "libs/sub anchor should be a file"
[ -f libs/sub/core.c ]   || fail "libs/sub content missing — fresh mount failed"

# Marker for the new mount.
grep -q '^be: get libs/sub' 02.get.got.err \
    || fail "mount marker for libs/sub missing"

note "get/17-sub-rename: vendor/sub → libs/sub at same pin"
