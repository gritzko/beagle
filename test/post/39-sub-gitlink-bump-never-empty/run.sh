#!/bin/sh
#  post/39-sub-gitlink-bump-never-empty — REPRO for SUBS-019.  `be post`
#  must NEVER record the git empty-blob sha
#  e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 (a non-commit, mode 100644)
#  as a submodule gitlink.
#
#  The parent post stages a sub-pin bump as a `put <subpath>#<40-hex>`
#  row (written by the post-order recursion's `be put <subpath>`, which
#  reads the mounted sub's tip via SNIFFSubReadTip).  When the gitlink
#  path ALSO shows up in the parent's wt scan as a regular file — which
#  a `be patch` / `be get` sub re-mount can leave behind where the mount
#  dir used to be — sniff's classifier used to fall through to hashing
#  that on-disk file as a blob and record the parent gitlink as the
#  empty-blob sha with mode 100644.  The resulting commit is poisoned:
#  the next `be get` / `be patch` of that sub dies `sniff: not a commit`
#  (SUBSPARSE).  This blocked MEM-021's land.
#
#  Fix (sniff/POST.c post_classify_step): a 40-hex `put <path>#<sha>`
#  row is unambiguously a gitlink bump and ALWAYS emits a `0160000`
#  gitlink ADD at that sha, regardless of any wt-scan entry at the path.
#
#  Assertion: the parent's committed `vendor/sub` tree entry is a
#  `160000` gitlink (NOT `100644`) and is NOT the empty-blob sha.
#  Pre-fix the entry is `100644 sub …e69de29b…`.  Registered WILL_FAIL
#  until SUBS-019 lands.

. "$(dirname "$0")/../../lib/submodules.sh"

EMPTY_BLOB=e69de29bb2d1d6434b8b29ae775ad8c2e48c5391

#  --- Custom fixture: a sub.git plus a parent.git with NO sub on master
#  (mirrors post/15).  A fresh submodule-add is the cleanest carrier of
#  the bug: no baseline gitlink (`base=0`), a 40-hex bump put row, and a
#  wt-scan entry at the gitlink path.
PSEED="$ETMP/p39-parent"; SSEED="$ETMP/p39-sub"
rm -rf "$PSEED" "$SSEED"

git init "$SSEED" >/dev/null 2>&1
git -C "$SSEED" config user.email t@t; git -C "$SSEED" config user.name T
git -C "$SSEED" checkout -b master >/dev/null 2>&1 || true
printf 'int core(void){return 1;}\n' > "$SSEED/core.c"
git -C "$SSEED" add core.c >/dev/null; git -C "$SSEED" commit -qm 'sub: core'
P39_SUB_BARE="$SCRATCH/p39-sub.git"
git init --bare "$P39_SUB_BARE" >/dev/null 2>&1
git -C "$SSEED" push -q "$P39_SUB_BARE" master:master
P39_SUB_TIP=$(git -C "$SSEED" rev-parse HEAD)
P39_SUB_URL="ssh://localhost/$REL_SCRATCH/p39-sub.git"

git init "$PSEED" >/dev/null 2>&1
git -C "$PSEED" config user.email t@t; git -C "$PSEED" config user.name T
git -C "$PSEED" checkout -b master >/dev/null 2>&1 || true
printf 'int main(void){return 0;}\n' > "$PSEED/main.c"
git -C "$PSEED" add main.c >/dev/null; git -C "$PSEED" commit -qm 'parent: main'
P39_PARENT_BARE="$SCRATCH/p39-parent.git"
git init --bare "$P39_PARENT_BARE" >/dev/null 2>&1
git -C "$PSEED" push -q "$P39_PARENT_BARE" master:master
P39_PARENT_URL="ssh://localhost/$REL_SCRATCH/p39-parent.git"

note "post/39: sub tip = $P39_SUB_TIP"

#  --- Clone the parent (no sub on master).
mkdir wt wt/.be && cd wt
"$BE" get "$P39_PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"

#  Declare + mount the sub (fetch + checkout into vendor/sub).
cat > .gitmodules <<EOF
[submodule "vendor/sub"]
	path = vendor/sub
	url = $P39_SUB_URL
EOF
"$BIN/sniff" sub-mount "./vendor/sub#$P39_SUB_TIP" >02.sm.out 2>02.sm.err \
    || fail "sniff sub-mount exited $?; stderr:
