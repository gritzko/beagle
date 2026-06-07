#!/bin/sh
#  head/21-sub-dirty-reported — `be head ?` must change its report when
#  a submodule's working tree goes dirty (SUBS-007).
#
#  Submodules.mkd line 20 promises `be head` recurses pre-order
#  "reporting ahead/behind AND dirty state per sub".  The real contract:
#  the per-sub HEAD section surfaces the sub's working-tree status, so a
#  dirtied sub file CHANGES the `be head ?` output.  head/04 only greps
#  the combined output for `core.c` (which already appears in HEAD's
#  committed ahead/behind file list regardless of the edit) — a false
#  positive.  This case pins the diff: clean `be head ?` and dirty
#  `be head ?` must differ, and the difference must mention the dirty
#  path.
#
#  Setup (from submodules.sh): parent.git pins vendor/sub; clone mounts
#  vendor/sub.
#
#  Steps:
#    1. Clone parent — sub mounts at vendor/sub.
#    2. `be head ?` with a clean tree            -> clean.out
#    3. Append a line to vendor/sub/core.c (sub goes dirty).
#    4. `be head ?` again                        -> dirty.out
#    5. clean.out and dirty.out MUST differ, and the new bytes name the
#       sub's dirty path (a `mod`/`dirty` line for vendor/sub/core.c).
#    6. Outer wt is clean: main.c / util.c are NOT flagged dirty.
#    7. Read-only invariant: the sub's `.be` wtlog line count is
#       unchanged across the two HEAD calls.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
[ -f vendor/sub/core.c ] || fail "fixture: sub not mounted"

sub_lines_before=$(wc -l < vendor/sub/.be)

# --- 2. Clean `be head ?`. -------------------------------------------
"$BE" head '?' >clean.out 2>clean.err
rc=$?
[ "$rc" = 0 ] || fail "be head (clean) exited $rc; stderr:
$(cat clean.err)"

# --- 3. Dirty exactly one file in the sub; outer untouched. ----------
echo '/* dirty edit for head/21 */' >> vendor/sub/core.c

# --- 4. Dirty `be head ?`. -------------------------------------------
"$BE" head '?' >dirty.out 2>dirty.err
rc=$?
[ "$rc" = 0 ] || fail "be head (dirty) exited $rc; stderr:
$(cat dirty.err)"

# --- 5. The two reports MUST differ — that is the whole contract. ----
if cmp -s clean.out dirty.out; then
    fail "be head ? output is byte-identical clean vs dirty; the sub's
dirty state is not wired into HEAD's per-sub report.
--- clean ---
$(cat clean.out)
--- dirty ---
$(cat dirty.out)"
fi

# The new lines (in dirty.out, not in clean.out) must name the dirty
# sub path with a mod/dirty marker.
added=$(comm -13 "clean.out" "dirty.out" 2>/dev/null || \
        diff clean.out dirty.out | grep '^>')
echo "$added" | grep -qE 'vendor/sub.*core\.c|core\.c' \
    || fail "dirty HEAD report does not name the sub's dirty path; added:
$added"
echo "$added" | grep -qiE 'mod|dirty' \
    || fail "dirty HEAD report has no mod/dirty marker; added:
$added"

# --- 6. Outer wt stays clean: main.c / util.c not flagged dirty. -----
combined=$(cat dirty.out dirty.err)
echo "$combined" | grep -qiE '\bmain\.c\b.*(dirty|mod)|\butil\.c\b.*(dirty|mod)' \
    && fail "outer falsely reported dirty:
$combined"

# --- 7. Read-only invariant: sub wtlog did not grow. -----------------
sub_lines_after=$(wc -l < vendor/sub/.be)
[ "$sub_lines_after" = "$sub_lines_before" ] \
    || fail "sub .be (wtlog file) grew: $sub_lines_before -> $sub_lines_after"

note "head/21-sub-dirty-reported: dirtied sub file changed be head ? output"
