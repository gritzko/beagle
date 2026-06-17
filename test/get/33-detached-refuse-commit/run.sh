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
#    5. GET-024: `be get ?#<sha>` (pin trunk to a STATE sha) writes a
#       trunk-state `?#<sha>` row — NOT the detached `?<sha>` form — so a
#       follow-up POST commits (the regression `be-put-06-triangle` R4 hit).

. "$(dirname "$0")/../../lib/case.sh"

# T0 baseline on trunk.
sleep 0.02; echo "v0" > x.txt
"$BE" put x.txt  > /dev/null
"$BE" post '#t0' > 01.post.err 2>&1
#  POST-018: the commit confirmation is a ROWS `post ?<hashlet>#<subj>`
#  row (8-hex hashlet), not a raw `sniff: commit <40hex>` line.  Read the
#  canonical full 40-char sha from the wtlog post row's `?#<sha>` tail.
SHA=$(awk -F'\t' '$2=="post"{l=$3} END{h=l;sub(/^[^#]*#/,"",h);print h}' .be/wtlog)
[ "${#SHA}" = 40 ] || { echo "FAIL: no commit sha from post"; cat 01.post.err; tail -2 .be/wtlog; exit 1; }

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
#  POST-018: the commit row rides STDOUT now (the `post:` banner hunk),
#  not stderr.
"$BE" post '#t1' > 04.post.out 2> 04.post.err
grep -qE 'post[[:space:]]+\??[0-9a-f]{6,}#t1' 04.post.out || {
    echo "FAIL: trunk-state POST should have committed" >&2
    cat 04.post.out 04.post.err >&2
    exit 1
}

# --- 5. GET-024: `be get ?#<sha>` is TRUNK-STATE, not detached -------
#  Pin trunk to an earlier STATE sha (the original t0 commit, an
#  ancestor of the current tip).  Pre-fix, the GET-023 arm recorded the
#  detached `?<sha>` row → POST refused POSTDET.  It must record the
#  trunk-state `?#<sha>` row and let the next POST commit on top.
"$BE" get "?#$SHA" > /dev/null 2>&1
ROW=$(tail -1 .be/wtlog | cut -f3)
[ "$ROW" = "?#$SHA" ] || {
    echo "FAIL: GET-024 ?#sha row should be '?#$SHA' (trunk-state), got '$ROW'" >&2
    exit 1
}
sleep 0.02; echo "v2-pinned" > x.txt
"$BE" put x.txt > /dev/null 2>&1
"$BE" post '#t2' > 05.post.out 2> 05.post.err
grep -qE 'post[[:space:]]+\??[0-9a-f]{6,}#t2' 05.post.out || {
    echo "FAIL: GET-024 trunk-state ?#sha POST should have committed (not POSTDET)" >&2
    cat 05.post.out 05.post.err >&2
    exit 1
}

# --- 6. GET-025: `be get ?#<SHORT sha>` records the FULL 40-char tip --
#  A short hashlet must be EXPANDED to the resolved 40-char sha when the
#  cur-tip row is recorded.  Pre-fix the raw input went into the row
#  (`?#<short>`), so the baseline resolver (40-char only) read EMPTY —
#  whole tree `new`, keeper-tip append silently rejected, `be head` stuck.
SHORT=$(printf '%s' "$SHA" | cut -c1-8)
"$BE" get "?#$SHORT" > 06.get.err 2>&1
ROW=$(tail -1 .be/wtlog | cut -f3)
[ "$ROW" = "?#$SHA" ] || {
    echo "FAIL: GET-025 ?#<short> must expand to full '?#$SHA', got '$ROW'" >&2
    exit 1
}
sleep 0.02; echo "v3-short" > x.txt
"$BE" put x.txt > /dev/null 2>&1
"$BE" post '#t3' > 06.post.err 2>&1
grep -qE 'post[[:space:]]+\??[0-9a-f]{6,}#t3' 06.post.err || {
    echo "FAIL: GET-025 short-sha trunk-state POST should have committed" >&2
    cat 06.post.err >&2
    exit 1
}
