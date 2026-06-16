#!/bin/sh
#  blame-readd.sh — BLAME-006a regression guard: a file that is DELETED
#  then RE-ADDED must blame to the RE-ADD commit, never leak back to the
#  ROOT (or any pre-delete) commit.
#
#  Pre-fix `GRAFFileWeave` SKIPPED a commit where the path is ABSENT
#  (`BLAME.c:480/492` `continue`) instead of folding an explicit empty
#  (all-delete) layer.  The old tokens therefore never died at the delete
#  commit; when the file was re-added with STRUCTURALLY-SIMILAR content (a
#  `# Title` H1 + blank lines), the WEAVE EQ keep-seq branch matched those
#  new tokens to the still-alive ancestor tokens, so they inherited the
#  ROOT commit's seq.  `be blame:` then pinned the re-added title/blanks to
#  `commit:?<root>`.
#
#  The fix folds the delete as an all-INS-killing empty layer, so the
#  re-add is a clean all-insert at the re-add commit.
#
#  Table-driven: each case is `path|root-content|readd-content`, where the
#  readd-content shares a title line + blank lines with the root-content
#  so the RAPHash collides across the delete.  After add→delete→readd we
#  assert:
#    1. blame references the RE-ADD commit URI, and
#    2. blame references NO pre-delete (root) commit URI.

set -eu
BIN=${BIN:-$(dirname "$(command -v be)")}
BE="$BIN/be"; GRAF="$BIN/graf"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
for t in "$BE" "$GRAF"; do
    [ -x "$t" ] || { echo "FAIL: $t not executable" >&2; exit 1; }
done

TEST_ID=${TEST_ID:-GRAFblamereadd}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base); mkdir -p "$TMP"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null||true; rmdir "${TMP%/*/*}" 2>/dev/null||true; }' EXIT INT TERM
fail(){ echo "FAIL: $*" >&2; exit 1; }

#  --- case table: file | root-content | readd-content ----------------
#  Newlines are encoded as literal \n (printf %b decodes them).  Both
#  contents open with a `# <Title>` H1 and carry blank lines, the exact
#  structural tokens whose content-only RAPHash collides across the
#  delete/readd and used to inherit the root seq.
CASES='
DOC.md|# Old Doc\n\nold body line one\nold body line two\n|# New Doc\n\nbrand new content A\n\nbrand new content B\n
sub/page.md|# Heading\n\nfirst original paragraph\n|# Heading\n\ncompletely different prose here\n\nand more of it\n
'

ncase=0
printf '%s\n' "$CASES" | while IFS='|' read -r FPATH ROOTC READDC; do
    [ -n "$FPATH" ] || continue
    ncase=$((ncase + 1))
    R="$TMP/repo$ncase"; rs_wt_at "$R"
    git init --quiet -b main .; git config user.email t@t; git config user.name t

    #  --- c1 (ROOT): create the file with the original doc -----------
    mkdir -p "$(dirname "$FPATH")" 2>/dev/null || true
    printf '%b' "$ROOTC" > "$FPATH"
    touch -d "2026-01-01 12:00:00" "$FPATH"
    "$BE" post -m c1root '?v0.0.1' >/dev/null 2>&1 || fail "[$FPATH] c1 post failed"

    #  --- c2: DELETE the file ----------------------------------------
    rm "$FPATH"
    touch -d "2026-02-01 12:00:00" .
    "$BE" post -m c2del '?v0.0.2' >/dev/null 2>&1 || fail "[$FPATH] c2 post failed"

    #  --- c3 (RE-ADD): structurally-similar but different content ----
    #  Sniff's mtime change-detection does not pick a deleted-then-
    #  recreated path back up on its own, so stage the re-add explicitly
    #  with `be put` before posting (else the post is an empty commit and
    #  the file stays untracked).
    mkdir -p "$(dirname "$FPATH")" 2>/dev/null || true
    printf '%b' "$READDC" > "$FPATH"
    touch -d "2026-03-01 12:00:00" "$FPATH"
    "$BE" put "$FPATH" >/dev/null 2>&1 || fail "[$FPATH] c3 put failed"
    "$BE" post -m c3readd '?v0.0.3' >/dev/null 2>&1 || fail "[$FPATH] c3 post failed"

    "$GRAF" index >/dev/null 2>&1 || fail "[$FPATH] graf index failed"

    S1=$("$BE" sha1:?v0.0.1 2>/dev/null) || fail "[$FPATH] sha1 v0.0.1 failed"
    S3=$("$BE" sha1:?v0.0.3 2>/dev/null) || fail "[$FPATH] sha1 v0.0.3 failed"
    [ -n "$S1" ] && [ -n "$S3" ] || fail "[$FPATH] empty shas (S1=$S1 S3=$S3)"
    [ "$S1" != "$S3" ] || fail "[$FPATH] root and re-add are the same commit"
    H1=$(printf '%.8s' "$S1"); H3=$(printf '%.8s' "$S3")

    #  Guard: the re-add must have actually captured the file (else sniff
    #  produced an empty commit and the whole scenario is vacuous).
    "$GRAF" get "$FPATH?$S3" >/dev/null 2>&1 \
        || fail "[$FPATH] re-add commit $H3 does not contain the file (empty commit?)"

    OUT=$("$GRAF" --plain "blame:$FPATH" 2>/dev/null) || fail "[$FPATH] blame failed"

    #  (1) the re-add commit must own (at least some of) the file.
    printf '%s\n' "$OUT" | grep -q "commit:?$H3" \
        || fail "[$FPATH] blame does not reference re-add commit:?$H3 (root-leak):
$OUT"

    #  (2) NO token may be pinned to the pre-delete root commit — the
    #  whole file was introduced at the re-add.
    if printf '%s\n' "$OUT" | grep -q "commit:?$H1"; then
        fail "[$FPATH] blame leaks to ROOT commit:?$H1 (deleted-then-readd mis-attribution):
$OUT"
    fi

    echo "  - [$FPATH] re-add owns the file (commit:?$H3); no root leak (commit:?$H1)"
done

echo "PASS: deleted-then-readd files blame to the re-add commit, not root"
