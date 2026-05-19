#!/bin/sh
#  post/06-fetch-incremental — incremental fetch regression.
#
#  Per VERBS.md §POST, `be post ssh://origin` is FF-push-only — it
#  does not fetch or rebase.  The original incremental-fetch
#  regression (haves-from-REFADV bug) lives under HEAD now, since
#  fetching from a remote into the local cache is `be head
#  ssh://origin`'s job (VERBS.md §HEAD: "Fetch refs + minimal pack
#  from origin, update `.be/refs`").  This case retains the same
#  shape of assertion — second-pass `Total N` from upload-pack must
#  be small (incremental haves negotiated) — but drives it through
#  the spec-correct verb.
#
#  Repro:
#    1. Seed origin with version A on master.
#    2. Clone via ssh into wt (fetches A's reachable closure).
#    3. Advance origin to B (one new commit).
#    4. `be head ssh://origin` — incremental fetch: upload-pack
#        should send only B's 3 new objects (commit + tree + blob).
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
# 3. advance origin to B (one new commit; 3 new objects atop A's 3)
# ====================================================================
cd ..
sleep 0.02; printf 'A\nB\n' > "$SEED/hello.txt"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm B
git -C "$SEED" push -q "$ORIGIN" master:master

# ====================================================================
# 4. `be head ssh://origin` — incremental fetch.  Upload-pack on the
#    server side prints `Total N (delta D), reused …`; only the 3
#    new objects atop A should travel the wire.
# ====================================================================
cd wt
"$BE" head "ssh://localhost/$REL_ORIGIN" \
    >02.head.got.out 2>02.head.got.err || true

total=$(grep -oE 'Total [0-9]+' 02.head.got.err | tail -1 | awk '{print $2}')
[ -n "$total" ] || {
    echo "post/06: no 'Total N' line in upload-pack stderr" >&2
    sed -n 1,40p 02.head.got.err >&2
    exit 1
}
[ "$total" -le 3 ] || {
    echo "post/06: upload-pack sent $total objects; expected <=3" >&2
    echo "post/06: indicates haves negotiation failed (full re-clone)" >&2
    sed -n 1,40p 02.head.got.err >&2
    exit 1
}
