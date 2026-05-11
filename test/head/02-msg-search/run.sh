#!/bin/sh
#  head/02-msg-search — `be head '#wonderful'` finds the cur-reachable
#  commit whose message body contains "wonderful" and prints its log
#  row.  Walks the first-parent chain via the DAG index in graf.

. "$(dirname "$0")/../../lib/case.sh"

cd "$SCRATCH"

# Three commits on cur; "wonderful" only in the middle one.
echo a > a.txt;  "$BE" put a.txt >/dev/null;  "$BE" post '#alpha commit'   >/dev/null
echo b > b.txt;  "$BE" put b.txt >/dev/null;  "$BE" post '#beta wonderful' >/dev/null
echo c > c.txt;  "$BE" put c.txt >/dev/null;  "$BE" post '#gamma final'    >/dev/null

# Match prints the matching commit row.
"$BE" head '#wonderful' >01.match.got.out 2>01.match.got.err
match_re "$CASE/01.match.want.txt" 01.match.got.out
empty 01.match.got.err

# No-match exits non-zero with a stderr hint; stdout empty.
# Inline (mustnt swallows stderr, so we can't use it here).
if "$BE" head '#zzz-no-match' >02.miss.got.out 2>02.miss.got.err; then
    echo "head: expected non-zero exit on miss" >&2
    exit 1
fi
empty 02.miss.got.out
match_re "$CASE/02.miss.err.txt" 02.miss.got.err

# HEAD must NOT mutate cur (no commit, no ref move).
TIP_BEFORE=$(awk -F'\t' '$2=="post"{last=$3} END{
    h=last; sub(/^[^#]*#/, "", h); print h }' .be/wtlog)
"$BE" head '#wonderful' >/dev/null 2>&1
TIP_AFTER=$(awk -F'\t' '$2=="post"{last=$3} END{
    h=last; sub(/^[^#]*#/, "", h); print h }' .be/wtlog)
[ "$TIP_AFTER" = "$TIP_BEFORE" ] || {
    echo "head: cur tip changed after read-only HEAD" >&2
    exit 1
}
