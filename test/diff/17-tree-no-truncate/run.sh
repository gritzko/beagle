#!/bin/sh
#  diff/17-tree-no-truncate — DIFF-008 regression.
#
#  `graf_diff_tree_refs_inner` used to call `GRAFDiff2Layer` per file as
#  a BARE call: the return was ignored and the weave's internal BASS
#  scratch (a 16 MB outtext carve + the WEAVE_DECODE acquires) was NEVER
#  rewound between files.  Scratch accumulated across the file list until
#  `a_carve` returned NOROOM, after which every remaining file's diff
#  SILENTLY failed and was dropped — the tail of a `diff:?from..to` tree
#  diff was truncated with exit 0.  BLAME-006b's binary-skip masked one
#  trigger (a 9.5 MB blob) but not the root; a large-enough TEXT set
#  still truncated.
#
#  The per-file weave carves a FIXED ~17 MB scratch block (a 16 MB
#  `outtext` carve plus side/uri buffers) regardless of blob size, so the
#  accumulation NOROOMs after ~58 files independent of file content.  On
#  the unfixed tree this case's diff stopped around file ~56 of N.  We
#  seed N=80 tiny distinct text files (well past that threshold), modify
#  EVERY one, and assert that the `diff:?v1..v2` tree diff renders ALL N
#  files — in particular the LAST file's `--- a/<last>` header is present
#  and its hunk body is complete — proving scratch is now bounded to one
#  file per iteration.  Files are tiny so the case is fast (well under the
#  ctest per-test budget); the trigger is the fixed-size per-file carve,
#  not the byte count.

. "$(dirname "$0")/../../lib/case.sh"

N=80    #  comfortably past the ~58-file pre-fix truncation threshold

#  seed_rev <tag> <token> — write N tiny distinct files stamped with
#  <token>, put + post them under the ref label <tag>.
seed_rev() {
    _tag=$1; _tok=$2
    i=0
    while [ "$i" -lt "$N" ]; do
        _f=$(printf 'f%03d.txt' "$i")
        printf 'file %03d alpha %s payload\nfile %03d beta %s payload\n' \
            "$i" "$_tok" "$i" "$_tok" > "$_f"
        i=$((i + 1))
    done
    "$BE" put f*.txt        >/dev/null
    "$BE" post -m "$_tag" "?$_tag" >/dev/null
}

sleep 0.02; seed_rev v1 ORIGINAL
sleep 0.02; seed_rev v2 MODIFIED

"$BE" "diff:?v1..v2" >tree.got.out 2>tree.got.err

#  Empty stderr — no silent NOROOM warnings leaking through.
empty tree.got.err

FAIL=0

#  Every file 0..N-1 must have its own `--- a/<file>` diff header.
HDRS=$(grep -c '^--- a/f' tree.got.out 2>/dev/null || echo 0)
if [ "$HDRS" != "$N" ]; then
    echo "FAIL: $HDRS / $N file headers present (tail truncated)" >&2
    echo "  highest header: $(grep '^--- a/f' tree.got.out | tail -1)" >&2
    FAIL=$((FAIL + 1))
fi

#  The LAST file (the canary BLAME-006b watched) must be present and its
#  hunk body complete — both an ORIGINAL `-` and a MODIFIED `+` line.
LAST=$(printf 'f%03d.txt' "$((N - 1))")
LASTID=$(printf '%03d' "$((N - 1))")
if ! grep -q "^--- a/$LAST" tree.got.out; then
    echo "FAIL: last file $LAST missing from tree diff" >&2
    FAIL=$((FAIL + 1))
fi
if ! grep -q "^+file $LASTID .*MODIFIED payload" tree.got.out; then
    echo "FAIL: $LAST MODIFIED (+) body missing — hunk truncated" >&2
    FAIL=$((FAIL + 1))
fi
if ! grep -q "^-file $LASTID .*ORIGINAL payload" tree.got.out; then
    echo "FAIL: $LAST ORIGINAL (-) body missing — hunk truncated" >&2
    FAIL=$((FAIL + 1))
fi

if [ "$FAIL" != 0 ]; then
    echo "=== last 12 lines of tree diff ===" >&2
    tail -12 tree.got.out >&2
    exit 1
fi
