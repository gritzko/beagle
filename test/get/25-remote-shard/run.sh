#!/bin/sh
#  get/25-remote-shard — under the FLAT store layout, remotes are FOLDED
#  into the single project shard.  There is NO per-host `remotes/` class
#  dir and NO `.be/<project>/remotes/<host>/` subdir.  Remote-tracking
#  refs land in the project shard's flat `refs` ULOG (peer-form keys like
#  `//host?heads/...`).
#
#  The wire intentionally fails (nonexistent .invalid host); we only
#  assert the on-disk layout, which BEEnsureProjectRepo writes BEFORE
#  the keeper fetch step runs.  Mirror of get/19-be-url-project's
#  shape — same scenario, flat-layout assertion.

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

#  Flat layout: NO separate remotes/ class dir, NO per-host subdir.
[ ! -d ".be/myproj/remotes" ] || {
    echo "remotes/ class dir must NOT exist (folded into project shard)" >&2
    ls -la .be/myproj 2>&1 >&2 || true
    exit 1
}
[ ! -d ".be/myproj/remotes/${HOST}" ] || {
    echo "per-host remote shard must NOT exist (flat layout)" >&2
    exit 1
}

#  The remote-tracking ref must land in the project shard's flat refs.
[ -f ".be/myproj/refs" ] || {
    echo "missing flat project refs .be/myproj/refs after remote get" >&2
    ls -la ".be/myproj" 2>&1 >&2 || true
    exit 1
}
grep -q "//${HOST}" ".be/myproj/refs" 2>/dev/null || {
    echo "no peer-URI ref //${HOST} in flat project refs .be/myproj/refs" >&2
    cat ".be/myproj/refs" >&2 2>/dev/null || true
    exit 1
}

#  Idempotency: a second `be get` against the same URI must not create
#  a remotes/ dir, and the flat refs must still carry the peer URI.
rc=0
timeout 10 "$BE" get "be://${HOST}?/myproj/main" \
    >02.get.got.out 2>02.get.got.err || rc=$?

[ ! -d ".be/myproj/remotes" ] || {
    echo "remotes/ dir appeared on second invocation (flat layout)" >&2
    exit 1
}
grep -q "//${HOST}" ".be/myproj/refs" 2>/dev/null || {
    echo "peer-URI ref vanished from flat refs on second invocation" >&2
    exit 1
}
