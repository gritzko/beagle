#!/bin/sh
#  post/46-git-push-long-history-no-bnoroom — POST-021.
#
#  A `be post //git-remote` of a tree with a LONG commit history failed
#  outright: the push-side closure walk leaked ~1 MiB of BASS scratch per
#  commit (KEEPWalkTree carved a 1 MiB blob buffer even on the !eager
#  push path, and wpush_walk_commit recursed the parent chain in plain C
#  with no per-commit call()/try() frame, so nothing rewound it).  After
#  ~900 commits the 1 GiB BASS arena drained, `a_carve` returned BNOROOM
#  mid-pack-build, and the spawned git-receive-pack died:
#    fatal: the remote end hung up unexpectedly / Error: BNOROOM /
#    Error: BEDOGEXIT  — and the remote ref never advanced.
#  Two undersized client buffers compounded it once the walk was fixed:
#  the per-object pack buffer (16 MiB) and the whole-pack buffer (64 MiB
#  BASS carve) overflowed on a large blob / a big total object stream.
#
#  This case builds a 1000-commit git history (via fast-import, fast),
#  clones it into a beagle worktree, then pushes the whole thing to a
#  fresh bare git remote.  Pre-fix this dies with BNOROOM and the remote
#  stays empty; post-fix the remote's `main` FF-advances to cur's tip and
#  `git fsck` confirms the pack is complete (no missing objects).
#
#  Fully offline (file://), no ssh — runs under WITH_SSH=OFF.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

NCOMMITS=1000

#  --- 1. build a long git history fast via fast-import ----------------
SRC="$SCRATCH/src"
git init -q -b main "$SRC" || { echo "FAIL(setup): git init src" >&2; exit 1; }
{
    echo "reset refs/heads/main"
    i=1
    while [ "$i" -le "$NCOMMITS" ]; do
        echo "commit refs/heads/main"
        echo "mark :$i"
        echo "committer T <t@t> $((1000000000 + i)) +0000"
        msg="c$i"; echo "data ${#msg}"; printf '%s' "$msg"; echo
        [ "$i" -gt 1 ] && echo "from :$((i - 1))"
        echo "M 100644 inline f.txt"
        echo "data ${#i}"; printf '%s' "$i"; echo
        i=$((i + 1))
    done
} | git -C "$SRC" fast-import --quiet \
    || { echo "FAIL(setup): fast-import" >&2; exit 1; }
have=$(git -C "$SRC" rev-list --count main)
[ "$have" = "$NCOMMITS" ] \
    || { echo "FAIL(setup): got $have commits, want $NCOMMITS" >&2; exit 1; }

#  --- 2. bare git remote to clone FROM (so beagle clones a git repo) --
SRC_BARE="$SCRATCH/src.git"
git init -q --bare -b main "$SRC_BARE"
git -C "$SRC_BARE" config protocol.file.allow always
git -C "$SRC" -c protocol.file.allow=always push -q "$SRC_BARE" main:main \
    || { echo "FAIL(setup): seed bare push" >&2; exit 1; }

#  --- 3. clone the long history into a beagle worktree ---------------
WT="$SCRATCH/wt"
mkdir -p "$WT/.be"
( cd "$WT" && "$BE" get --nosub "file://$SRC_BARE" \
        >../03.get.out 2>../03.get.err ) \
    || { echo "FAIL: clone long git history into beagle wt" >&2
         cat "$SCRATCH/03.get.err" >&2; exit 1; }
[ -f "$WT/f.txt" ] || { echo "FAIL: clone did not check out f.txt" >&2; exit 1; }
src_tip=$(git -C "$SRC_BARE" rev-parse main)

#  --- 4. THE CHECK: push the whole long history to a fresh git remote -
DEST="$SCRATCH/dest.git"
git init -q --bare -b main "$DEST"
git -C "$DEST" config protocol.file.allow always
git -C "$DEST" config receive.denyCurrentBranch ignore
dest_before=$(git -C "$DEST" show-ref | wc -l)

( cd "$WT" && "$BE" post --nosub "file://localhost$DEST?main" \
        >../04.push.out 2>../04.push.err )
rc=$?

#  (a) MUST exit clean — pre-fix this was 157 (BEDOGEXIT after BNOROOM).
[ "$rc" = 0 ] || { echo "FAIL: long-history git push exited $rc (expected 0)" >&2
                   cat "$SCRATCH/04.push.err" >&2; exit 1; }

#  (b) MUST NOT have raised BNOROOM (the bug) on the wire.
! grep -q 'BNOROOM' "$SCRATCH/04.push.err" \
    || { echo "FAIL: BNOROOM on the push wire (BASS exhaustion, POST-021)" >&2
         cat "$SCRATCH/04.push.err" >&2; exit 1; }

#  (c) MUST NOT have left the receive-pack peer hung up.
! grep -q 'remote end hung up' "$SCRATCH/04.push.err" \
    || { echo "FAIL: git-receive-pack hung up mid-push (POST-021)" >&2
         cat "$SCRATCH/04.push.err" >&2; exit 1; }

#  (d) the remote's `main` FF-advanced to cur's tip.
dest_tip=$(git -C "$DEST" rev-parse main 2>/dev/null)
[ "$dest_tip" = "$src_tip" ] \
    || { echo "FAIL: remote main did not advance to cur" >&2
         printf 'src :%s\ndest:%s\n' "$src_tip" "$dest_tip" >&2
         cat "$SCRATCH/04.push.err" >&2; exit 1; }

#  (e) the pushed pack is COMPLETE — git fsck finds no missing objects.
git -C "$DEST" fsck --full 2>"$SCRATCH/05.fsck.err"
if grep -qiE 'missing|broken|error' "$SCRATCH/05.fsck.err"; then
    echo "FAIL: remote fsck reports an incomplete pack" >&2
    cat "$SCRATCH/05.fsck.err" >&2; exit 1
fi
dest_count=$(git -C "$DEST" rev-list --count main)
[ "$dest_count" = "$NCOMMITS" ] \
    || { echo "FAIL: remote has $dest_count commits, want $NCOMMITS" >&2; exit 1; }

echo "post/46-git-push-long-history-no-bnoroom: OK ($NCOMMITS commits pushed, main -> $dest_tip, fsck clean)"
