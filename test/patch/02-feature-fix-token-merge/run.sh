#!/bin/sh
#
#  patch/02-feature-fix-token-merge
#
#  Branch shape (cur sits on `feature` after step 7):
#
#      trunk ─ baseline
#         └── feature ─ feat-one (ALPHA) ─ feat-two (DELTA)     ←── cur
#                  └── fix      ─ fix-one (GAMMA)
#
#  All three edits sit on the *same line* of `lib.txt`, each
#  rewriting a different word.  This is the stress test for a
#  token-level merge: a line-level merge sees the same line modified
#  by both sides and conflicts; the JOIN-based token merge sees three
#  disjoint replacements within one line and folds them together.
#
#  Once feature diverges (feat-two lands after fix forks off), the
#  three input trees PATCH sees on `lib.txt` are:
#
#      base   (feature.feat-one)  the ALPHA beta gamma delta omega line
#      ours   (feature.feat-two)  the ALPHA beta gamma DELTA omega line
#      theirs (fix.fix-one)       the ALPHA beta GAMMA delta omega line
#      want   (token-merge)       the ALPHA beta GAMMA DELTA omega line
#
#  Feature also has a wt change on `notes.txt` (a file `fix` never
#  touches) staged via `be put` before the patch.  The wt edit must
#  survive the merge intact, since PATCH only rewrites the file set
#  it computes from the {base, ours, theirs} triple.

. "$(dirname "$0")/../../lib/case.sh"

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

# --- 1. trunk: baseline ---------------------------------------------
cp "$CASE/01.lib.baseline.txt"   lib.txt
cp "$CASE/02.notes.baseline.txt" notes.txt
"$BE" put lib.txt notes.txt >/dev/null 2>&1
"$BE" post 'baseline msg'         >/dev/null 2>&1

# --- 2. fork feature off trunk and switch to it ---------------------
"$BE" put '?./feature' >/dev/null 2>&1
"$BE" get  '?feature'   >/dev/null 2>&1

# --- 3. feat-one on feature: ALPHA ----------------------------------
cp "$CASE/03.lib.feat1.txt" lib.txt
"$BE" put lib.txt   >/dev/null 2>&1
"$BE" post 'feat-one msg' >/dev/null 2>&1

# --- 4. fork fix off feature ----------------------------------------
"$BE" put '?./fix' >/dev/null 2>&1

# --- 5. switch to fix, fix-one: GAMMA -------------------------------
"$BE" get '?feature/fix' >/dev/null 2>&1
cp "$CASE/04.lib.fix1.txt" lib.txt
"$BE" put lib.txt  >/dev/null 2>&1
"$BE" post 'fix-one msg' >/dev/null 2>&1

# --- 6. back to feature, feat-two: DELTA (divergence) ---------------
"$BE" get  '?feature' >/dev/null 2>&1
cp "$CASE/05.lib.feat2.txt" lib.txt
"$BE" put lib.txt   >/dev/null 2>&1
"$BE" post 'feat-two msg' >/dev/null 2>&1

# --- 7. feature wt edit on a file fix never touched -----------------
#  Stage with `be put` so PATCH's dirty-file gate accepts.  The bytes
#  on disk don't change — only the sniff stamp moves into the new put
#  row, marking the file as a known-clean wt edit.
cp "$CASE/06.notes.wt.txt" notes.txt
"$BE" put notes.txt >/dev/null 2>&1

# --- 8. patch fix into feature's wt — the merge under test ----------
"$BE" patch '?./fix' >"$OUT/08.patch.out" 2>"$OUT/08.patch.err"

# --- 9. assert token-level merge on lib.txt + untouched wt edit -----
match "$CASE/08.lib.want.txt"   lib.txt
match "$CASE/09.notes.want.txt" notes.txt
