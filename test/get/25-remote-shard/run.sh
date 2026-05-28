#!/bin/sh
#  get/25-remote-shard — `be get be://host?/project/branch` lays down
#  the per-host remote shard `.be/<project>/remotes/<host>/refs`
#  parallel to the project shard.  STORE.md §"Repo dir layout":
#  remotes live in a `remotes/` class dir next to branches, one
#  subdir per host.  Cache-only — no wtlog seed inside the remote
#  shard.
#
#  The wire intentionally fails (nonexistent .invalid host); we only
#  assert the on-disk layout, which BEEnsureProjectRepo writes BEFORE
#  the keeper fetch step runs.  Mirror of get/19-be-url-project's
#  shape — same scenario, additional assertion.

. "$(dirname "$0")/../../lib/case.sh"

HOST=nonexistent.invalid

rc=0
timeout 10 "$BE" get "be://${HOST}?/myproj/main" \
    >01.get.got.out 2>01.get.got.err || rc=$?

#  Wire failure is expected; we don't assert the exit code.

[ -d .be/myproj ] || {
    echo "missing project shard dir .be/myproj/" >&2
    ls -la .be 2>&1 >&2 || true
    exit 1
}

#  Remote shard layout per STORE.md §"Repo dir layout".
[ -d ".be/myproj/remotes" ] || {
    echo "missing remotes class dir .be/myproj/remotes/" >&2
    ls -la .be/myproj 2>&1 >&2 || true
    exit 1
}
[ -d ".be/myproj/remotes/${HOST}" ] || {
    echo "missing per-host remote shard .be/myproj/remotes/${HOST}/" >&2
    ls -la .be/myproj/remotes 2>&1 >&2 || true
    exit 1
}
[ -f ".be/myproj/remotes/${HOST}/refs" ] || {
    echo "missing .be/myproj/remotes/${HOST}/refs seed" >&2
    ls -la ".be/myproj/remotes/${HOST}" 2>&1 >&2 || true
    exit 1
}

#  Negative: remote shards are caches, not worktrees — no wtlog.
[ ! -e ".be/myproj/remotes/${HOST}/wtlog" ] || {
    echo "unexpected wtlog inside remote shard (should be cache-only)" >&2
    exit 1
}

#  Idempotency: a second `be get` against the same URI must not
#  duplicate or churn the remote shard.  Refs file stays empty and
#  the dir is still there.
rc=0
timeout 10 "$BE" get "be://${HOST}?/myproj/main" \
    >02.get.got.out 2>02.get.got.err || rc=$?

[ -d ".be/myproj/remotes/${HOST}" ] || {
    echo "remote shard vanished on second invocation" >&2
    exit 1
}
[ -f ".be/myproj/remotes/${HOST}/refs" ] || {
    echo "refs seed vanished on second invocation" >&2
    exit 1
}
