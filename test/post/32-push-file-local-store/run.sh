#!/bin/sh
#  post/32-push-file-local-store — POST-008: `be post` must push to a
#  HOST-LESS local beagle store (`file:///abs/path`), not just a
#  host-bearing ssh/be:// URI.  This is the gap the ticket calls out:
#  `keeper_post`'s old `u8csEmpty(g->host)` gate rejected `file://`
#  (which carries no authority/host) with
#  `keeper: post needs a remote URI (ssh://host/path[?branch])` /
#  KEEPFAIL — so pushing to a LOCAL store was unreachable, even though
#  fetch (`be get file://...`) already worked over the same edge.
#
#  Unlike post/22 (`be://localhost`, an ssh-to-localhost hop, gated
#  under WITH_SSH), this case drives the LOCAL-EXEC keeper edge
#  (`file://` → `keeper receive-pack` spawned directly), so it needs
#  NO ssh and runs in the default suite.
#
#  Setup (all under one $SCRATCH, two worktrees):
#    A — the store/peer.  One commit on trunk (00.hello.c).
#    B — cloned from A via `file://`.  Edits hello.c → 01.hello.c,
#        commits, then FF-pushes back to A via `be post file://<abs-A>`.
#
#  Asserts:
#    1. The push exits 0 and prints `keeper: pushed`.
#    2. A's trunk ref FF-advanced to B's tip (objects transferred +
#       ref moved — the end-to-end push, not just passing the gate).
#    3. A's hello.c on disk reflects B's bytes (recv wt-advance).

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh seeds $SCRATCH/.be to shield walk-up; per-node anchors below.
rm -rf "$SCRATCH/.be"

A="$SCRATCH/A"
B="$SCRATCH/B"
mkdir -p "$A/.be" "$B/.be"

# ====================================================================
# 1. seed A with one commit on trunk (cur).
# ====================================================================
( cd "$A"
  cp "$CASE/00.hello.c" hello.c
  sleep 0.02; "$BE" put hello.c       > "$SCRATCH/01a.put.out"  2> "$SCRATCH/01a.put.err"
  sleep 0.02; "$BE" post 'seed at A'  > "$SCRATCH/01a.post.out" 2> "$SCRATCH/01a.post.err" )

A_tip_before=$( cd "$A" && "$BE" sha1:? )
[ -n "$A_tip_before" ] || { echo "post/32: A has no tip after seed" >&2; exit 1; }
match "$CASE/00.hello.c" "$A/hello.c"

# ====================================================================
# 2. clone A into B via file:// (host-less local store).
# ====================================================================
sleep 0.02
( cd "$B"
  "$BE" get "file://$A?" > "$SCRATCH/02b.get.out" 2> "$SCRATCH/02b.get.err"
) || { echo "post/32: B failed to clone A: $(cat "$SCRATCH/02b.get.err")" >&2; exit 1; }
match "$CASE/00.hello.c" "$B/hello.c"

# ====================================================================
# 3. B edits, commits, then FF-pushes to A via file:// (the bug site).
#    Pre-fix this dies with `keeper: post needs a remote URI` / KEEPFAIL
#    because file:// carries no host.
# ====================================================================
sleep 0.02
( cd "$B"
  cp "$CASE/01.hello.c" hello.c
  sleep 0.02; "$BE" post 'B edits hello.c'   > "$SCRATCH/03b.post.out" 2> "$SCRATCH/03b.post.err"
  sleep 0.02; "$BE" post "file://$A"         > "$SCRATCH/04b.push.out" 2> "$SCRATCH/04b.push.err"
) || { echo "post/32: B push failed: $(cat "$SCRATCH/04b.push.err")" >&2; exit 1; }
B_tip=$( cd "$B" && "$BE" sha1:? )

#  The push must have announced success, not the host-required refusal.
grep -q "keeper: pushed" "$SCRATCH/04b.push.out" || {
    echo "post/32: push did not report success" >&2
    echo "  stdout:" >&2; cat "$SCRATCH/04b.push.out" >&2
    echo "  stderr:" >&2; cat "$SCRATCH/04b.push.err" >&2
    exit 1
}
grep -q "needs a remote URI" "$SCRATCH/04b.push.err" && {
    echo "post/32: push hit the host-required gate (POST-008 regression)" >&2
    cat "$SCRATCH/04b.push.err" >&2
    exit 1
}

# ====================================================================
# 4. A's trunk FF-advanced to B's tip (objects transferred + ref moved).
# ====================================================================
A_tip_after=$( cd "$A" && "$BE" sha1:? )
[ -n "$A_tip_after" ] || { echo "post/32: A has no tip after push" >&2; exit 1; }
[ "$A_tip_after" != "$A_tip_before" ] || {
    echo "post/32: A's trunk did not advance after file:// push" >&2
    echo "  before=$A_tip_before  after=$A_tip_after" >&2
    exit 1
}
[ "$A_tip_after" = "$B_tip" ] || {
    echo "post/32: A's trunk ($A_tip_after) != B's pushed tip ($B_tip)" >&2
    exit 1
}

# ====================================================================
# 5. A's hello.c on disk reflects B's bytes (recv wt-advance).
# ====================================================================
match "$CASE/01.hello.c" "$A/hello.c"

echo "post/32: A advanced $A_tip_before -> $A_tip_after via file:// push; wt followed"
