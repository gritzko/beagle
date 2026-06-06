#!/bin/sh
#  post/28-untracked-alias-not-swept — REPRO for POST-005.  A commit-all
#  `be post` must NOT sweep an untracked file into the commit just because
#  its mtime aliases an absorbed (patch / sub-bump put) row's stamp.  POST
#  resolves a stamp by timestamp alone (SNIFFAtRowAtTs, never checking the
#  row's path), so an untracked sibling whose mtime collides with a
#  patch-stamped file is misattributed as that row's work and silently
#  ADDed to the commit (data inclusion the user never staged).
#
#  Deterministic collision: absorb a child branch that adds `feature.c`
#  (PATCH stamps it with the patch row's ts), then `touch -r feature.c
#  junk.txt` so junk.txt carries the exact same mtime, then commit-all.
#  Registered WILL_FAIL until POST-005 lands.

. "$(dirname "$0")/../../lib/branches.sh"

#  trunk: one tracked file + a baseline commit.
printf 'int tracked(void){return 1;}\n' > tracked.c
"$BE" put tracked.c >/dev/null
"$BE" post '#initial' >/dev/null

#  child branch `b` adds a brand-new file feature.c.
"$BE" put '?./b' >/dev/null
"$BE" get '?./b' >/dev/null
sleep 0.02
printf 'int feature(void){return 2;}\n' > feature.c
"$BE" put feature.c >/dev/null
"$BE" post '#feat' >/dev/null
"$BE" get '?..' >/dev/null
[ -f feature.c ] && fail "feature.c should be absent on trunk before patch"

#  Squash-absorb b into the wt: PATCH writes feature.c and stamps it with
#  the patch row's ts.
sleep 0.02
"$BE" patch '?./b' >"$ETMP/p.out" 2>"$ETMP/p.err" \
    || fail "be patch ?./b failed: $(cat "$ETMP/p.err")"
[ -f feature.c ] || fail "feature.c not absorbed into the wt"

#  Untracked junk aliased to feature.c's patch stamp (exact mtime).
printf 'JUNK MUST NOT BE COMMITTED\n' > junk.txt
touch -r feature.c junk.txt

#  Commit-all post: a `patch` row is in scope, so POST runs commit-all
#  (NOT selective).  feature.c (absorbed) must land; junk.txt must not.
"$BE" post '#absorb b' >"$ETMP/c.out" 2>"$ETMP/c.err" \
    || fail "be post failed: $(cat "$ETMP/c.err")"
R=$(head_hex)

"$BE" tree:"?$R" >"$ETMP/tree.out" 2>&1 \
    || fail "be tree:?$R failed: $(cat "$ETMP/tree.out")"

#  No-regression: the absorbed file MUST be committed.
grep -q 'feature\.c' "$ETMP/tree.out" \
    || fail "regression: absorbed feature.c missing from the commit:
$(cat "$ETMP/tree.out")"

#  The fix: the aliased untracked file MUST NOT be committed.
if grep -q 'junk\.txt' "$ETMP/tree.out"; then
    fail "POST-005: untracked junk.txt was swept into the commit via mtime \
alias; committed tree:
$(cat "$ETMP/tree.out")"
fi

note "post/28 ok: aliased untracked junk.txt kept out; absorbed feature.c committed"
echo "=== post/28-untracked-alias-not-swept: OK ==="
