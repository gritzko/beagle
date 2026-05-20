#!/bin/sh
#  put/03-branch-shard — `be put ?<branch>` creates the branch shard
#  dir, and any subsequent `be post` on that branch writes its pack
#  log INTO that shard (`.be/<branch>/NNNNNNNNNN.keeper`), NOT into
#  the trunk root.
#
#  Pinned behaviour (keeper/KEEP.h "On-disk layout"):
#
#     .be/                            trunk pack log lives here
#       NNNNNNNNNN.keeper
#       <branch>/                     created by `be put ?<branch>`
#         NNNNNNNNNN.keeper           every post on <branch> lands here
#         NNNNNNNNNN.keeper.idx
#
#  Closest existing coverage was `test/branches/01-lifecycle`, which
#  asserts `.be/<branch>/` exists after `be put ?./<branch>` but never
#  inspects the resulting pack file's *location*.  Without that check
#  a regression that flushed branch-commit packs into the trunk root
#  could silently slip through.

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

#  Snapshot trunk's pack inventory before any branch work.  Branch
#  creation must not move/copy this file.
TRUNK_PACKS_BEFORE=$(ls -1 .be/$P/*.keeper 2>/dev/null | sort)
[ -n "$TRUNK_PACKS_BEFORE" ] || {
    echo "FAIL: no trunk pack at .be/$P/*.keeper after baseline post" >&2
    ls -la .be >&2
    exit 1
}

# ---------------------------------------------------------------
# 2. `be put ?./feat` — create the feature branch.  Shard dir must
#    materialise immediately; trunk's pack inventory must not change.
# ---------------------------------------------------------------
"$BE" put '?./feat' > /dev/null
[ -d .be/$P/feat ] || {
    echo "FAIL: .be/$P/feat shard dir missing after 'be put ?./feat'" >&2
    ls -la .be/$P >&2
    exit 1
}
TRUNK_PACKS_AFTER_PUT=$(ls -1 .be/$P/*.keeper 2>/dev/null | sort)
[ "$TRUNK_PACKS_BEFORE" = "$TRUNK_PACKS_AFTER_PUT" ] || {
    echo "FAIL: trunk packs changed across branch-create:" >&2
    echo "  before: $TRUNK_PACKS_BEFORE" >&2
    echo "  after : $TRUNK_PACKS_AFTER_PUT" >&2
    exit 1
}

#  Before any commit on `feat`, its shard must NOT have a pack file
#  yet (REFS entry only — see KEEPCreateBranch).
[ -z "$(ls -1 .be/$P/feat/*.keeper 2>/dev/null)" ] || {
    echo "FAIL: .be/$P/feat/*.keeper present before any post on the branch" >&2
    ls -la .be/$P/feat >&2
    exit 1
}

# ---------------------------------------------------------------
# 3. Switch wt to ?feat, edit a file, stage, commit.  The resulting
#    pack file MUST land at .be/feat/NNNNNNNNNN.keeper.  The trunk
#    pack inventory must STILL be unchanged.
# ---------------------------------------------------------------
"$BE" get '?feat'         > /dev/null
sleep 0.02
echo "feat v2" > x.txt
"$BE" put x.txt           > /dev/null
"$BE" post 'feat msg'     > /dev/null

FEAT_PACKS=$(ls -1 .be/$P/feat/*.keeper 2>/dev/null | sort)
[ -n "$FEAT_PACKS" ] || {
    echo "FAIL: no pack at .be/$P/feat/*.keeper after 'be post' on the branch" >&2
    ls -laR .be >&2
    exit 1
}

#  The branch dir must also carry at least one `.keeper.idx` sidecar.
#  Idx files have their own LSM seqno space (the LSM run that indexes
#  the just-written objects sits on top of the ladder; KEEPPackClose
#  picks its seqno from the global counter, so pack and idx names need
#  not share digits).  Either way, the branch dir must be self-
#  sufficient: a reader walking only `.be/feat/` can find both the
#  pack bytes and the index entries pointing into them.
FEAT_IDXES=$(ls -1 .be/$P/feat/*.keeper.idx 2>/dev/null | sort)
[ -n "$FEAT_IDXES" ] || {
    echo "FAIL: no .keeper.idx sidecar in .be/$P/feat/ after the branch post" >&2
    ls -la .be/$P/feat >&2
    exit 1
}

#  Pack + idx filenames must match the documented format: 10 RON64
#  digits + extension (KEEP.h "feature-branch pack logs at
#  <branch>/NNNNN.keeper").  RON64 alphabet =
#  0-9A-Z_a-z~ — 64 chars covering 6 bits per digit, lex-sortable
#  numerically.
for f in $FEAT_PACKS $FEAT_IDXES; do
    base=${f##*/}
    case "$base" in
        [0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~].keeper) ;;
        [0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~][0-9A-Za-z_~].keeper.idx) ;;
        *) echo "FAIL: branch file $f has non-canonical name '$base'" >&2
           exit 1;;
    esac
done

#  Trunk's pack inventory is unchanged by the post on `feat`.
TRUNK_PACKS_AFTER_POST=$(ls -1 .be/$P/*.keeper 2>/dev/null | sort)
[ "$TRUNK_PACKS_BEFORE" = "$TRUNK_PACKS_AFTER_POST" ] || {
    echo "FAIL: trunk packs changed when posting on ?feat:" >&2
    echo "  before:    $TRUNK_PACKS_BEFORE" >&2
    echo "  after-post: $TRUNK_PACKS_AFTER_POST" >&2
    exit 1
}
