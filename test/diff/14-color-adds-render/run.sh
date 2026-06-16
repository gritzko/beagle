#!/bin/sh
#  diff/14-color-adds-render — BRO-004 repro: `be --color diff:` must
#  render the ADDED (`+`/IN) side of a change, AND keep the
#  line-OR-token-by-volume behaviour (small edits stay inline).
#
#  The defect (bro/BRO.c bro_walk_hunk): a modified block whose
#  trailing IN-content line has its terminating `\n` token-LCS-matched
#  onto the deleted (RM) side leaves the IN bytes accumulated past the
#  block's last in-pass-visible `\n`.  The in-pass loop only flushed a
#  row on a visible `\n`, so those trailing IN bytes never got a row
#  and the whole addition vanished from the colour render.  Plain
#  (`HUNKu8sFeedLineBased`) buffers per logical line and was unaffected.
#
#  Two scenarios, each in its own subdir:
#   - big:   one long inserted line replacing several deleted lines,
#            sharing a trailing token suffix → MOD_SPLIT block where the
#            IN line's `\n` matched onto an RM line.  The `+` side
#            (`void*…`) must appear in the RAW colour bytes.
#   - small: a single in-place word edit (`quick`→`speedy`) → MOD_INLINE.
#            Old+new fuse onto ONE row (per-token tinting); stripping
#            ANSI naturally yields "speedyquick" — that is EXPECTED for
#            inline mode, NOT a bug.  We assert it stays a single row
#            (no line-based split) and both sides are present.
#
#  Verify the `+` side in the RAW colour output (grep -a), never by
#  comparing ANSI-stripped colour to --plain.

. "$(dirname "$0")/../../lib/case.sh"

#  run_pair <subdir> <old-fixture> <new-fixture> — seed a two-rev repo
#  and set OLD_SHA / NEW_SHA in caller scope.
run_pair() {
    _sub=$1; _oldf=$2; _newf=$3
    mkdir -p "$SCRATCH/$_sub/.be"; cd "$SCRATCH/$_sub"
    cp "$_oldf" doc.md
    "$BE" put  doc.md            >/dev/null
    "$BE" post -m v1 '?v1'       >/dev/null
    OLD_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')
    sleep 0.02
    cp "$_newf" doc.md
    "$BE" put  doc.md            >/dev/null
    "$BE" post -m v2 '?v2'       >/dev/null
    NEW_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')
}

strip_ansi() { sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g'; }

# ---- big: the added (`+`) side must render in colour ----------------
run_pair big "$CASE/01.big.old" "$CASE/02.big.new"

#  plain diff sanity: both sides present.
"$BE" "diff:doc.md?${OLD_SHA}#${NEW_SHA}" --plain >big.plain 2>/dev/null
if ! grep -qE '^\+.*void\*' big.plain; then
    echo "FAIL: plain diff missing the '+ …void*…' addition" >&2
    cat big.plain >&2; exit 1
fi

#  colour diff: assert on RAW bytes (grep -a).  Use single-word anchors:
#  the renderer wraps each token (and punctuation like `*`/`-`) in its
#  own ANSI span, so a multi-word or punctuated phrase (`void*`,
#  `Bullet-form`) is split across escapes in the raw stream — only
#  whole words survive a raw grep.
"$BE" "diff:doc.md?${OLD_SHA}#${NEW_SHA}" --color >big.color 2>/dev/null
if ! grep -a -q 'void' big.color; then
    echo "FAIL: colour diff dropped the ADDED (+) side (no 'void')" >&2
    strip_ansi <big.color >&2; exit 1
fi
if ! grep -a -q 'wrappers' big.color; then
    echo "FAIL: colour diff dropped the inserted line tail ('wrappers')" >&2
    strip_ansi <big.color >&2; exit 1
fi
#  the removed (-) side must still be there too.
if ! grep -a -q 'Bullet' big.color; then
    echo "FAIL: colour diff dropped the REMOVED (-) side ('Bullet')" >&2
    strip_ansi <big.color >&2; exit 1
fi

# ---- small: inline edit stays a single row --------------------------
cd "$SCRATCH"
run_pair small "$CASE/03.small.old" "$CASE/04.small.new"

"$BE" "diff:doc.md?${OLD_SHA}#${NEW_SHA}" --color >small.color 2>/dev/null
strip_ansi <small.color >small.stripped

#  The shared context word 'brown fox' must appear on exactly ONE row —
#  i.e. the modified line stayed inline (NORMAL pass), not split into a
#  separate `-old` row + `+new` row.
_bf=$(grep -c 'brown fox' small.stripped || true)
if [ "$_bf" -ne 1 ]; then
    echo "FAIL: inline edit split into $_bf rows (want 1 — inline preserved)" >&2
    cat small.stripped >&2; exit 1
fi
#  Both the new (IN) and old (RM) word must be present in the raw bytes.
if ! grep -a -q 'speedy' small.color; then
    echo "FAIL: inline colour diff dropped the new word 'speedy'" >&2
    cat small.stripped >&2; exit 1
fi
if ! grep -a -q 'quick' small.color; then
    echo "FAIL: inline colour diff dropped the old word 'quick'" >&2
    cat small.stripped >&2; exit 1
fi
