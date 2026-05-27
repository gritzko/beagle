#!/bin/sh
#  put/06-triangle — triangular `be put` (unconstrained push)
#  propagation across a three-node ring with all three edge types
#  (be→be, be→git, git→be).  Companion to get/23 and post/18.
#  See test/TRIANGLE.todo.md for spec gaps surfaced here.
#
#  Topology
#  --------
#      A (be wt+store)  ──►  B (be wt+store)  ──►  C (git bare)
#         ▲                                              │
#         └──────────────────────────────────────────────┘
#
#  Edge types and verbs:
#    A → B   be→be       A does `be put be://localhost/<B>`
#    B → C   be→git      B does `be put ssh://localhost/<C.git>`
#    C → A   git→be      C does `git push --receive-pack="keeper
#                              receive-pack" --force ssh://localhost$ABS_A
#                              main:main`
#
#  Per VERBS.md §PUT: PUT is *unconstrained* — non-FF (force) pushes
#  are allowed; PUT is the force-push verb (POST is FF-only).  The
#  three R1/R2/R3 rounds are still FF-shaped (each is one commit
#  forward); R4 (non-FF tail) is the distinguishing case: B rewinds
#  cur's tip to an earlier commit by re-`be put`-ing it locally (a
#  reflog-only label move), then propagates the rewind around the
#  triangle.  POST would have refused this with POSTNOFF — PUT must
#  let it through.
#
#  Setup (R0) and per-edge wt sync use `be get` so PUT remains the
#  exclusive propagation mechanism in the body of the test.
#
#  Requires passwordless ssh to localhost + `keeper` on PATH.

. "$(dirname "$0")/../../lib/case.sh"

export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || { echo "put/06: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "put/06: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

rm -rf "$SCRATCH/.be"

A="$SCRATCH/A"
B="$SCRATCH/B"
C_BARE="$SCRATCH/C.git"
C_SEED="$SCRATCH/C-seed"
REL_A="$REL_SCRATCH/A"
REL_B="$REL_SCRATCH/B"
REL_C="$REL_SCRATCH/C.git"

KEEPER="$(dirname "$BE")/keeper"
[ -x "$KEEPER" ] || { echo "put/06: keeper binary not found at $KEEPER" >&2; exit 1; }
UP="$KEEPER upload-pack"
RP="$KEEPER receive-pack"

mkdir -p "$A/.be" "$B/.be" "$C_SEED"
git init --bare -b main "$C_BARE" >/dev/null

# ====================================================================
# R0 — seed at A, fan out (same shape as get/23 and post/18 setup).
# ====================================================================
( cd "$A"
  cp "$CASE/00.hello.c" hello.c
  sleep 0.02; "$BE" put hello.c  >r0a.put.out  2>r0a.put.err
  sleep 0.02; "$BE" post 'R0 seed at A' >r0a.post.out 2>r0a.post.err )

sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r0ab.get.out 2>r0ab.get.err )
match "$CASE/00.hello.c" "$B/hello.c"

sleep 0.02
git -C "$C_BARE" fetch --upload-pack="$UP" \
    "ssh://localhost$B" main:main \
    > "$SCRATCH/r0bc.fetch.out" 2> "$SCRATCH/r0bc.fetch.err"

git -C "$C_SEED" init -q -b main >/dev/null 2>&1 || true
git -C "$C_SEED" config user.email t@t
git -C "$C_SEED" config user.name  T
sleep 0.02
git -C "$C_SEED" fetch -q "$C_BARE" main
git -C "$C_SEED" checkout -q -B main FETCH_HEAD

# Snapshot R0's tip — R4's non-FF tail rewinds to it.
TIP_R0=$( git -C "$C_BARE" rev-parse main )

# ====================================================================
# R1 — commit at A (modify hello.c); put A→B→C→A.
# ====================================================================
sleep 0.02
( cd "$A"
  cp "$CASE/01.A.hello.c" hello.c
  sleep 0.02; "$BE" post 'R1: A modifies hello.c' >r1a.post.out 2>r1a.post.err )

