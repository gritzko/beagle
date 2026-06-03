#!/bin/sh
#  33-detached-refuse-commit — DIS-009 detached-wt no-commit invariant.
#
#  Model (maintainer, final):
#    * `?<sha>`  = sha in QUERY, fragment EMPTY → DETACHED.  POST and
#      PATCH must REFUSE until the wt re-attaches to a branch.
#    * `?#<sha>` = QUERY empty (=trunk), sha in FRAGMENT → trunk-state
#      (attached to trunk at that sha).  POST commits back to trunk —
#      legitimate, must NOT refuse.
#
#  The bug (pre-fix): GET wrote a detached checkout's wtlog row as
#  `?#<sha>` (sha moved into the fragment), aliasing trunk-state, so
#  the detached-wt detector never fired and POST/PATCH silently grafted
#  commits onto trunk/empty branch.  This case pins:
#    1. `be get <sha>` and `be get ?<sha>` both write `get ?<sha>`
#       (sha in QUERY, empty fragment).
#    2. POST on a detached wt refuses (POSTDET) and writes no commit row.
#    3. PATCH on a detached wt refuses (PATCHDET).
#    4. A trunk-state wt (`be get ?` → `?#<sha>`) still COMMITS on POST.

. "$(dirname "$0")/../../lib/case.sh"

# T0 baseline on trunk.
sleep 0.02; echo "v0" > x.txt
"$BE" put x.txt  > /dev/null
"$BE" post '#t0' > 01.post.err 2>&1
SHA=$(grep -oE '[0-9a-f]{40}' 01.post.err | head -1)
[ -n "$SHA" ] || { echo "FAIL: no commit sha from post"; cat 01.post.err; exit 1; }

# --- 1. `be get <sha>` writes a detached `?<sha>` row ----------------
"$BE" get "$SHA" > /dev/null 2>&1
ROW=$(tail -1 .be/wtlog | cut -f3)
[ "$ROW" = "?$SHA" ] || {
    echo "FAIL: bare-sha detached row should be '?$SHA', got '$ROW'" >&2
    exit 1
}

# --- `be get ?<sha>` writes the same detached `?<sha>` row -----------
"$BE" get "?$SHA" > /dev/null 2>&1
ROW=$(tail -1 .be/wtlog | cut -f3)
[ "$ROW" = "?$SHA" ] || {
    echo "FAIL: ?sha detached row should be '?$SHA', got '$ROW'" >&2
    exit 1
}

# --- 2. POST on a detached wt must REFUSE ----------------------------
sleep 0.02; echo "v1-detached" > x.txt
"$BE" put x.txt > /dev/null 2>&1
ROWS_BEFORE=$(wc -l < .be/wtlog)
set +e
"$BE" post '#graft' > 02.post.out 2> 02.post.err
RC=$?
set -e
[ "$RC" -ne 0 ] || {
    echo "FAIL: POST on detached wt should have refused (rc=$RC)" >&2
    cat 02.post.err >&2
    exit 1
}
grep -q "refusing on detached wt" 02.post.err || {
    echo "FAIL: expected detached-wt refusal in POST stderr" >&2
    cat 02.post.err >&2
    exit 1
}
# No commit row appended (the put row above is fine; only assert no post row).
tail -1 .be/wtlog | grep -q '	post	' && {
    echo "FAIL: refused POST appended a commit row" >&2
    tail -2 .be/wtlog >&2
    exit 1
}

# --- 3. PATCH on a detached wt must REFUSE ---------------------------
set +e
"$BE" patch '?master' > 03.patch.out 2> 03.patch.err
RC=$?
set -e
[ "$RC" -ne 0 ] || {
    echo "FAIL: PATCH on detached wt should have refused (rc=$RC)" >&2
    cat 03.patch.err >&2
    exit 1
}
grep -q "refusing on detached wt" 03.patch.err || {
    echo "FAIL: expected detached-wt refusal in PATCH stderr" >&2
    cat 03.patch.err >&2
    exit 1
}

# --- 4. Trunk-state wt still COMMITS ---------------------------------
#  Re-attach to trunk (`be get ?` writes `?#<sha>`, NOT detached) and
#  confirm the discard of the detached edit + a fresh trunk POST lands.
"$BE" get '?' > /dev/null 2>&1
ROW=$(tail -1 .be/wtlog | cut -f3)
case "$ROW" in
    "?#"*) ;;  # trunk-state, good
    *) echo "FAIL: trunk re-attach row should be '?#<sha>', got '$ROW'" >&2
       exit 1 ;;
esac
sleep 0.02; echo "v1-trunk" > x.txt
"$BE" put x.txt > /dev/null 2>&1
"$BE" post '#t1' > 04.post.out 2> 04.post.err
grep -qE '^sniff: commit [0-9a-f]{40}$' 04.post.err || {
    echo "FAIL: trunk-state POST should have committed" >&2
    cat 04.post.err >&2
    exit 1
}
