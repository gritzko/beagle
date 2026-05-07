#!/bin/sh
#  07-untracked-still-dirty — `be patch` dirty-scan classification.
#
#  The dirty-scan in `SNIFFAtScanDirty` (called by PATCH's
#  `refuse_if_dirty`) currently asks one question per file: "is the
#  mtime in the stamp-set?".  Anything else is reported as dirty.
#  That over-reports four very different conditions:
#
#     case               setup                         expected
#     ─────────────────  ────────────────────────────  ────────
#     tracked-changed    rewrite tracked file bytes    DIRTY
#     touched-unchanged  `touch` tracked file (mtime
#                         shifts, bytes stay)          not dirty
#     untracked          new file at wt root, no put   not dirty
#     ignored            new file matching .gitignore  not dirty
#
#  This test sets up all four conditions in one wt and runs
#  `be patch ?./feat` against a divergent feat branch.  Per the
#  new spec (VERBS.md §PATCH "Weave merge into dirty wt"), PATCH
#  no longer refuses on dirty wt — it weaves and reports per-file
#  status.  edit.txt must appear with `patch dirty edit.txt`;
#  bump.txt / new.txt / scratch.tmp must NOT.

. "$(dirname "$0")/../../lib/case.sh"

LOGS=$(cd .. && pwd)/logs-$NAME
rm -rf "$LOGS"; mkdir -p "$LOGS"

# ------------------------------------------------------------------
# 1. Baseline on trunk.  `*.tmp` is gitignored from the start so the
#    "ignored" case can attach.
# ------------------------------------------------------------------
echo "*.tmp"  > .gitignore
echo "keep"   > keep.txt
echo "bump"   > bump.txt
echo "edit"   > edit.txt
"$BE" put .gitignore keep.txt bump.txt edit.txt > /dev/null
"$BE" post 'baseline'                           > /dev/null

# ------------------------------------------------------------------
# 2. Fork ?feat with a divergent edit on a different file (keep.txt)
#    so PATCH has something real to absorb without colliding with
#    the dirty scenarios we'll set up below.
# ------------------------------------------------------------------
"$BE" put '?./feat'      > /dev/null
"$BE" get '?feat'        > /dev/null
sleep 0.02
echo "feat side" > keep.txt
"$BE" put keep.txt       > /dev/null
"$BE" post 'feat msg'    > /dev/null

"$BE" get '?..'          > /dev/null

# ------------------------------------------------------------------
# 3. Build all four dirty-class scenarios in the wt.
# ------------------------------------------------------------------
#  touched-unchanged: mtime moves, content stays.
sleep 0.02
touch bump.txt

#  tracked-changed: real local edit.
sleep 0.02
echo "USER EDIT" > edit.txt

#  untracked: never staged, no .gitignore match.
echo "fresh" > new.txt

#  ignored: matches the *.tmp pattern in .gitignore.
echo "scratch" > scratch.tmp

# ------------------------------------------------------------------
# 4. THE TEST: be patch ?./feat.  edit.txt must be reported as dirty;
#    others must NOT appear in the status report.  PATCH succeeds
#    (no more refuse-on-dirty per the new spec).
# ------------------------------------------------------------------
"$BE" patch '?./feat' > "$LOGS/patch.out" 2> "$LOGS/patch.err"

#  edit.txt must appear in a `patch dirty edit.txt` row.
grep -E '^patch[[:space:]]+dirty[[:space:]]+(\./)?edit\.txt$' "$LOGS/patch.err" \
    || { echo "FAIL: edit.txt not reported as dirty" >&2
         cat "$LOGS/patch.err" >&2
         exit 1; }

#  bump.txt / new.txt / scratch.tmp must NOT be flagged.
for forbidden in 'bump\.txt' 'new\.txt' 'scratch\.tmp'; do
    if grep -E "patch[[:space:]]+(dirty|merged|conflict|applied)[[:space:]]+(\./)?$forbidden" "$LOGS/patch.err"; then
        echo "FAIL: $forbidden was flagged (should not be):" >&2
        cat "$LOGS/patch.err" >&2
        exit 1
    fi
done

rm -rf "$LOGS"
