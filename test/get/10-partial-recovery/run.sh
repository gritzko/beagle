#!/bin/sh
#  10-partial-recovery — atomicity invariant (positive half).
#
#  GETCheckout appends the `get` ULOG row + advances the local
#  REFS BEFORE WALKTreeLazy mutates the wt.  When the mutation
#  phase fails partway (e.g., a write-protected subdir denies
#  FILECreate), the baseline still reflects the target tip — the
#  wt is the only thing in flux, and a follow-up
#  `be get --force ?<branch>` completes the materialisation
#  idempotently.
#
#  Setup: T0 has files in `locked/` + outside `locked/`.  T1
#  modifies both.  Get back to T0, `chmod 555 locked/`, try to
#  advance to T1.  GET must fail (write into locked/ is denied);
#  `.be/wtlog` must have grown (commit point fired); `be get
#  --force ?feat` must then bring the wt to T1.

. "$(dirname "$0")/../../lib/case.sh"

# T0: file inside `locked/` + outside file.
mkdir locked
sleep 0.02; echo "v1"        >  locked/file.txt
sleep 0.02; echo "outer-v1"  >  outer.txt
"$BE" put locked/file.txt outer.txt > /dev/null
sleep 0.02   # ratchet catch-up after the 2-row put tail
"$BE" post '#t0' > /dev/null

# Fork ?feat and advance to T1 (both files rewritten).
"$BE" put '?./feat' > /dev/null
"$BE" get '?feat'   > /dev/null
sleep 0.02; echo "v2"        >  locked/file.txt
sleep 0.02; echo "outer-v2"  >  outer.txt
"$BE" put locked/file.txt outer.txt > /dev/null
sleep 0.02
"$BE" post '#t1' > /dev/null

# Back to trunk (T0).  Sanity-check the wt rolled back.
"$BE" get '?..' > /dev/null
[ "$(cat locked/file.txt)" = "v1" ] || {
    echo "FAIL: trunk locked/file.txt should be v1" >&2; exit 1
}
[ "$(cat outer.txt)" = "outer-v1" ] || {
    echo "FAIL: trunk outer.txt should be outer-v1" >&2; exit 1
}

# Snapshot wtlog length so we can prove the commit point fired
# even on a failing GET.
ROWS_BEFORE=$(wc -l < .be/wtlog)

# Lock both the subdir AND the existing file: WALKTreeLazy's
# unlink-then-O_CREAT|O_TRUNC sequence needs write on the dir to
# remove the entry and write on the file to truncate.  Locking only
# the dir lets O_TRUNC succeed via the file's own write bit; we
# revoke both so the open() in get_write_one fails with EACCES.
chmod 444 locked/file.txt
chmod 555 locked

# Try to advance to ?feat.  WALKTreeLazy fails on locked/file.txt.
set +e
"$BE" get '?feat' > 01.get.got.out 2> 01.get.got.err
RC=$?
set -e
chmod 755 locked              # always restore, even on failure path
chmod 644 locked/file.txt 2>/dev/null || true

[ "$RC" -ne 0 ] || {
    echo "FAIL: GET into locked subdir should have failed (rc=$RC)" >&2
    cat 01.get.got.err >&2
    exit 1
}

# THE ATOMICITY CLAIM: the commit point fired before the wt-write
# failure, so `.be/wtlog` now carries the T1 `get` row even though
# the materialisation aborted.  Pre-reorder, this row was only
# written at the END of GETCheckout, so a failing write would have
# left wtlog at T0 — recovery semantics impossible to define.
ROWS_AFTER=$(wc -l < .be/wtlog)
[ "$ROWS_AFTER" -gt "$ROWS_BEFORE" ] || {
    echo "FAIL: wtlog did not grow after a failing GET — the" >&2
    echo "       commit point did not fire ahead of WALKTreeLazy." >&2
    echo "       before=$ROWS_BEFORE after=$ROWS_AFTER" >&2
    exit 1
}

# Recovery: `be get --force` skips the noop overlay, overwrites
# every target path, and brings the wt fully to T1.  This is the
# documented recovery path for a baseline-correct/wt-mid-flux state.
"$BE" get --force '?feat' > 02.recover.got.out 2> 02.recover.got.err
[ "$(cat locked/file.txt)" = "v2" ] || {
    echo "FAIL: after --force recovery, locked/file.txt should be v2" >&2
    echo "      got '$(cat locked/file.txt)'" >&2
    exit 1
}
[ "$(cat outer.txt)" = "outer-v2" ] || {
    echo "FAIL: after --force recovery, outer.txt should be outer-v2" >&2
    echo "      got '$(cat outer.txt)'" >&2
    exit 1
}
