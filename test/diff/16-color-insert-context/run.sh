#!/bin/sh
#  diff/16-color-insert-context — BRO-004 Part 3: a single-line INSERT
#  adjacent to a structurally-similar context line must NOT duplicate
#  that context line in the COLOUR `be --color diff:` render.
#
#  Repro (mirrors the ABC.md edit): a new bullet `- **Raw bytes** …` is
#  inserted between the context bullets `- **C-string-literal slice — …`
#  and `- **Iteration — `$for` … …`.  All three share the `- **X — `…`
#  shape, so token-LCS marks the whole inserted line — INCLUDING its
#  terminating `\n` — as IN, and keeps the `**Iteration**` line (and its
#  `\n`) as EQ context.  `--plain` renders one `+` line, `**Iteration**`
#  once as context.
#
#  The defect (bro/BRO.c bro_walk_hunk block-end scan): the inserted
#  line ends in an IN-side `\n`, which the rm-pass cannot see; the Part-2
#  fix treated ANY non-EQ boundary `\n` as a row continuation and pulled
#  the following EQ `**Iteration**` line into the change block.  It then
#  emitted that eq line TWICE — once on the rm side (spurious removed
#  copy, bg 48;5;224) and once on the in side (the duplicate, bg
#  48;5;194) — sandwiching the added line.  Fix: a non-EQ boundary `\n`
#  continues the row only when its side is HIDDEN from the pass that
#  renders the prior line's content (an IN `\n` continues a line with RM
#  bytes, an RM `\n` continues a line with IN bytes).  A self-terminating
#  pure insert (IN line + IN `\n`) does NOT continue, so the eq line is a
#  genuine context boundary and renders ONCE.
#
#  Asserted on the RAW colour bytes (NEVER ANSI-stripped — stripping
#  fuses inline tokens and hides the bug; see bro-color-diff-modes): the
#  `**Iteration**` bullet appears EXACTLY ONCE and never carries a
#  removed background (48;5;224 / 48;5;217).

. "$(dirname "$0")/../../lib/case.sh"

mkdir -p "$SCRATCH/ins/.be"; cd "$SCRATCH/ins"
cp "$CASE/01.old" doc.md
"$BE" put  doc.md      >/dev/null
"$BE" post -m v1 '?v1' >/dev/null
OLD_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')
sleep 0.02
cp "$CASE/02.new" doc.md
"$BE" put  doc.md      >/dev/null
"$BE" post -m v2 '?v2' >/dev/null
NEW_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')

"$BE" "diff:doc.md?${OLD_SHA}#${NEW_SHA}" --color >ins.color 2>/dev/null

#  Sanity: the colour render carries the inserted line (added side).
if ! grep -a -q 'Raw bytes' ins.color; then
    echo "FAIL: colour diff dropped the inserted line ('Raw bytes')" >&2
    cat -v ins.color >&2; exit 1
fi

#  The unchanged 'Iteration' bullet must appear EXACTLY ONCE.  The bug
#  rendered it twice (removed + inserted) around the added line.
_itercnt=$(grep -a -c 'Iteration' ins.color)
if [ "$_itercnt" -ne 1 ]; then
    echo "FAIL: 'Iteration' context line rendered $_itercnt times (want 1)" >&2
    cat -v ins.color >&2; exit 1
fi

#  It must NOT carry a removed background (224 / 217 = removed shades).
#  Find the Iteration row and assert its raw bytes hold no removed bg.
if grep -a 'Iteration' ins.color | grep -aqE '48;5;(224|217)'; then
    echo "FAIL: 'Iteration' context line carries a removed bg (48;5;224/217)" >&2
    cat -v ins.color >&2; exit 1
fi

exit 0
