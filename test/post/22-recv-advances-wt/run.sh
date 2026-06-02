#!/bin/sh
#  post/22-recv-advances-wt — when a peer pushes to a branch via the
#  wire (`keeper receive-pack`), the receiver's primary wt on that
#  branch must follow: files updated to the new tip, `.be/wtlog`
#  grows a row recording the new base.  Without this, the user's
#  on-disk tree silently lags REFS — exactly the symptom that
#  prompted this test (`/home/gritzko/beagle` left at the old tip
#  after a remote post advanced trunk).
#
#  Setup:
#    A — receiver wt + project shard.  Has one commit (00.hello.c).
#    B — peer wt cloned from A.  Edits hello.c → 01.hello.c, commits,
#        FF-pushes to A via `be post ssh://localhost/<rel-A>`.
#
#  Asserts:
#    1. A's REFS advanced (sanity — that already works pre-fix).
#    2. A's hello.c on disk has B's bytes.  (The bug.)
#    3. A's `.be/wtlog` grew by ≥1 row referencing the new tip.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "post/22: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "post/22: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

#  case.sh seeds $SCRATCH/.be to shield walk-up; per-node anchors below.
rm -rf "$SCRATCH/.be"

A="$SCRATCH/A"
B="$SCRATCH/B"
REL_A="$REL_SCRATCH/A"

mkdir -p "$A/.be" "$B/.be"

# ====================================================================
# 1. seed A with one commit on trunk (cur).
# ====================================================================
( cd "$A"
  cp "$CASE/00.hello.c" hello.c
  sleep 0.02; "$BE" put hello.c        > "$SCRATCH/01a.put.out"  2> "$SCRATCH/01a.put.err"
  sleep 0.02; "$BE" post 'seed at A'   > "$SCRATCH/01a.post.out" 2> "$SCRATCH/01a.post.err" )

A_tip_before=$( cd "$A" && "$BE" sha1:? )
[ -n "$A_tip_before" ] || { echo "post/22: A has no tip after seed" >&2; exit 1; }
match "$CASE/00.hello.c" "$A/hello.c"
A_wtlog_lines_before=$(wc -l < "$A/.be/wtlog")

# ====================================================================
# 2. clone A into B via ssh — gives B the same baseline.
# ====================================================================
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" > "$SCRATCH/02b.get.out" 2> "$SCRATCH/02b.get.err"
) || { echo "post/22: B failed to clone A: $(cat $SCRATCH/02b.get.err)" >&2; exit 1; }
match "$CASE/00.hello.c" "$B/hello.c"

# ====================================================================
# 3. B edits, commits, pushes to A via the wire.
#    This is the action that triggers `keeper receive-pack` on A.
# ====================================================================
sleep 0.02
( cd "$B"
  cp "$CASE/01.hello.c" hello.c
  sleep 0.02; "$BE" post 'B edits hello.c'      > "$SCRATCH/03b.post.out"  2> "$SCRATCH/03b.post.err"
  sleep 0.02; "$BE" post "be://localhost/$REL_A" > "$SCRATCH/04b.push.out" 2> "$SCRATCH/04b.push.err"
) || { echo "post/22: B push failed: $(cat $SCRATCH/04b.push.err)" >&2; exit 1; }

# ====================================================================
# 4. Sanity: A's REFS moved.  (Pre-fix already passes.)
# ====================================================================
A_tip_after=$( cd "$A" && "$BE" sha1:? )
[ -n "$A_tip_after" ] || { echo "post/22: A has no tip after push" >&2; exit 1; }
[ "$A_tip_after" != "$A_tip_before" ] || {
    echo "post/22: A's REFS did not advance after push" >&2
    echo "  before=$A_tip_before  after=$A_tip_after" >&2
    exit 1
}

# ====================================================================
# 5. THE BUG: A's hello.c on disk must reflect B's bytes.  Pre-fix this
#    fails — files on A still hold 00.hello.c's bytes even though REFS
#    advanced past them.
# ====================================================================
match "$CASE/01.hello.c" "$A/hello.c"

# ====================================================================
# 6. A's wtlog must have grown to record the new tip.
# ====================================================================
A_wtlog_lines_after=$(wc -l < "$A/.be/wtlog")
[ "$A_wtlog_lines_after" -gt "$A_wtlog_lines_before" ] || {
    echo "post/22: A's wtlog did not grow (before=$A_wtlog_lines_before after=$A_wtlog_lines_after)" >&2
    cat "$A/.be/wtlog" >&2
    exit 1
}

#  The most recent A wtlog row must mention the new tip.
grep -q "$A_tip_after" "$A/.be/wtlog" || {
    echo "post/22: A's wtlog has no row referencing new tip $A_tip_after" >&2
    cat "$A/.be/wtlog" >&2
    exit 1
}

echo "post/22: A advanced from $A_tip_before to $A_tip_after; wt + wtlog followed"
