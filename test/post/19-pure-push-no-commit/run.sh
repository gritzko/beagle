#!/bin/sh
#  post/19-pure-push-no-commit — `be post //origin` (pure-push form)
#  must NOT mint a new commit on cur, even when a sub-mount triggers
#  BEActSubsPost's gitlink-bump auto-`put`.
#
#  Spec (VERBS.md §POST):
#    "`be post //origin` — FF-push cur's tip to origin's counterpart
#     over the wire. **No commit.**"
#    "Standalone (`be post //origin`) is a pure push — no commit;
#     equivalent in effect to `be put //origin`."
#
#  Bug site (cross-references TRIANGLE.todo.md §"BEActSubsPost…
#  selective mode"):  with a sub mount present, BEActSubsPost runs
#  BEFORE BEActSniffPost in BE_PLAN_POST.  It recurses into each
#  sub, posts/pushes there, then synthesises a
#  `put vendor/sub#<sha>` row on the parent (the gitlink bump).
#  Sniff sees that put row, classifies the parent as a "selective-
#  mode commit"; if any other condition (e.g. an unrelated tracked-
#  dirty file the user didn't touch this session) trips the
#  commit-all path on top of that, the parent silently grows a new
#  commit object on cur even though POST was called with no `#frag`
#  and only a `//remote` slot.  In the originating trace the cur
#  tip moved past a local commit (f922149d, see Bug 6) onto a
#  freshly minted commit on top of the server tip.
#
#  This test pins the simpler shape: with a clean parent and a sub
#  that locally commits + FF-pushes, the gitlink bump alone must
#  NOT mint a parent commit on top of cur.
#
#  Repro shape (rides on lib/submodules.sh's parent + bare-sub
#  fixture):
#    1. Mount parent ?master into wt.  Sub lands at vendor/sub
#       pinned to PARENT_PINNED (=SUB_C2).
#    2. Enter the sub and add ONE commit there (sub now tip > pin).
#    3. From the parent, run `be post //localhost` — pure push.
#    4. Assert: parent's cur tip is UNCHANGED across the call.
#       (The sub may have been pushed; that's fine.  The bug is
#       that the parent grew a commit.)
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err || \
    fail "be get parent failed: $(cat 01.get.got.err)"

#  Snapshot parent's cur tip before any of the sub work.
parent_tip_before=$(head_hex)
[ -n "$parent_tip_before" ] || fail "no parent tip after get"

#  FF the sub mount to bare's tip (SUB_C3) so a subsequent local
#  sub commit can FF-push.  Without this step the sub baseline is
#  the parent's pin (SUB_C2) and any local commit on top of C2
#  diverges from the bare's existing C3.
( cd vendor/sub
  "$BE" get "$SUB_URL?master" > "$ETMP/02a.subff.out" 2> "$ETMP/02a.subff.err"
) || fail "sub FF to tip failed: $(cat $ETMP/02a.subff.err 2>/dev/null)"

#  Now add a commit in the sub on top of SUB_C3.
sleep 0.02
( cd vendor/sub
  echo "// sub edit on top of C3" >> core.c
  "$BE" put core.c       > "$ETMP/02.subput.out"  2> "$ETMP/02.subput.err"
  "$BE" post '#sub edit' > "$ETMP/03.subpost.out" 2> "$ETMP/03.subpost.err"
) || fail "sub-side post failed: $(cat $ETMP/03.subpost.err 2>/dev/null)"

sub_tip_after_local_post=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                                        END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
                             vendor/sub/.be)

#  PARENT must still be at its baseline tip — we only edited the sub.
parent_tip_pre_push=$(head_hex)
[ "$parent_tip_pre_push" = "$parent_tip_before" ] \
    || fail "parent tip moved before push?! ($parent_tip_before -> $parent_tip_pre_push)"

#  ----------------------------------------------------------------
#  The check: pure-push `be post //localhost` from the PARENT.  No
#  msg, no `?branch`, only `//remote`.  Must NOT mint a commit on
#  cur.  Whether the push fails or succeeds against the bare repo
#  isn't the point of this test — the point is that the parent's
#  cur tip is UNCHANGED.
#  ----------------------------------------------------------------
"$BE" post "//localhost" >04.post.got.out 2>04.post.got.err || true

parent_tip_after=$(head_hex)
[ "$parent_tip_after" = "$parent_tip_before" ] || {
    echo "FAIL: pure-push 'be post //localhost' minted a commit on cur" >&2
    echo "      parent tip before: $parent_tip_before" >&2
    echo "      parent tip after : $parent_tip_after" >&2
    echo "      post stderr:" >&2
    sed 's/^/        /' 04.post.got.err >&2
    exit 1
}

note "post/19: pure-push left parent tip $parent_tip_before unchanged"
