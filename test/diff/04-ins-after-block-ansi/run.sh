#!/bin/sh
#  diff/04-ins-after-block-ansi — repro for the ANSI renderer dropping
#  the inserted line.
#
#  Setup: OLD has a 3-field struct, NEW collapses two fields into one
#  longer line that ends with a `// comment`.  Token-level LCS matches
#  the trailing `\n` of the inserted NEW line as EQ with the trailing
#  `\n` of one of the deleted OLD lines, so the merged-text stream has
#  IN content with no IN-side `\n` to terminate it.
#
#  Plaintext (HUNKu8sFeedLineBased) buffers per logical line and
#  recovers — the `+    u8cs path; ...` line is emitted.
#
#  ANSI (bro/BRO.c BROPlain colour mode) walks `\n`-segments and uses
#  the boundary `\n`'s side to decide row visibility per pass.  When
#  the IN content's `\n` is matched as EQ on a different segment,
#  `bro_walk_hunk`'s in-pass loop never emits a row for the IN bytes:
#  the addition vanishes from the output entirely.
#
#  This test asserts the ANSI render contains the inserted line text,
#  even after stripping ANSI escape sequences.

. "$(dirname "$0")/../../lib/case.sh"

#  --- v1 (OLD) -------------------------------------------------------
sleep 0.02; cp "$CASE/01.foo.c.old" foo.c
"$BE" put  foo.c                  >/dev/null
"$BE" post -m v1 '?v1'       >/dev/null
OLD_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')

#  --- v2 (NEW) -------------------------------------------------------
sleep 0.02; cp "$CASE/02.foo.c.new" foo.c
"$BE" put  foo.c                  >/dev/null
"$BE" post -m v2 '?v2'       >/dev/null
NEW_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')

#  --- diff plaintext: must include the addition ----------------------
"$BE" "diff:foo.c?${OLD_SHA}#${NEW_SHA}" \
    >diff.plain.out 2>diff.plain.err

if ! grep -qE '^\+.*u8cs path' diff.plain.out; then
    echo "FAIL: plaintext diff missing '+    u8cs path; ...' line" >&2
    cat diff.plain.out >&2
    exit 1
fi

#  --- diff ANSI: same line must appear once colour escapes stripped --
"$BE" "diff:foo.c?${OLD_SHA}#${NEW_SHA}" --ansi \
    >diff.ansi.out 2>diff.ansi.err

#  Strip CSI sequences (ESC `[` ... letter) so we test the visible text.
sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g' diff.ansi.out >diff.ansi.stripped

if ! grep -qE 'u8cs path;.*borrowed.*arena' diff.ansi.stripped; then
    echo "FAIL: ANSI diff dropped the inserted line" >&2
    echo "=== stripped ANSI output ===" >&2
    cat diff.ansi.stripped >&2
    exit 1
fi
