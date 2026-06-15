#!/bin/sh
#  diff/11-file-scope-full — DIFF-003.  A file-scoped `diff:<file>`
#  (the `U` hunk nav-URI target) renders the WHOLE file with the
#  changed lines highlighted in place, like `git diff --color` over the
#  full file / a review pane.  A tree/dir-scoped `diff:` (no single-file
#  path) keeps emitting changed-hunks-only.
#
#  Setup: a 30-line file, committed as baseline, then line 20 changed.
#  An unchanged line FAR from the change (`line 01 original`, 19 lines
#  away — well past the 3-line context window WEAVEEmitDiff uses) is the
#  tell:
#    - file-scope MUST emit it (whole-file view), in Plain/Color/TLV.
#    - tree-scope MUST NOT emit it (hunks-only regression guard).
#  The changed line (`line 20 CHANGED HERE`) must appear in every mode.

. "$(dirname "$0")/../../lib/case.sh"

#  --- baseline: 30 identical lines -----------------------------------
i=1
: > big.c
while [ "$i" -le 30 ]; do
    printf 'line %02d original\n' "$i" >> big.c
    i=$((i + 1))
done
"$BE" put  big.c            >/dev/null
"$BE" post -m base big.c    >/dev/null

#  --- working-tree edit: change ONLY line 20 -------------------------
i=1
: > big.c
while [ "$i" -le 30 ]; do
    if [ "$i" -eq 20 ]; then
        printf 'line 20 CHANGED HERE\n' >> big.c
    else
        printf 'line %02d original\n' "$i" >> big.c
    fi
    i=$((i + 1))
done

FAR='line 01 original'        # 19 lines above the change — never in a hunk window
CHG='line 20 CHANGED HERE'    # the edited line

#  --- (a) file-scope PLAIN: whole file ------------------------------
"$BE" 'diff:big.c#L20' >file.plain.out 2>file.plain.err
empty file.plain.err
if ! grep -qF "$FAR" file.plain.out; then
    echo "FAIL: file-scope plain diff missing far unchanged line '$FAR'" >&2
    cat file.plain.out >&2; exit 1
fi
if ! grep -qF "$CHG" file.plain.out; then
    echo "FAIL: file-scope plain diff missing changed line '$CHG'" >&2
    cat file.plain.out >&2; exit 1
fi

#  --- (b) file-scope COLOR (--ansi): whole file, escapes stripped ----
"$BE" 'diff:big.c#L20' --ansi >file.color.out 2>file.color.err
sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g' file.color.out >file.color.stripped
if ! grep -qF "$FAR" file.color.stripped; then
    echo "FAIL: file-scope color diff missing far unchanged line '$FAR'" >&2
    cat file.color.stripped >&2; exit 1
fi
if ! grep -qF "$CHG" file.color.stripped; then
    echo "FAIL: file-scope color diff missing changed line '$CHG'" >&2
    cat file.color.stripped >&2; exit 1
fi

#  --- (c) file-scope TLV: whole file in the wire stream -------------
"$BE" 'diff:big.c#L20' --tlv >file.tlv.out 2>file.tlv.err
if ! grep -qF "$FAR" file.tlv.out; then
    echo "FAIL: file-scope TLV diff missing far unchanged line '$FAR'" >&2
    exit 1
fi
if ! grep -qF "$CHG" file.tlv.out; then
    echo "FAIL: file-scope TLV diff missing changed line '$CHG'" >&2
    exit 1
fi

#  --- (d) REGRESSION: tree-scope `diff:` stays hunks-only -----------
#  Whole-tree diff (no path) — the same file's change appears as a 3-line
#  context hunk, so the far line must be ABSENT while the change shows.
"$BE" 'diff:' >tree.plain.out 2>tree.plain.err
empty tree.plain.err
if grep -qF "$FAR" tree.plain.out; then
    echo "FAIL: tree-scope diff leaked the far unchanged line '$FAR'" \
         "(should be hunks-only)" >&2
    cat tree.plain.out >&2; exit 1
fi
if ! grep -qF "$CHG" tree.plain.out; then
    echo "FAIL: tree-scope diff missing changed line '$CHG'" >&2
    cat tree.plain.out >&2; exit 1
fi

echo "OK 11-file-scope-full"
