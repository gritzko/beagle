#!/bin/sh
#  branches/05-auto-sync — `be patch ?<other>#` + `be post` rebases CUR
#  by absorbing ?<other>'s newest unreachable commit as a foster
#  header.  POST never moves a non-cur ref (per VERBS.md §POST).
#  Three flavours: parent (`?..`), descendant (`?./child`), sibling.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# --- setup: trunk with one commit ---
echo "x v1" > x.txt
"$BE" post 'v1 msg' >/dev/null
T25_pre=$(head_hex)
[ -n "$T25_pre" ] || fail "no trunk tip after seed post"
note "§25: trunk pre = $T25_pre"

echo "=== 25. be patch ?..# + be post: cur absorbs parent's tip as foster ==="
"$BE" put "?./fix1" >/dev/null || fail "§25: be put ?./fix1 failed"
"$BE" get "?fix1" >/dev/null   || fail "§25: be get ?fix1 failed"
sleep 0.01
echo "fix1-25 v1" > f25.txt
"$BE" put f25.txt >/dev/null
"$BE" post 'fix1-25 c1' >/dev/null || fail "§25: be post fix1-25 c1 failed"
F25_C1=$(head_hex)
[ -n "$F25_C1" ] && [ "$F25_C1" != "$T25_pre" ] \
    || fail "§25: ?fix1 didn't advance"
note "§25: ?fix1 at $F25_C1 (cur)"

# A peer wt shares the keeper, advances trunk while cur is on ?fix1.
WT25="$ETMP/wt25"
mkdir -p "$WT25"
# Secondary wt: `.be` is a regular file = its own wtlog seeded from
# the primary's (row-0 `repo` URI names the shared store).
cp "$WT/.be/wtlog" "$WT25/.be"
( cd "$WT25" && "$BE" get "?" >/dev/null ) \
    || fail "§25: WT25 be get ? failed"
( cd "$WT25" && sleep 0.01 && echo "trunk advance 25" > tr25.txt \
    && "$BE" put tr25.txt >/dev/null \
    && "$BE" post 'trunk-advance-25 msg' >/dev/null ) \
    || fail "§25: WT25 trunk advance failed"

T25_advance=$(ref_tip "?")
[ -n "$T25_advance" ] && [ "$T25_advance" != "$T25_pre" ] \
    || fail "§25: trunk REFS didn't advance"
note "§25: trunk advanced to $T25_advance"

# Back in cur (on ?fix1): be patch '?..#' brings trunk's new commit
# into cur's stack as foster.  Subsequent `be post` creates the new
# fix1 tip with parent=F25_C1, foster=T25_advance.  Trunk stays put.
cd "$WT"
"$BE" patch "?..#" 2>"$ETMP/p25.err" >/dev/null \
    || { cat "$ETMP/p25.err"; fail "§25: be patch ?..# failed"; }
"$BE" post 'fix1 absorb trunk' 2>>"$ETMP/p25.err" >/dev/null \
    || { cat "$ETMP/p25.err"; fail "§25: be post (after patch) failed"; }
T25_after=$(ref_tip "?")
F25_after=$(ref_tip "?fix1")
[ "$T25_after" = "$T25_advance" ] \
    || fail "§25: trunk drifted ($T25_advance -> $T25_after); POST must not move it"
[ -n "$F25_after" ] && [ "$F25_after" != "$F25_C1" ] \
    || fail "§25: ?fix1 didn't advance from C1=$F25_C1"

BODY=$("$KEEPER" get "?fix1#$F25_after" 2>/dev/null) \
    || fail "§25: keeper get fix1 tip failed"
echo "$BODY" | grep -q "^parent $F25_C1$" \
    || fail "§25: parent != F25_C1; body:\n$BODY"
echo "$BODY" | grep -q "^foster $T25_advance$" \
    || fail "§25: missing foster $T25_advance; body:\n$BODY"
note "§25: ?fix1 -> $F25_after; trunk unchanged at $T25_after"
note "§25 OK: patch ?..# + post absorbs trunk's commit as foster"

# cleanup — switch off ?fix1 first so delete isn't refused
"$BE" get "?.." >/dev/null 2>&1 || true
"$BE" delete "?fix1" >/dev/null 2>&1 || true
rm -f f25.txt tr25.txt

# 26. be patch '?./fix2#' + be post — cur (?fix1) absorbs descendant's
# tip as foster.  ?fix1/fix2 must not move.
echo "=== 26. be patch ?./fix2# + be post: cur absorbs descendant as foster ==="
cd "$WT"
"$BE" put "?./fix1" >/dev/null || fail "§26: be put ?./fix1 failed"
"$BE" get "?fix1" >/dev/null   || fail "§26: be get ?fix1 failed"
sleep 0.01
echo "fix1-26 v1" > f1_26.txt
"$BE" put f1_26.txt >/dev/null
"$BE" post 'fix1-26 c1' >/dev/null || fail "§26: be post fix1-26 c1 failed"
F1_TIP=$(head_hex)
[ -n "$F1_TIP" ] || fail "§26: no fix1 tip"

"$BE" put "?./fix2" >/dev/null || fail "§26: be put ?./fix2 failed"
"$BE" get "?fix1/fix2" >/dev/null || fail "§26: be get ?fix1/fix2 failed"
[ "$(cur_branch)" = "fix1/fix2" ] || fail "§26: wt should be on fix1/fix2"
sleep 0.01
echo "fix2-26 v1" > f2_26.txt
"$BE" put f2_26.txt >/dev/null
"$BE" post 'fix2-26 c1' >/dev/null || fail "§26: be post fix2-26 c1 failed"
F2_TIP_BEFORE=$(head_hex)
[ -n "$F2_TIP_BEFORE" ] && [ "$F2_TIP_BEFORE" != "$F1_TIP" ] \
    || fail "§26: ?fix1/fix2 didn't advance"

