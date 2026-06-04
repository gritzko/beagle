#!/bin/sh
#  post/17-sub-msg-resolution-per-level — POST's "exactly one usable
#  msg" rule (VERBS.md §POST "Message resolution") runs at each
#  level INDEPENDENTLY: the sub auto-resolves its commit msg from
#  its own in-scope patch row, while the parent uses its explicit
#  #frag.  No msg inheritance across the recursion boundary.
#
#  Setup (submodules.sh): parent + sub.  Sub bare repo has SUB_C2
#  (gitlink pin) and SUB_C3 (advertised tip with msg "sub: add-by-n").
#  After mount, sub's keeper has both commits via WIREFetchAll.
#
#  Test:
#    1. `be get $PARENT_URL?master` — mount parent + vendor/sub at C2.
#    2. Inside vendor/sub: `be patch '#<SUB_C3-sha>'` — cherry-pick
#       C3 into the sub's wt.  Emits a patch row whose locator is
#       SUB_C3 — POST will auto-resolve to C3's message body.
#    3. Outer dirty.
#    4. `be post '#outer-explicit' --sub-msg vendor/sub=` — outer
#       uses its explicit msg; empty `--sub-msg` blanks the per-sub
#       msg URI, forcing the sub to auto-resolve from its patch row.
#    5. Verify:
#       - Outer's commit body contains "outer-explicit".
#       - Sub's commit body contains a substring of SUB_C3's msg
#         ("add-by-n" per submodules.sh seed).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

# Cherry-pick SUB_C3 into the sub mount.
(cd vendor/sub && "$BE" patch "#$SUB_C3" \
    >../../02.patch.got.out 2>../../02.patch.got.err)
rc=$?
[ "$rc" = 0 ] || fail "sub patch exited $rc; stderr:
$(cat 02.patch.got.err)"

# Outer dirty (independent of sub).
sleep 0.02
cat >> util.c <<'EOF'

int util_indep(void) { return 17; }
EOF

# Recursive post: outer uses '#outer-explicit', sub auto-resolves
# from its single patch row (blanked via empty --sub-msg).
"$BE" post --sub-msg vendor/sub= '#outer-explicit' \
    >03.post.got.out 2>03.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 03.post.got.err)"

outer_tip=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            .be/wtlog)
sub_tip=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                       END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
          vendor/sub/.be)

# Outer's commit body.
"$BE" "commit:?$outer_tip" >04.outer.commit.out 2>04.outer.commit.err
grep -q 'outer-explicit' 04.outer.commit.out \
    || fail "outer commit missing 'outer-explicit'; body:
$(cat 04.outer.commit.out)"
if grep -q 'add-by-n' 04.outer.commit.out; then
    fail "outer commit unexpectedly carries sub's msg:
$(cat 04.outer.commit.out)"
fi

# Sub's commit body — should carry SUB_C3's msg fragment.
(cd vendor/sub && "$BE" "commit:?$sub_tip" \
    >../../05.sub.commit.out 2>../../05.sub.commit.err)
grep -q 'add-by-n' 05.sub.commit.out \
    || fail "sub commit missing C3's 'add-by-n' (auto-resolved msg); body:
$(cat 05.sub.commit.out)"
if grep -q 'outer-explicit' 05.sub.commit.out; then
    fail "sub commit unexpectedly carries parent's msg:
$(cat 05.sub.commit.out)"
fi

note "post/17-sub-msg-resolution-per-level: each level resolved its own msg"
