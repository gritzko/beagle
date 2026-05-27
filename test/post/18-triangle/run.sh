#!/bin/sh
#  post/18-triangle — triangular `be post` (FF-push) propagation
#  across a three-node ring with all three edge types
#  (be→be, be→git, git→be).  Companion to get/23-triangle.
#  See test/TRIANGLE.todo.md for spec gaps surfaced here.
#
#  Topology
#  --------
#      A (be wt+store)  ──►  B (be wt+store)  ──►  C (git bare)
#         ▲                                              │
#         └──────────────────────────────────────────────┘
#
#  Edge types and verbs:
#    A → B   be→be       A does `be post be://localhost/<B>` (FF push)
#    B → C   be→git      B does `be post ssh://localhost/<C.git>`
#    C → A   git→be      C does `git push --receive-pack="keeper
#                              receive-pack" ssh://localhost$ABS_A
#                              main:main`
#
#  Rounds: same edit choreography as get/23-triangle (modify / add /
#  delete) but propagation is push-driven.  POST is FF-only: divergent
#  pushes (POSTNOFF) are NOT exercised here — that's put/06-triangle's
#  job.
#
#  Setup (R0) uses `be get` / `git fetch` for the initial fan-out so
#  every node holds the same baseline before push-driven rounds begin.
#  This isolates POST as the exclusive propagation mechanism in the
#  body of the test.
#
#  Requires passwordless ssh to localhost + `keeper` on PATH.

. "$(dirname "$0")/../../lib/case.sh"

export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || { echo "post/18: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "post/18: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
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
[ -x "$KEEPER" ] || { echo "post/18: keeper binary not found at $KEEPER" >&2; exit 1; }
UP="$KEEPER upload-pack"
RP="$KEEPER receive-pack"

mkdir -p "$A/.be" "$B/.be" "$C_SEED"
git init --bare -b main "$C_BARE" >/dev/null

# ====================================================================
# R0 — seed at A, fan out via the same wire forms get/23 uses so all
# three nodes start at one tip.  This is bootstrap, not the test
# proper: post-driven propagation begins in R1.
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

# C-seed mirrors C-bare for the C-side git commits.
git -C "$C_SEED" init -q -b main >/dev/null 2>&1 || true
git -C "$C_SEED" config user.email t@t
git -C "$C_SEED" config user.name  T
sleep 0.02
git -C "$C_SEED" fetch -q "$C_BARE" main
git -C "$C_SEED" checkout -q -B main FETCH_HEAD

# ====================================================================
# R1 — commit at A (modify hello.c); push A→B→C→A.
# ====================================================================
sleep 0.02
( cd "$A"
  cp "$CASE/01.A.hello.c" hello.c
  sleep 0.02; "$BE" post 'R1: A modifies hello.c' >r1a.post.out 2>r1a.post.err )

# A → B (be→be, FF push)
sleep 0.02
( cd "$A"
  "$BE" post "be://localhost/$REL_B" >r1ab.post.out 2>r1ab.post.err ) || {
    echo "FAIL R1 A→B: be post be://<B> failed" >&2
    cat "$A/r1ab.post.err" >&2; exit 1; }

# After receive-pack, B's keeper holds the new tip but B's wt is
# untouched (POST is keeper-side push; wt sync needs a separate GET
# step).  Bring B's wt up to date so subsequent assertions can read
# files off disk.
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r1b.sync.out 2>r1b.sync.err )
match "$CASE/01.A.hello.c" "$B/hello.c"

# B → C (be→git, FF push)
sleep 0.02
( cd "$B"
  "$BE" post "ssh://localhost/$REL_C" >r1bc.post.out 2>r1bc.post.err ) || {
    echo "FAIL R1 B→C: be post ssh://<C.git> failed" >&2
    cat "$B/r1bc.post.err" >&2; exit 1; }

# C → A (git→be, FF push from git client to be store)
sleep 0.02
git -C "$C_BARE" fetch "$C_BARE" main >/dev/null 2>&1 || true
sleep 0.02
git -C "$C_BARE" push --receive-pack="$RP" \
    "ssh://localhost$A" main:main \
    > "$SCRATCH/r1ca.push.out" 2> "$SCRATCH/r1ca.push.err" || {
    echo "FAIL R1 C→A: git push to be store failed" >&2
    cat "$SCRATCH/r1ca.push.err" >&2; exit 1; }
# A's wt already has R1 bytes (A committed it locally).  The push
# from C is a loop-back FF; assert content hasn't regressed.
match "$CASE/01.A.hello.c" "$A/hello.c"

# ====================================================================
# R2 — commit at B (add extra.c); push B→C→A→B.
# ====================================================================
sleep 0.02
( cd "$B"
  cp "$CASE/02.B.extra.c" extra.c
  sleep 0.02; "$BE" put extra.c >r2b.put.out 2>r2b.put.err
  sleep 0.02; "$BE" post 'R2: B adds extra.c' >r2b.post.out 2>r2b.post.err )

