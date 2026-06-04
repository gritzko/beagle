#!/bin/sh
#  post/11-sub-only-sub-dirty — outer clean, sub dirty.  `be post
#  '#tweak'` commits the sub; the parent commits exactly one tree
#  whose only diff vs baseline is the gitlink at vendor/sub.
#
#  Plan §POST: "a sub with no dirty paths just no-ops; parent doesn't
#  bump that gitlink".  Symmetric: when sub IS dirty, parent SHOULD
#  bump that one gitlink even though the outer worktree is otherwise
#  untouched — proving the wrapper stages bump rows independently of
#  the parent's own put/delete history.
#
#  Test sequence:
#    1. `be get $PARENT_URL?master` mounts parent + vendor/sub.
#    2. Edit vendor/sub/core.c.  Outer wt is untouched.
#    3. `be post '#tweak'`.
#    4. Sub's tip moves; outer's tip moves; outer tip's vendor/ tree
#       lists the new sub sha.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing"

baseline_sub=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                            END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
               vendor/sub/.be)
baseline_outer=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                              END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
                 .be/wtlog)

#  Outer is left clean.  Only the sub gets an edit.
sleep 0.02
cat >> vendor/sub/core.c <<'EOF'

void sub_zap(void) { sub_counter = 0; }
EOF

"$BE" post '#tweak' >02.post.got.out 2>02.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 02.post.got.err)"

sub_after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            vendor/sub/.be)
outer_after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                           END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
              .be/wtlog)

[ "$sub_after" != "$baseline_sub" ] \
    || fail "sub tip did not advance (still $sub_after); stderr:
$(cat 02.post.got.err)"
[ "$outer_after" != "$baseline_outer" ] \
    || fail "outer tip did not advance (still $outer_after); stderr:
$(cat 02.post.got.err)"

"$BE" "tree:vendor/?$outer_after" >03.tree.got.out 2>03.tree.got.err
grep -q "$sub_after" 03.tree.got.out \
    || fail "parent's vendor/ tree does not reference $sub_after; dump:
$(cat 03.tree.got.out)"

note "post/11: outer clean, sub dirty → both bumped, gitlink advanced"
