#!/bin/sh
#  post/35-noop-file-push-no-fail — POST-010: a no-op re-push over the
#  local-exec `file://` edge (nothing new to send, peer already at our
#  tip) must NOT print a stray `FILEFAIL`.
#
#  Root cause: on a no-op push the client short-circuits (peer at tip),
#  sends only a flush, and closes its read end early; the spawned
#  `keeper receive-pack` peer then writes its `unpack ok` report into a
#  pipe whose read end is already gone → EPIPE → FILEFAIL.  The peer
#  process exits non-OK and PRO.h's MAIN wrapper prints `Error:
#  FILEFAIL`, even though the overall push still reports success.
#  RECVEmitResponse now treats EPIPE on a zero-update (`n == 0`) report
#  as a benign no-op — a real ref-update report failing still surfaces
#  FILEFAIL.
#
#  Like post/32 this drives the LOCAL-EXEC keeper edge (`file://` →
#  `keeper receive-pack` spawned directly), so it needs NO ssh and runs
#  in the default suite.
#
#  Setup (one $SCRATCH, two worktrees):
#    A — the store/peer.  One commit on trunk (00.hello.c).
#    B — cloned from A via file://.  Edits hello.c → 01.hello.c, commits,
#        FF-pushes to A once (advances the peer), then pushes a SECOND
#        time with nothing new.
#
#  Asserts on the SECOND (no-op) push:
#    1. exit 0.
#    2. `keeper: pushed` still printed (success reported).
#    3. NO `FILEFAIL` anywhere in its stdout or stderr (the bug).

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
[ -n "$A_tip_before" ] || { echo "post/35: A has no tip after seed" >&2; exit 1; }

# ====================================================================
# 2. clone A into B via file:// (host-less local store).
# ====================================================================
sleep 0.02
( cd "$B"
  "$BE" get "file://$A?" > "$SCRATCH/02b.get.out" 2> "$SCRATCH/02b.get.err"
) || { echo "post/35: B failed to clone A: $(cat "$SCRATCH/02b.get.err")" >&2; exit 1; }
match "$CASE/00.hello.c" "$B/hello.c"

# ====================================================================
# 3. B edits, commits, then FF-pushes to A via file:// (advances peer).
# ====================================================================
sleep 0.02
( cd "$B"
  cp "$CASE/01.hello.c" hello.c
  sleep 0.02; "$BE" post 'B edits hello.c' > "$SCRATCH/03b.post.out" 2> "$SCRATCH/03b.post.err"
  sleep 0.02; "$BE" post "file://$A"       > "$SCRATCH/04b.push.out" 2> "$SCRATCH/04b.push.err"
) || { echo "post/35: B first push failed: $(cat "$SCRATCH/04b.push.err")" >&2; exit 1; }

grep -q "keeper: pushed" "$SCRATCH/04b.push.out" || {
    echo "post/35: first push did not report success" >&2
    cat "$SCRATCH/04b.push.out" "$SCRATCH/04b.push.err" >&2
    exit 1
}

# A must now be at B's tip (so the second push is a genuine no-op).
A_tip_mid=$( cd "$A" && "$BE" sha1:? )
B_tip=$( cd "$B" && "$BE" sha1:? )
[ "$A_tip_mid" = "$B_tip" ] || {
    echo "post/35: A ($A_tip_mid) != B ($B_tip) after first push — not a no-op setup" >&2
    exit 1
}

# ====================================================================
# 4. SECOND push — nothing new.  This is the bug site (POST-010):
#    pre-fix the no-op re-push prints a stray `Error: FILEFAIL` even
#    though it exits 0 and reports `keeper: pushed`.
# ====================================================================
sleep 0.02
nooprc=0
( cd "$B"
  "$BE" post "file://$A" > "$SCRATCH/05b.noop.out" 2> "$SCRATCH/05b.noop.err"
) || nooprc=$?

# 4a. exit 0.
[ "$nooprc" -eq 0 ] || {
    echo "post/35: no-op re-push exited $nooprc (want 0)" >&2
    echo "  stdout:" >&2; cat "$SCRATCH/05b.noop.out" >&2
    echo "  stderr:" >&2; cat "$SCRATCH/05b.noop.err" >&2
    exit 1
}

# 4b. success still reported.
grep -q "keeper: pushed" "$SCRATCH/05b.noop.out" || {
    echo "post/35: no-op re-push did not report success" >&2
    echo "  stdout:" >&2; cat "$SCRATCH/05b.noop.out" >&2
    echo "  stderr:" >&2; cat "$SCRATCH/05b.noop.err" >&2
    exit 1
}

# 4c. NO stray FILEFAIL anywhere (the POST-010 bug).
if grep -q "FILEFAIL" "$SCRATCH/05b.noop.out" "$SCRATCH/05b.noop.err"; then
    echo "post/35: no-op re-push printed a stray FILEFAIL (POST-010 regression)" >&2
    echo "  stdout:" >&2; cat "$SCRATCH/05b.noop.out" >&2
    echo "  stderr:" >&2; cat "$SCRATCH/05b.noop.err" >&2
    exit 1
fi

# ====================================================================
# 5. A's tip is unchanged by the no-op (sanity).
# ====================================================================
A_tip_after=$( cd "$A" && "$BE" sha1:? )
[ "$A_tip_after" = "$A_tip_mid" ] || {
    echo "post/35: A's tip moved on a no-op push ($A_tip_mid -> $A_tip_after)" >&2
    exit 1
}

echo "post/35: no-op file:// re-push exited 0, reported success, printed no FILEFAIL"
