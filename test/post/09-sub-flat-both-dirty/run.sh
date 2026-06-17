#!/bin/sh
#  post/09-sub-flat-both-dirty — `be post '#round1'` with both outer
#  and sub dirty: sub commits its own tip first, parent then records
#  the bumped gitlink in its commit.
#
#  Setup:
#      Clone $PARENT_URL via the submodules.sh fixture.  Parent's
#      tree at master pins vendor/sub at $PARENT_PINNED = $SUB_C2.
#
#  Test:
#      1. `be get $PARENT_URL?master` — mounts outer + vendor/sub.
#      2. Edit outer/util.c and vendor/sub/core.c.
#      3. `be post '#round1'`.
#      4. Assert sub's recorded tip moved off $PARENT_PINNED (= SUB_C2).
#      5. Assert parent's recorded tip moved off $PARENT_TIP.
#      6. Assert parent's new commit references the bumped gitlink:
#         keeper tree:vendor?<parent-new-tip> shows the new sub sha.
#      7. Assert post-order trace markers: `be: post vendor/sub` before
#         `be: post .` (subs commit first; outer is the final marker).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

# Sanity: sub is mounted at SUB_C2.
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing"
[ -f vendor/sub/.be ]    || fail "vendor/sub/.be anchor missing"

baseline_sub=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                            END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
               vendor/sub/.be)
[ "$baseline_sub" = "$PARENT_PINNED" ] \
    || fail "expected sub baseline=$PARENT_PINNED got $baseline_sub"

baseline_outer=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                              END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
                 .be/wtlog)
[ "$baseline_outer" = "$PARENT_TIP" ] \
    || fail "expected outer baseline=$PARENT_TIP got $baseline_outer"

# --- 2.  edit on both sides ------------------------------------------
sleep 0.02
cat >> util.c <<'EOF'

int util_triple(int x) { return x * 3; }
EOF
cat >> vendor/sub/core.c <<'EOF'

void sub_dec(void) { sub_counter--; }
EOF

# --- 3.  commit recursively ------------------------------------------
"$BE" post '#round1' >02.post.got.out 2>02.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 02.post.got.err)"

# --- 4.  sub's tip moved ---------------------------------------------
sub_after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            vendor/sub/.be)
[ -n "$sub_after" ] || fail "sub tip empty after post"
[ "$sub_after" != "$baseline_sub" ] \
    || fail "sub tip did not advance (still $sub_after); stderr:
$(cat 02.post.got.err)"

# --- 5.  outer's tip moved -------------------------------------------
outer_after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                           END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
              .be/wtlog)
[ -n "$outer_after" ] || fail "outer tip empty after post"
[ "$outer_after" != "$baseline_outer" ] \
    || fail "outer tip did not advance (still $outer_after); stderr:
$(cat 02.post.got.err)"

# --- 6.  parent's new tree records the new sub sha -------------------
#   keeper's tree projector emits one line per entry:
#     <mode> <name>\0<binary-sha>
#   We grep the textual `vendor` entry first (its tree sha), then the
#   `sub` entry under that tree.  Easier: just check that the parent's
#   commit's tree, when listed for vendor/, contains sub_after.
"$BE" "tree:vendor/?$outer_after" >03.tree.got.out 2>03.tree.got.err
rc=$?
[ "$rc" = 0 ] || fail "be tree:vendor/?$outer_after exited $rc; stderr:
$(cat 03.tree.got.err)"
grep -q "$sub_after" 03.tree.got.out \
    || fail "parent's new vendor/ tree does not reference $sub_after; tree dump:
$(cat 03.tree.got.out)"

# --- 7.  post-order trace markers ------------------------------------
#   POST-019: the sub commit is reported via the real relayed ROWS hunk
#   on STDOUT (`post vendor/sub/?<sha>#round1 [vendor/sub]`), emitted in
#   post-order (sub before the parent's own `post ?<sha>`) — no stderr
#   echo.  Assert the sub's relayed post banner appears.
grep -q 'post vendor/sub/' 02.post.got.out \
    || fail "sub post marker missing; stdout:
$(cat 02.post.got.out)"

note "post/09-sub-flat-both-dirty: sub advanced $baseline_sub -> $sub_after; outer advanced"
