#!/bin/sh
#  post/06-fetch-incremental — `be post ssh://origin` after a clone
#  must be incremental: the second upload-pack should send only the
#  one new commit advanced on origin, NOT re-pack the whole history.
#
#  Repro:
#    1. Seed origin with version A on master.
#    2. Clone via ssh into wt (fetches A's reachable closure).
#    3. Advance origin to B (one new commit).
#    4. `be post ssh://origin` — should rebase cur onto B, fetching
#        only B's commit + tree + new blob (3 objects).
#
#  With the haves-from-REFADV bug, REFADV deduped peer-observed rows
#  against the local cur row, so the cached A-tip never made it into
#  the upload-pack `have` list.  Server treated the request as a
#  fresh clone and resent everything (Total ≥ A's full closure).
#
#  Asserts: the second upload-pack's `Total N` <= small bound (4) so
#  a re-fetch of A's blob/tree would tip the test red.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "post/06: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "post/06: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"

cd "$SCRATCH"

# ====================================================================
# 1. seed origin with A on master (3 objects: commit, tree, blob)
# ====================================================================
git init --bare "$ORIGIN" >/dev/null
git init "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
git -C "$SEED" checkout -b master >/dev/null || true
printf 'A\n' > "$SEED/hello.txt"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# ====================================================================
# 2. clone via ssh
# ====================================================================
mkdir wt && cd wt
"$BE" get "ssh://localhost/$REL_ORIGIN?master" \
    >01.clone.got.out 2>01.clone.got.err
[ -f hello.txt ] || { echo "post/06: clone left no hello.txt" >&2; exit 1; }

# ====================================================================
# 3. advance origin to B (one new commit; one new blob, one new tree,
#    one new commit = 3 new objects on top of A's 3)
# ====================================================================
cd ..
sleep 0.02; printf 'A\nB\n' > "$SEED/hello.txt"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm B
git -C "$SEED" push -q "$ORIGIN" master:master

# ====================================================================
# 4. `be post ssh://origin` — fetch should be incremental
# ====================================================================
cd wt
"$BE" post "ssh://localhost/$REL_ORIGIN" \
    >02.post.got.out 2>02.post.got.err || true

# Server's progress prints `Total N (delta D), reused …`.  Incremental
# fetch should send 3 objects (B's commit + tree + new blob); a
# regression that resends A's closure too prints Total >= 4.
total=$(grep -oE 'Total [0-9]+' 02.post.got.err | tail -1 | awk '{print $2}')
[ -n "$total" ] || {
    echo "post/06: no 'Total N' line in upload-pack stderr" >&2
    sed -n 1,40p 02.post.got.err >&2
    exit 1
}
[ "$total" -le 3 ] || {
    echo "post/06: upload-pack sent $total objects; expected <=3" >&2
    echo "post/06: indicates haves negotiation failed (full re-clone)" >&2
    sed -n 1,40p 02.post.got.err >&2
    exit 1
}
