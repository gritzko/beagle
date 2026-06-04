#!/bin/sh
#  post/12-sub-only-parent-dirty — outer dirty, sub clean.  Plan
#  §POST: "a sub with no dirty paths just no-ops; parent doesn't bump
#  that gitlink".
#
#  Test sequence:
#    1. `be get $PARENT_URL?master` mounts parent + vendor/sub.
#    2. Edit outer/util.c only.  Sub stays at $PARENT_PINNED.
#    3. `be post '#outer'`.
#    4. Sub's tip is UNCHANGED.
#    5. Outer's tip moved.
#    6. Outer's new vendor/ tree records the SAME sub sha as
#       baseline ($PARENT_PINNED) — no gitlink drift.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

baseline_sub=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                            END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
               vendor/sub/.be)
baseline_outer=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                              END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
                 .be/wtlog)
[ "$baseline_sub" = "$PARENT_PINNED" ] \
    || fail "expected sub baseline=$PARENT_PINNED got $baseline_sub"

#  Edit only on the outer side.
sleep 0.02
cat >> util.c <<'EOF'

int util_negate(int x) { return -x; }
EOF

#  BEActSubsPost auto-emits `put vendor/sub#<sha>` rows for each
#  sub before the parent post runs.  That auto-put trips sniff into
#  selective mode (per VERBS.md §POST "Per-file classification via
#  stamps"), where only explicitly-staged files get committed.
#  Without an explicit `be put util.c`, the parent edit is invisible
#  to the post → POSTNONE.  Stage it ourselves so the implicit-mode
#  assumption (commit-all-tracked-dirty) doesn't matter.
sleep 0.02
"$BE" put util.c >01a.put.got.out 2>01a.put.got.err
"$BE" post '#outer' >02.post.got.out 2>02.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 02.post.got.err)"

sub_after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            vendor/sub/.be)
outer_after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                           END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
              .be/wtlog)

[ "$sub_after" = "$baseline_sub" ] \
    || fail "sub tip moved unexpectedly ($baseline_sub -> $sub_after); stderr:
$(cat 02.post.got.err)"
[ "$outer_after" != "$baseline_outer" ] \
    || fail "outer tip did not advance (still $outer_after); stderr:
$(cat 02.post.got.err)"

"$BE" "tree:vendor/?$outer_after" >03.tree.got.out 2>03.tree.got.err
grep -q "$baseline_sub" 03.tree.got.out \
    || fail "parent's vendor/ tree no longer references baseline $baseline_sub; dump:
$(cat 03.tree.got.out)"

note "post/12: outer dirty, sub clean → outer commits, gitlink unchanged"
