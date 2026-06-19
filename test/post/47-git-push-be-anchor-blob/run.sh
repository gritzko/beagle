#!/bin/sh
#  post/47-git-push-be-anchor-blob — GIT-009.
#
#  A foreign git history can commit the `.be` store-anchor name as a
#  blob entry in its trees (e.g. someone `git add`-ed a `.be` file).
#  beagle clones such a history verbatim — the committed tree objects
#  still carry the `.be` entry, even though `.be` is the gitignored
#  store dir and is never checked out.
#
#  Pushing that history to a BRAND-NEW / empty git remote produced an
#  inconsistent pack: the pack-build closure walk (keeper/WALK.c, the
#  DPATHVerify filter) deliberately SKIPPED `.be`, so the blob the tree
#  entry points at was never written into the pack.  A fresh remote has
#  no copy, so git-receive-pack / `git fsck` rejected the push with
#    fatal: missing blob object '<sha>'
#    remote: ref update rejected: missing necessary objects
#  → Error: WIRECLFL / Error: BEDOGEXIT (exit 157), remote never advanced.
#  Against a remote that already had the blob (every incremental FF push,
#  the normal path) it was harmless — only fresh/empty remotes failed.
#
#  Fix (GIT-009): object-closure walks (push/serve pack-build) pass
#  WALK_INCL_ANCHOR so the `.be` blob is collected into the pack, keeping
#  the shipped tree's references self-consistent.  Working-tree walks
#  still drop the untracked anchor.
#
#  This case builds a 2-commit git history whose trees carry a `.be`
#  blob (via fast-import), clones it into a beagle worktree, then pushes
#  to a fresh bare git remote.  Pre-fix this dies with `missing blob
#  object` and the remote stays empty; post-fix the remote's `main`
#  FF-advances and `git fsck` confirms the pack is complete.
#
#  Fully offline (file://), no ssh — runs under WITH_SSH=OFF.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

#  --- 1. build a git history whose trees carry a `.be` blob ----------
SRC="$SCRATCH/src"
git init -q -b main "$SRC" || { echo "FAIL(setup): git init src" >&2; exit 1; }
{
    echo "reset refs/heads/main"
    echo "commit refs/heads/main"
    echo "mark :1"
    echo "committer T <t@t> 1000000001 +0000"
    msg="c1"; echo "data ${#msg}"; printf '%s' "$msg"; echo
    echo "M 100644 inline f.txt"
    echo "data 1"; printf 'a'; echo
    #  THE TRIGGER: a `.be` blob entry recorded in the committed tree.
    echo "M 100644 inline .be"
    anchor="anchor-blob"; echo "data ${#anchor}"; printf '%s' "$anchor"; echo
    echo "commit refs/heads/main"
    echo "mark :2"
    echo "committer T <t@t> 1000000002 +0000"
    msg="c2"; echo "data ${#msg}"; printf '%s' "$msg"; echo
    echo "from :1"
    echo "M 100644 inline f.txt"
    echo "data 2"; printf 'ab'; echo
} | git -C "$SRC" fast-import --quiet \
    || { echo "FAIL(setup): fast-import" >&2; exit 1; }
#  Confirm the committed tree really carries the `.be` entry.
git -C "$SRC" ls-tree HEAD | grep -q '	\.be$' \
    || { echo "FAIL(setup): .be not recorded in tree" >&2
         git -C "$SRC" ls-tree HEAD >&2; exit 1; }

#  --- 2. bare git remote to clone FROM (so beagle clones a git repo) --
SRC_BARE="$SCRATCH/src.git"
git init -q --bare -b main "$SRC_BARE"
git -C "$SRC_BARE" config protocol.file.allow always
git -C "$SRC" -c protocol.file.allow=always push -q "$SRC_BARE" main:main \
    || { echo "FAIL(setup): seed bare push" >&2; exit 1; }
src_tip=$(git -C "$SRC_BARE" rev-parse main)

#  --- 3. clone the `.be`-bearing history into a beagle worktree ------
WT="$SCRATCH/wt"
mkdir -p "$WT/.be"
( cd "$WT" && "$BE" get --nosub "file://$SRC_BARE" \
        >../03.get.out 2>../03.get.err ) \
    || { echo "FAIL: clone .be-bearing git history into beagle wt" >&2
         cat "$SCRATCH/03.get.err" >&2; exit 1; }
[ -f "$WT/f.txt" ] || { echo "FAIL: clone did not check out f.txt" >&2; exit 1; }

#  --- 4. THE CHECK: push the whole history to a FRESH git remote -----
DEST="$SCRATCH/dest.git"
git init -q --bare -b main "$DEST"
git -C "$DEST" config protocol.file.allow always
git -C "$DEST" config receive.denyCurrentBranch ignore

( cd "$WT" && "$BE" post --nosub "file://localhost$DEST?main" \
        >../04.push.out 2>../04.push.err )
rc=$?

#  (a) MUST exit clean — pre-fix this was 157 (BEDOGEXIT after WIRECLFL).
[ "$rc" = 0 ] || { echo "FAIL: fresh-remote git push exited $rc (expected 0)" >&2
                   cat "$SCRATCH/04.push.err" >&2; exit 1; }

#  (b) MUST NOT have reported a missing blob object (the bug).
! grep -q 'missing blob object' "$SCRATCH/04.push.err" \
    || { echo "FAIL: pack omitted the .be blob (GIT-009)" >&2
         cat "$SCRATCH/04.push.err" >&2; exit 1; }

#  (c) MUST NOT have been rejected for missing objects.
! grep -q 'missing necessary objects' "$SCRATCH/04.push.err" \
    || { echo "FAIL: receive-pack rejected the pack (GIT-009)" >&2
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
[ "$dest_count" = 2 ] \
    || { echo "FAIL: remote has $dest_count commits, want 2" >&2; exit 1; }

echo "post/47-git-push-be-anchor-blob: OK (.be-bearing history pushed, main -> $dest_tip, fsck clean)"
