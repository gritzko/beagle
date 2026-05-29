#!/bin/sh
#  put/03-branch-shard — under the FLAT store layout, `be put ?<branch>`
#  makes the branch resolvable as a REFS row only; there is NO per-branch
#  object dir.  A subsequent `be post` on that branch writes its pack log
#  directly into the single project shard `.be/$P/`, NOT into a
#  `.be/$P/<branch>/` subdir (which is never created).
#
#  Pinned behaviour (flat model):
#
#     .be/$P/                          one shard per project
#       NNNNNNNNNN.keeper              every post (trunk or branch) lands here
#       NNNNNNNNNN.keeper.idx
#       refs                           branch tips as REFS rows
#
#  Objects are shared in the one pool; branches are pure REFS rows.

. "$(dirname "$0")/../../lib/case.sh"

# Anchor project shard at .be/$P/ so subsequent be invocations
# don't derive the project name from the first URI's basename.
"$BE" put "?/$P/" 2>/dev/null || true

# ---------------------------------------------------------------
# 1. Trunk baseline: one file + one post.
# ---------------------------------------------------------------
echo "trunk v1" > x.txt
"$BE" put x.txt   > /dev/null
"$BE" post 't0'   > /dev/null
#  Resolve the trunk tip from the shard's REFS file (a `post ?#<40hex>`
#  row).  `be get .` prints only to stderr, so it can't capture the sha.
T0=$(grep -oE '\?#[0-9a-f]{40}' .be/$P/refs | grep -oE '[0-9a-f]{40}' | tail -n1)

#  A pack must exist directly in the project shard.
[ -n "$(ls -1 .be/$P/*.keeper 2>/dev/null)" ] || {
    echo "FAIL: no pack at .be/$P/*.keeper after baseline post" >&2
    ls -la .be/$P >&2 2>/dev/null || ls -la .be >&2
    exit 1
}

# ---------------------------------------------------------------
# 2. `be put ?./feat` — create the feature branch.  Flat layout:
#    NO per-branch shard dir is ever created; the branch is a REFS row.
# ---------------------------------------------------------------
"$BE" put '?./feat' > /dev/null
[ ! -d .be/$P/feat ] || {
    echo "FAIL: per-branch shard dir .be/$P/feat must NOT exist (flat layout)" >&2
    ls -la .be/$P >&2
    exit 1
}
#  ?feat must resolve right after create.
"$BE" get '?feat' > /dev/null || {
    echo "FAIL: ?feat does not resolve after 'be put ?./feat'" >&2
    exit 1
}

# ---------------------------------------------------------------
# 3. Switch wt to ?feat, edit a file, stage, commit.  The resulting
#    pack lands in the single project shard .be/$P/ — no subdir.
# ---------------------------------------------------------------
"$BE" get '?feat'         > /dev/null
sleep 0.02
echo "feat v2" > x.txt
"$BE" put x.txt           > /dev/null
"$BE" post 'feat msg'     > /dev/null

#  The post produced a pack + idx directly in the project shard.
[ -n "$(ls -1 .be/$P/*.keeper 2>/dev/null)" ] || {
    echo "FAIL: no pack at .be/$P/*.keeper after 'be post' on ?feat" >&2
    ls -laR .be >&2
    exit 1
}
[ -n "$(ls -1 .be/$P/*.keeper.idx 2>/dev/null)" ] || {
    echo "FAIL: no .keeper.idx sidecar in .be/$P/ after the branch post" >&2
    ls -la .be/$P >&2
    exit 1
}

#  Still no per-branch dir after committing on the branch.
[ ! -d .be/$P/feat ] || {
    echo "FAIL: per-branch shard dir .be/$P/feat appeared after post (flat layout)" >&2
    ls -la .be/$P >&2
    exit 1
}

#  ?feat resolves to the new commit, distinct from trunk tip.  Read the
#  latest feat tip from the shard's REFS file (a `post ?feat#<40hex>` row;
#  the newest such row is the post-commit tip).
T1=$(grep -oE '\?feat#[0-9a-f]{40}' .be/$P/refs | grep -oE '[0-9a-f]{40}' | tail -n1)
[ -n "$T1" ] && [ "$T1" != "$T0" ] || {
    echo "FAIL: ?feat did not advance past trunk (T0=$T0 T1=$T1)" >&2
    exit 1
}
