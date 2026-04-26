#!/bin/bash
# Test spot --diff output for artifacts (duplicated lines).
# Lines that exist in both old and new files should appear ONCE in diff output,
# never as both DEL+INS (which shows them twice without colors).
set -e

GRAF="${1:-./build-debug/graf/graf}"
DATADIR="$(dirname "$0")/data"

OLD="$DATADIR/diff_old.c"
NEW="$DATADIR/diff_new.c"

if [ ! -x "$GRAF" ]; then
    echo "FAIL: graf binary not found at $GRAF"
    exit 1
fi

OUT=$("$GRAF" diff "$OLD" "$NEW" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g')
FAILS=0

# Lines present in both old and new must appear at most once in diff output.
# These are context lines that should be EQ, not DEL+INS duplicates.
check_unique() {
    local line="$1"
    local count
    count=$(echo "$OUT" | grep -cF "$line" || true)
    if [ "$count" -gt 1 ]; then
        echo "FAIL: '$line' appears $count times (expected 1)"
        FAILS=$((FAILS + 1))
    elif [ "$count" -eq 0 ]; then
        echo "FAIL: '$line' not found in output"
        FAILS=$((FAILS + 1))
    else
        echo "  OK: '$line'"
    fi
}

echo "=== diff artifact test ==="
check_unique "if (flag != OK) {"
check_unique "init_data();"
check_unique "done_old();"
check_unique "if (map) unmap(map);"

# Lines only in old should appear at most once (as DEL)
# With word-level comment splitting, "// " is shared EQ prefix.
check_unique "Old comment line about processing"
# Lines only in new should appear at most once (as INS)
check_unique "New comment about the processing step"

if [ "$FAILS" -gt 0 ]; then
    echo "FAILED: $FAILS checks failed"
    echo "--- full output ---"
    echo "$OUT"
    exit 1
fi

echo "PASSED"

# --- inline highlighting (hili) test ---
# When a single token changes mid-line, the context prefix must appear
# exactly once — no duplicate full-line copies.

HILI_OLD="$DATADIR/hili_old.c"
HILI_NEW="$DATADIR/hili_new.c"

HOUT=$("$GRAF" diff "$HILI_OLD" "$HILI_NEW" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g')
HFAILS=0

echo "=== inline highlight test ==="

# The line prefix before the changed token must appear exactly once
check_hili() {
    local line="$1"
    local count
    count=$(echo "$HOUT" | grep -cF "$line" || true)
    if [ "$count" -gt 1 ]; then
        echo "FAIL: '$line' appears $count times (expected 1)"
        HFAILS=$((HFAILS + 1))
    elif [ "$count" -eq 0 ]; then
        echo "FAIL: '$line' not found in output"
        HFAILS=$((HFAILS + 1))
    else
        echo "  OK: '$line'"
    fi
}

check_hili "u32 tlo = (ti > 0) ?"
check_hili "u32 thi ="
check_hili "process(tlo, thi);"

if [ "$HFAILS" -gt 0 ]; then
    echo "FAILED: $HFAILS inline highlight checks failed"
    echo "--- full hili output ---"
    echo "$HOUT"
    exit 1
fi

echo "PASSED"

# Color rendering moved to bro/ — coloring tests live there now.

# --- NEIL cascade test ---
# When consecutive lines change (MSTXxx -> HITXxx), the unchanged arguments
# like "(heap, &count);" must NOT be duplicated as DEL+INS.  NEIL must
# protect EQ regions that contain a code line before the newline.

NEIL_OLD="$DATADIR/neil_old.c"
NEIL_NEW="$DATADIR/neil_new.c"

NOUT=$("$GRAF" diff "$NEIL_OLD" "$NEIL_NEW" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g')
NFAILS=0

echo "=== NEIL cascade test ==="

check_neil() {
    local line="$1"
    local count
    count=$(echo "$NOUT" | grep -cF "$line" || true)
    if [ "$count" -gt 1 ]; then
        echo "FAIL: '$line' appears $count times (expected 1)"
        NFAILS=$((NFAILS + 1))
    elif [ "$count" -eq 0 ]; then
        echo "FAIL: '$line' not found in output"
        NFAILS=$((NFAILS + 1))
    else
        echo "  OK: '$line'"
    fi
}

# Shared arguments between renamed calls must appear once (EQ), not twice
check_neil "(heap, &count);"
check_neil "(heap, &index);"
check_neil "(heap, &value);"

if [ "$NFAILS" -gt 0 ]; then
    echo "FAILED: $NFAILS NEIL cascade checks failed"
    echo "--- full output ---"
    echo "$NOUT"
    exit 1
fi

echo "PASSED"

# --- Token boundary test ---
# When `for (i = 0` becomes `for (size_t i = 0`, the shared prefix
# `for (` must not cause the DEL and INS content to concatenate on
# the same output line.  `for (` must appear exactly once in context
# or deletion, never as `for (for (`.

TOKBND_OLD="$DATADIR/tokbnd_old.c"
TOKBND_NEW="$DATADIR/tokbnd_new.c"

TOUT=$("$GRAF" diff "$TOKBND_OLD" "$TOKBND_NEW" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g')
TFAILS=0

echo "=== token boundary test ==="

check_tokbnd() {
    local pattern="$1"
    local desc="$2"
    if echo "$TOUT" | grep -qF "$pattern"; then
        echo "FAIL: found '$pattern' — $desc"
        TFAILS=$((TFAILS + 1))
    else
        echo "  OK: no '$pattern'"
    fi
}

# These concatenations must NOT appear
check_tokbnd "for (for (" "DEL/INS concatenated"
check_tokbnd "for (size_t for (" "INS/DEL concatenated"

# The deletion must contain 'for (' (the shared prefix that's deleted
# because the old line structure differs).  Token-level diff may split
# the old line differently than line-level diff.
if ! echo "$TOUT" | grep -q '^-.*for ('; then
    echo "FAIL: no deletion line with 'for (' found"
    TFAILS=$((TFAILS + 1))
else
    echo "  OK: deletion with 'for (' present"
fi

# The correct insertion line must exist
if ! echo "$TOUT" | grep -qF 'for (size_t i = 0'; then
    echo "FAIL: insertion 'for (size_t i = 0' not found"
    TFAILS=$((TFAILS + 1))
else
    echo "  OK: insertion 'for (size_t i = 0' present"
fi

if [ "$TFAILS" -gt 0 ]; then
    echo "FAILED: $TFAILS token boundary checks failed"
    echo "--- full output ---"
    echo "$TOUT"
    exit 1
fi

echo "PASSED"
