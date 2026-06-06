#!/bin/sh
#  put/11-sub-add-refusal — SUBS-009.  `be put <subpath>` on a directory
#  that is meant to become a NEW submodule but is not yet mounted (no
#  child `.be` anchor) MUST give an actionable refusal that names the
#  missing piece — declare `.gitmodules` + `sniff sub-mount` — instead
#  of the misleading bare "does not exist — skipped" / PUTNONE (206).
#
#  Two probes:
#    A.  `vendor/sub2` is declared in `.gitmodules` (URL present) and the
#        dir exists on disk, but it has not been mounted yet.  PUT must
#        refuse with a sub-add hint, NOT "does not exist".
#    B.  Once the user follows the named flow (sniff sub-mount), the
#        existing PUT short-circuit stages the gitlink bump and a POST
#        records BOTH the `.gitmodules` section AND the `160000` entry —
#        proving the refusal points at a flow that actually works.
#
#  Custom 2-sub fixture (mirrors post/15): a parent.git with `vendor/sub`
#  pinned on master plus a separate sub2.git to be added.

. "$(dirname "$0")/../../lib/branches.sh"

export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || fail "put/11: \$HOME unset"
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) fail "put/11: SCRATCH=$SCRATCH not under \$HOME=$HOME" ;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

# case.sh seeds $SCRATCH/.be/; submodule tests want a clean primary-wt
# layout under $SCRATCH/wt so they are the sole project at the root.
rmdir "$SCRATCH/.be" 2>/dev/null || true

PARENT_BARE="$SCRATCH/parent.git"
SUB_BARE="$SCRATCH/sub.git"
SUB2_BARE="$SCRATCH/sub2.git"
PARENT_URL="ssh://localhost/$REL_SCRATCH/parent.git"
SUB_URL="ssh://localhost/$REL_SCRATCH/sub.git"
SUB2_URL="ssh://localhost/$REL_SCRATCH/sub2.git"

git init --bare "$PARENT_BARE" >/dev/null
git init --bare "$SUB_BARE"    >/dev/null
git init --bare "$SUB2_BARE"   >/dev/null

PSEED="$ETMP/parent-seed-11"
SSEED="$ETMP/sub-seed-11"
S2SEED="$ETMP/sub2-seed-11"
rm -rf "$PSEED" "$SSEED" "$S2SEED"

# --- sub (already pinned in parent) ----------------------------------
git init "$SSEED" >/dev/null
git -C "$SSEED" config user.email t@t; git -C "$SSEED" config user.name T
git -C "$SSEED" checkout -b master >/dev/null 2>&1 || true
echo "sub payload" > "$SSEED/core.c"
git -C "$SSEED" add core.c >/dev/null
git -C "$SSEED" commit -qm 'sub: initial'
SUB_TIP=$(git -C "$SSEED" rev-parse HEAD)
git -C "$SSEED" push -q "$SUB_BARE" master:master

# --- sub2 (to be added this session) ---------------------------------
git init "$S2SEED" >/dev/null
git -C "$S2SEED" config user.email t@t; git -C "$S2SEED" config user.name T
git -C "$S2SEED" checkout -b master >/dev/null 2>&1 || true
echo "sub2 payload" > "$S2SEED/lib.c"
git -C "$S2SEED" add lib.c >/dev/null
git -C "$S2SEED" commit -qm 'sub2: initial'
SUB2_TIP=$(git -C "$S2SEED" rev-parse HEAD)
git -C "$S2SEED" push -q "$SUB2_BARE" master:master

# --- parent (main.c + .gitmodules{vendor/sub} + gitlink) -------------
git init "$PSEED" >/dev/null
git -C "$PSEED" config user.email t@t; git -C "$PSEED" config user.name T
git -C "$PSEED" checkout -b master >/dev/null 2>&1 || true
echo "int main(void){return 0;}" > "$PSEED/main.c"
cat > "$PSEED/.gitmodules" <<EOF
[submodule "vendor/sub"]
	path = vendor/sub
	url = $SUB_URL
