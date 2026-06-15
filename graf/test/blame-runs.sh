#!/bin/sh
#  blame-runs.sh — BLAME-005: `blame:` emits a dog/HUNK stream of one
#  content hunk PER MAXIMAL COMMIT-RUN, not one flat blob with a fixed
#  per-line gutter.
#
#  Each maximal run of consecutive source lines sharing one commit is a
#  single hunk whose URI is `commit:?<sha40>` (the run commit, a bro
#  click-target), ts = the commit's author time, and body = the run's
#  verbatim source carrying syntax tok32 tags.  The commit no longer
#  rides the code line, so the old `BLAME_HW`-wide hashlet gutter (a
#  7-hex prefix on EVERY line) is gone.
#
#  Scenario: f.c is created with 3 lines in c1, then lines 2..3 are
#  rewritten and a 4th appended in c2.  Token-level blame attributes
#  line 1 to c1 and lines 2..4 to c2 — exactly two committed runs, so:
#    * --plain: a `--- commit:?<sha1> ---` hunk (body "aaa1") followed by
#      a `--- commit:?<sha2> ---` hunk (body "BBB2\nBBB3\naaa4"); the
#      shas resolve to v0.0.1 / v0.0.2 (== `be sha1:?vN`).
#    * --tlv: each H record carries a U-tagged `commit:?<sha40>` URI.
#    * NO per-line gutter: source lines start at column 0, never with a
#      `<7-hex> ` hashlet column (the pre-fix shape).
#  A worktree edit adds a 3rd, commit-less run (its hunk has NO
#  `commit:?` URI — the uncommitted line still renders).

set -eu
BIN=${BIN:-$(dirname "$(command -v be)")}
BE="$BIN/be"; GRAF="$BIN/graf"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
for t in "$BE" "$GRAF"; do
    [ -x "$t" ] || { echo "FAIL: $t not executable" >&2; exit 1; }
done

TEST_ID=${TEST_ID:-GRAFblameruns}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base); mkdir -p "$TMP"
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null||true; rmdir "${TMP%/*/*}" 2>/dev/null||true; }' EXIT INT TERM
fail(){ echo "FAIL: $*" >&2; exit 1; }

R="$TMP/repo"; rs_wt_at "$R"
git init --quiet -b main .; git config user.email t@t; git config user.name t

#  --- c1: 3 lines ---------------------------------------------------
printf 'aaa1\naaa2\naaa3\n' > f.c
touch -d "2026-04-20 12:00:00" f.c
"$BE" post -m c1 '?v0.0.1' >/dev/null 2>&1

#  --- c2: rewrite lines 2..3, append a 4th ---------------------------
printf 'aaa1\nBBB2\nBBB3\naaa4\n' > f.c
touch -d "2026-04-21 12:00:00" f.c
"$BE" post -m c2 '?v0.0.2' >/dev/null 2>&1

"$GRAF" index >/dev/null 2>&1 || fail "graf index failed"

S1=$("$BE" sha1:?v0.0.1 2>/dev/null) || fail "sha1:?v0.0.1 failed"
S2=$("$BE" sha1:?v0.0.2 2>/dev/null) || fail "sha1:?v0.0.2 failed"
[ -n "$S1" ] && [ -n "$S2" ] || fail "empty commit shas (S1=$S1 S2=$S2)"
#  Blame uris carry an 8-char hashlet (not the full sha40) plus a #subject
#  fragment: `commit:?<8hex>#<msg>`.
H1=$(printf '%.8s' "$S1"); H2=$(printf '%.8s' "$S2")

#  --- PLAIN: per-run content hunks, headed by commit:?<sha> ----------
PLAIN=$("$GRAF" --plain 'blame:f.c' 2>/dev/null) || fail "plain blame failed"

#  Exactly two committed run-hunks, in topo order, with the right shas.
NHUNK=$(printf '%s\n' "$PLAIN" | grep -cE 'commit:\?[0-9a-f]{8}#')
[ "$NHUNK" -eq 2 ] || fail "expected 2 commit run-hunks, got $NHUNK:
$PLAIN"
printf '%s\n' "$PLAIN" | grep -q "commit:?$H1" \
    || fail "no run-hunk for c1 (commit:?$H1):
$PLAIN"
printf '%s\n' "$PLAIN" | grep -q "commit:?$H2" \
    || fail "no run-hunk for c2 (commit:?$H2):
$PLAIN"

#  Run bodies: c1's run is line 1 ("aaa1"); c2's run is lines 2..4.
#  Assert the first hunk body is exactly the c1 run and contains no c2
#  lines (proving the regroup is per-commit, not per-line).
C1BODY=$(printf '%s\n' "$PLAIN" | awk '
    /commit:\?[0-9a-f]/{h++; next} h==1 && NF{print}')
[ "$C1BODY" = "aaa1" ] || fail "c1 run body = '$C1BODY', want 'aaa1'"

#  --- gutter is GONE: source lines start at column 0 -----------------
#  Pre-fix every source line began with a 7-char hashlet column
#  ("c32b035 sniff   15Jun aaa1").  Assert NO body line matches that
#  `<7-hex-or-spaces> ` gutter shape.
GUTTER=$(printf '%s\n' "$PLAIN" | grep -E '^[0-9a-f]{7} |^ {7,} [^ ]' || true)
[ -z "$GUTTER" ] || fail "per-line gutter still present:
$GUTTER"
printf '%s\n' "$PLAIN" | grep -qx 'aaa1' \
    || fail "source line 'aaa1' not emitted verbatim (gutter still on it?):
$PLAIN"

#  --- TLV: each hunk URI is the U-tagged commit:?<8hex>#<subject> ----
"$GRAF" --tlv 'blame:f.c' >"$TMP/tlv.bin" 2>/dev/null || fail "tlv blame failed"
grep -aq "commit:?$H1" "$TMP/tlv.bin" || fail "tlv: no commit:?$H1 URI"
grep -aq "commit:?$H2" "$TMP/tlv.bin" || fail "tlv: no commit:?$H2 URI"
#  The dropped `diff:?<hashlet>` gutter anchor must be gone.
grep -aq 'diff:?' "$TMP/tlv.bin" && fail "tlv: stale diff:? gutter anchor present"

#  --- worktree edit -> a 3rd, commit-less run ------------------------
printf 'aaa1\nBBB2\nBBB3\naaa4\nWTLINE\n' > f.c
WTOUT=$("$GRAF" --plain 'blame:f.c' 2>/dev/null) || fail "wt blame failed"
printf '%s\n' "$WTOUT" | grep -qx 'WTLINE' \
    || fail "worktree line not blamed:
$WTOUT"
#  Still only TWO commit:? hunks; the wt run carries no commit URI.
NWT=$(printf '%s\n' "$WTOUT" | grep -cE 'commit:\?[0-9a-f]{8}#')
[ "$NWT" -eq 2 ] || fail "wt edit: expected 2 commit run-hunks, got $NWT:
$WTOUT"

echo "PASS: blame emits per-commit-run hunks (commit:?<sha> uri, no gutter);" \
     "2 committed runs + 1 worktree run"
