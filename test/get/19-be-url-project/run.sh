#!/bin/sh
#  get/19-be-url-project — `be get be://host?/project/branch`:
#  project segment is derived from the FIRST path component of the
#  query, not from the URL's path basename (the rule for non-be
#  schemes).  Confirms `be_url_project` (beagle/BE.cli.c) routes the
#  be-scheme branch into the query-prefix code path and
#  `be_ensure_project_repo` lays down `.be/<project>/{refs,wtlog}`
#  plus a `.be/wtlog` row-0 anchor pinning this wt to that shard.
#
#  The wire intentionally fails (nonexistent .invalid host); we only
#  assert the on-disk layout, which `be_ensure_project_repo` writes
#  BEFORE the keeper fetch step runs.

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh pre-creates an empty `.be/` to shield walk-up from a real
#  `.be` in any parent.  The shield is harmless here:
#  `be_ensure_project_repo` gates on the wtlog's row-0 `repo` anchor
#  (populated via home_anchor_resolve's primary-wt peek), not on the
#  bare existence of `.be/`.  Empty `.be/` ⇒ no anchor ⇒ fresh-init
#  runs and writes the shard.

rc=0
timeout 10 "$BE" get 'be://nonexistent.invalid?/myproj/main' \
    >01.get.got.out 2>01.get.got.err || rc=$?

#  Wire failure is expected.  We don't assert the exit code.

[ -d .be/myproj ] || {
    echo "missing project shard dir .be/myproj/" >&2
    ls -la .be 2>&1 >&2 || true
    exit 1
}
[ -f .be/myproj/refs ] || {
    echo "missing .be/myproj/refs" >&2
    exit 1
}
[ -f .be/myproj/wtlog ] || {
    echo "missing .be/myproj/wtlog" >&2
    exit 1
}

[ -s .be/wtlog ] || {
    echo ".be/wtlog empty — row-0 anchor missing" >&2
    exit 1
}

#  Row-0 shape: `<ts>\tget\tfile:<abs>/.be/myproj/` (the wt->store
#  anchor; verb `get` since the get-unification, formerly `repo`).
#  Loose match on the suffix (skip the timestamp + absolute path) so
#  realpath drift between $SCRATCH and `pwd` doesn't break it.
head -n1 .be/wtlog | grep -F 'get	file:' >/dev/null && \
    head -n1 .be/wtlog | grep -F '/.be/myproj/' >/dev/null || {
    echo ".be/wtlog row-0 missing project anchor; got:" >&2
    head -n1 .be/wtlog >&2
    exit 1
}

#  Negative: there must be no `.be/nonexistent.invalid/` dir — that
#  would mean we mistakenly used the URL-basename rule on a be:
#  scheme URI.
[ ! -d .be/nonexistent.invalid ] || {
    echo "unexpected shard .be/nonexistent.invalid/ — be: scheme " \
         "should NOT use URL-basename derivation" >&2
    exit 1
}

#  Idempotency: a second `be get` on the same project URI must hit
#  the anchored-gate in `be_ensure_project_repo` and skip fresh-init.
#  Witness: `.be/wtlog` row count stays at 1 (no duplicate row-0
#  anchor appended).
wtlog_rows_before=$(wc -l < .be/wtlog)
[ "$wtlog_rows_before" -eq 1 ] || {
    echo "pre-condition: expected 1 wtlog row, got $wtlog_rows_before" >&2
    exit 1
}

rc=0
timeout 10 "$BE" get 'be://nonexistent.invalid?/myproj/main' \
    >02.get.got.out 2>02.get.got.err || rc=$?

wtlog_rows_after=$(wc -l < .be/wtlog)
[ "$wtlog_rows_after" -eq 1 ] || {
    echo "anchored-gate failed: wtlog grew from 1 to $wtlog_rows_after row(s) on re-invocation" >&2
    cat .be/wtlog >&2
    exit 1
}