# A → B (be→be, PUT)
sleep 0.02
( cd "$A"
  "$BE" put "be://localhost/$REL_B" >r1ab.put.out 2>r1ab.put.err ) || {
    echo "FAIL R1 A→B: be put be://<B> failed" >&2
    cat "$A/r1ab.put.err" >&2; exit 1; }
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r1b.sync.out 2>r1b.sync.err )
match "$CASE/01.A.hello.c" "$B/hello.c"

# B → C (be→git, PUT)
sleep 0.02
( cd "$B"
  "$BE" put "ssh://localhost/$REL_C" >r1bc.put.out 2>r1bc.put.err ) || {
    echo "FAIL R1 B→C: be put ssh://<C.git> failed" >&2
    cat "$B/r1bc.put.err" >&2; exit 1; }

# C → A (git→be force push)
sleep 0.02
git -C "$C_BARE" push --receive-pack="$RP" --force \
    "ssh://localhost$A" main:main \
    > "$SCRATCH/r1ca.push.out" 2> "$SCRATCH/r1ca.push.err" || {
    echo "FAIL R1 C→A: git force-push to be store failed" >&2
    cat "$SCRATCH/r1ca.push.err" >&2; exit 1; }
match "$CASE/01.A.hello.c" "$A/hello.c"

# ====================================================================
# R2 — commit at B (add extra.c); put B→C→A→B.
# ====================================================================
sleep 0.02
( cd "$B"
  cp "$CASE/02.B.extra.c" extra.c
  sleep 0.02; "$BE" put extra.c >r2b.put.out 2>r2b.put.err
  sleep 0.02; "$BE" post 'R2: B adds extra.c' >r2b.post.out 2>r2b.post.err )

sleep 0.02
( cd "$B"
  "$BE" put "ssh://localhost/$REL_C" >r2bc.put.out 2>r2bc.put.err ) || {
    echo "FAIL R2 B→C: put failed" >&2; exit 1; }

sleep 0.02
git -C "$C_BARE" push --receive-pack="$RP" --force \
    "ssh://localhost$A" main:main \
    > "$SCRATCH/r2ca.push.out" 2> "$SCRATCH/r2ca.push.err" || {
    echo "FAIL R2 C→A: git force-push failed" >&2; exit 1; }
sleep 0.02
( cd "$A"
  "$BE" get "be://localhost/$REL_A?" >r2a.sync.out 2>r2a.sync.err )
[ -f "$A/extra.c" ] || {
    echo "FAIL R2 C→A: A wt missing extra.c after git→be push" >&2
    ls -la "$A" >&2; exit 1; }
match "$CASE/02.B.extra.c" "$A/extra.c"

sleep 0.02
( cd "$A"
  "$BE" put "be://localhost/$REL_B" >r2ab.put.out 2>r2ab.put.err ) || {
    echo "FAIL R2 A→B (loop-back): put failed" >&2; exit 1; }

# ====================================================================
# R3 — commit at C (delete hello.c); put C→A→B→C.
# ====================================================================
sleep 0.02
git -C "$C_SEED" fetch -q "$C_BARE" main
git -C "$C_SEED" checkout -q -B main FETCH_HEAD
sleep 0.02
git -C "$C_SEED" rm -q hello.c
sleep 0.02
git -C "$C_SEED" commit -qm 'R3: C deletes hello.c'
sleep 0.02
git -C "$C_SEED" push -q "$C_BARE" main:main

sleep 0.02
git -C "$C_BARE" push --receive-pack="$RP" --force \
    "ssh://localhost$A" main:main \
    > "$SCRATCH/r3ca.push.out" 2> "$SCRATCH/r3ca.push.err" || {
    echo "FAIL R3 C→A: git force-push failed" >&2; exit 1; }
