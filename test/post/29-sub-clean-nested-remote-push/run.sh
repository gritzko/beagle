#!/bin/sh
#  post/29-sub-clean-nested-remote-push — SUBS-006 repro.
#
#  A `be post ssh://<host>/<parent>.git?master` (git-peer remote) must
#  not abort when the only dirty project is the parent and the nested
#  subs are CLEAN.  The `.git` suffix routes the remote as a git peer,
#  so the push itself does NOT recurse into subs (the user manages git
#  submodules) — but the LOCAL-commit recursion still descends into
#  every mounted sub before the parent's git push.
#
#  Bug (SUBS-006): the local-commit recursion forwarded a bare
#  `be post -q ?/<sub>/.<parent>/master` (no `#msg`) into each clean,
#  detached sub.  With no msg and no in-scope patch rows that routes to
#  POSTPromote, which refused the missing synthetic branch with
#  NOBRANCH ("does not exist — `be put ?<branch>` first").  `be post`
#  then aborted the whole push: "aborting parent commit — sub recursion
#  failed" (exit 157).  A clean sub with nothing to commit must be a
#  NO-OP, not an abort.
#
#  Depth-3 forest (parent → vendor/sub → vendor/sub/vendor/leaf); only
#  the parent is edited, so BOTH subs stay clean and detached at their
#  gitlink pins.
#
#  Test:
#    1. `be get $PARENT_URL?master` mounts all three levels.
#    2. Edit + stage parent's main.c only; subs stay clean.
#    3. `be post '#parent only'` — local parent commit (no push).
#    4. `be post $PARENT_URL` — git-peer remote push.  The local-commit
#       recursion descends into the clean subs; each must no-op rather
#       than refuse NOBRANCH.  The parent's git origin advances to the
#       new local tip.

. "$(dirname "$0")/../../lib/sub-deep.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f vendor/sub/sub.txt ] \
    || fail "vendor/sub/sub.txt missing — sub not mounted"
[ -f vendor/sub/vendor/leaf/leaf.txt ] \
    || fail "vendor/sub/vendor/leaf/leaf.txt missing — leaf not mounted"

#  Record each level's clean baseline tip (subs must NOT move).
base_sub=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                        END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
           vendor/sub/.be)
base_leaf=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            vendor/sub/vendor/leaf/.be)
[ "$base_sub"  = "$SUB_TIP"  ] || fail "sub baseline=$SUB_TIP got $base_sub"
[ "$base_leaf" = "$LEAF_TIP" ] || fail "leaf baseline=$LEAF_TIP got $base_leaf"

par_orig_pre=$(git -C "$PARENT_BARE" rev-parse master)

#  --- 2. dirty ONLY the parent; subs stay clean. ---------------------
sleep 0.02
cat >> main.c <<'EOF'

int parent_only_v2(void) { return 7; }
EOF
#  Stage explicitly: the auto-emitted `put vendor/sub#<sha>` rows trip
#  sniff into selective mode, so an unstaged parent edit would be
#  invisible (see post/12 note).
"$BE" put main.c >01a.put.got.out 2>01a.put.got.err
rc=$?
[ "$rc" = 0 ] || fail "be put main.c exited $rc; stderr:
$(cat 01a.put.got.err)"

#  --- 3. local parent commit (no push yet). --------------------------
"$BE" post '#parent only' >02.post.got.out 2>02.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post (local) exited $rc; stderr:
$(cat 02.post.got.err)"

par_local=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            .be/wtlog)
[ "$par_local" != "$PARENT_TIP" ] || fail "parent did not commit locally"

#  --- 4. git-peer remote push.  MUST NOT abort on the clean subs. ----
"$BE" post "$PARENT_URL" >03.push.got.out 2>03.push.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post (push) exited $rc — clean nested sub aborted the push; stderr:
$(cat 03.push.got.err)"

#  The NOBRANCH refusal must not surface for a clean sub.
if grep -q 'does not exist' 03.push.got.err; then
    fail "clean sub raised NOBRANCH on git-peer push:
$(cat 03.push.got.err)"
fi
if grep -q 'aborting parent commit' 03.push.got.err; then
    fail "git-peer push aborted on a clean sub:
$(cat 03.push.got.err)"
fi

#  Parent's git origin advanced to the new local tip.
par_orig_post=$(git -C "$PARENT_BARE" rev-parse master)
[ "$par_orig_post" != "$par_orig_pre" ] \
    || fail "parent git origin did not advance: $par_orig_pre (still); stderr:
$(cat 03.push.got.err)"

#  Subs stayed clean — their tips did not move.
sub_after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            vendor/sub/.be)
leaf_after=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                          END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
             vendor/sub/vendor/leaf/.be)
[ "$sub_after"  = "$base_sub"  ] || fail "clean sub tip moved: $base_sub -> $sub_after"
[ "$leaf_after" = "$base_leaf" ] || fail "clean leaf tip moved: $base_leaf -> $leaf_after"

note "post/29-sub-clean-nested-remote-push: clean nested subs no-op; git push advanced parent origin"
