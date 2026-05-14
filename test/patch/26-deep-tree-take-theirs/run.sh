#!/bin/sh
#  26-deep-tree-take-theirs — regression for the file-zeroing bug
#  where PATCH wrote 0-byte blobs for files under sub-sub-directories.
#
#  Root cause: `patch_walk`'s recursive call passed the per-recursion
#  TREE shas (`fork/our/thr`) as the sha source for `fetch_blob` /
#  `fetch_tree`.  Both go through graf's `get_resolve_qref`, which
#  insists on COMMIT-typed objects and rejects trees with `GETFAIL` —
#  so beneath the root level, every blob fetch returned an empty
#  buffer, and `write_blob` silently created a 0-byte file.  The
#  `applied`/`take-theirs` status row covered up the corruption.
#
#  This test seeds a tree deep enough to force recursion past the
#  root level and asserts that the patched bytes match the source
#  byte-for-byte (i.e. the file is NOT a 0-byte stub).
#
#  Topology:
#       T0 ── T1            ← cur (trunk):
#         \                   T1 = adds an unrelated trunk file
#          F1                ← ?feat:
#                              F1 = (a) edits  src/a/b/deep.txt
#                                   (b) adds   src/a/b/new.txt
#                              both files live two levels deep.
#
#  After `be patch ?feat`:
#    * src/a/b/deep.txt must equal F1's bytes (NOT 0 bytes — the
#      take-theirs arm hit fetch_blob at the deep recursion);
#    * src/a/b/new.txt  must equal F1's bytes (NOT 0 bytes — the
#      add-theirs arm hit fetch_blob at the deep recursion).
#
#  Repro recipe outside the suite: two divergent commits, one branch
#  modifies/adds a file two directories deep, `be patch` the branch
#  from the other side, inspect the file sizes.  Pre-fix every such
#  file came out 0 bytes; per-file `applied`/`merged` status hid the
#  corruption from the user until a build broke later.

. "$(dirname "$0")/../../lib/branches.sh"

mkdir -p src/a/b

cp "$CASE/01.deep.t0.txt" src/a/b/deep.txt
echo "trunk root file"  > root.txt
"$BE" put root.txt src/a/b/deep.txt >/dev/null
"$BE" post 't0 baseline' >/dev/null
T0=$(head_hex)

#  Fork ?feat at T0 (cur stays on trunk).
"$BE" put '?./feat' >/dev/null

#  Trunk T1: edit root.txt only (so trunk's branch tip changes and
#  patch has a non-trivial merge base to walk).
sleep 0.02
echo "trunk root edited" > root.txt
"$BE" put root.txt >/dev/null
"$BE" post 't1 edit root' >/dev/null
T1=$(head_hex)

#  Switch to feat at T0; F1 edits the deep file AND adds a new
#  file in the same deep subtree.
"$BE" get '?feat' >/dev/null
[ "$(head_hex)" = "$T0" ] || fail "feat should fork at T0; head=$(head_hex)"

sleep 0.02
cp "$CASE/02.deep.f1.txt" src/a/b/deep.txt
cp "$CASE/03.new.f1.txt"  src/a/b/new.txt
"$BE" put src/a/b/deep.txt src/a/b/new.txt >/dev/null
"$BE" post 'f1 edit deep add new' >/dev/null
F1=$(head_hex)

#  Back to trunk at T1.
"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T1" ] || fail "trunk should be at T1; head=$(head_hex)"

#  THE ACTION: squash ?feat into trunk.  The deep files exercise the
#  recursive descent in patch_walk → fetch_blob via the take-theirs
#  arm (deep.txt) and the add-theirs arm (new.txt).
"$BE" patch '?feat' >/dev/null 2>"$ETMP/p.err" \
    || fail "be patch ?feat failed: $(cat $ETMP/p.err)"

#  Per-file status rows must appear.
grep -E '^patch[[:space:]]+applied[[:space:]]+(\./)?src/a/b/deep\.txt$' "$ETMP/p.err" \
    || fail "expected 'patch applied src/a/b/deep.txt'; got: $(cat $ETMP/p.err)"
grep -E '^patch[[:space:]]+applied[[:space:]]+(\./)?src/a/b/new\.txt$' "$ETMP/p.err" \
    || fail "expected 'patch applied src/a/b/new.txt'; got: $(cat $ETMP/p.err)"

#  THE REGRESSION ASSERT: both files must be non-empty AND match the
#  source bytes.  Pre-fix the patch wrote 0-byte stubs and `match`
#  caught only the diff (which showed the file as empty).
[ -s src/a/b/deep.txt ] \
    || fail "src/a/b/deep.txt is 0 bytes after patch — file-zeroing regression"
[ -s src/a/b/new.txt ] \
    || fail "src/a/b/new.txt is 0 bytes after patch — file-zeroing regression"
match "$CASE/02.deep.f1.txt" src/a/b/deep.txt
match "$CASE/03.new.f1.txt"  src/a/b/new.txt

#  POST must commit the bytes (not a 0-byte tree).  Read the new
#  commit back via keeper to confirm.
"$BE" post '#patch deep files' >/dev/null \
    || fail "be post failed after patch"
P=$(head_hex)

#  Round-trip: read the file back via the keeper-resolved path.  If
#  the committed blob is 0 bytes, this would print empty / mismatch.
ROUND=$("$BE" blob:src/a/b/deep.txt?$P 2>/dev/null)
[ -n "$ROUND" ] \
    || fail "round-trip of src/a/b/deep.txt yielded empty bytes — post wrote 0-byte blob"

note "deep-tree patch OK: take-theirs and add-theirs both wrote real bytes"
echo "=== patch/26-deep-tree-take-theirs: OK ==="
