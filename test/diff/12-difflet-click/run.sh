#!/bin/sh
#  diff/12-difflet-click — DIFF-004.  A diff hunk's per-hunk `U` click
#  target must carry the version range in the QUERY as `?<from>..<to>`,
#  with the fragment kept purely as the `#L<n>` line anchor, so clicking
#  it re-opens the file-scoped whole-file range diff (DIFF-003) — NOT
#  the empty-query `diff:<path>#L` form, which runs wt-vs-base and is
#  empty for a committed file ("be: no results").
#
#  Setup: a 30-line file committed as baseline, then line 20 changed and
#  committed again — two real commits FROM..TO.  A far unchanged line
#  (`line 01 original`, 19 lines from the change, well past the 3-line
#  context window) is the whole-file tell.  Assertions:
#    (a) `diff:<path>?<from>..<to>#L<n>` resolves to the file's RANGE
#        diff — whole file (far unchanged line) + the change — in
#        Plain and TLV (range comes from the query, not wt-vs-base).
#    (b) a commit-show diff's per-file hunk nav `U` URI is the
#        `?<from>..<to>#L` form, NOT the empty-query `diff:<path>#L`.
#    (c) the bare `?..` parent-branch relative ref is NOT mis-split into
#        an empty from..to range (the split guards on non-empty halves).

. "$(dirname "$0")/../../lib/case.sh"

#  --- baseline: 30 lines, committed ----------------------------------
i=1
: > big.c
while [ "$i" -le 30 ]; do
    printf 'line %02d original\n' "$i" >> big.c
    i=$((i + 1))
done
"$BE" put  big.c          >/dev/null
"$BE" post -m base big.c  >/dev/null

#  --- second commit: change ONLY line 20 ----------------------------
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
"$BE" put  big.c            >/dev/null
"$BE" post -m change big.c  >/dev/null

FAR='line 01 original'        # 19 lines above the change — only a whole-file view shows it
CHG='line 20 CHANGED HERE'    # the edited line

#  Resolve the two commit shas (TO = tip, FROM = its parent) from the
#  file's log; the order is tip-first.
FROM=$("$BE" 'log:big.c' 2>/dev/null | grep -oE '^[0-9a-f]{8}' | sed -n '2p')
TO=$(  "$BE" 'log:big.c' 2>/dev/null | grep -oE '^[0-9a-f]{8}' | sed -n '1p')
[ -n "$FROM" ] && [ -n "$TO" ] || {
    echo "FAIL: could not resolve FROM/TO commit shas" >&2
    "$BE" 'log:big.c' >&2; exit 1
}

#  === (a) range form `diff:<path>?<from>..<to>#L<n>` → whole-file ===
"$BE" "diff:big.c?$FROM..$TO#L20" >range.plain.out 2>range.plain.err
empty range.plain.err
if ! grep -qF "$FAR" range.plain.out; then
    echo "FAIL: range diff missing far unchanged line '$FAR' (not whole-file)" >&2
    cat range.plain.out >&2; exit 1
fi
if ! grep -qF "$CHG" range.plain.out; then
    echo "FAIL: range diff missing changed line '$CHG'" >&2
    cat range.plain.out >&2; exit 1
fi

#  === (a') same range form in TLV ===
"$BE" "diff:big.c?$FROM..$TO#L20" --tlv >range.tlv.out 2>range.tlv.err
if ! grep -qF "$FAR" range.tlv.out; then
    echo "FAIL: range TLV diff missing far unchanged line '$FAR'" >&2
    exit 1
fi
if ! grep -qF "$CHG" range.tlv.out; then
    echo "FAIL: range TLV diff missing changed line '$CHG'" >&2
    exit 1
fi

#  === (b) commit-show per-file nav `U` URI = `?from..to#L`, not `#L` ===
#  `diff:?<sha>` shows commit <sha> vs its first parent; each per-file
#  hunk carries a hidden `U` nav URI.  Pull the diff:big.c click target
#  out of the TLV stream and assert it carries the `?...#L` range.
"$BE" "diff:?$TO" --tlv >show.tlv.out 2>show.tlv.err
NAV=$(strings show.tlv.out | grep -oE 'diff:big\.c[^[:space:]]*' | head -1)
[ -n "$NAV" ] || {
    echo "FAIL: no diff:big.c nav URI in commit-show TLV" >&2
    cat show.tlv.err >&2; exit 1
}
#  Must carry a `?...#L` range query, NOT the bare empty-query `#L` form.
case "$NAV" in
    diff:big.c'?'*..*'#L'*) ;;   # good: ?from..to#L
    diff:big.c'#L'*)
        echo "FAIL: commit-show nav URI is empty-query form '$NAV'" \
             "(want diff:big.c?<from>..<to>#L<n>)" >&2
        exit 1 ;;
    *)
        echo "FAIL: commit-show nav URI unexpected shape '$NAV'" >&2
        exit 1 ;;
esac

#  === (c) bare `?..` parent-branch ref is NOT mis-split ==============
#  A relative `..` (parent branch, URI.mkd:42) has empty from/to halves;
#  the range split must NOT fire on it, and a relative ref must NOT be
#  spliced into a hunk nav URI as a bogus `?....<base>` range.  The tell
#  is the per-hunk nav `U` URI: it must stay the BARE `diff:big.c#L<n>`
#  form (clicking falls back to wt-vs-base / branch-vs-base), never a
#  `?<range>#L` form built from the relative `..`.
"$BE" 'diff:big.c?..' --tlv >dotdot.tlv 2>dotdot.err || true
DNAV=$(strings dotdot.tlv | grep -oE 'diff:big\.c[^[:space:]]*' | head -1)
[ -n "$DNAV" ] || {
    echo "FAIL: no diff:big.c nav URI in '?..' TLV" >&2
    cat dotdot.err >&2; exit 1
}
case "$DNAV" in
    diff:big.c'#L'*) ;;   # good: bare line-anchor, no range query
    diff:big.c'?'*)
        echo "FAIL: '?..' mis-split — nav URI carries a bogus range '$DNAV'" \
             "(relative ref must not become a ?from..to range)" >&2
        exit 1 ;;
    *)
        echo "FAIL: '?..' nav URI unexpected shape '$DNAV'" >&2
        exit 1 ;;
esac

echo "OK 12-difflet-click"
