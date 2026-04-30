#!/bin/sh
#  branches/05-auto-sync — extracted from workflow-branches.sh stages 25-27.
#  ?.. auto-sync; ?./fix2 promote-into-child; sibling promote (no auto-sync).

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# --- setup: trunk with one commit ---
echo "x v1" > x.txt
"$BE" post v1 >/dev/null
T25_pre=$(head_hex)
[ -n "$T25_pre" ] || fail "no trunk tip after seed post"
note "§25: trunk pre = $T25_pre"

echo "=== 25. ?.. auto-sync ==="
#  Create a fresh ?fix1 child for this scenario.
"$BE" post "?./fix1" >/dev/null || fail "§25: be post ?./fix1 failed"
"$BE" get "?fix1" >/dev/null || fail "§25: be get ?fix1 failed"
sleep 0.01
echo "fix1-25 v1" > f25.txt
"$BE" put f25.txt >/dev/null
"$BE" post fix1-25 c1 >/dev/null || fail "§25: be post fix1-25 c1 failed"
F25_C1=$(head_hex)
[ -n "$F25_C1" ] && [ "$F25_C1" != "$T25_pre" ] \
    || fail "§25: ?fix1 didn't advance"
note "§25: ?fix1 at $F25_C1 (cur)"

# A peer wt shares the keeper, advances trunk while cur is on ?fix1.
WT25="$ETMP/wt25"
mkdir -p "$WT25"
ln -s "$WT/.dogs" "$WT25/.dogs"
( cd "$WT25" && "$BE" get "?" >/dev/null ) \
    || fail "§25: WT25 be get ? failed"
( cd "$WT25" && sleep 0.01 && echo "trunk advance 25" > tr25.txt \
    && "$BE" put tr25.txt >/dev/null \
    && "$BE" post trunk-advance-25 >/dev/null ) \
    || fail "§25: WT25 trunk advance failed"

T25_advance=$(ref_tip "?")
[ -n "$T25_advance" ] && [ "$T25_advance" != "$T25_pre" ] \
    || fail "§25: trunk REFS didn't advance"
note "§25: trunk advanced to $T25_advance"

# Back in cur (on ?fix1): be post ?.. — promote + auto-sync
cd "$WT"
"$BE" post "?.." 2>"$ETMP/p25.err" >/dev/null \
    || { cat "$ETMP/p25.err"; fail "§25: be post ?.. failed"; }
T25_after=$(ref_tip "?")
F25_after=$(ref_tip "?fix1")
[ -n "$T25_after" ] && [ "$T25_after" != "$T25_advance" ] \
    || fail "§25: trunk REFS didn't advance past T25_advance"
[ "$F25_after" = "$T25_after" ] \
    || fail "§25: cur (?fix1) auto-sync failed: ?fix1=$F25_after, ?=$T25_after"
note "§25: trunk -> $T25_after; ?fix1 auto-synced"
note "§25 OK: ?.. auto-sync"

# cleanup
"$BE" delete "?fix1" >/dev/null 2>&1 || true
rm -f f25.txt tr25.txt

# 26. ?./fix2 promote-into-child
echo "=== 26. ?./fix2 promote-into-child ==="
cd "$WT"
"$BE" get "?.." >/dev/null || fail "§26: be get ?.. failed"
"$BE" post "?./fix1" >/dev/null || fail "§26: be post ?./fix1 failed"
"$BE" get "?fix1" >/dev/null || fail "§26: be get ?fix1 failed"
sleep 0.01
echo "fix1-26 v1" > f1_26.txt
"$BE" put f1_26.txt >/dev/null
"$BE" post fix1-26 c1 >/dev/null || fail "§26: be post fix1-26 c1 failed"
F1_TIP=$(head_hex)
[ -n "$F1_TIP" ] || fail "§26: no fix1 tip"