sleep 0.02
( cd "$A"
  "$BE" get "be://localhost/$REL_A?" >r3a.sync.out 2>r3a.sync.err )
[ ! -f "$A/hello.c" ] || {
    echo "FAIL R3 C→A: A still has hello.c" >&2; exit 1; }
[ -f "$A/extra.c" ] || {
    echo "FAIL R3 C→A: A lost extra.c" >&2; exit 1; }

sleep 0.02
( cd "$A"
  "$BE" put "be://localhost/$REL_B" >r3ab.put.out 2>r3ab.put.err ) || {
    echo "FAIL R3 A→B: put failed" >&2; exit 1; }
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r3b.sync.out 2>r3b.sync.err )
[ ! -f "$B/hello.c" ] || {
    echo "FAIL R3 A→B: B still has hello.c" >&2; exit 1; }

sleep 0.02
( cd "$B"
  "$BE" put "ssh://localhost/$REL_C" >r3bc.put.out 2>r3bc.put.err ) || {
    echo "FAIL R3 B→C (loop-back): put failed" >&2; exit 1; }

# ====================================================================
# R4 — non-FF tail.  B rewrites cur's tip on top of R0 (skipping
# R1/R2/R3), creating a tip that is NOT a descendant of the current
# refs.  PUT must let this propagate; POST would refuse.
#
# Mechanism: `be put ?#<sha>` resets cur to <sha>.  Per VERBS.md §PUT:
# "PUT writes one row to .be/REFS … (`be put ?br#sha`) Reset `?br`
# to sha `abc1234`.  Non-FF rewrite is allowed (PUT is unconstrained
# — local or remote)."
# ====================================================================
sleep 0.02
# Bring B's wt forward to R3 first (it's been a while since B synced).
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r4b.sync.out 2>r4b.sync.err )

# B locally rewinds cur to R0's tip, then commits R4 on top.  This is
# a non-FF rewrite (R4's tip's first-parent is R0, skipping R1/R2/R3).
sleep 0.02
( cd "$B"
  "$BE" put "?#$TIP_R0" >r4b.rewind.out 2>r4b.rewind.err ) || {
    echo "FAIL R4: be put ?#<sha> (local non-FF rewind) refused" >&2
    cat "$B/r4b.rewind.err" >&2; exit 1; }
# Materialise R0's state in B's wt to match the rewound ref.
sleep 0.02
( cd "$B"
  "$BE" get --force "?#$TIP_R0" >r4b.reset.out 2>r4b.reset.err )

# B replaces hello.c with the rewrite payload, commits R4 atop R0.
sleep 0.02
cp "$CASE/04.rewrite.hello.c" "$B/hello.c"
sleep 0.02
( cd "$B"
  "$BE" post 'R4: B rewrites tip atop R0 (non-FF)' \
      >r4b.post.out 2>r4b.post.err )

# B → C  (force push; non-FF must be accepted by PUT path).
sleep 0.02
( cd "$B"
  "$BE" put "ssh://localhost/$REL_C" >r4bc.put.out 2>r4bc.put.err ) || {
    echo "FAIL R4 B→C: be put (non-FF) refused" >&2
    cat "$B/r4bc.put.err" >&2; exit 1; }

# C → A  (git force-push)
sleep 0.02
git -C "$C_BARE" push --receive-pack="$RP" --force \
    "ssh://localhost$A" main:main \
    > "$SCRATCH/r4ca.push.out" 2> "$SCRATCH/r4ca.push.err" || {
    echo "FAIL R4 C→A: git --force push refused" >&2; exit 1; }
sleep 0.02
( cd "$A"
  "$BE" get --force "be://localhost/$REL_A?" >r4a.sync.out 2>r4a.sync.err )
match "$CASE/04.rewrite.hello.c" "$A/hello.c"
[ ! -f "$A/extra.c" ] || {
    echo "FAIL R4 C→A: A still has extra.c after non-FF rewind" >&2
    exit 1; }
