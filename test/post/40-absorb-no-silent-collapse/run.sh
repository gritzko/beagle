#!/bin/sh
#  post/39-absorb-no-silent-collapse — POST-015 (silent-drop half),
#  driven by DIS-038.  After `be patch <scheme>://<peer>?/<proj>!`
#  absorbs a peer's commit, `be post '#msg'` must record a REAL commit
#  that advances cur — never silently collapse to the existing tip and
#  never drop the absorbed change back out of the worktree.
#
#  Root cause (now fixed): the fetched peer tip's parent edges were not
#  DAG-indexed, so DAGAncestors bottomed out at the tip and the absorbed
#  commit looked already-reachable; the patch-id/reachability dedup then
#  skipped it, so `be post` emitted a commit whose hash was identical to
#  the prior trunk tip (8d313c50 live) and the absorbed edit vanished.
#  DIS-038's keeper-commit-body parent fallback in DAGAncestorsTunable
#  (graf/DAG.c) makes reachability self-sufficient on fetched-but-
#  unindexed history, so the absorb now lands as a real commit.  The
#  ahead/behind reachability half of DIS-038 is gated directly by
#  test/head/24-divergent-fetched-ahead-behind; THIS case is the
#  end-to-end POST-side lock: an absorb-then-post records a real
#  commit carrying the peer's bytes and never collapses to the prior
#  tip.  (NOTE: a SEPARATE, still-open defect — DIS-042 — is that when
#  cur has its OWN divergent commit, the absorb-then-post replays cur's
#  stack ONTO the peer and drops cur's own committed bytes from the
#  tree; this case keeps cur at the shared base to isolate the
#  silent-collapse symptom from that.)
#
#  Topology (two file:// stores, shared base v0):
#      A:  v0 ── T (peer, appends PEER-DIS035 at EOF)
#      B:  v0  (clone of A at v0; cur sits at v0)
#  From B: `be patch file://A?/A!` absorbs T into B's wt, then
#  `be post '#absorbed'` must:
#    1. exit 0 and advance B's tip OFF v0 (no collapse to the prior tip),
#    2. leave the absorbed PEER-DIS035 line in B's worktree file, and
#    3. land it in the COMMITTED tree (proved by a fresh re-checkout),
#    4. emit NO `walk: bad path '.be'` spam (POST-015 walk half).

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh seeds $SCRATCH/.be to shield walk-up; per-node anchors below.
rm -rf "$SCRATCH/.be"

A="$SCRATCH/A"
B="$SCRATCH/B"
C="$SCRATCH/C"
mkdir -p "$A/.be" "$B/.be" "$C/.be"

# --- 1. seed A with one trunk commit (v0). --------------------------
( cd "$A"
  printf 'line1\nline2\nline3\n' > f.c
  sleep 0.05; "$BE" put f.c     > "$SCRATCH/1a.put.out"  2> "$SCRATCH/1a.put.err"
  sleep 0.05; "$BE" post 'A v0' > "$SCRATCH/1a.post.out" 2> "$SCRATCH/1a.post.err" )
A_v0=$( cd "$A" && "$BE" sha1:? )
[ -n "$A_v0" ] || { echo "post/39: A has no tip after v0" >&2; exit 1; }

# --- 2. clone A into B via file:// at v0. ---------------------------
sleep 0.05
( cd "$B"
  "$BE" get "file://$A?/A" > "$SCRATCH/2b.get.out" 2> "$SCRATCH/2b.get.err"
) || { echo "post/39: B failed to clone A: $(cat "$SCRATCH/2b.get.err")" >&2; exit 1; }
B_before=$( cd "$B" && "$BE" sha1:? )
[ "$B_before" = "$A_v0" ] || { echo "post/39: B clone not at v0 ($B_before)" >&2; exit 1; }

# --- 3. A advances to T (peer commit, distinctive EOF line). --------
sleep 0.05
( cd "$A"
  printf 'line1\nline2\nline3\nPEER-DIS035\n' > f.c
  sleep 0.05; "$BE" put f.c       > "$SCRATCH/3a.put.out"  2> "$SCRATCH/3a.put.err"
  sleep 0.05; "$BE" post 'A v1 T' > "$SCRATCH/3a.post.out" 2> "$SCRATCH/3a.post.err" )
A_T=$( cd "$A" && "$BE" sha1:? )
[ "$A_T" != "$A_v0" ] || { echo "post/39: A did not advance to T" >&2; exit 1; }

# --- 4. from B, fetch-then-absorb A's peer commit T. ----------------
sleep 0.05
( cd "$B"
  "$BE" patch "file://$A?/A!" > "$SCRATCH/4b.patch.out" 2> "$SCRATCH/4b.patch.err"
) || {
    echo "post/39: be patch file://A?/A! failed" >&2
    cat "$SCRATCH/4b.patch.out" "$SCRATCH/4b.patch.err" >&2
    exit 1; }

#  The absorb must NOT no-op: pre-DIS-038 the fetched tip's ancestry
#  bottomed out, the merge saw nothing new, and the line never arrived.
grep -q 'PEER-DIS035' "$B/f.c" || {
    echo "post/39: absorbed PEER line absent from B's wt after patch" >&2
    cat "$SCRATCH/4b.patch.out" "$SCRATCH/4b.patch.err" >&2
    exit 1; }

# --- 5. commit the absorption.  Must NOT silently collapse. ---------
sleep 0.05
( cd "$B"
  "$BE" post '#absorbed' > "$SCRATCH/5b.post.out" 2> "$SCRATCH/5b.post.err"
) || {
    echo "post/39: be post '#absorbed' failed" >&2
    cat "$SCRATCH/5b.post.out" "$SCRATCH/5b.post.err" >&2
    exit 1; }

#  POST-015 walk half: no per-entry `bad path '.be'` spam.
if grep -q "walk: bad path '.be'" "$SCRATCH/5b.post.err"; then
    echo "post/39: post emitted 'walk: bad path .be' spam" >&2
    grep -c "walk: bad path '.be'" "$SCRATCH/5b.post.err" >&2
    exit 1
fi

B_after=$( cd "$B" && "$BE" sha1:? )
#  The silent-drop symptom: tip collapses back to the pre-absorb tip
#  (== v0 here) instead of advancing to a real commit carrying T.
[ "$B_after" != "$B_before" ] || {
    echo "post/39: SILENT COLLAPSE — tip did not advance off v0 ($B_after)" >&2
    cat "$SCRATCH/5b.post.out" >&2
    exit 1; }

#  The absorbed line must survive in the worktree after the commit.
grep -q 'PEER-DIS035' "$B/f.c" || {
    echo "post/39: PEER line dropped from B's wt after post" >&2
    exit 1; }

# --- 6. prove it landed in the COMMITTED tree (fresh re-checkout). --
sleep 0.05
( cd "$C"
  "$BE" get "file://$B/.be?/A/" > "$SCRATCH/6c.get.out" 2> "$SCRATCH/6c.get.err"
) || { echo "post/39: fresh checkout of B's new tip failed" >&2; exit 1; }
grep -q 'PEER-DIS035' "$C/f.c" || {
    echo "post/39: PEER line absent from the COMMITTED tree (silent drop)" >&2
    cat "$C/f.c" >&2
    exit 1; }

echo "post/39-absorb-no-silent-collapse: OK"