# B → C
sleep 0.02
( cd "$B"
  "$BE" post "ssh://localhost/$REL_C" >r2bc.post.out 2>r2bc.post.err ) || {
    echo "FAIL R2 B→C: post failed" >&2
    cat "$B/r2bc.post.err" >&2; exit 1; }

# C → A
sleep 0.02
git -C "$C_BARE" push --receive-pack="$RP" \
    "ssh://localhost$A" main:main \
    > "$SCRATCH/r2ca.push.out" 2> "$SCRATCH/r2ca.push.err" || {
    echo "FAIL R2 C→A: git push failed" >&2
    cat "$SCRATCH/r2ca.push.err" >&2; exit 1; }
# Sync A's wt to its new tip.
sleep 0.02
( cd "$A"
  "$BE" get "be://localhost/$REL_A?" >r2a.sync.out 2>r2a.sync.err )
[ -f "$A/extra.c" ] || {
    echo "FAIL R2 C→A: A's wt did not receive extra.c after push" >&2
    ls -la "$A" >&2; exit 1; }
match "$CASE/02.B.extra.c" "$A/extra.c"

# A → B  (loop-back; FF no-op)
sleep 0.02
( cd "$A"
  "$BE" post "be://localhost/$REL_B" >r2ab.post.out 2>r2ab.post.err ) || {
    echo "FAIL R2 A→B (loop-back): post failed" >&2
    cat "$A/r2ab.post.err" >&2; exit 1; }

# ====================================================================
# R3 — commit at C (delete hello.c via git); push C→A→B→C.
# C-seed first re-syncs with C-bare (which got B's extra.c in R2).
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

# C → A
sleep 0.02
git -C "$C_BARE" push --receive-pack="$RP" \
    "ssh://localhost$A" main:main \
    > "$SCRATCH/r3ca.push.out" 2> "$SCRATCH/r3ca.push.err" || {
    echo "FAIL R3 C→A: git push failed" >&2
    cat "$SCRATCH/r3ca.push.err" >&2; exit 1; }
# Sync A's wt to new tip (post is push; doesn't move wt).
sleep 0.02
( cd "$A"
  "$BE" get "be://localhost/$REL_A?" >r3a.sync.out 2>r3a.sync.err )
[ ! -f "$A/hello.c" ] || {
    echo "FAIL R3 C→A: A still has hello.c after pulling deletion" >&2
    exit 1; }
[ -f "$A/extra.c" ] || {
    echo "FAIL R3 C→A: A lost extra.c" >&2
    exit 1; }

# A → B
sleep 0.02
( cd "$A"
  "$BE" post "be://localhost/$REL_B" >r3ab.post.out 2>r3ab.post.err ) || {
    echo "FAIL R3 A→B: post failed" >&2
    cat "$A/r3ab.post.err" >&2; exit 1; }
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r3b.sync.out 2>r3b.sync.err )
[ ! -f "$B/hello.c" ] || {
    echo "FAIL R3 A→B: B still has hello.c" >&2
    exit 1; }

# B → C  (loop-back; FF no-op)
sleep 0.02
( cd "$B"
  "$BE" post "ssh://localhost/$REL_C" >r3bc.post.out 2>r3bc.post.err ) || {
    echo "FAIL R3 B→C (loop-back): post failed" >&2
    cat "$B/r3bc.post.err" >&2; exit 1; }

# ====================================================================
# R4 — empty rotation.  No commits.  Re-running each push must be a
# no-op FF; wt state at A and B must match end-of-R3.
# ====================================================================
sleep 0.02
( cd "$A"
  "$BE" post "be://localhost/$REL_B" >r4ab.post.out 2>r4ab.post.err ) || {
    echo "FAIL R4 A→B: empty-rotation post failed" >&2; exit 1; }
sleep 0.02
( cd "$B"
  "$BE" post "ssh://localhost/$REL_C" >r4bc.post.out 2>r4bc.post.err ) || {
    echo "FAIL R4 B→C: empty-rotation post failed" >&2; exit 1; }
sleep 0.02
git -C "$C_BARE" push --receive-pack="$RP" \
    "ssh://localhost$A" main:main \
    > "$SCRATCH/r4ca.push.out" 2> "$SCRATCH/r4ca.push.err" || {
    echo "FAIL R4 C→A: empty-rotation push failed" >&2; exit 1; }

for node in "$A" "$B"; do
    [ ! -f "$node/hello.c" ] || {
        echo "FAIL R4: empty rotation re-introduced hello.c at $node" >&2
        exit 1; }
    [ -f "$node/extra.c" ] || {
        echo "FAIL R4: empty rotation dropped extra.c at $node" >&2
        exit 1; }
    match "$CASE/02.B.extra.c" "$node/extra.c"
done
