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

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

# --- Step 1: clone at master (vendor/sub mounted). -------------------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
[ -f vendor/sub/.be ]    || fail "step-1: vendor/sub not mounted"
[ -f vendor/sub/core.c ] || fail "step-1: sub content missing"
[ ! -e libs/sub ]        || fail "step-1: libs/sub unexpectedly present"

# --- Step 2: switch to rename — vendor/sub unmounts, libs/sub mounts.
#  --tlv so the relay (stdout) carries the new mount's rebased hunk; the
#  unmount log still goes to stderr unconditionally (beagle/SUBS.c:537).
"$BE" get "$PARENT_URL?rename" --tlv >02.get.got.out 2>02.get.got.err
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

#  New mount proven via the --tlv relay (GET-026): the parent emits its
#  own rows (e.g. the `.gitmodules` change), THEN relays the renamed sub
#  with its rows rebased under the new mount path (`libs/sub?<hashlet>`);
#  pre-order ⇒ the parent's own row precedes the relayed sub hunk.  The
#  old `be: get libs/sub` stderr marker is now trace-only (ABC_TRACE).
#  TLV is NUL-laden binary; split on NUL into lines for stream order.
LC_ALL=C tr '\0' '\n' < 02.get.got.out > 02.tlv.lines
LC_ALL=C grep -aqE 'libs/sub\?[0-9a-f]' 02.tlv.lines \
    || fail "no relayed 'libs/sub' mount hunk in --tlv stream:
$(cat -v 02.tlv.lines | head -c 400)"
parent_ln=$(LC_ALL=C grep -anE 'cat:.gitmodules|commit:' 02.tlv.lines | head -1 | cut -d: -f1)
libs_ln=$(  LC_ALL=C grep -anE 'libs/sub\?[0-9a-f]'      02.tlv.lines | head -1 | cut -d: -f1)
[ -n "$parent_ln" ] && [ -n "$libs_ln" ] && [ "$parent_ln" -lt "$libs_ln" ] \
    || fail "expected parent row (ln=$parent_ln) before libs/sub hunk (ln=$libs_ln) in --tlv relay"

note "get/17-sub-rename: vendor/sub → libs/sub at same pin"