EOF
git -C "$PSEED" add main.c .gitmodules >/dev/null
git -C "$PSEED" update-index --add --cacheinfo 160000,"$SUB_TIP",vendor/sub
git -C "$PSEED" commit -qm 'parent: with vendor/sub'
git -C "$PSEED" push -q "$PARENT_BARE" master:master

note "put/11 fixture: sub2_tip=$SUB2_TIP"

# ---- clone parent recursively (--sub: git source) -------------------
mkdir wt wt/.be && cd wt
"$BE" get --sub "$PARENT_URL?master" >01.get.out 2>01.get.err \
    || fail "be get exited $?; stderr:
$(cat 01.get.err)"
[ -f vendor/sub/.be ] || fail "vendor/sub not mounted after clone"

# ---- declare the new sub in .gitmodules, make its dir present -------
cat >> .gitmodules <<EOF
[submodule "vendor/sub2"]
	path = vendor/sub2
	url = $SUB2_URL
EOF
mkdir -p vendor/sub2

# ==== Probe A: PUT on the declared-but-unmounted sub =================
rc=0
"$BE" put vendor/sub2 >02.put.out 2>02.put.err || rc=$?
[ "$rc" != 0 ] \
    || fail "be put vendor/sub2 (unmounted) unexpectedly exited 0; out:
$(cat 02.put.out 02.put.err)"

#  MUST NOT be the misleading bare "does not exist" message.
if grep -q 'does not exist' 02.put.err; then
    fail "SUBS-009: be put on an unmounted sub-add candidate still says \
'does not exist' — refusal must name the missing piece (declare \
.gitmodules + sniff sub-mount); stderr:
$(cat 02.put.err)"
fi
#  MUST name the actionable flow (sub-mount).
grep -qi 'sub-mount' 02.put.err \
    || fail "SUBS-009: refusal does not name 'sub-mount' (the missing \
mount step); stderr:
$(cat 02.put.err)"

note "probe A ok: actionable sub-add refusal, not 'does not exist'"

# ==== Probe B: follow the named flow → it actually works ============
"$BIN/sniff" sub-mount "./vendor/sub2#$SUB2_TIP" \
    >03.mount.out 2>03.mount.err \
    || fail "sniff sub-mount exited $?; stderr:
$(cat 03.mount.err)"
[ -f vendor/sub2/.be ] || fail "vendor/sub2 anchor missing after sub-mount"

"$BE" put vendor/sub2 >04.put.out 2>04.put.err \
    || fail "be put vendor/sub2 (mounted) exited $?; stderr:
$(cat 04.put.err)"
grep -qE 'put[[:space:]]+vendor/sub2#[0-9a-f]{40}' .be/wtlog \
    || fail "mounted sub2 gitlink-bump row not staged:
$(cat .be/wtlog)"

"$BE" post --sub '#add vendor/sub2' >05.post.out 2>05.post.err \
    || fail "be post exited $?; stderr:
$(cat 05.post.err)"

outer=$(awk -F'\t' '$2=="post"{last=$3}
                    END{h=last; sub(/^.*#/,"",h); print h}' .be/wtlog)
[ -n "$outer" ] || fail "no post tip in wtlog"

"$BE" "tree:vendor/?$outer" >06.vtree.out 2>06.vtree.err
grep -q "$SUB2_TIP" 06.vtree.out \
    || fail "parent vendor/ tree missing sub2 gitlink $SUB2_TIP:
$(cat 06.vtree.out)"
"$BE" "blob:.gitmodules?$outer" >07.gm.out 2>07.gm.err
grep -q 'vendor/sub2' 07.gm.out \
    || fail "committed .gitmodules missing vendor/sub2 section:
$(cat 07.gm.out)"

note "put/11 ok: refusal names a flow that records sub2 (gitlink + .gitmodules)"
