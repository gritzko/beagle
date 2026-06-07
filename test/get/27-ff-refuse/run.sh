#!/bin/sh
#  get/27-ff-refuse — `be get ssh://origin?<br>` must refuse when
#  cur's local tip is NOT an ancestor of the incoming remote tip.
#
#  Spec (https://replicated.wiki/html/wiki/GET.html §GET):
#    "For the transport-scheme remote form ... GET is also fast-forward-
#     only on the local branch's tip: it refuses if the local tip is
#     not an ancestor of the incoming remote tip."
#
#  Locking-in test.  The FF gate at sniff/GET.c::~1131 fires when
#  the source URI starts with `?<branch>`; both `be get
#  ssh://origin?master` and the bareword `be get ssh://origin`
#  shapes go through it because the dispatcher fills in cur's
#  branch.  This test guards that contract against regressions.
#
#  Known gap (not covered here): the originating trace was a
#  sub-mount `be get //spot` that silently reset 5 files.  The
#  sub-recursion path may bypass this gate; a sub-mount repro is
#  tracked separately (test/TRIANGLE.todo.md).
#
#  Repro shape — *linear ahead* (matches the originating trace where
#  local cur f922149d sat directly on top of remote tip aa9f3e20,
#  not a fork):
#    A — origin's seed; cloned into wt at A.
#    B — wt commits locally on top of A → cur at B.  Origin still at A.
#  Local(B) is NOT an ancestor of incoming(A); A is ancestor of B.
#  Per spec FF rule, `be get ssh://origin` (default-branch shape)
#  must refuse — pulling A would silently drop B's commit.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "get/27: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "get/27: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2; exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"

LOGS="$SCRATCH/logs"; mkdir -p "$LOGS"

# ----------------------------------------------------------------
# 1. seed origin (master @ A) via plain git.
# ----------------------------------------------------------------
git init --bare -b master "$ORIGIN" >/dev/null
git init -b master "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
echo "version A" > "$SEED/hello.c"
git -C "$SEED" add hello.c >/dev/null
git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# ----------------------------------------------------------------
# 2. clone into wt via ssh.
# ----------------------------------------------------------------
mkdir wt wt/.be
cd wt
"$BE" get "ssh://localhost/$REL_ORIGIN?master" \
    > "$LOGS/01.clone.out" 2> "$LOGS/01.clone.err" || {
    echo "FAIL: initial clone failed" >&2
    cat "$LOGS/01.clone.err" >&2
    exit 1
}
[ "$(cat hello.c)" = "version A" ] || {
    echo "FAIL: post-clone hello.c != 'version A'" >&2
    exit 1
}

# ----------------------------------------------------------------
# 3. wt commits locally on top of A → cur at B.  Origin stays at A.
# ----------------------------------------------------------------
sleep 0.02
echo "version B (client ahead)" > hello.c
"$BE" post 'client ahead' \
    > "$LOGS/02.client-post.out" 2> "$LOGS/02.client-post.err" || {
    echo "FAIL: client-side be post failed" >&2
    cat "$LOGS/02.client-post.err" >&2
    exit 1
}

# Snapshot the wt's pre-GET content — it must survive the refusal.
CONTENT_BEFORE=$(cat hello.c)
cd ..; cd wt  # noop, keeps cwd consistent for the remaining steps

# ----------------------------------------------------------------
# 5. `be get //localhost` — bare cached form, default branch.
#    Per https://replicated.wiki/html/wiki/Verbs.html the FF refusal applies to the "transport-scheme
#    remote form"; the cached form should also not silently demote
#    the local tip past where the cached refs say.  In the
#    originating trace this shape silently reset 5 files.
# ----------------------------------------------------------------
set +e
"$BE" get "//localhost" \
    > "$LOGS/03.get.out" 2> "$LOGS/03.get.err"
RC=$?
set -e

if [ "$RC" -eq 0 ]; then
    echo "FAIL: GET unexpectedly succeeded across divergent histories" >&2
    echo "      stderr:" >&2
    cat "$LOGS/03.get.err" >&2
    echo "      wt hello.c is now: '$(cat hello.c)'" >&2
    exit 1
fi

# Spec wants a "not a fast-forward" diagnostic with the recovery
# pointer (`be patch //...?<br>#` + `be post`).
grep -q -i 'fast-forward\|not a fast\|FF' "$LOGS/03.get.err" || {
    echo "FAIL: stderr lacks an FF-refusal diagnostic:" >&2
    cat "$LOGS/03.get.err" >&2
    exit 1
}

# ----------------------------------------------------------------
# 6. wt must hold the client's C bytes (not reset to A or B).
# ----------------------------------------------------------------
[ "$(cat hello.c)" = "$CONTENT_BEFORE" ] || {
    echo "FAIL: hello.c changed across refused GET" >&2
    echo "      before: '$CONTENT_BEFORE'" >&2
    echo "      after : '$(cat hello.c)'" >&2
    exit 1
}

rm -rf "$LOGS"