"$BE" get "?fix1" >/dev/null || fail "§26: be get ?fix1 failed"
F1_PRE=$(ref_tip "?fix1")
F2_PRE=$(ref_tip "?fix1/fix2")
[ "$F1_PRE" = "$F1_TIP" ] || fail "§26: ?fix1 unexpectedly moved"

"$BE" patch "?./fix2#" 2>"$ETMP/p26.err" >/dev/null \
    || { cat "$ETMP/p26.err"; fail "§26: be patch ?./fix2# failed"; }
"$BE" post 'fix1 absorb fix2' 2>>"$ETMP/p26.err" >/dev/null \
    || { cat "$ETMP/p26.err"; fail "§26: be post (after patch) failed"; }

F1_POST=$(ref_tip "?fix1")
F2_POST=$(ref_tip "?fix1/fix2")
[ "$F2_POST" = "$F2_PRE" ] \
    || fail "§26: ?fix1/fix2 drifted; POST may not move a non-cur ref"
[ -n "$F1_POST" ] && [ "$F1_POST" != "$F1_PRE" ] \
    || fail "§26: ?fix1 didn't advance from F1_PRE=$F1_PRE"

BODY=$("$KEEPER" get "?fix1#$F1_POST" 2>/dev/null) \
    || fail "§26: keeper get fix1 tip failed"
echo "$BODY" | grep -q "^parent $F1_PRE$" \
    || fail "§26: parent != F1_PRE; body:\n$BODY"
echo "$BODY" | grep -q "^foster $F2_PRE$" \
    || fail "§26: missing foster $F2_PRE; body:\n$BODY"
note "§26: ?fix1 -> $F1_POST (parent=$F1_PRE, foster=$F2_PRE); ?fix1/fix2 unchanged"

"$BE" get "?.." >/dev/null
"$BE" delete "?fix1/fix2" >/dev/null 2>&1 || true
"$BE" delete "?fix1"      >/dev/null 2>&1 || true
rm -f f1_26.txt f2_26.txt

# 27. be patch '?fix2#' + be post — cur (?fix1) absorbs sibling ?fix2.
echo "=== 27. be patch ?fix2# + be post: cur absorbs sibling as foster ==="
cd "$WT"
"$BE" get "?.." >/dev/null     || fail "§27: be get ?.. failed"
"$BE" put "?./fix1" >/dev/null || fail "§27: be put ?./fix1 failed"
"$BE" put "?./fix2" >/dev/null || fail "§27: be put ?./fix2 (peer) failed"

"$BE" get "?fix2" >/dev/null || fail "§27: be get ?fix2 failed"
sleep 0.01
echo "fix2-27 v1" > f27_2.txt
"$BE" put f27_2.txt >/dev/null
"$BE" post 'fix2-27 c1' >/dev/null || fail "§27: be post fix2-27 c1 failed"
F2_TIP_PRE=$(ref_tip "?fix2")
[ -n "$F2_TIP_PRE" ] || fail "§27: no ?fix2 tip"

"$BE" get "?fix1" >/dev/null || fail "§27: be get ?fix1 failed"
sleep 0.01
echo "fix1-27 v1" > f27_1.txt
"$BE" put f27_1.txt >/dev/null
"$BE" post 'fix1-27 c1' >/dev/null || fail "§27: be post fix1-27 c1 failed"
F1_TIP_PRE=$(ref_tip "?fix1")
TRUNK_PRE_27=$(ref_tip "?")
[ "$(cur_branch)" = "fix1" ] || fail "§27: wt should be on ?fix1"

"$BE" patch "?fix2#" 2>"$ETMP/p27.err" >/dev/null \
    || { cat "$ETMP/p27.err"; fail "§27: be patch ?fix2# failed"; }
"$BE" post 'fix1 absorb fix2 sibling' 2>>"$ETMP/p27.err" >/dev/null \
    || { cat "$ETMP/p27.err"; fail "§27: be post (after patch) failed"; }

F2_TIP_POST=$(ref_tip "?fix2")
F1_TIP_POST=$(ref_tip "?fix1")
TRUNK_POST_27=$(ref_tip "?")

[ "$F2_TIP_POST" = "$F2_TIP_PRE" ] \
    || fail "§27: ?fix2 drifted; POST may not move a non-cur ref"
[ -n "$F1_TIP_POST" ] && [ "$F1_TIP_POST" != "$F1_TIP_PRE" ] \
    || fail "§27: ?fix1 (cur) didn't advance after rebase onto sibling"
[ "$TRUNK_POST_27" = "$TRUNK_PRE_27" ] \
    || fail "§27: trunk drifted ($TRUNK_PRE_27 -> $TRUNK_POST_27)"

BODY=$("$KEEPER" get "?fix1#$F1_TIP_POST" 2>/dev/null) \
    || fail "§27: keeper get fix1 tip failed"
echo "$BODY" | grep -q "^parent $F1_TIP_PRE$" \
    || fail "§27: parent != F1_TIP_PRE; body:\n$BODY"
echo "$BODY" | grep -q "^foster $F2_TIP_PRE$" \
    || fail "§27: missing foster $F2_TIP_PRE; body:\n$BODY"
note "§27 OK: ?fix1 rebased (foster $F2_TIP_PRE); ?fix2/trunk untouched"

echo "=== branches/05-auto-sync: OK ==="
