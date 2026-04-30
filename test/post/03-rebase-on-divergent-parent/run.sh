#!/bin/sh
#  03-rebase-on-divergent-parent — `be post ?..` from a child branch
#  whose parent advanced out from under it must rebase (not ff) and
#  auto-sync cur to the new parent tip.
#
#  Sequence:
#    T1 baseline (a=alpha, b=beta) on trunk.
#    Create ?fix1 (be post ?./fix1), switch to it, edit a.txt to
#    alpha-fix1, put + post → ?fix1 at C1 (parent T1).
#    Switch back to trunk, edit b.txt to beta-trunk, put + post → T2
#    (parent T1).  ?fix1 is now divergent: its parent T1 is no longer
#    the trunk tip.
#    Switch to ?fix1 (wt resets to fix1's tree: a=alpha-fix1, b=beta).
#    `be post ?..` from ?fix1: rebases C1 onto T2.  Trunk advances to
#    T3; ?fix1 auto-syncs to T3.
#
#  Asserts:
#    * trunk tip changed (T1 -> T2 -> T3, all distinct).
#    * ?fix1 tip == trunk's new tip (auto-sync).
#    * T3's first parent == T2 (proves rebase, not a ff/no-op).
#    * wt has both edits: a=alpha-fix1, b=beta-trunk.
#
#  Note: redirected stderr/stdout files are kept OUTSIDE the wt
#  (one dir up from SCRATCH).  Otherwise `be get` would treat them as
#  dirty wt entries and refuse cross-branch checkouts.

. "$(dirname "$0")/../../lib/case.sh"

#  Resolve the keeper binary the same way case.sh resolves BE.
KEEPER=${KEEPER:-${BIN:+$BIN/keeper}}
KEEPER=${KEEPER:-$(command -v keeper || true)}
[ -n "$KEEPER" ] && [ -x "$KEEPER" ] || {
    echo "run.sh: cannot locate \`keeper\`" >&2
    exit 2
}

#  Logs dir lives one level up from the wt so it stays out of `be`'s
#  dirty-scan radar.
LOGS=$(cd .. && pwd)/logs-$NAME
rm -rf "$LOGS"
mkdir -p "$LOGS"

