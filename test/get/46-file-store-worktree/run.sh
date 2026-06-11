#!/bin/sh
#  46-file-store-worktree — GET-012.  `be get file://<store>?/<proj>`
#  from a FRESH dir (no pre-existing `.be`) must attach a store-backed
#  WORKTREE: a `.be` regular FILE anchoring the shared store, NOT a full
#  independent clone (a `.be/` dir with its own object shard).  A post
#  from the worktree must advance the SHARED store's project tip — proof
#  the work lands in the colocated store, not a throwaway clone.
#
#  Contrast (be-post-37): a cwd that pre-creates `.be/` (a directory)
#  asks for an independent clone; a FRESH cwd asks for a worktree.  The
#  two share the same `file://<store>?/<proj>` URL — the cwd `.be` shape
#  is the only signal.

. "$(dirname "$0")/../../lib/case.sh"
rm -rf "$SCRATCH/.be"
HOME="$SCRATCH/home"
export HOME
mkdir -p "$HOME/.be"

#  --- seed a named project `proj` into a bare central store ----------
STORE="$SCRATCH/store/.be"
mkdir -p "$STORE"
S="$SCRATCH/proj"
mkdir -p "$S/.be"
( cd "$S"
  printf 'hello\n' > f.c
  sleep 0.02; "$BE" put f.c          > /dev/null 2>&1
  sleep 0.02; "$BE" post 'seed proj' > /dev/null 2>&1 )
[ -d "$S/.be/proj" ] || {
    echo "FAIL(setup): seed shard not named 'proj': $(ls "$S/.be")" >&2; exit 1; }
cp -r "$S/.be/proj" "$STORE/proj"
cp    "$S/.be/wtlog" "$STORE/wtlog"

#  --- GET-012: fresh dir → store-backed worktree, not a clone --------
mkdir -p "$SCRATCH/wt"
( cd "$SCRATCH/wt"
  "$BE" get "file://$STORE?/proj/" > "$SCRATCH/get.out" 2> "$SCRATCH/get.err" ) || {
    echo "FAIL: be get file://<store>?/proj failed" >&2
    cat "$SCRATCH/get.err" >&2; exit 1; }

[ -f "$SCRATCH/wt/.be" ] && [ ! -d "$SCRATCH/wt/.be" ] || {
    echo "FAIL: wt/.be must be a regular file (worktree), not a clone dir" >&2
    ls -la "$SCRATCH/wt" >&2; exit 1; }
[ ! -d "$SCRATCH/wt/.be/proj" ] || {
    echo "FAIL: wt grew its own object shard — it cloned, not worktree'd" >&2
    exit 1; }
match "$S/f.c" "$SCRATCH/wt/f.c"

#  --- a post from the worktree advances the SHARED store -------------
TIP_BEFORE=$( cd "$SCRATCH/wt" && "$BE" sha1:? )
[ -n "$TIP_BEFORE" ] || { echo "FAIL: worktree has no tip" >&2; exit 1; }
( cd "$SCRATCH/wt"
  printf 'hello v2\n' > f.c
  sleep 0.02; "$BE" post 'wt edits f.c' > "$SCRATCH/post.out" 2> "$SCRATCH/post.err" ) || {
    echo "FAIL: post from worktree failed" >&2; cat "$SCRATCH/post.err" >&2; exit 1; }
TIP_AFTER=$( cd "$SCRATCH/wt" && "$BE" sha1:? )
[ "$TIP_BEFORE" != "$TIP_AFTER" ] || {
    echo "FAIL: post did not advance the tip" >&2; exit 1; }
#  No local shard (asserted above) + advanced tip read through the
#  anchor ⇒ the commit landed in the shared store $STORE/proj.
echo "get/46-file-store-worktree: OK"
