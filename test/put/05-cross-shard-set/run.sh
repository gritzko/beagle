#!/bin/sh
#  put/05-cross-shard-set — `be put ?<branch>#<sha>` migrates the
#  named tip's reachable closure into the target branch's shard via
#  KEEPMoveCommits, so a reader opening only the target shard finds
#  every commit/tree/blob the new REFS row claims to cover.
#
#  Topology:
#
#       T1 ─ T2 (trunk: x.txt at v1, then v2)
#         └─ ?feat at T1 ── F1 (y.txt on ?feat)
#
#  From cur=trunk we run:
#    * `be put ?feat#T2` (existing ?feat at F1).  FP chain from T2 is
#      [T2, T1]; stop = F1 which is on a sibling — !reached_stop.
#      Refuse with "no shared ancestry".
#    * `be delete ?feat` then `be put ?feat#T2` (new ref).  Chain
#      walks to root (T1 has no parent) without cap-hit.  Migration
#      copies T2 + T1 into .be/feat/.  Reads from `?feat` then see
#      T2's tree.
#    * `be put ?feat#<bogus-40-hex>` → RESOLVE rejects pre-migration.

. "$(dirname "$0")/../../lib/case.sh"

LOGS=$(cd .. && pwd)/logs-$NAME
rm -rf "$LOGS"; mkdir -p "$LOGS"

ref_tip() {
    "$BIN/keeper" refs 2>/dev/null | awk -v k="$1" '
        { sub(/^[[:space:]]+/, "")
          tab = index($0, "\t"); if (tab == 0) next
          kf = substr($0, 1, tab - 1); if (kf != k) next
          n = split($0, toks, /[[:space:]]+/)
          v = toks[n]; sub(/^\?/, "", v); print v; exit
        }'
}

pack_bytes() {
    _dir=$1
    [ -d "$_dir" ] || { echo 0; return; }
    _total=0
    for _f in "$_dir"/*.keeper; do
        [ -f "$_f" ] || continue
        _sz=$(wc -c < "$_f" | tr -d ' ')
        _total=$((_total + _sz))
    done
    echo "$_total"
}

# ------------------------------------------------------------------
# 1. T1 on trunk.
# ------------------------------------------------------------------
sleep 0.02
echo "x v1" > x.txt
must "$BE" put x.txt   > "$LOGS/01.out" 2> "$LOGS/01.err"
must "$BE" post 't1'   > "$LOGS/02.out" 2> "$LOGS/02.err"
T1=$(ref_tip '?')
[ -n "$T1" ] || { echo "no T1 after baseline" >&2; cat "$LOGS"/*.err >&2; exit 1; }

# ------------------------------------------------------------------
# 2. Fork ?feat at T1, switch into it, F1 commit (y.txt).
# ------------------------------------------------------------------
must "$BE" put '?./feat'  > "$LOGS/03.out" 2> "$LOGS/03.err"
must "$BE" get '?feat'    > "$LOGS/04.out" 2> "$LOGS/04.err"
sleep 0.2
echo "y v1" > y.txt
must "$BE" put y.txt      > "$LOGS/05.out" 2> "$LOGS/05.err"
must "$BE" post 'f1'      > "$LOGS/06.out" 2> "$LOGS/06.err"
F1=$(ref_tip '?feat')
[ -n "$F1" ] && [ "$F1" != "$T1" ] \
    || { echo "?feat F1 didn't advance past T1=$T1 (got $F1)" >&2; exit 1; }

# ------------------------------------------------------------------
# 3. Back to trunk, T2.
# ------------------------------------------------------------------
must "$BE" get '?'        > "$LOGS/07.out" 2> "$LOGS/07.err"
sleep 0.2
echo "x v2" > x.txt
must "$BE" put x.txt      > "$LOGS/08.out" 2> "$LOGS/08.err"
must "$BE" post 't2'      > "$LOGS/09.out" 2> "$LOGS/09.err"
T2=$(ref_tip '?')
[ -n "$T2" ] && [ "$T2" != "$T1" ] \
    || { echo "trunk T2 didn't advance past T1 (got $T2)" >&2; exit 1; }

# ------------------------------------------------------------------
# 4. Negative: `be put ?feat#T2` from cur=trunk.  ?feat.tip is F1
#    (sibling, not on T2's FP chain).  Refuse.  (Use explicit rc
#    capture so we can also assert on stderr — `mustnt` discards it.)
# ------------------------------------------------------------------
set +e
"$BE" put "?feat#$T2" > "$LOGS/10.out" 2> "$LOGS/10.err"
RC=$?
set -e
[ "$RC" -ne 0 ] \
    || { echo "FAIL: expected non-zero exit for cross-shard non-FF PUT" >&2; exit 1; }
grep -q 'no shared ancestry' "$LOGS/10.err" \
    || { echo "FAIL: expected 'no shared ancestry' diagnostic" >&2
         cat "$LOGS/10.err" >&2; exit 1; }
[ "$(ref_tip '?feat')" = "$F1" ] \
    || { echo "FAIL: ?feat moved despite refusal: $(ref_tip '?feat')" >&2; exit 1; }

# ------------------------------------------------------------------
# 5. Drop ?feat, then `be put ?feat#T2` (new ref).  Migration runs.
# ------------------------------------------------------------------
must "$BE" delete '?feat' > "$LOGS/11.out" 2> "$LOGS/11.err"

PRE_PACKS=$(pack_bytes .be/feat)
must "$BE" put "?feat#$T2" \
    > "$LOGS/12.out" 2> "$LOGS/12.err"
[ "$(ref_tip '?feat')" = "$T2" ] \
    || { echo "?feat didn't land at T2=$T2 (got $(ref_tip '?feat'))" >&2
         cat "$LOGS/12.err" >&2; exit 1; }
POST_PACKS=$(pack_bytes .be/feat)
[ "$POST_PACKS" -gt "$PRE_PACKS" ] \
    || { echo "?feat shard pack bytes didn't grow (was $PRE_PACKS, now $POST_PACKS) — KEEPMoveCommits didn't copy" >&2
         ls -la .be/feat/ >&2 2>/dev/null
         exit 1; }

# ------------------------------------------------------------------
# 6. `be get ?feat` materialises T2's tree (x@v2, no y).
# ------------------------------------------------------------------
must "$BE" get '?feat'    > "$LOGS/13.out" 2> "$LOGS/13.err"
[ "$(cat x.txt 2>/dev/null)" = "x v2" ] \
    || { echo "x.txt @ ?feat after migration: '$(cat x.txt 2>/dev/null)'" >&2
         exit 1; }
[ ! -e y.txt ] \
    || { echo "y.txt should not be present at T2" >&2; exit 1; }

# ------------------------------------------------------------------
# 7. Negative: nonexistent sha → resolver refuses before migration.
# ------------------------------------------------------------------
BOGUS=0000000000000000000000000000000000000000
set +e
"$BE" put "?feat#$BOGUS" > "$LOGS/14.out" 2> "$LOGS/14.err"
RC=$?
set -e
[ "$RC" -ne 0 ] \
    || { echo "FAIL: expected non-zero exit for bogus sha" >&2; exit 1; }
grep -q 'cannot resolve' "$LOGS/14.err" \
    || { echo "FAIL: expected 'cannot resolve' diagnostic for bogus sha" >&2
         cat "$LOGS/14.err" >&2; exit 1; }

rm -rf "$LOGS"
