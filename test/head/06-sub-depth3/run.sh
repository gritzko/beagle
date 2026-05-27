#!/bin/sh
#  head/06-sub-depth3 — `be head ?` recurses through a 3-level
#  forest (parent → sub → leaf).  Asserts:
#    * every level's content materialises after the seeding `be get`.
#    * `be head ?` emits `be: head <relpath>` markers at every level
#      in pre-order (outer first, then deeper).
#    * read-only invariant: no level's wtlog grows after head.
#
#  Built on `test/lib/sub-deep.sh` which seeds three bare repos with
#  cross-pinned gitlinks.

. "$(dirname "$0")/../../lib/sub-deep.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

# --- Seed checkout: be get parent → recursively mounts sub + leaf. ---
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "seed be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f main.c ]                       || fail "parent main.c missing"
[ -f vendor/sub/sub.txt ]           || fail "sub.txt missing"
[ -f vendor/sub/.be ]               || fail "vendor/sub anchor missing"
[ -f vendor/sub/vendor/leaf/leaf.txt ] \
    || fail "leaf.txt missing — recursion didn't reach 3rd level"
[ -f vendor/sub/vendor/leaf/.be ]   || fail "leaf anchor missing"

# --- Snapshot wtlog tails at each level (read-only invariant). -------
parent_wt=$(wtlog_path)
parent_lines_before=$(wc -l < "$parent_wt")
sub_lines_before=$(wc -l   < vendor/sub/.be)
leaf_lines_before=$(wc -l  < vendor/sub/vendor/leaf/.be)

# --- be head ? — pre-order recursion across the forest. -------------
"$BE" head '?' >02.head.got.out 2>02.head.got.err
rc=$?
[ "$rc" = 0 ] || fail "be head exited $rc; stderr:
$(cat 02.head.got.err)"

#  Outer + 2nd-level markers.  3rd-level marker fires in a grandchild
#  process; stderr inherits all the way up so we see all of them in
#  02.head.got.err.
grep -q '^be: head [.]'                02.head.got.err \
    || fail "outer marker missing; stderr:
$(cat 02.head.got.err)"
grep -q '^be: head vendor/sub'         02.head.got.err \
    || fail "2nd-level marker missing"
grep -q '^be: head vendor/leaf'        02.head.got.err \
    || fail "3rd-level marker (vendor/leaf) missing"

#  Pre-order: outer before 2nd before 3rd.  Use line numbers; the
#  3rd-level marker comes from a grandchild process so multiple
#  `^be: head [.]` lines exist (each level's own outer).  We pin
#  only the relative ordering of the first occurrence of each.
outer_line=$(grep -n '^be: head [.]'         02.head.got.err | head -1 | cut -d: -f1)
sub_line=$(grep -n   '^be: head vendor/sub'  02.head.got.err | head -1 | cut -d: -f1)
leaf_line=$(grep -n  '^be: head vendor/leaf' 02.head.got.err | head -1 | cut -d: -f1)
[ -n "$outer_line" ] && [ -n "$sub_line" ] && [ -n "$leaf_line" ] \
    && [ "$outer_line" -lt "$sub_line" ] \
    && [ "$sub_line"   -lt "$leaf_line" ] \
    || fail "pre-order violated: outer=$outer_line sub=$sub_line leaf=$leaf_line"

# --- Read-only invariant at every level. -----------------------------
parent_lines_after=$(wc -l < "$parent_wt")
sub_lines_after=$(wc -l    < vendor/sub/.be)
leaf_lines_after=$(wc -l   < vendor/sub/vendor/leaf/.be)
[ "$parent_lines_after" = "$parent_lines_before" ] \
    || fail "parent wtlog grew: $parent_lines_before → $parent_lines_after"
[ "$sub_lines_after" = "$sub_lines_before" ] \
    || fail "sub wtlog grew: $sub_lines_before → $sub_lines_after"
[ "$leaf_lines_after" = "$leaf_lines_before" ] \
    || fail "leaf wtlog grew: $leaf_lines_before → $leaf_lines_after"

note "head/06-sub-depth3: outer + sub + leaf visited in pre-order"
