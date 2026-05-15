#!/bin/sh
#  09-refuse-no-commit — atomicity invariant (negative half).
#
#  GETCheckout was reordered so the `get` ULOG row and the local
#  branch REFS advance fire BEFORE any wt mutation.  The flip side
#  is that pre-flight refusals must run BEFORE that commit point —
#  otherwise a refused GET would still leave a stray `get` row
#  whose tip the wt doesn't actually match.  This case pins down
#  the cross-branch-dirty refusal (SNIFFDRTY): a dirty wt rejecting
#  a switch must not append anything to `.be/wtlog`.
#
#  Setup: T0 on trunk, fork + advance ?feat so the branches
#  diverge, switch back to trunk, dirty the wt without staging.
#  Then `be get ?feat` must refuse and the wtlog row count must be
#  unchanged.

. "$(dirname "$0")/../../lib/case.sh"

# T0 baseline on trunk.
sleep 0.02; echo "v0" > x.txt
"$BE" put x.txt  > /dev/null
"$BE" post '#t0' > /dev/null

# Fork ?feat and advance it so trunk ≠ feat.
"$BE" put '?./feat' > /dev/null
"$BE" get '?feat'   > /dev/null
sleep 0.02; echo "v0-feat" > x.txt
"$BE" put x.txt    > /dev/null
"$BE" post '#feat' > /dev/null

# Back to trunk; sanity-check the rollback.
"$BE" get '?..' > /dev/null
[ "$(cat x.txt)" = "v0" ] || {
    echo "FAIL: trunk x.txt should be v0 after switch back, got '$(cat x.txt)'" >&2
    exit 1
}

# Dirty the wt (unstaged user edit).
sleep 0.02; echo "dirty" > x.txt

# Snapshot wtlog length: the refusal below must not grow it.
ROWS_BEFORE=$(wc -l < .be/wtlog)

# Cross-branch GET on a dirty wt — must refuse with SNIFFDRTY.
set +e
"$BE" get '?feat' > 01.get.got.out 2> 01.get.got.err
RC=$?
set -e
[ "$RC" -ne 0 ] || {
    echo "FAIL: cross-branch GET on dirty wt should have refused (rc=$RC)" >&2
    cat 01.get.got.err >&2
    exit 1
}
grep -q "wt is dirty" 01.get.got.err || {
    echo "FAIL: expected 'wt is dirty' message in stderr" >&2
    cat 01.get.got.err >&2
    exit 1
}

# Atomicity: refusal must not have written a `get` row.
ROWS_AFTER=$(wc -l < .be/wtlog)
[ "$ROWS_BEFORE" = "$ROWS_AFTER" ] || {
    echo "FAIL: refused GET appended wtlog row(s) " \
         "(was $ROWS_BEFORE, now $ROWS_AFTER)" >&2
    tail -2 .be/wtlog >&2
    exit 1
}

# Atomicity: refusal must not have touched wt content.
[ "$(cat x.txt)" = "dirty" ] || {
    echo "FAIL: refused GET mutated wt content (now '$(cat x.txt)')" >&2
    exit 1
}
