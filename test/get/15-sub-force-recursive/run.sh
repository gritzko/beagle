#!/bin/sh
#  get/15-sub-force-recursive — `--force` propagates into submodule
#  recursion.  With the sub already at its pinned commit, a fresh
#  `be get ?master` without `--force` would noop-overlay-skip the
#  sub's files (preserving any dirty edits).  Adding `--force`
#  must reach the sub's `sniff get` and overwrite the dirty bytes.
#
#  Setup (from submodules.sh):
#      Clone parent at master.  Parent pins vendor/sub at SUB_C2.
#
#  Test:
#      1. Clone parent into wt/ — sub mounts at SUB_C2.
#      2. Dirty vendor/sub/core.c with a sentinel line.
#      3. Sanity: `be get ?master` (no --force) is a no-op for the
#         already-at-pin sub; dirty edit survives the noop overlay.
#      4. `be get --force ?master` — recursion forwards --force into
#         the sub; dirty edit gets overwritten back to SUB_C2's
#         content (no sentinel).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt && cd wt

# --- 1. Clone parent (sub mounts at SUB_C2). -------------------------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "stage-1 be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f vendor/sub/core.c ] || fail "fixture: sub not mounted"

# --- 2. Dirty the sub's core.c with a sentinel. ----------------------
SENTINEL='/* get/15 sentinel — should not survive --force */'
echo "$SENTINEL" >> vendor/sub/core.c
grep -q 'get/15 sentinel' vendor/sub/core.c \
    || fail "sentinel write failed"

# --- 3. Sanity: no-force pass preserves dirty bytes. -----------------
"$BE" get "?master" >02.get.got.out 2>02.get.got.err
#  Exit code may be non-zero on dirty refusal at parent level; the
#  invariant we care about is that dirty bytes survive AT THE SUB.
grep -q 'get/15 sentinel' vendor/sub/core.c \
    || fail "sentinel disappeared after no-force pass (would-be regression):
$(cat 02.get.got.err)"

# --- 4. --force pass overwrites the sub's dirty bytes. ---------------
"$BE" get --force "?master" >03.get.got.out 2>03.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "stage-4 --force be get exited $rc; stderr:
$(cat 03.get.got.err)"

if grep -q 'get/15 sentinel' vendor/sub/core.c; then
    echo "sentinel survived --force; --force did not reach the sub" >&2
    echo "vendor/sub/core.c:" >&2
    cat vendor/sub/core.c >&2
    echo "stderr:" >&2
    cat 03.get.got.err >&2
    exit 1
fi

#  Sub's core.c should match SUB_C2's content (has sub_inc, lacks
#  sub_add).
grep -q 'sub_inc' vendor/sub/core.c \
    || fail "sub core.c lost sub_inc after --force overwrite"
grep -q 'sub_add' vendor/sub/core.c \
    && fail "sub core.c has sub_add — pin advanced past SUB_C2 erroneously"

note "get/15-sub-force-recursive: --force overwrites dirty sub bytes"
