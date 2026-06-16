#!/bin/sh
#  diff/15-color-emdash-coherent — BRO-004 Part 2: the colour `diff:`
#  FIRST hunk must render COHERENTLY when a large multi-line region is
#  replaced by one long line that shares a trailing suffix containing an
#  em-dash (`—`).
#
#  Repro (mirrors the ABC.md abstract edit): the old text wraps the
#  shared tail `the canonical docs — read them for full coverage:` onto
#  its own line; the new text folds everything onto one long line ending
#  in that same tail.  Token-level LCS makes the shared tail EQ and lands
#  the inserted line's terminating `\n` on the RM side.
#
#  The defect (bro/BRO.c bro_walk_hunk block-boundary scan): the block
#  ended at a NON-eq (`\n` on the in/rm side) boundary, so the shared eq
#  tail was (a) swallowed into the in-pass row via a hidden-`\n` overrun
#  AND (b) re-emitted as a standalone NORMAL row — the eq line rendered
#  twice, interleaved old/new, with the rm side missing its tail.  Fix:
#  extend the block across a non-eq boundary `\n` so the eq tail renders
#  once per surviving side, matching `--plain`.
#
#  Coherence is asserted on the RAW colour bytes / per-row structure
#  (NOT ANSI-stripped-vs-plain, which fuses inline edits — see
#  bro-color-diff-modes): the inserted line is ONE contiguous row
#  (`Any use … coverage:`), and the standalone shared-tail row precedes
#  it (old side reconstructed first), never after (interleaved).

. "$(dirname "$0")/../../lib/case.sh"

mkdir -p "$SCRATCH/em/.be"; cd "$SCRATCH/em"
cp "$CASE/01.old" doc.md
"$BE" put  doc.md      >/dev/null
"$BE" post -m v1 '?v1' >/dev/null
OLD_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')
sleep 0.02
cp "$CASE/02.new" doc.md
"$BE" put  doc.md      >/dev/null
"$BE" post -m v2 '?v2' >/dev/null
NEW_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')

strip_ansi() { sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g'; }

"$BE" "diff:doc.md?${OLD_SHA}#${NEW_SHA}" --color >em.color 2>/dev/null
strip_ansi <em.color >em.rows

#  Sanity: the colour render must carry both sides (raw bytes).
if ! grep -a -q 'void' em.color; then
    echo "FAIL: colour diff dropped the ADDED (+) side (no 'void')" >&2
    cat em.rows >&2; exit 1
fi
if ! grep -a -q 'Bullet' em.color; then
    echo "FAIL: colour diff dropped the REMOVED (-) side ('Bullet')" >&2
    cat em.rows >&2; exit 1
fi

#  The inserted line must be ONE contiguous row: a single row holds both
#  'Any use' (its head) and 'coverage:' (the shared em-dash tail).  The
#  buggy clip split it so 'Any use' and the tail landed on separate rows.
_inrow=$(grep -n 'Any use' em.rows | head -1 | cut -d: -f1)
if [ -z "$_inrow" ]; then
    echo "FAIL: inserted line ('Any use …') missing from colour render" >&2
    cat em.rows >&2; exit 1
fi
if ! sed -n "${_inrow}p" em.rows | grep -q 'coverage:'; then
    echo "FAIL: inserted line split mid-line (row $_inrow lacks 'coverage:' tail)" >&2
    cat em.rows >&2; exit 1
fi

#  The standalone shared-tail row (a row that is JUST 'the canonical docs
#  …', the old side's wrapped continuation) must come BEFORE the inserted
#  line, not interleaved AFTER it.  The bug emitted it after the new line
#  (old/new interleaved); coherent render reconstructs the old side first.
_tailrow=$(grep -nE '^the canonical docs .* read them for full coverage:[[:space:]]*$' \
           em.rows | head -1 | cut -d: -f1)
if [ -z "$_tailrow" ]; then
    echo "FAIL: shared em-dash tail line missing as a standalone (old-side) row" >&2
    cat em.rows >&2; exit 1
fi
if [ "$_tailrow" -gt "$_inrow" ]; then
    echo "FAIL: old/new interleaved — standalone tail (row $_tailrow) after the inserted line (row $_inrow)" >&2
    cat em.rows >&2; exit 1
fi

exit 0
