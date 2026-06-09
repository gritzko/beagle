#!/bin/sh
#  post/33-push-proj-selector — POST-009: `be post file://<peer>?/<proj>`
#  (the PROJECT-SELECTOR push form) must FF-advance the peer's trunk AND
#  advance the peer worktree, exactly like the bare `<path>` form (post/32).
#
#  The bug: the sender treated the `?/<proj>` query as a BRANCH name and
#  pushed onto a bogus `refs/heads//<proj>` ref.  The peer's trunk never
#  moved (the pushed objects landed on a phantom branch), the recv
#  wt-advance (`be get ?`) then resolved trunk to the OLD tip, and
#  `be sha1:?` reported the stale sha — the push silently did not land.
#  Fix (keeper/KEEP.exe.c keeper_post): a leading-`/` query is a project
#  selector, not a branch; strip it (DOGQueryStripProject) so what's left
#  is the branch (empty = trunk).  See [POST-009].
#
#  Hermetic: runs entirely under $SCRATCH (case.sh roots it at $HOME/tmp,
#  ext4) with a `.be` shield + firewall — never the real `~/.be`.  No ssh;
#  drives the local-exec keeper `file://` edge (same as post/32).
#
#  Setup (two worktrees under one $SCRATCH):
#    A — the peer.  Project name = "A" (its dir basename).  One commit.
#    B — cloned from A.  Edits hello.c, commits, then FF-pushes back to A
#        via `be post file://<abs-A>?/A` (the project-selector form).
#
#  Asserts:
#    1. The push exits 0 and reports a TRUNK push (not `?/A` / `?/proj`).
#    2. A's trunk ref FF-advanced to B's tip (`be sha1:?`).
#    3. A's hello.c on disk reflects B's bytes (recv wt-advance landed).

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh seeds $SCRATCH/.be to shield walk-up; per-node anchors below.
rm -rf "$SCRATCH/.be"

A="$SCRATCH/A"
B="$SCRATCH/B"
mkdir -p "$A/.be" "$B/.be"

# ====================================================================
# 1. seed A with one commit on trunk (cur).  Project = basename "A".
# ====================================================================
( cd "$A"
  cp "$CASE/00.hello.c" hello.c
  sleep 0.02; "$BE" put hello.c       > "$SCRATCH/01a.put.out"  2> "$SCRATCH/01a.put.err"
  sleep 0.02; "$BE" post 'seed at A'  > "$SCRATCH/01a.post.out" 2> "$SCRATCH/01a.post.err" )

A_tip_before=$( cd "$A" && "$BE" sha1:? )
[ -n "$A_tip_before" ] || { echo "post/33: A has no tip after seed" >&2; exit 1; }
match "$CASE/00.hello.c" "$A/hello.c"

# ====================================================================
# 2. clone A into B via file:// (host-less local store).
# ====================================================================
sleep 0.02
( cd "$B"
  "$BE" get "file://$A?" > "$SCRATCH/02b.get.out" 2> "$SCRATCH/02b.get.err"
) || { echo "post/33: B failed to clone A: $(cat "$SCRATCH/02b.get.err")" >&2; exit 1; }
match "$CASE/00.hello.c" "$B/hello.c"

# ====================================================================
# 3. B edits, commits, then FF-pushes to A via the PROJECT-SELECTOR
#    form `file://<A>?/A`.  Pre-fix this lands objects on a phantom
#    `refs/heads//A` ref and trunk never advances.
# ====================================================================
sleep 0.02
( cd "$B"
  cp "$CASE/01.hello.c" hello.c
  sleep 0.02; "$BE" post 'B edits hello.c'  > "$SCRATCH/03b.post.out" 2> "$SCRATCH/03b.post.err"
  sleep 0.02; "$BE" post "file://$A?/A"     > "$SCRATCH/04b.push.out" 2> "$SCRATCH/04b.push.err"
) || { echo "post/33: B push failed: $(cat "$SCRATCH/04b.push.err")" >&2; exit 1; }
B_tip=$( cd "$B" && "$BE" sha1:? )

#  The push must announce a TRUNK push, never a `?/A` / `?/proj` branch.
grep -q "keeper: pushed" "$SCRATCH/04b.push.out" || {
    echo "post/33: push did not report success" >&2
    echo "  stdout:" >&2; cat "$SCRATCH/04b.push.out" >&2
    echo "  stderr:" >&2; cat "$SCRATCH/04b.push.err" >&2
    exit 1
}
grep -q "pushed ?/" "$SCRATCH/04b.push.out" && {
    echo "post/33: push targeted a project-named branch (POST-009 regression)" >&2
    cat "$SCRATCH/04b.push.out" >&2
    exit 1
}

# ====================================================================
# 4. A's trunk FF-advanced to B's tip (objects transferred + ref moved).
# ====================================================================
A_tip_after=$( cd "$A" && "$BE" sha1:? )
[ -n "$A_tip_after" ] || { echo "post/33: A has no tip after push" >&2; exit 1; }
[ "$A_tip_after" != "$A_tip_before" ] || {
    echo "post/33: A's trunk did not advance after ?/proj push (rolled back)" >&2
    echo "  before=$A_tip_before  after=$A_tip_after" >&2
    exit 1
}
[ "$A_tip_after" = "$B_tip" ] || {
    echo "post/33: A's trunk ($A_tip_after) != B's pushed tip ($B_tip)" >&2
    exit 1
}

# ====================================================================
# 5. A's hello.c on disk reflects B's bytes (recv wt-advance landed).
# ====================================================================
match "$CASE/01.hello.c" "$A/hello.c"

echo "post/33: A advanced $A_tip_before -> $A_tip_after via ?/proj push; wt followed"
