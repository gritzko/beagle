#!/bin/sh
#  post/10-sub-depth3-cascading — 3-level forest commits cascade
#  post-order: leaf commits first, sub commits next (bumping leaf
#  gitlink), parent commits last (bumping sub gitlink).
#
#  Setup (sub-deep.sh): parent → vendor/sub → vendor/leaf.  All
#  three projects are dirty.
#
#  Test:
#    1. `be get $PARENT_URL?master` mounts all 3 levels.
#    2. Edit one file at each level.
#    3. `be post '#cascade'`.
#    4. Each level's wtlog tail advanced.
#    5. Parent's new vendor/ tree records the new sub sha.
#    6. Sub's new vendor/ tree records the new leaf sha.
#    7. Recursion-order trace: `be: post vendor/sub/vendor/leaf` appears
#       in stderr (the leaf marker is printed by the sub's wrapper
#       when it iterates into leaf).

. "$(dirname "$0")/../../lib/sub-deep.sh"

mkdir wt && cd wt
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f vendor/sub/sub.txt ]            || fail "vendor/sub/sub.txt missing"
[ -f vendor/sub/vendor/leaf/leaf.txt ] || fail "vendor/sub/vendor/leaf/leaf.txt missing"

base_outer=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                          END { h=last; sub(/^[^#]*#/, "", h); print h }' \
             .be/wtlog)
base_sub=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                        END { h=last; sub(/^[^#]*#/, "", h); print h }' \
           vendor/sub/.be)
base_leaf=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; sub(/^[^#]*#/, "", h); print h }' \
            vendor/sub/vendor/leaf/.be)
[ "$base_outer" = "$PARENT_TIP" ] || fail "outer baseline=$PARENT_TIP got $base_outer"
[ "$base_sub"   = "$SUB_TIP"    ] || fail "sub baseline=$SUB_TIP got $base_sub"
[ "$base_leaf"  = "$LEAF_TIP"   ] || fail "leaf baseline=$LEAF_TIP got $base_leaf"

#  Edit one file per level.
sleep 0.02
cat >> main.c <<'EOF'

int outer_v2(void) { return 2; }
EOF
cat >> vendor/sub/sub.txt <<'EOF'
sub payload v2
EOF
cat >> vendor/sub/vendor/leaf/leaf.txt <<'EOF'
leaf payload v2
EOF

"$BE" post '#cascade' >02.post.got.out 2>02.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 02.post.got.err)"

after_outer=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                            END { h=last; sub(/^[^#]*#/, "", h); print h }' \
              .be/wtlog)
after_sub=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                          END { h=last; sub(/^[^#]*#/, "", h); print h }' \
            vendor/sub/.be)
after_leaf=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                          END { h=last; sub(/^[^#]*#/, "", h); print h }' \
             vendor/sub/vendor/leaf/.be)

[ "$after_outer" != "$base_outer" ] || fail "outer tip stuck"
[ "$after_sub"   != "$base_sub"   ] || fail "sub tip stuck"
[ "$after_leaf"  != "$base_leaf"  ] || fail "leaf tip stuck"

#  Parent's new tree records new sub sha; sub's new tree records new
#  leaf sha.  Use the tree projector to read the relevant slot.
"$BE" "tree:vendor/?$after_outer" >03.outer.tree.out 2>03.outer.tree.err
grep -q "$after_sub" 03.outer.tree.out \
    || fail "outer vendor/ tree does not reference new sub $after_sub:
$(cat 03.outer.tree.out)"

# For the sub→leaf gitlink, query the SUB's vendor/ tree at sub's new tip.
# Easiest path: cd into the sub mount and run `be tree:vendor/?<sha>`.
(cd vendor/sub && "$BE" "tree:vendor/?$after_sub" >../../04.sub.tree.out 2>../../04.sub.tree.err)
grep -q "$after_leaf" 04.sub.tree.out \
    || fail "sub vendor/ tree does not reference new leaf $after_leaf:
$(cat 04.sub.tree.out)"

#  Post-order recursion trace: at least the leaf marker should appear
#  in the captured stderr — it surfaces from inside the sub's own
#  BEPost wrapper.
grep -q '^be: post vendor/leaf' 02.post.got.err \
    || fail "leaf post marker missing:
$(cat 02.post.got.err)"

note "post/10-sub-depth3-cascading: all 3 levels advanced; gitlinks chained"
