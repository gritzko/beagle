#!/bin/sh
#  spot/10-sub-recurse-search — SUBS-011 repro.  A repo-wide search
#  projector (`be grep:` / `be spot:` / `be regex:`) must descend into
#  every mounted submodule, query that sub's own trigram index, and
#  emit hits path-prefixed under the mount (`vendor/sub/core.c#Ln`) in
#  one merged stream.
#
#  Two coupled causes the fix addresses:
#    1. On mount, `be get` now indexes the sub's checked-out tree, so
#       `.be/<sub-shard>/*.spot.idx` exists.
#    2. `BEProjector` enumerates mounted subs (BESubsHere) and relays
#       the search into each (BERelaySub), path-prefixing the hits.
#
#  The headline assertion: a sub-ONLY symbol (`sub_counter`, defined
#  only in vendor/sub/core.c, never in the parent) surfaces from a
#  repo-wide `be grep:` run at the parent root.  `--nosub` suppresses
#  the descent.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing after get"

#  `sub_counter` is defined ONLY in vendor/sub/core.c (parent files
#  main.c / util.c never mention it).  Confirm the fixture invariant.
grep -q sub_counter vendor/sub/core.c \
    || fail "fixture: sub_counter not in vendor/sub/core.c"
if grep -rq sub_counter main.c util.c 2>/dev/null; then
    fail "fixture: sub_counter unexpectedly present in a parent file"
fi

#  The mount must have produced a SUB-shard spot index (cause #1).
#  The sub shard is keyed by the URL basename (`sub` here); the parent
#  shard (`parent`) is indexed regardless, so glob the sub shard only.
ls .be/sub/*.spot.idx >/dev/null 2>&1 \
    || fail "no sub-shard .spot.idx after mount; sub never indexed.
.be contents:
$(ls -R .be 2>/dev/null)"

#  HEADLINE: repo-wide grep for the sub-only symbol must list the
#  path-prefixed sub hit (cause #2).
"$BE" grep:.c#sub_counter >02.grep.got.out 2>02.grep.got.err
rc=$?
[ "$rc" = 0 ] || fail "be grep: exited $rc; stderr:
$(cat 02.grep.got.err)
stdout:
$(cat 02.grep.got.out)"

grep -q "vendor/sub/core.c" 02.grep.got.out \
    || fail "repo-wide grep did not surface vendor/sub/core.c; stdout:
$(cat 02.grep.got.out)"

#  --nosub suppresses the descent: the sub-only symbol must NOT appear.
"$BE" grep:.c#sub_counter --nosub >03.grep.got.out 2>03.grep.got.err || true
if grep -q "vendor/sub/core.c" 03.grep.got.out; then
    fail "be grep: --nosub still descended into the sub; stdout:
$(cat 03.grep.got.out)"
fi

note "spot/10: repo-wide grep finds sub-only symbol under vendor/sub/"
