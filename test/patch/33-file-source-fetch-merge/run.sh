#!/bin/sh
#  patch/33-file-source-fetch-merge — DIS-035: `be patch
#  <scheme>://<host>?<proj>!` (the remote-source fetch-then-merge form
#  [PATCH] §"CLI use" documents) must FETCH the source then weave/squash
#  it.  A LOCAL `file://` source isolates the bug from the separately
#  flaky `be://` wire (GET-005/GET-007).
#
#  Bug (regression after the DIS-030+031 bang model, `bbefb693`): the
#  whole-branch scope `!` rides on the QUERY (`?/<proj>!`).  The keeper
#  fetch path debanged the want-REF but NOT the `?/<project>` selector,
#  so the project title derived as `<proj>!` — a shard that does not
#  exist — and the fetch died `HOMENOPROJ` → `WIRECLFL` → `BEDOGEXIT`,
#  with the source never fetched.  Fix: shed the trailing query-bang at
#  KEEPGetRemote entry so the project selector AND the want-ref are both
#  scope-free; the bang's whole-branch meaning is carried separately to
#  sniff PATCH by `be`'s BEActResolveRemote.
#
#  Unlike patch/21 (`ssh://localhost`, gated under WITH_SSH), this case
#  drives the LOCAL-EXEC keeper edge (`file://` → `keeper upload-pack`
#  spawned directly), so it needs NO ssh and runs in the default suite.
#
#  Setup (all under one $SCRATCH, two beagle worktrees):
#    A — the source/peer store.  Two trunk commits (v0, then v1).
#    B — cloned from A via `file://` at v0.  Then `be patch file://A?/A!`
#        from B must fetch A's v1 and weave it into B's wt.
#
#  Asserts:
#    1. `be patch file://A?/A!` exits 0 (pre-fix: WIRECLFL/BEDOGEXIT).
#    2. The fetched bytes (v1) actually landed in B's wt file.
#    3. The stale-shard refusal (`HOMENOPROJ`) did NOT surface.

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh seeds $SCRATCH/.be to shield walk-up; per-node anchors below.
rm -rf "$SCRATCH/.be"

A="$SCRATCH/A"
B="$SCRATCH/B"
mkdir -p "$A/.be" "$B/.be"

# ====================================================================
# 1. seed A with one trunk commit (v0).
# ====================================================================
( cd "$A"
  cp "$CASE/00.f.v0.c" f.c
  sleep 0.02; "$BE" put f.c          > "$SCRATCH/01a.put.out"  2> "$SCRATCH/01a.put.err"
  sleep 0.02; "$BE" post 'A v0'      > "$SCRATCH/01a.post.out" 2> "$SCRATCH/01a.post.err" )
A_tip_v0=$( cd "$A" && "$BE" sha1:? )
[ -n "$A_tip_v0" ] || { echo "patch/33: A has no tip after v0 seed" >&2; exit 1; }

# ====================================================================
# 2. clone A into B via file:// (host-less local store), at v0.
# ====================================================================
sleep 0.02
( cd "$B"
  "$BE" get "file://$A?/A" > "$SCRATCH/02b.get.out" 2> "$SCRATCH/02b.get.err"
) || { echo "patch/33: B failed to clone A: $(cat "$SCRATCH/02b.get.err")" >&2; exit 1; }
match "$CASE/00.f.v0.c" "$B/f.c"

# ====================================================================
# 3. A advances to v1 (a second trunk commit with distinctive bytes).
# ====================================================================
sleep 0.02
( cd "$A"
  cp "$CASE/01.f.v1.c" f.c
  sleep 0.02; "$BE" put f.c          > "$SCRATCH/03a.put.out"  2> "$SCRATCH/03a.put.err"
  sleep 0.02; "$BE" post 'A v1'      > "$SCRATCH/03a.post.out" 2> "$SCRATCH/03a.post.err" )
A_tip_v1=$( cd "$A" && "$BE" sha1:? )
[ "$A_tip_v1" != "$A_tip_v0" ] || { echo "patch/33: A did not advance to v1" >&2; exit 1; }

# ====================================================================
# 4. THE BUG SITE: from B, fetch-then-merge A's whole branch.
#    Pre-fix this dies HOMENOPROJ/WIRECLFL/BEDOGEXIT — A's v1 never
#    fetched, B's f.c stays at v0.
# ====================================================================
sleep 0.02
( cd "$B"
  "$BE" patch "file://$A?/A!" > "$SCRATCH/04b.patch.out" 2> "$SCRATCH/04b.patch.err"
) || {
    echo "patch/33: be patch file://A?/A! failed (DIS-035 regression)" >&2
    echo "  stdout:" >&2; cat "$SCRATCH/04b.patch.out" >&2
    echo "  stderr:" >&2; cat "$SCRATCH/04b.patch.err" >&2
    exit 1
}

#  The stale-shard refusal must NOT have surfaced.
grep -q "HOMENOPROJ" "$SCRATCH/04b.patch.err" && {
    echo "patch/33: patch hit HOMENOPROJ — query-bang leaked into the project selector" >&2
    cat "$SCRATCH/04b.patch.err" >&2
    exit 1
}
grep -q "WIRECLFL" "$SCRATCH/04b.patch.err" && {
    echo "patch/33: patch hit WIRECLFL — source never fetched" >&2
    cat "$SCRATCH/04b.patch.err" >&2
    exit 1
}

# ====================================================================
# 5. The fetched bytes (A's v1) actually landed in B's wt.
# ====================================================================
match "$CASE/01.f.v1.c" "$B/f.c"

echo "patch/33: file:// fetch-then-merge absorbed A v0->v1 into B"
