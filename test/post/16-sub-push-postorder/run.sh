#!/bin/sh
#  post/16-sub-push-postorder — post-order push ordering.
#
#  Plan §POST: when pushing, the sub's tip must land on its origin
#  BEFORE the parent's tree (which references that new sub sha)
#  lands on the parent's origin.  Otherwise a fresh `git clone
#  --recurse-submodules` would see a parent tree pointing at a sha
#  the sub bare repo doesn't have.
#
#  Note on remotes.  Under the current shared-store layout (sub
#  anchors to parent's `.be/`), parent and sub share one REFS
#  table — both ssh fetches register under `host=localhost`, so
#  `be post //localhost` is ambiguous (substring match picks the
#  last localhost row).  Until per-shard ALIAS lands (plan
#  §"sub-shard ALIAS seeding"), this case drives push order
#  manually: recursive commit first, then explicit ssh push per
#  project in post-order.  The assertion stays the same — sub's
#  bare repo must hold the sha that parent's pushed tree
#  references.
#
#  Test:
#    1. `be get $PARENT_URL?master` — mount.  Rewind $SUB_BARE to
#       $PARENT_PINNED so subsequent sub commits FF.
#    2. Edit outer + sub.
#    3. `be post '#sync'` — recursive commit (no push yet).
#    4. `cd vendor/sub && be post ssh://localhost/.../sub.git` —
#       FF-push sub first.
#    5. `cd back && be post ssh://localhost/.../parent.git` — push
#       parent.  Server side: sub origin advanced first, parent
#       references a sub sha that origin now holds.

. "$(dirname "$0")/../../lib/submodules.sh"

# Rewind sub.git/master so the local mount is at-tip → FF clean.
git -C "$SUB_BARE" update-ref refs/heads/master "$PARENT_PINNED"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

sub_orig_pre=$(git -C "$SUB_BARE" rev-parse master)
par_orig_pre=$(git -C "$PARENT_BARE" rev-parse master)

# --- 2. dirty both sides ---------------------------------------------
sleep 0.02
cat >> util.c <<'EOF'

int util_postord(void) { return 42; }
EOF
cat >> vendor/sub/core.c <<'EOF'

void sub_postord(void) { sub_counter += 10; }
EOF

# --- 3. recursive commit (no push yet) -------------------------------
"$BE" post '#sync' >02.post.got.out 2>02.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post exited $rc; stderr:
$(cat 02.post.got.err)"

sub_local=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; sub(/^[^#]*#/, "", h); print h }' \
            vendor/sub/.be)
par_local=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; sub(/^[^#]*#/, "", h); print h }' \
            .be/wtlog)
[ "$sub_local" != "$PARENT_PINNED" ] || fail "sub did not commit locally"
[ "$par_local" != "$PARENT_TIP"    ] || fail "parent did not commit locally"

# --- 4. push sub FIRST (post-order) ----------------------------------
(cd vendor/sub && "$BE" post "$SUB_URL" \
    >../../03.subpush.out 2>../../03.subpush.err)
sub_orig_after_subpush=$(git -C "$SUB_BARE" rev-parse master)
[ "$sub_orig_after_subpush" = "$sub_local" ] \
    || fail "sub push did not advance origin: $sub_orig_pre -> $sub_orig_after_subpush (want $sub_local); stderr:
$(cat 03.subpush.err)"

# --- 5. push parent SECOND -------------------------------------------
"$BE" post "$PARENT_URL" >04.parentpush.out 2>04.parentpush.err
par_orig_after=$(git -C "$PARENT_BARE" rev-parse master)
[ "$par_orig_after" = "$par_local" ] \
    || fail "parent push did not advance origin: $par_orig_pre -> $par_orig_after (want $par_local); stderr:
$(cat 04.parentpush.err)"

# --- 6. server-side check: parent's tree references a sha sub origin has ---
cd "$SCRATCH"
mkdir verify && cd verify
git clone -q "$PARENT_BARE" .
tree_sub_sha=$(git ls-tree HEAD vendor/sub | awk '{print $3}')
[ "$tree_sub_sha" = "$sub_local" ] \
    || fail "parent push references stale sub sha: tree=$tree_sub_sha want=$sub_local"
git -C "$SUB_BARE" cat-file -t "$sub_local" >/dev/null 2>&1 \
    || fail "sub origin lacks $sub_local — push order broke"

note "post/16-sub-push-postorder: sub origin advanced first; parent push references it"
