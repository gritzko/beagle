#!/bin/sh
#  post/15-sub-add-then-post — adding a new submodule + committing
#  it: parent's commit tree must contain BOTH the new `.gitmodules`
#  blob AND the `160000` gitlink entry at the mount path.
#
#  Per plan §POST step 3 ("Parent synthesises a new `.gitmodules`
#  via `SNIFFSubsSynth` if the live mount set differs from
#  baseline's blob; stages it as `put .gitmodules`") the wrapper
#  could synthesise `.gitmodules` itself.  Until that auto-synth
#  lands, this case drives it through the user-explicit path:
#  edit `.gitmodules` on disk, mount the sub via `sniff sub-mount`,
#  stage both via `be put`, then `be post`.  Same end state — the
#  parent's pushed tree has the new sub recorded — and the test
#  pins the contract `be post` needs to keep honoring once the
#  auto-synth fires.
#
#  Custom fixture: a parent.git WITHOUT any submodule on master plus
#  a separate sub.git.  (submodules.sh always lands a sub at master;
#  using its `?prev` branch puts keeper into a branch-shard, and the
#  fetched sub objects can't be picked up by the secondary-wt child
#  sniff which opens trunk only.)
#
#  Test:
#    1. Clone parent (no sub on master).
#    2. Declare `.gitmodules` for vendor/sub.
#    3. `sniff sub-mount ./vendor/sub#<sub-tip>` — fetch + mount.
#    4. Edit vendor/sub/core.c.
#    5. `be put .gitmodules`.
#    6. `be post '#add vendor/sub'`.
#    7. Verify .gitmodules + gitlink in parent's new commit tree.

. "$(dirname "$0")/../../lib/branches.sh"

export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || fail "post/15: \$HOME unset"
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) fail "post/15: SCRATCH=$SCRATCH not under \$HOME=$HOME" ;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

# case.sh seeds `$SCRATCH/.be/` for ambient sniff-meta; submodule
# tests want a clean primary-wt layout under $SCRATCH/wt so they
# can be the sole project at the cwd root.  Drop the case.sh `.be/`
# here (mirrors submodules.sh).
rmdir "$SCRATCH/.be" 2>/dev/null || true

# ---- bare repos -----------------------------------------------------
PARENT_BARE="$SCRATCH/parent.git"
SUB_BARE="$SCRATCH/sub.git"
PARENT_URL="ssh://localhost/$REL_SCRATCH/parent.git"
SUB_URL="ssh://localhost/$REL_SCRATCH/sub.git"

git init --bare "$PARENT_BARE" >/dev/null
git init --bare "$SUB_BARE"    >/dev/null

PSEED="$ETMP/parent-seed-15"
SSEED="$ETMP/sub-seed-15"
rm -rf "$PSEED" "$SSEED"

# sub seed (one commit, one file)
git init "$SSEED" >/dev/null
git -C "$SSEED" config user.email t@t
git -C "$SSEED" config user.name  T
git -C "$SSEED" checkout -b master >/dev/null 2>&1 || true
echo "sub payload" > "$SSEED/core.c"
git -C "$SSEED" add core.c >/dev/null
git -C "$SSEED" commit -qm 'sub: initial'
SUB_TIP=$(git -C "$SSEED" rev-parse HEAD)
git -C "$SSEED" push -q "$SUB_BARE" master:master

# parent seed (one commit, one file — NO sub yet)
git init "$PSEED" >/dev/null
git -C "$PSEED" config user.email t@t
git -C "$PSEED" config user.name  T
git -C "$PSEED" checkout -b master >/dev/null 2>&1 || true
echo "int main(void) { return 0; }" > "$PSEED/main.c"
git -C "$PSEED" add main.c >/dev/null
git -C "$PSEED" commit -qm 'parent: initial'
PARENT_TIP=$(git -C "$PSEED" rev-parse HEAD)
git -C "$PSEED" push -q "$PARENT_BARE" master:master

note "post/15 fixture: parent_tip=$PARENT_TIP sub_tip=$SUB_TIP"

# ---- 1. clone parent (no sub) ---------------------------------------
mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f main.c ]      || fail "main.c missing after clone"
[ -f .gitmodules ] && fail ".gitmodules should not exist before sub-add"

# ---- 2. declare the sub ---------------------------------------------
cat > .gitmodules <<EOF
[submodule "vendor/sub"]
	path = vendor/sub
	url = $SUB_URL
EOF

# ---- 3. mount the sub -----------------------------------------------
mkdir -p vendor/sub
"$BIN/sniff" sub-mount "./vendor/sub#$SUB_TIP" \
    >02.mount.got.out 2>02.mount.got.err
rc=$?
[ "$rc" = 0 ] || fail "sniff sub-mount exited $rc; stderr:
$(cat 02.mount.got.err)"
[ -f vendor/sub/.be ]    || fail "sub anchor missing"
[ -f vendor/sub/core.c ] || fail "sub core.c missing"

# ---- 4. dirty the sub -----------------------------------------------
sleep 0.02
cat >> vendor/sub/core.c <<'EOF'
EXTRA
EOF

# ---- 5. stage .gitmodules -------------------------------------------
"$BE" put .gitmodules >03.put.got.out 2>03.put.got.err
rc=$?
[ "$rc" = 0 ] || fail "be put .gitmodules exited $rc; stderr:
$(cat 03.put.got.err)"

# ---- 6. recursive post ----------------------------------------------
"$BE" post '#add vendor/sub' >04.post.got.out 2>04.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 04.post.got.err)"

outer_tip=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            .be/wtlog)
sub_committed=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                              END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
                vendor/sub/.be)
[ -n "$outer_tip" ]                    || fail "outer tip empty"
[ "$sub_committed" != "$SUB_TIP" ]     || fail "sub did not commit"

# ---- 7. parent's new tree contains both pieces ----------------------
"$BE" "tree:?$outer_tip" >05.tree.got.out 2>05.tree.got.err
grep -q '\.gitmodules' 05.tree.got.out \
    || fail "parent tree missing .gitmodules:
$(cat 05.tree.got.out)"

"$BE" "tree:vendor/?$outer_tip" >06.vendor.tree.got.out 2>06.vendor.tree.got.err
grep -q "$sub_committed" 06.vendor.tree.got.out \
    || fail "parent vendor/ tree does not reference sub tip $sub_committed:
$(cat 06.vendor.tree.got.out)"

note "post/15-sub-add-then-post: .gitmodules + gitlink committed"
