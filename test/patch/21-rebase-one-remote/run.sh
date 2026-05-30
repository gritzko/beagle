#!/bin/sh
#  patch/21-rebase-one-remote — `be patch ssh://origin?<br>#` (rebase-one
#  over a transport scheme) must STAY rebase-one.
#
#  Bug: BEActResolveRemote rewrites `scheme://host?ref` → `?<sha>` and
#  re-attached the user's `#frag` only when it was NON-empty
#  (`u8csEmpty` test).  A rebase-one's fragment is *present but empty*
#  (`?<br>#`), so the `#` got dropped → the row degraded to a SQUASH,
#  and the next `be post` couldn't auto-resolve the message
#  (POSTNOMSG).  Fix: re-attach `#` whenever the fragment is PRESENT
#  (fragment[0] != NULL), matching patch_shape's `has_f` test.
#
#  THE CHECK: after a remote rebase-one, `be post` with NO `#msg`
#  succeeds (rebase-one auto-reuses the replayed commit's message).  On
#  the bug it degraded to squash → POSTNOMSG → non-zero exit.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "patch/21: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "patch/21: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2; exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"
L="$SCRATCH/logs"; mkdir -p "$L"   # capture outside the wt so a
                                    # commit-all POST can't sweep them in

cd "$SCRATCH"

# 1. seed origin with A on master
git init --bare "$ORIGIN" >/dev/null
git init "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
git -C "$SEED" checkout -b master >/dev/null 2>&1 || true
printf 'one\n' > "$SEED/f.txt"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# 2. clone into wt via ssh (cur at A)
mkdir wt wt/.be && cd wt
"$BE" get "ssh://localhost/$REL_ORIGIN?master" >"$L/01.clone.out" 2>"$L/01.clone.err"

# 3. origin advances to B with a distinctive message
cd ..
sleep 0.02
printf 'one\ntwo\n' > "$SEED/f.txt"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm 'remote commit B'
git -C "$SEED" push -q "$ORIGIN" master:master

# 4. rebase-one B onto cur over the wire (the bug site).
cd wt
sleep 0.02
"$BE" patch "ssh://localhost/$REL_ORIGIN?master#" \
    >"$L/02.patch.out" 2>"$L/02.patch.err" \
    || { echo "patch/21: be patch ?master# failed" >&2
         cat "$L/02.patch.err" >&2; exit 1; }
# sanity: B's change actually landed in the wt.
grep -q two f.txt || { echo "patch/21: rebase didn't apply B (no 'two')" >&2; exit 1; }

# 5. THE CHECK: `be post` with NO message must succeed (rebase-one
#    auto-reuses B's subject).  Degraded-to-squash → POSTNOMSG → fail.
sleep 0.02
"$BE" post >"$L/03.post.out" 2>"$L/03.post.err" || {
    echo "patch/21: be post (no msg) failed — rebase-one degraded to squash?" >&2
    cat "$L/03.post.err" >&2
    exit 1; }
