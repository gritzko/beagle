#!/bin/sh
#  03-rebase-on-divergent-parent — `be patch ?..#` + `be post` from
#  a child branch whose parent has advanced absorbs the parent's tip
#  as a foster on cur's new tip.  Per VERBS.md §POST: cur is the only
#  ref POST moves; the named branch is read-only.
#
#  Sequence:
#    T1 baseline (a=alpha, b=beta) on trunk.
#    Create ?fix1 (be put ?./fix1), switch to it, edit a.txt to
#    alpha-fix1, put + post → ?fix1 at C1 (parent T1).
#    Switch back to trunk, edit b.txt to beta-trunk, put + post → T2
#    (parent T1).  ?fix1 is now divergent: its parent T1 is no longer
#    the trunk tip.
#    Switch to ?fix1 (wt resets to fix1's tree: a=alpha-fix1, b=beta).
#    `be patch ?..#` + `be post` from ?fix1: ?..tip (T2) absorbed as
#    foster on fix1's new tip C1' (parent C1).  Wt picks up T2's
#    b.txt edit via 3-way merge.  Trunk stays at T2.
#
#  Asserts:
#    * trunk tip stays at T2 (POST never moves a non-cur ref).
#    * ?fix1 tip moves from C1 to C1'.
#    * C1'.parent == C1 and C1'.foster == T2 (proves absorb-as-foster).
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
                    h = last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h)
                    if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
                }' .be/wtlog
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
sleep 0.02; cp "$CASE/01.a.txt" a.txt
sleep 0.02; cp "$CASE/02.b.txt" b.txt
must "$BE" put a.txt b.txt > "$LOGS/01.put.out" 2> "$LOGS/01.put.err"
must "$BE" post 'baseline msg'   > "$LOGS/02.post.out" 2> "$LOGS/02.post.err"
T1=$(head_hex)
[ -n "$T1" ] || { echo "no T1 after baseline post" >&2; exit 1; }

# ------------------------------------------------------------------
# 2. create ?fix1 off trunk.
# ------------------------------------------------------------------
must "$BE" put '?./fix1' > "$LOGS/03.create.out" 2> "$LOGS/03.create.err"
FIX1_AT_T1=$(ref_tip "?fix1")
[ "$FIX1_AT_T1" = "$T1" ] \
    || { echo "?fix1 should fork at T1=$T1; got $FIX1_AT_T1" >&2; exit 1; }

# ------------------------------------------------------------------
# 3. switch to ?fix1, edit a.txt, put + post → C1.
# ------------------------------------------------------------------
must "$BE" get "?fix1" > "$LOGS/04.get-fix1.out" 2> "$LOGS/04.get-fix1.err"
sleep 0.2                             # distinct mtime
sleep 0.02; cp "$CASE/03.a-fix1.txt" a.txt
must "$BE" put a.txt    > "$LOGS/05.put.out" 2> "$LOGS/05.put.err"
must "$BE" post 'fix1 c1' > "$LOGS/06.post.out" 2> "$LOGS/06.post.err"
FIX1_C1=$(head_hex)
[ -n "$FIX1_C1" ] && [ "$FIX1_C1" != "$T1" ] \
    || { echo "?fix1 didn't advance past T1" >&2; exit 1; }

# ------------------------------------------------------------------
# 4. switch back to trunk, edit b.txt, put + post → T2.
# ------------------------------------------------------------------
must "$BE" get "?.." > "$LOGS/07.get-trunk.out" 2> "$LOGS/07.get-trunk.err"
sleep 0.2
sleep 0.02; cp "$CASE/04.b-trunk.txt" b.txt
must "$BE" put b.txt   > "$LOGS/08.put.out" 2> "$LOGS/08.put.err"
must "$BE" post 't2 msg'     > "$LOGS/09.post.out" 2> "$LOGS/09.post.err"
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
# 6. `be patch ?..#` then `be post` from ?fix1: absorbs T2 as foster.
#    fix1 moves from C1 → C1' (parent C1, foster T2); trunk stays at T2.
# ------------------------------------------------------------------
sleep 0.02
if ! "$BE" patch '?..#' > "$LOGS/11.patch.out" 2> "$LOGS/11.patch.err"; then
    echo "be patch ?..# failed; stderr:" >&2
    cat "$LOGS/11.patch.err" >&2
    exit 1
fi
if ! "$BE" post 'fix1 absorb trunk' > "$LOGS/12.post.out" 2> "$LOGS/12.post.err"; then
    echo "be post (after patch) failed; stderr:" >&2
    cat "$LOGS/12.post.err" >&2
    exit 1
fi

# ------------------------------------------------------------------
# 7. assert: trunk stays at T2; ?fix1 advances from C1 to C1'.
# ------------------------------------------------------------------
TRUNK_AFTER=$(ref_tip "?")
FIX1_NEW_TIP=$(ref_tip "?fix1")
[ "$TRUNK_AFTER" = "$T2" ] \
    || { echo "trunk should stay at T2=$T2; got $TRUNK_AFTER" >&2; exit 1; }
[ -n "$FIX1_NEW_TIP" ] && [ "$FIX1_NEW_TIP" != "$FIX1_C1" ] \
    || { echo "?fix1 did not advance from C1=$FIX1_C1" >&2; exit 1; }

# ------------------------------------------------------------------
# 8. assert: C1'.parent == C1; C1'.foster == T2 (proves absorb-as-foster).
# ------------------------------------------------------------------
"$KEEPER" get "?fix1#$FIX1_NEW_TIP" > "$LOGS/13.commit.out" \
    2> "$LOGS/13.commit.err" \
    || { echo "keeper get .#$FIX1_NEW_TIP failed" >&2
         cat "$LOGS/13.commit.err" >&2; exit 1; }
PARENT_SHA=$(awk '/^parent / { print $2; exit }' "$LOGS/13.commit.out")
[ "$PARENT_SHA" = "$FIX1_C1" ] \
    || { echo "C1'.parent=$PARENT_SHA; want $FIX1_C1 (cur's old tip)" >&2
         exit 1; }
FOSTER_SHA=$(awk '/^foster / { print $2; exit }' "$LOGS/13.commit.out")
[ "$FOSTER_SHA" = "$T2" ] \
    || { echo "C1'.foster=$FOSTER_SHA; want T2=$T2" >&2
         cat "$LOGS/13.commit.out" >&2; exit 1; }

# ------------------------------------------------------------------
# 9. wt content after patch + post.  Wt reflects 3-way merge:
# a=alpha-fix1 from C1's edit, b=beta-trunk from T2.
# ------------------------------------------------------------------
match "$CASE/05.a.want.txt" a.txt
match "$CASE/06.b.want.txt" b.txt

#  All assertions passed — drop the logs dir on success.
rm -rf "$LOGS"