$(cat 02.sm.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing (sub not mounted)"

#  Stage the .gitmodules blob + the gitlink bump.  `be put vendor/sub`
#  reads the mounted sub's real tip and writes a `put vendor/sub#<40-hex>`
#  row — exactly what the post-order recursion stages.
"$BE" put .gitmodules >03.put.out 2>03.put.err \
    || fail "be put .gitmodules exited $?; stderr:
$(cat 03.put.err)"
"$BE" put vendor/sub >04.put.out 2>04.put.err \
    || fail "be put vendor/sub exited $?; stderr:
$(cat 04.put.err)"
BUMP=$(grep '	put	vendor/sub#' .be/wtlog | tail -1 | sed 's/.*#//')
case "$BUMP" in
    ????????????????????????????????????????) : ;;   # exactly 40 hex
    *) fail "no 40-hex gitlink bump row staged; wtlog:
$(cat .be/wtlog)" ;;
esac

#  Reproduce the sub re-mount stray: a `be patch`/`be get` re-mount can
#  leave the gitlink path as a bare regular file in the wt where the
#  mount dir used to be, so the POST wt scan classifies vendor/sub as an
#  (empty) regular file.  With the bump row already staged, POST must
#  STILL record the gitlink — never the on-disk file.
rm -rf vendor/sub
: > vendor/sub          # empty regular file at the gitlink path

#  Commit.
"$BE" post '#add vendor/sub' >05.post.out 2>05.post.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 05.post.err)"

#  --- SUBS-019 core assertion: the parent's committed vendor/sub entry
#  is a `160000` gitlink at the real sub tip, NEVER an `e69de29b` blob.
PCOMMIT=$(grep '	post	' .be/wtlog | tail -1 | sed 's/.*#//')
[ -n "$PCOMMIT" ] || fail "no post row in wtlog:
$(cat .be/wtlog)"

#  Resolve commit -> top tree -> vendor subtree.  Keeper stores
#  byte-exact git trees: each entry is "<octal-mode> <name>\0<20-raw>",
#  so the 40-hex of the `vendor` subtree is the 20 bytes right after
#  the ASCII "vendor\0" marker.
TOPTREE=$("$KEEPER" get ".#$PCOMMIT" 2>/dev/null | sed -n 's/^tree //p')
[ -n "$TOPTREE" ] || fail "could not read tree of parent commit $PCOMMIT"
TOPHEX=$("$KEEPER" get ".#$TOPTREE" 2>/dev/null | od -An -v -tx1 | tr -d ' \n')
VENHEX=$(printf 'vendor' | od -An -v -tx1 | tr -d ' \n')
AFTER=${TOPHEX#*"${VENHEX}00"}
[ "$AFTER" != "$TOPHEX" ] || fail "vendor subtree not found in top tree $TOPTREE"
VTREE=$(printf '%s' "$AFTER" | cut -c1-40)

#  Dump the vendor subtree; squeeze all whitespace so the `sub` entry's
#  mode prefix reads as space-separated octal digits.  `160000 sub` =
#  good gitlink; `100644 sub` = the SUBS-019 poison (a blob).
VDUMP=$("$KEEPER" get ".#$VTREE" 2>/dev/null | od -An -c | tr -s ' \n' ' ')

if printf '%s' "$VDUMP" | grep -q '1 0 0 6 4 4 s u b'; then
    fail "SUBS-019: parent committed vendor/sub as a 100644 BLOB (poisoned \
gitlink), not a 160000 gitlink.  vendor subtree dump:
$("$KEEPER" get ".#$VTREE" 2>/dev/null | od -An -c)"
fi
printf '%s' "$VDUMP" | grep -q '1 6 0 0 0 0 s u b' \
    || fail "SUBS-019: vendor/sub is not a 160000 gitlink in the committed \
tree; vendor subtree dump:
$("$KEEPER" get ".#$VTREE" 2>/dev/null | od -An -c)"

#  Belt-and-suspenders: the committed gitlink sha must NOT be the
#  empty-blob sha.
"$KEEPER" get ".#$VTREE" 2>/dev/null | od -An -v -tx1 | tr -d ' \n' \
    | grep -qi "$EMPTY_BLOB" \
    && fail "SUBS-019: committed vendor/sub gitlink IS the empty-blob sha \
$EMPTY_BLOB (poisoned non-commit)"

note "post/39 ok: gitlink bump recorded a 160000 sub commit, never e69de29b"
