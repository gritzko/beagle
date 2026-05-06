#!/bin/sh
. "$(dirname "$0")/../../lib/case.sh"

# Outputs land in a sibling dir so they don't pollute the wt
# (PATCH treats unstamped files in cwd as dirty and refuses).
OUT="$SCRATCH/../out"
mkdir -p "$OUT"

# Spec workflow: PATCH absorbs source into cur.  The natural squash-
# merge pattern is "on parent, pull in fix" — i.e. on the parent
# branch, run `be patch ?./fix` to merge fix's stack into parent's wt,
# then `be post` to commit a single-parent commit on parent containing
# both branches' edits.

# Step 1: baseline on trunk — a.txt + b.txt
sleep 0.02; cp "$CASE/01.a.txt" a.txt
sleep 0.02; cp "$CASE/02.b.txt" b.txt
"$BE" put a.txt b.txt >/dev/null
"$BE" post 'baseline msg' >/dev/null

# Step 2: fork the child branch (cur stays on trunk)
"$BE" put '?./fix1' >/dev/null

# Step 3: switch to child, edit b.txt, commit
"$BE" get '?fix1' >/dev/null
sleep 0.02; cp "$CASE/04.b-fix.txt" b.txt
"$BE" put b.txt >/dev/null
"$BE" post 'c1 msg' >/dev/null

# Step 4: switch back to trunk, edit a.txt, commit (trunk diverges)
"$BE" get '?..' >/dev/null
sleep 0.02; cp "$CASE/03.a-trunk.txt" a.txt
"$BE" put a.txt >/dev/null
"$BE" post 't2 msg' >/dev/null

# Step 5: on trunk, patch fix1 in — absorb fix1's stack as a 3-way merge
"$BE" patch '?./fix1' >"$OUT/05.patch.got.out" 2>"$OUT/05.patch.got.err"

# Step 6: commit the merged state on trunk
"$BE" post 'merge msg' >"$OUT/06.post.got.out" 2>"$OUT/06.post.got.err"

# Step 7: assert wt content has both edits — trunk's a.txt edit AND
# fix1's b.txt edit.  Both files live in the wt.
match "$CASE/07.a.want.txt" a.txt
match "$CASE/08.b.want.txt" b.txt
