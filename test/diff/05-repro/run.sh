#!/bin/sh
#  diff/05-repro — behavioural regressions for the WEAVE diff
#  renderer (NEIL cascade, inline hili, token boundary, EQ artifacts).
#
#  Each section seeds a two-revision sniff repo in its own subdir,
#  then drives `be diff:f.c?<from>#<to>` and greps the plain unified
#  output (`HUNKu8sFeedLineBased`).

. "$(dirname "$0")/../../lib/case.sh"

#  run_diff <subdir> <old-fixture> <new-fixture>
#  Sets OUT (CSI-stripped unified diff) in caller scope.
run_diff() {
    _sub=$1; _oldf=$2; _newf=$3
    mkdir -p "$_sub/.be"; cd "$_sub"
    cp "$_oldf" f.c
    "$BE" put  f.c                  >/dev/null
    "$BE" post -m v1 '?v1'          >/dev/null
    OLD_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')
    sleep 0.02
    cp "$_newf" f.c
    "$BE" put  f.c                  >/dev/null
    "$BE" post -m v2 '?v2'          >/dev/null
    NEW_SHA=$(grep -oE '#[0-9a-f]{40}' .be/wtlog | tail -1 | tr -d '#')
    OUT=$("$BE" "diff:f.c?${OLD_SHA}#${NEW_SHA}" 2>&1 \
            | sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g')
    cd "$SCRATCH"
}

count() { echo "$1" | grep -cF "$2" 2>/dev/null || true; }

# -------------------------------------------------------------------
#  1. EQ artifact: shared lines must not appear as paired DEL+INS.
# -------------------------------------------------------------------
run_diff "$SCRATCH/01.diff" "$CASE/01.diff.c.old" "$CASE/01.diff.c.new"

FAILS=0
check_unique() {
    _line=$1
    _c=$(count "$OUT" "$_line")
    if [ "$_c" -gt 1 ]; then
        echo "FAIL [eq-artifact]: '$_line' x$_c (want 1)" >&2
        FAILS=$((FAILS + 1))
    elif [ "$_c" -eq 0 ]; then
        echo "FAIL [eq-artifact]: '$_line' missing" >&2
        FAILS=$((FAILS + 1))
    fi
}
check_unique "if (flag != OK) {"
check_unique "init_data();"
check_unique "done_old();"
check_unique "if (map) unmap(map);"
check_unique "Old comment line about processing"
check_unique "New comment about the processing step"

# -------------------------------------------------------------------
#  2. Inline highlight: line-level mods split into `-old/+new`, so
#     shared tokens may appear up to twice — guard 1 ≤ count ≤ 2.
# -------------------------------------------------------------------
run_diff "$SCRATCH/02.hili" "$CASE/02.hili.c.old" "$CASE/02.hili.c.new"

check_hili() {
    _line=$1
    _c=$(count "$OUT" "$_line")
    if [ "$_c" -gt 2 ]; then
        echo "FAIL [hili]: '$_line' x$_c (want 1 or 2)" >&2
        FAILS=$((FAILS + 1))
    elif [ "$_c" -eq 0 ]; then
        echo "FAIL [hili]: '$_line' missing" >&2
        FAILS=$((FAILS + 1))
    fi
}
check_hili "u32 tlo = (ti > 0) ?"
check_hili "u32 thi ="
check_hili "process(tlo, thi);"

# -------------------------------------------------------------------
#  3. NEIL cascade: renamed prefix tokens on consecutive lines must
#     keep their unchanged `(heap, &name);` tails as EQ, not DEL+INS.
# -------------------------------------------------------------------
run_diff "$SCRATCH/03.neil" "$CASE/03.neil.c.old" "$CASE/03.neil.c.new"

check_neil() {
    _line=$1
    _c=$(count "$OUT" "$_line")
    if [ "$_c" -gt 2 ]; then
        echo "FAIL [neil]: '$_line' x$_c (want 1 or 2)" >&2
        FAILS=$((FAILS + 1))
    elif [ "$_c" -eq 0 ]; then
        echo "FAIL [neil]: '$_line' missing" >&2
        FAILS=$((FAILS + 1))
    fi
}
check_neil "(heap, &count);"
check_neil "(heap, &index);"
check_neil "(heap, &value);"

# -------------------------------------------------------------------
#  4. Token boundary: shared `for (` prefix must not yield
#     `for (for (` or `for (size_t for (` concatenations on output.
# -------------------------------------------------------------------
run_diff "$SCRATCH/04.tokbnd" "$CASE/04.tokbnd.c.old" "$CASE/04.tokbnd.c.new"

check_absent() {
    _pat=$1
    if echo "$OUT" | grep -qF "$_pat"; then
        echo "FAIL [tokbnd]: '$_pat' present (concatenation bug)" >&2
        FAILS=$((FAILS + 1))
    fi
}
check_absent "for (for ("
check_absent "for (size_t for ("

if ! echo "$OUT" | grep -q '^-.*int i;'; then
    echo "FAIL [tokbnd]: no deletion line with 'int i;'" >&2
    FAILS=$((FAILS + 1))
fi
if ! echo "$OUT" | grep -qF 'for (size_t i = 0'; then
    echo "FAIL [tokbnd]: insertion 'for (size_t i = 0' missing" >&2
    FAILS=$((FAILS + 1))
fi

if [ "$FAILS" -gt 0 ]; then
    echo "FAIL: $FAILS check(s) failed" >&2
    exit 1
fi