"$BE" post "?./fix2" >/dev/null || fail "§26: be post ?./fix2 failed"
"$BE" get "?fix1/fix2" >/dev/null || fail "§26: be get ?fix1/fix2 failed"
[ "$(cur_branch)" = "fix1/fix2" ] || fail "§26: wt should be on fix1/fix2"
sleep 0.01
echo "fix2-26 v1" > f2_26.txt
"$BE" put f2_26.txt >/dev/null
"$BE" post fix2-26 c1 >/dev/null || fail "§26: be post fix2-26 c1 failed"
F2_TIP_BEFORE=$(head_hex)
[ -n "$F2_TIP_BEFORE" ] && [ "$F2_TIP_BEFORE" != "$F1_TIP" ] \
    || fail "§26: ?fix1/fix2 didn't advance"

"$BE" get "?fix1" >/dev/null || fail "§26: be get ?fix1 failed"
F1_PRE=$(ref_tip "?fix1")
F2_PRE=$(ref_tip "?fix1/fix2")
[ "$F1_PRE" = "$F1_TIP" ] || fail "§26: ?fix1 unexpectedly moved"

"$BE" post "?./fix2" 2>"$ETMP/p26.err" >/dev/null \
    || { cat "$ETMP/p26.err"; fail "§26: be post ?./fix2 failed"; }

F1_POST=$(ref_tip "?fix1")
F2_POST=$(ref_tip "?fix1/fix2")
[ "$F1_POST" = "$F1_PRE" ] || fail "§26: cur (?fix1) moved across promote"
[ "$F2_POST" != "$F1_PRE" ] \
    || fail "§26: ?fix1/fix2 collapsed to fix1"
note "§26: ?fix1 unchanged at $F1_POST; ?fix1/fix2 -> $F2_POST"

"$BE" get "?.." >/dev/null
"$BE" delete "?fix1/fix2" >/dev/null 2>&1 || true
"$BE" delete "?fix1"      >/dev/null 2>&1 || true
rm -f f1_26.txt f2_26.txt

# 27. sibling promote (NO auto-sync)
echo "=== 27. sibling promote ==="
cd "$WT"
"$BE" get "?.." >/dev/null || fail "§27: be get ?.. failed"
"$BE" post "?./fix1" >/dev/null || fail "§27: be post ?./fix1 failed"
"$BE" post "?./fix2" >/dev/null || fail "§27: be post ?./fix2 (peer) failed"

"$BE" get "?fix2" >/dev/null || fail "§27: be get ?fix2 failed"
sleep 0.01
echo "fix2-27 v1" > f27_2.txt
"$BE" put f27_2.txt >/dev/null
"$BE" post fix2-27 c1 >/dev/null || fail "§27: be post fix2-27 c1 failed"
F2_TIP_PRE=$(ref_tip "?fix2")
[ -n "$F2_TIP_PRE" ] || fail "§27: no ?fix2 tip"

"$BE" get "?fix1" >/dev/null || fail "§27: be get ?fix1 failed"
sleep 0.01
echo "fix1-27 v1" > f27_1.txt
"$BE" put f27_1.txt >/dev/null
"$BE" post fix1-27 c1 >/dev/null || fail "§27: be post fix1-27 c1 failed"
F1_TIP_PRE=$(ref_tip "?fix1")
TRUNK_PRE_27=$(ref_tip "?")
[ "$(cur_branch)" = "fix1" ] || fail "§27: wt should be on ?fix1"

"$BE" post "?fix2" 2>"$ETMP/p27.err" >/dev/null \
    || { cat "$ETMP/p27.err"; fail "§27: be post ?fix2 failed"; }

F2_TIP_POST=$(ref_tip "?fix2")
F1_TIP_POST=$(ref_tip "?fix1")
TRUNK_POST_27=$(ref_tip "?")

[ "$F1_TIP_POST" = "$F1_TIP_PRE" ] \
    || fail "§27: cur (?fix1) auto-synced when sibling promote should leave it"
[ "$F2_TIP_POST" != "$F2_TIP_PRE" ] \
    || fail "§27: sibling ?fix2 did not advance"
[ "$TRUNK_POST_27" = "$TRUNK_PRE_27" ] \
    || fail "§27: trunk drifted ($TRUNK_PRE_27 -> $TRUNK_POST_27)"
note "§27 OK: ?fix2 advanced; ?fix1 untouched (no auto-sync for sibling)"

echo "=== branches/05-auto-sync: OK ==="
