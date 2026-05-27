#!/bin/sh
#  get/23-triangle — triangular `be get` propagation across a
#  three-node ring with all three edge types (be→be, be→git, git→be).
#  Per the user's brief: "Add 1 commit at each step, test three-step
#  propagation."  See test/TRIANGLE.todo.md for the running list of
#  spec features encountered here that are not yet implemented.
#
#  Topology
#  --------
#      A (be wt+store)  ──►  B (be wt+store)  ──►  C (git bare)
#         ▲                                              │
#         └──────────────────────────────────────────────┘
#
#  Edge types covered (per VERBS.md "Schemes — cached vs transport"
#  and keeper/WIRECLI.c "transport spawn"):
#    A → B   be→be       B does `be get be://localhost/<A>?master`
#                        → `ssh localhost keeper upload-pack <A>`
#    B → C   be→git      C does `git fetch --upload-pack="keeper upload-pack"
#                                ssh://localhost/<B> master:master`
#    C → A   git→be      A does `be get ssh://localhost/<C.git>?master`
#                        → `ssh localhost git-upload-pack <C.git>`
#
#  Rounds:
#    R0  seed: build "0" at A, propagate A→B→C around the ring so all
#        three hold the same baseline.
#    R1  A modifies hello.c (pure modify).  Propagate A→B→C→A.
#    R2  B adds extra.c (new file).         Propagate B→C→A→B.
#    R3  C deletes hello.c (drop file).     Propagate C→A→B→C.
#    R4  empty rotation (no commits): one more A→B→C; assert every
#        node's checked-out content is unchanged from R3.
#
#  Tip-equality checks via `be sha1:?` confirm every full rotation
#  ends with all three nodes at the same commit tip.  (Initially
#  `sha1:` was unimplemented; landed alongside this test — see
#  TRIANGLE.todo.md.)
#
#  Requires passwordless ssh to localhost + `keeper` on PATH on both
#  the local and remote (== same host) side of every wire call.  Gated
#  under WITH_SSH.

. "$(dirname "$0")/../../lib/case.sh"

export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || { echo "get/23: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "get/23: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

# Walk-up shielding: case.sh placed an empty `.be/` at $SCRATCH to
# stop be from walking up into the user's $HOME/.be (if any).  Each
# per-node subdir gets its OWN empty `.be/` placeholder so be
# operations inside that subdir scope to a separate wt — the
# $SCRATCH/.be is no longer needed once per-node shields exist.
rm -rf "$SCRATCH/.be"

A="$SCRATCH/A"
B="$SCRATCH/B"
C_BARE="$SCRATCH/C.git"
C_SEED="$SCRATCH/C-seed"
REL_A="$REL_SCRATCH/A"
REL_B="$REL_SCRATCH/B"
REL_C="$REL_SCRATCH/C.git"

KEEPER="$(dirname "$BE")/keeper"
[ -x "$KEEPER" ] || { echo "get/23: keeper binary not found at $KEEPER" >&2; exit 1; }
UP="$KEEPER upload-pack"

mkdir -p "$A/.be" "$B/.be" "$C_SEED"
# `-b main` matches be's trunk → refs/heads/main wire alias
# (keeper/WIRECLI.c §"be ↔ git wire ref translation").
git init --bare -b main "$C_BARE" >/dev/null

#  Per-step branch handling — all three nodes track ONE branch
#  (trunk on the be side, refs/heads/main on the wire).  Be URLs use
#  `?` (empty query = trunk).  Git URLs are master-less, so the
#  refspec is `main:main`.  Mixing `?master` triggered a be-side
#  branch-switch bug — see TRIANGLE.todo.md.

# ====================================================================
# R0 — seed at A, propagate A→B→C.
# ====================================================================
( cd "$A"
  cp "$CASE/00.hello.c" hello.c
  sleep 0.02; "$BE" put hello.c  >r0a.put.out  2>r0a.put.err
  sleep 0.02; "$BE" post 'R0 seed at A' >r0a.post.out 2>r0a.post.err )

# A → B (be→be)
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r0ab.get.out 2>r0ab.get.err )
[ -f "$B/hello.c" ] || {
    echo "FAIL R0 A→B: B has no hello.c" >&2
    cat "$B/r0ab.get.err" >&2; exit 1; }
match "$CASE/00.hello.c" "$B/hello.c"

# B → C (be→git)
sleep 0.02
git -C "$C_BARE" fetch --upload-pack="$UP" \
    "ssh://localhost$B" main:main \
    > "$SCRATCH/r0bc.fetch.out" 2> "$SCRATCH/r0bc.fetch.err" || {
    echo "FAIL R0 B→C: git fetch from be (keeper upload-pack) failed" >&2
    cat "$SCRATCH/r0bc.fetch.err" >&2; exit 1; }

# C → A (git→be, loop-back; expect no-op FF)
sleep 0.02
( cd "$A"
  "$BE" get "ssh://localhost/$REL_C?" >r0ca.get.out 2>r0ca.get.err ) || {
    echo "FAIL R0 C→A: be get from git bare failed" >&2
    cat "$A/r0ca.get.err" >&2; exit 1; }
match "$CASE/00.hello.c" "$A/hello.c"

# ====================================================================
# R1 — commit at A (modify hello.c).
# ====================================================================
sleep 0.02
( cd "$A"
  cp "$CASE/01.A.hello.c" hello.c
  sleep 0.02; "$BE" post 'R1: A modifies hello.c' >r1a.post.out 2>r1a.post.err )

