#!/bin/sh
#  post/05-rebase-on-remote — `be post //origin` rebases cur onto
#  the remote's counterpart with be's token-level merge.  Canonical
#  "rebase local trunk on top of remote main" workflow per VERBS.md
#  §"POST" + §"Schemes — cached vs transport".
#
#  Single file (`hello.c`) edited on BOTH sides, with overlapping
#  TOKEN edits in the same line plus non-overlapping line adds and
#  a token edit elsewhere.  The point is: be's weave merge resolves
#  per-token, so two sides editing different tokens of the same
#  line merge cleanly without conflict.
#
#    A  (origin's seed)              hello world, return 0
#    B  (origin's advance, server)   "Hello, world" → "Hello, team"
#                                     + new line printf("done\n");
#    C  (client's local commit)      "Hello, world" → "Hi, world"
#                                     + #include <stdlib.h>
#                                     + return 0 → return EXIT_SUCCESS
#
#  After `be head ssh://origin && be post //origin`, the rebased
#  hello.c carries every contribution: "Hi, team!" (token-merge on
#  the printf line), the stdlib include, the done-print, and the
#  EXIT_SUCCESS return.
#
#  Status: drafted alongside the spec rewrite at VERBS.md.  Today
#  this test is registered with WILL_FAIL=TRUE; flip when sniff
#  learns to resolve `//remote` URIs to the cached tracking ref
#  and rebase against it (see VERBS.todo.md §"POST").
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "post/05: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "post/05: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       echo "         keeper's ssh-side path resolution requires it" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"

cd "$SCRATCH"

# ====================================================================
# 1. seed origin with version A on master
# ====================================================================
git init --bare "$ORIGIN" >/dev/null
git init "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
git -C "$SEED" checkout -b master >/dev/null || true
sleep 0.02; cp "$CASE/01.A.hello.c" "$SEED/hello.c"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# ====================================================================
# 2. clone into wt via ssh
# ====================================================================
mkdir wt && cd wt
"$BE" get "ssh://localhost/$REL_ORIGIN?master" \
    >01.clone.got.out 2>01.clone.got.err
empty 01.clone.got.out
match "$CASE/01.A.hello.c" hello.c

# ====================================================================
# 3. advance origin to version B
# ====================================================================
cd ..
sleep 0.02; cp "$CASE/02.B.hello.c" "$SEED/hello.c"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm B
git -C "$SEED" push -q "$ORIGIN" master:master

# ====================================================================
# 4. local commit on cur (parent = A) — version C
# ====================================================================
cd wt
sleep 0.02; cp "$CASE/03.client.hello.c" hello.c
"$BE" post 'client edits' >02.post.got.out 2>02.post.got.err
empty 02.post.got.out
match "$CASE/03.client.hello.c" hello.c

# ====================================================================
# 5. fetch via HEAD (refs+pack land in keeper, .dogs/refs updated)
# ====================================================================
"$BE" head "ssh://localhost/$REL_ORIGIN" \
    >03.head.got.out 2>03.head.got.err

# ====================================================================
# 6. rebase: cur onto cached counterpart on localhost
#    REFSResolve matches the //auth needle against the ref log's
#    HOST field; here the peer is `ssh://localhost/...`, so the
#    cached-form lookup is `//localhost` (no alias registry).
# ====================================================================
"$BE" post "//localhost" >04.rebase.got.out 2>04.rebase.got.err
empty 04.rebase.got.out

# ====================================================================
# 7. verify wt's hello.c is the token-level merge of B and C
# ====================================================================
match "$CASE/04.rebased.hello.c" hello.c