#  Latest sniff baseline row's URI sha (post|get|patch).
head_hex() {
    awk -F'\t' '$2=="post"||$2=="get"||$2=="patch" { last=$3 }
                END {
                    h = last; sub(/^[^#]*#/, "", h)
                    if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
                }' .sniff
}

#  Tip recorded for KEY in `keeper refs` output.  Empty if KEY absent.
ref_tip() {
    "$KEEPER" refs 2>/dev/null | awk -v k="$1" '
        { sub(/^[[:space:]]+/, "")
          tab = index($0, "\t"); if (tab == 0) next
          kf = substr($0, 1, tab - 1); if (kf != k) next
          n = split($0, toks, /[[:space:]]+/)
          v = toks[n]; sub(/^\?/, "", v); print v; exit
        }'
}

# ------------------------------------------------------------------
# 1. trunk baseline T1: a=alpha, b=beta.
# ------------------------------------------------------------------
cp "$CASE/01.a.txt" a.txt
cp "$CASE/02.b.txt" b.txt
must "$BE" put a.txt b.txt > "$LOGS/01.put.out" 2> "$LOGS/01.put.err"
must "$BE" post baseline   > "$LOGS/02.post.out" 2> "$LOGS/02.post.err"
T1=$(head_hex)
[ -n "$T1" ] || { echo "no T1 after baseline post" >&2; exit 1; }

# ------------------------------------------------------------------
# 2. create ?fix1 off trunk.
# ------------------------------------------------------------------
must "$BE" post "?./fix1" > "$LOGS/03.create.out" 2> "$LOGS/03.create.err"
FIX1_AT_T1=$(ref_tip "?fix1")
[ "$FIX1_AT_T1" = "$T1" ] \
    || { echo "?fix1 should fork at T1=$T1; got $FIX1_AT_T1" >&2; exit 1; }

# ------------------------------------------------------------------
# 3. switch to ?fix1, edit a.txt, put + post → C1.
# ------------------------------------------------------------------
must "$BE" get "?fix1" > "$LOGS/04.get-fix1.out" 2> "$LOGS/04.get-fix1.err"
sleep 0.2                             # distinct mtime
cp "$CASE/03.a-fix1.txt" a.txt
must "$BE" put a.txt    > "$LOGS/05.put.out" 2> "$LOGS/05.put.err"
must "$BE" post fix1 c1 > "$LOGS/06.post.out" 2> "$LOGS/06.post.err"
FIX1_C1=$(head_hex)
[ -n "$FIX1_C1" ] && [ "$FIX1_C1" != "$T1" ] \
    || { echo "?fix1 didn't advance past T1" >&2; exit 1; }

# ------------------------------------------------------------------
# 4. switch back to trunk, edit b.txt, put + post → T2.
# ------------------------------------------------------------------
must "$BE" get "?.." > "$LOGS/07.get-trunk.out" 2> "$LOGS/07.get-trunk.err"
sleep 0.2
cp "$CASE/04.b-trunk.txt" b.txt
must "$BE" put b.txt   > "$LOGS/08.put.out" 2> "$LOGS/08.put.err"
must "$BE" post t2     > "$LOGS/09.post.out" 2> "$LOGS/09.post.err"
T2=$(head_hex)
[ -n "$T2" ] && [ "$T2" != "$T1" ] \
    || { echo "trunk T2 didn't advance past T1=$T1" >&2; exit 1; }
TRUNK_AT_T2=$(ref_tip "?")
[ "$TRUNK_AT_T2" = "$T2" ] \
    || { echo "trunk REFS should be T2=$T2; got $TRUNK_AT_T2" >&2; exit 1; }

# ------------------------------------------------------------------
# 5. switch to ?fix1: wt resets to fix1's tree (a=alpha-fix1, b=beta).
# ------------------------------------------------------------------
must "$BE" get "?fix1" > "$LOGS/10.get-fix1.out" 2> "$LOGS/10.get-fix1.err"
FIX1_OLD_TIP=$(ref_tip "?fix1")
[ "$FIX1_OLD_TIP" = "$FIX1_C1" ] \
    || { echo "?fix1 tip drifted: want $FIX1_C1 got $FIX1_OLD_TIP" >&2; exit 1; }

# ------------------------------------------------------------------
# 6. `be post ?..` from ?fix1: trunk-advance forces rebase of C1 onto
#    T2; cur (?fix1) auto-syncs.  Capture stderr for inline diagnostics
#    if the cross-branch promote dispatcher trips up.
# ------------------------------------------------------------------
if ! "$BE" post "?.." > "$LOGS/11.post.out" 2> "$LOGS/11.post.err"; then
    echo "TODO(spec): be post ?.. failed; stderr:" >&2
    cat "$LOGS/11.post.err" >&2
    exit 1
fi

# ------------------------------------------------------------------
# 7. assert: trunk advanced to T3 (≠ T1, ≠ T2); ?fix1 auto-synced to T3.
# ------------------------------------------------------------------
T3=$(ref_tip "?")
FIX1_NEW_TIP=$(ref_tip "?fix1")
[ -n "$T3" ] && [ "$T3" != "$T1" ] && [ "$T3" != "$T2" ] \
    || { echo "trunk did not advance past T2 (T3='$T3')" >&2; exit 1; }
[ "$FIX1_NEW_TIP" = "$T3" ] \
    || { echo "auto-sync failed: ?fix1=$FIX1_NEW_TIP, ?=$T3" >&2; exit 1; }

# ------------------------------------------------------------------
# 8. assert: T3's single parent is T2 (NOT T1) — proves rebase happened.
# ------------------------------------------------------------------
"$KEEPER" get ".#$T3" > "$LOGS/12.commit.out" 2> "$LOGS/12.commit.err" \
    || { echo "keeper get .#$T3 failed" >&2
         cat "$LOGS/12.commit.err" >&2; exit 1; }
PARENTS=$(grep -c '^parent ' "$LOGS/12.commit.out" || true)
[ "$PARENTS" = "1" ] \
    || { echo "T3 has $PARENTS parent line(s); want exactly 1" >&2
         cat "$LOGS/12.commit.out" >&2; exit 1; }
PARENT_SHA=$(awk '/^parent / { print $2; exit }' "$LOGS/12.commit.out")
[ "$PARENT_SHA" = "$T2" ] \
    || { echo "T3.parent=$PARENT_SHA; want T2=$T2 (rebase, not ff)" >&2
         exit 1; }

# ------------------------------------------------------------------
# 9. wt content after `be post ?..`.  POST refreshes the wt to the
# rebased tree as part of cur auto-sync — both edits should be
# visible without an explicit follow-up GET.
# ------------------------------------------------------------------
match "$CASE/05.a.want.txt" a.txt
match "$CASE/06.b.want.txt" b.txt

#  All assertions passed — drop the logs dir on success.
rm -rf "$LOGS"