# A → B
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r1ab.get.out 2>r1ab.get.err )
match "$CASE/01.A.hello.c" "$B/hello.c"

# B → C
sleep 0.02
git -C "$C_BARE" fetch --upload-pack="$UP" \
    "ssh://localhost$B" main:main \
    > "$SCRATCH/r1bc.fetch.out" 2> "$SCRATCH/r1bc.fetch.err"

# C → A  (loop-back; A already at this tip)
sleep 0.02
( cd "$A"
  "$BE" get "ssh://localhost/$REL_C?" >r1ca.get.out 2>r1ca.get.err )
match "$CASE/01.A.hello.c" "$A/hello.c"

# ====================================================================
# R2 — commit at B (add extra.c).
# ====================================================================
sleep 0.02
( cd "$B"
  cp "$CASE/02.B.extra.c" extra.c
  sleep 0.02; "$BE" put extra.c >r2b.put.out 2>r2b.put.err
  sleep 0.02; "$BE" post 'R2: B adds extra.c' >r2b.post.out 2>r2b.post.err )

# B → C
sleep 0.02
git -C "$C_BARE" fetch --upload-pack="$UP" \
    "ssh://localhost$B" main:main \
    > "$SCRATCH/r2bc.fetch.out" 2> "$SCRATCH/r2bc.fetch.err"

# C → A
sleep 0.02
( cd "$A"
  "$BE" get "ssh://localhost/$REL_C?" >r2ca.get.out 2>r2ca.get.err )
[ -f "$A/extra.c" ] || {
    echo "FAIL R2 C→A: A did not receive extra.c from C (git→be)" >&2
    ls -la "$A" >&2; exit 1; }
match "$CASE/02.B.extra.c" "$A/extra.c"

# A → B  (loop-back)
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r2ab.get.out 2>r2ab.get.err )
match "$CASE/02.B.extra.c" "$B/extra.c"

# ====================================================================
# R3 — commit at C (delete hello.c).
# ====================================================================
git -C "$C_SEED" init -q -b main >/dev/null 2>&1 || true
git -C "$C_SEED" config user.email t@t
git -C "$C_SEED" config user.name  T
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
( cd "$A"
  "$BE" get "ssh://localhost/$REL_C?" >r3ca.get.out 2>r3ca.get.err )
[ ! -f "$A/hello.c" ] || {
    echo "FAIL R3 C→A: A still has hello.c after pulling deletion" >&2
    exit 1; }
[ -f "$A/extra.c" ] || {
    echo "FAIL R3 C→A: A lost extra.c (regression from R2)" >&2
    exit 1; }

# A → B
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r3ab.get.out 2>r3ab.get.err )
[ ! -f "$B/hello.c" ] || {
    echo "FAIL R3 A→B: B still has hello.c after A→B pull" >&2
    exit 1; }
[ -f "$B/extra.c" ] || {
    echo "FAIL R3 A→B: B lost extra.c" >&2
    exit 1; }

# B → C  (loop-back)
sleep 0.02
git -C "$C_BARE" fetch --upload-pack="$UP" \
    "ssh://localhost$B" main:main \
    > "$SCRATCH/r3bc.fetch.out" 2> "$SCRATCH/r3bc.fetch.err"

# ====================================================================
# R4 — empty rotation.  No commits anywhere.  Re-running each edge
# must leave on-disk file state at each node unchanged from R3.
# (Per VERBS.md §GET: "GET is also fast-forward-only ... [refuses if
# the local tip is not an ancestor of the incoming remote tip]" —
# equal tips are an ancestor → no-op accepted.)
# ====================================================================
sleep 0.02
( cd "$B"
  "$BE" get "be://localhost/$REL_A?" >r4ab.get.out 2>r4ab.get.err )
sleep 0.02
git -C "$C_BARE" fetch --upload-pack="$UP" \
    "ssh://localhost$B" main:main \
    > "$SCRATCH/r4bc.fetch.out" 2> "$SCRATCH/r4bc.fetch.err"
sleep 0.02
( cd "$A"
  "$BE" get "ssh://localhost/$REL_C?" >r4ca.get.out 2>r4ca.get.err )

# State after R4 must match state at end of R3: hello.c gone
# everywhere, extra.c present everywhere with R2's bytes.
for node in "$A" "$B"; do
    [ ! -f "$node/hello.c" ] || {
        echo "FAIL R4: empty rotation re-introduced hello.c at $node" >&2
        exit 1; }
    [ -f "$node/extra.c" ] || {
        echo "FAIL R4: empty rotation dropped extra.c at $node" >&2
        exit 1; }
    match "$CASE/02.B.extra.c" "$node/extra.c"
done

# Final tip-equality across nodes via the `sha1:?` projector.  Be
# nodes resolve trunk via keeper REFS; the git bare via `git
# rev-parse main`.  All three must agree.
TIP_A=$( cd "$A" && "$BE" sha1:? )
TIP_B=$( cd "$B" && "$BE" sha1:? )
TIP_C=$( git -C "$C_BARE" rev-parse main )
[ -n "$TIP_A" ] && [ "$TIP_A" = "$TIP_B" ] && [ "$TIP_A" = "$TIP_C" ] || {
    echo "FAIL: post-R4 tips disagree:" >&2
    echo "  A=$TIP_A  B=$TIP_B  C=$TIP_C" >&2
    exit 1; }
